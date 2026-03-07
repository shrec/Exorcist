#include "workspaceindexer.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QTextStream>
#include <QThread>
#include <QThreadPool>
#include <QTimer>

// Directories to always skip regardless of .gitignore
static const QSet<QString> kDefaultIgnoreDirs = {
    QStringLiteral(".git"),
    QStringLiteral("node_modules"),
    QStringLiteral("__pycache__"),
    QStringLiteral(".cache"),
    QStringLiteral(".vs"),
    QStringLiteral(".vscode"),
    QStringLiteral("build"),
    QStringLiteral("build-llvm"),
    QStringLiteral("dist"),
    QStringLiteral("out"),
    QStringLiteral(".next"),
};

// File extensions to index (source code + config — not binary)
static const QSet<QString> kIndexableExtensions = {
    // C/C++
    QStringLiteral("c"), QStringLiteral("cpp"), QStringLiteral("cc"),
    QStringLiteral("cxx"), QStringLiteral("h"), QStringLiteral("hpp"),
    QStringLiteral("hxx"),
    // C#
    QStringLiteral("cs"),
    // Java/Kotlin
    QStringLiteral("java"), QStringLiteral("kt"), QStringLiteral("kts"),
    // Python
    QStringLiteral("py"),
    // JavaScript/TypeScript
    QStringLiteral("js"), QStringLiteral("ts"), QStringLiteral("jsx"),
    QStringLiteral("tsx"), QStringLiteral("mjs"), QStringLiteral("cjs"),
    // Rust
    QStringLiteral("rs"),
    // Go
    QStringLiteral("go"),
    // Swift
    QStringLiteral("swift"),
    // Ruby
    QStringLiteral("rb"),
    // PHP
    QStringLiteral("php"),
    // Dart
    QStringLiteral("dart"),
    // Lua
    QStringLiteral("lua"),
    // Shell
    QStringLiteral("sh"), QStringLiteral("bash"), QStringLiteral("zsh"),
    QStringLiteral("ps1"),
    // Config / markup
    QStringLiteral("json"), QStringLiteral("yaml"), QStringLiteral("yml"),
    QStringLiteral("toml"), QStringLiteral("xml"), QStringLiteral("html"),
    QStringLiteral("css"), QStringLiteral("scss"), QStringLiteral("less"),
    QStringLiteral("sql"), QStringLiteral("graphql"),
    QStringLiteral("proto"),
    // Build
    QStringLiteral("cmake"), QStringLiteral("pro"), QStringLiteral("qbs"),
    // Documentation
    QStringLiteral("md"), QStringLiteral("txt"), QStringLiteral("rst"),
};

static constexpr int kMaxFileSize = 512 * 1024;  // 512 KB
static constexpr int kChunkLines  = 60;           // default chunk size

WorkspaceIndexer::WorkspaceIndexer(QObject *parent)
    : QObject(parent)
{
}

WorkspaceIndexer::~WorkspaceIndexer()
{
    cancel();
}

void WorkspaceIndexer::indexWorkspace(const QString &rootPath)
{
    cancel();
    m_rootPath = rootPath;
    m_cancelled = false;

    loadGitignore(rootPath);

    // Set up file watcher
    if (!m_watcher) {
        m_watcher = new QFileSystemWatcher(this);
        connect(m_watcher, &QFileSystemWatcher::fileChanged,
                this, &WorkspaceIndexer::reindexFile);
    }

    // Run scan on a background thread so the UI stays responsive
    auto *runnable = QRunnable::create([this] { runScan(); });
    QThreadPool::globalInstance()->start(runnable);
}

void WorkspaceIndexer::cancel()
{
    m_cancelled = true;
}

void WorkspaceIndexer::runScan()
{
    m_indexing = true;
    emit indexingStarted();

    QMutexLocker lock(&m_mutex);
    m_chunks.clear();
    m_files.clear();
    lock.unlock();

    // Collect all indexable files
    QStringList filesToIndex;
    QDir root(m_rootPath);
    QDirIterator it(m_rootPath, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        if (m_cancelled) break;
        const QString path = it.next();
        const QString rel  = root.relativeFilePath(path);

        // Check extension
        const QString ext = QFileInfo(path).suffix().toLower();
        if (!kIndexableExtensions.contains(ext))
            continue;

        // Check ignore rules
        if (shouldIgnore(rel))
            continue;

        // Check file size
        if (QFileInfo(path).size() > kMaxFileSize)
            continue;

        filesToIndex << path;
    }

    const int total = filesToIndex.size();
    int processed = 0;

    for (const QString &path : std::as_const(filesToIndex)) {
        if (m_cancelled) break;

        processFile(path);
        ++processed;

        if (processed % 50 == 0)
            emit indexingProgress(processed, total);
    }

    // Add files to watcher on the main thread (limited to 500 to avoid OS handle exhaustion)
    const int watchLimit = qMin(filesToIndex.size(), 500);
    if (watchLimit > 0) {
        const QStringList watchPaths = filesToIndex.mid(0, watchLimit);
        QMetaObject::invokeMethod(this, [this, watchPaths]() {
            if (m_watcher)
                m_watcher->addPaths(watchPaths);
        }, Qt::QueuedConnection);
    }

    m_indexing = false;

    lock.relock();
    emit indexingFinished(m_files.size(), m_chunks.size());
}

void WorkspaceIndexer::processFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    const QString content = in.readAll();

    QVector<IndexChunk> chunks = chunkFile(filePath, content);

    QMutexLocker lock(&m_mutex);
    // Remove existing chunks for this file
    m_chunks.erase(
        std::remove_if(m_chunks.begin(), m_chunks.end(),
                       [&](const IndexChunk &c) { return c.filePath == filePath; }),
        m_chunks.end());

    m_chunks.append(chunks);

    if (!m_files.contains(filePath))
        m_files.append(filePath);
}

void WorkspaceIndexer::reindexFile(const QString &filePath)
{
    if (!QFile::exists(filePath))
        return;

    processFile(filePath);
    emit fileReindexed(filePath);
}

QVector<IndexChunk> WorkspaceIndexer::chunkFile(const QString &filePath,
                                                 const QString &content) const
{
    QVector<IndexChunk> result;
    const QStringList lines = content.split(QLatin1Char('\n'));

    if (lines.isEmpty())
        return result;

    const QString ext = QFileInfo(filePath).suffix().toLower();

    // Language-aware boundary detection regex
    //   C/C++/Java/C#/Dart/Go/Rust: function/class/struct definitions
    //   Python: def/class
    //   JS/TS: function/class/const.*=>
    static const QRegularExpression cppBoundary(
        QStringLiteral(R"(^(?:\s*(?:(?:static|virtual|inline|explicit|constexpr|override|const)\s+)*)"
                       R"((?:(?:void|int|bool|char|auto|float|double|unsigned|class|struct|enum|namespace)\b)"
                       R"(|(?:[A-Z]\w*(?:<[^>]*>)?(?:::\w+)*\s+\w+\s*\())))"));
    static const QRegularExpression pyBoundary(
        QStringLiteral(R"(^(?:def |class )\w+)"));
    static const QRegularExpression jsBoundary(
        QStringLiteral(R"(^(?:(?:export\s+)?(?:function|class|const|let|var|async function)\s+\w+))"));
    static const QRegularExpression rustBoundary(
        QStringLiteral(R"(^(?:(?:pub\s+)?(?:fn |struct |enum |impl |trait |mod )\w+))"));
    static const QRegularExpression goBoundary(
        QStringLiteral(R"(^(?:func |type |var )\w+)"));

    const QRegularExpression *boundary = nullptr;
    if (ext == QLatin1String("c") || ext == QLatin1String("cpp") ||
        ext == QLatin1String("cc") || ext == QLatin1String("cxx") ||
        ext == QLatin1String("h") || ext == QLatin1String("hpp") ||
        ext == QLatin1String("java") || ext == QLatin1String("cs") ||
        ext == QLatin1String("dart"))
        boundary = &cppBoundary;
    else if (ext == QLatin1String("py"))
        boundary = &pyBoundary;
    else if (ext == QLatin1String("js") || ext == QLatin1String("ts") ||
             ext == QLatin1String("jsx") || ext == QLatin1String("tsx"))
        boundary = &jsBoundary;
    else if (ext == QLatin1String("rs"))
        boundary = &rustBoundary;
    else if (ext == QLatin1String("go"))
        boundary = &goBoundary;

    // Split into chunks
    int chunkStart = 0;
    QString currentSymbol;

    for (int i = 0; i < lines.size(); ++i) {
        bool isBoundary = false;
        QString symbol;

        if (boundary) {
            auto match = boundary->match(lines[i]);
            if (match.hasMatch() && i > chunkStart) {
                isBoundary = true;
                // Extract symbol name — first word-like identifier
                static const QRegularExpression nameRx(QStringLiteral(R"(\b(\w+)\s*\()"));
                auto nm = nameRx.match(lines[i]);
                if (nm.hasMatch())
                    symbol = nm.captured(1);
            }
        }

        // Create chunk at boundary or when reaching max chunk size
        const bool atMaxSize = (i - chunkStart >= kChunkLines);
        if ((isBoundary || atMaxSize) && i > chunkStart) {
            IndexChunk chunk;
            chunk.filePath   = filePath;
            chunk.startLine  = chunkStart;
            chunk.endLine    = i - 1;
            chunk.symbolName = currentSymbol;
            QStringList chunkLines;
            for (int j = chunkStart; j < i && j < lines.size(); ++j)
                chunkLines << lines[j];
            chunk.content = chunkLines.join(QLatin1Char('\n'));
            result.append(chunk);

            chunkStart = i;
            currentSymbol = symbol;
        } else if (isBoundary) {
            currentSymbol = symbol;
        }
    }

    // Last chunk
    if (chunkStart < lines.size()) {
        IndexChunk chunk;
        chunk.filePath   = filePath;
        chunk.startLine  = chunkStart;
        chunk.endLine    = lines.size() - 1;
        chunk.symbolName = currentSymbol;
        QStringList chunkLines;
        for (int j = chunkStart; j < lines.size(); ++j)
            chunkLines << lines[j];
        chunk.content = chunkLines.join(QLatin1Char('\n'));
        result.append(chunk);
    }

    return result;
}

QVector<IndexChunk> WorkspaceIndexer::search(const QString &query, int maxResults) const
{
    QVector<IndexChunk> results;
    const QString lower = query.toLower();

    QMutexLocker lock(&m_mutex);
    for (const IndexChunk &chunk : m_chunks) {
        if (chunk.content.toLower().contains(lower) ||
            chunk.symbolName.toLower().contains(lower)) {
            results.append(chunk);
            if (results.size() >= maxResults)
                break;
        }
    }
    return results;
}

QStringList WorkspaceIndexer::allFiles() const
{
    QMutexLocker lock(&m_mutex);
    return m_files;
}

int WorkspaceIndexer::indexedFileCount() const
{
    QMutexLocker lock(&m_mutex);
    return m_files.size();
}

int WorkspaceIndexer::totalFileCount() const
{
    return indexedFileCount();  // after scan, these are the same
}

void WorkspaceIndexer::loadGitignore(const QString &rootPath)
{
    m_ignorePatterns.clear();
    m_ignoreExact = kDefaultIgnoreDirs;

    QFile gitignore(QDir(rootPath).filePath(QStringLiteral(".gitignore")));
    if (!gitignore.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&gitignore);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        // Simple patterns: directory names or extension globs
        if (!line.contains(QLatin1Char('*')) && !line.contains(QLatin1Char('?'))) {
            // Treat as exact directory/file name
            if (line.endsWith(QLatin1Char('/')))
                line.chop(1);
            m_ignoreExact.insert(line);
        } else {
            m_ignorePatterns << line;
        }
    }
}

bool WorkspaceIndexer::shouldIgnore(const QString &relativePath) const
{
    // Check each path component against exact ignore set
    const QStringList parts = relativePath.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (m_ignoreExact.contains(part))
            return true;
    }

    // Check glob patterns (simple *.ext matching)
    const QString fileName = parts.last();
    for (const QString &pattern : m_ignorePatterns) {
        if (pattern.startsWith(QStringLiteral("*."))) {
            // Extension pattern
            const QString ext = pattern.mid(2);
            if (fileName.endsWith(QLatin1Char('.') + ext))
                return true;
        }
    }

    return false;
}
