#include "workspacechunkindex.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <algorithm>

void WorkspaceChunkIndex::buildIndex(const QString &root)
{
    m_root = root;
    m_chunks.clear();
    emit indexingStarted();

    QStringList files;
    collectFiles(QDir(root), files);

    int processed = 0;
    for (const auto &file : files) {
        indexFile(file);
        if (++processed % 50 == 0)
            emit indexProgress(processed, files.size());
    }

    emit indexingFinished(m_chunks.size());
}

void WorkspaceChunkIndex::reindexFile(const QString &filePath)
{
    m_chunks.erase(std::remove_if(m_chunks.begin(), m_chunks.end(),
        [&](const CodeChunk &c) { return c.filePath == filePath; }),
        m_chunks.end());
    indexFile(filePath);
}

void WorkspaceChunkIndex::removeFile(const QString &filePath)
{
    m_chunks.erase(std::remove_if(m_chunks.begin(), m_chunks.end(),
        [&](const CodeChunk &c) { return c.filePath == filePath; }),
        m_chunks.end());
}

QList<CodeChunk> WorkspaceChunkIndex::search(const QString &query, int maxResults) const
{
    QList<CodeChunk> results;
    const QStringList terms = query.toLower().split(QRegularExpression(QStringLiteral("\\s+")),
                                                     Qt::SkipEmptyParts);

    struct ScoredChunk {
        int score;
        int index;
    };
    QList<ScoredChunk> scored;

    for (int i = 0; i < m_chunks.size(); ++i) {
        const auto &chunk = m_chunks[i];
        int score = 0;
        const QString lower = chunk.content.toLower();
        const QString symbolLower = chunk.symbol.toLower();

        for (const auto &term : terms) {
            if (symbolLower.contains(term)) score += 10;
            score += lower.count(term) * 2;
            if (chunk.filePath.contains(term, Qt::CaseInsensitive)) score += 5;
        }
        if (score > 0)
            scored.append({score, i});
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredChunk &a, const ScoredChunk &b) { return a.score > b.score; });

    for (int i = 0; i < qMin(maxResults, scored.size()); ++i)
        results.append(m_chunks[scored[i].index]);

    return results;
}

QList<CodeChunk> WorkspaceChunkIndex::searchSymbol(const QString &name) const
{
    QList<CodeChunk> results;
    for (const auto &chunk : m_chunks) {
        if (chunk.symbol.contains(name, Qt::CaseInsensitive))
            results.append(chunk);
    }
    return results;
}

bool WorkspaceChunkIndex::saveIndex(const QString &indexPath) const
{
    QJsonArray arr;
    for (const auto &c : m_chunks) {
        QJsonObject obj;
        obj[QStringLiteral("file")] = c.filePath;
        obj[QStringLiteral("start")] = c.startLine;
        obj[QStringLiteral("end")] = c.endLine;
        obj[QStringLiteral("symbol")] = c.symbol;
        obj[QStringLiteral("lang")] = c.language;
        obj[QStringLiteral("hash")] = QString::fromLatin1(c.contentHash.toHex());
        arr.append(obj);
    }
    QFile file(indexPath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    return true;
}

bool WorkspaceChunkIndex::loadIndex(const QString &indexPath, const QString &root)
{
    m_root = root;
    QFile file(indexPath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    m_chunks.clear();
    m_chunks.reserve(arr.size());

    for (const auto &item : arr) {
        const QJsonObject obj = item.toObject();
        CodeChunk c;
        c.filePath = obj.value(QStringLiteral("file")).toString();
        c.startLine = obj.value(QStringLiteral("start")).toInt();
        c.endLine = obj.value(QStringLiteral("end")).toInt();
        c.symbol = obj.value(QStringLiteral("symbol")).toString();
        c.language = obj.value(QStringLiteral("lang")).toString();
        c.contentHash = QByteArray::fromHex(
            obj.value(QStringLiteral("hash")).toString().toLatin1());

        QFile src(c.filePath);
        if (src.open(QIODevice::ReadOnly)) {
            const QStringList lines = QString::fromUtf8(src.readAll()).split(QLatin1Char('\n'));
            QStringList chunkLines;
            for (int l = c.startLine; l <= c.endLine && l < lines.size(); ++l)
                chunkLines << lines[l];
            c.content = chunkLines.join(QLatin1Char('\n'));
        }
        m_chunks.append(c);
    }
    return true;
}

void WorkspaceChunkIndex::collectFiles(const QDir &dir, QStringList &files) const
{
    static const QSet<QString> skipDirs = {
        QStringLiteral(".git"), QStringLiteral("node_modules"),
        QStringLiteral("build"), QStringLiteral("build-llvm"),
        QStringLiteral(".cache"), QStringLiteral("__pycache__"),
        QStringLiteral(".venv"), QStringLiteral("dist"),
    };
    static const QSet<QString> sourceExts = {
        QStringLiteral("cpp"), QStringLiteral("h"), QStringLiteral("hpp"),
        QStringLiteral("c"), QStringLiteral("cc"), QStringLiteral("cxx"),
        QStringLiteral("py"), QStringLiteral("js"), QStringLiteral("ts"),
        QStringLiteral("jsx"), QStringLiteral("tsx"), QStringLiteral("java"),
        QStringLiteral("cs"), QStringLiteral("rb"), QStringLiteral("go"),
        QStringLiteral("rs"), QStringLiteral("swift"), QStringLiteral("kt"),
        QStringLiteral("lua"), QStringLiteral("sh"), QStringLiteral("bash"),
        QStringLiteral("cmake"), QStringLiteral("md"), QStringLiteral("json"),
        QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("toml"),
        QStringLiteral("xml"), QStringLiteral("html"), QStringLiteral("css"),
    };

    for (const auto &entry : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.isDir()) {
            if (skipDirs.contains(entry.fileName())) continue;
            collectFiles(QDir(entry.filePath()), files);
        } else if (sourceExts.contains(entry.suffix().toLower())) {
            if (entry.size() > 1024 * 1024) continue;
            files << entry.filePath();
        }
    }
}

void WorkspaceChunkIndex::indexFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    const QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split(QLatin1Char('\n'));
    const QString ext = QFileInfo(filePath).suffix().toLower();
    const QString lang = languageFromExt(ext);

    int chunkStart = 0;
    static const QRegularExpression funcRx(QStringLiteral(
        R"(^(?:\s*(?:class|struct|enum|namespace|void|int|bool|auto|static|virtual|override|const)\b|)"
        R"(def |function |fn |func |public |private |protected ))"));

    for (int i = 0; i < lines.size(); ++i) {
        const bool isBoundary = funcRx.match(lines[i]).hasMatch();
        const bool isEnd = (i == lines.size() - 1);
        const bool isLargeChunk = (i - chunkStart >= 80);

        if ((isBoundary && i > chunkStart + 3) || isEnd || isLargeChunk) {
            int end = isEnd ? i : i - 1;
            if (end < chunkStart) continue;

            QStringList chunkLines;
            for (int l = chunkStart; l <= end; ++l)
                chunkLines << lines[l];
            const QString chunkContent = chunkLines.join(QLatin1Char('\n'));

            CodeChunk chunk;
            chunk.filePath = filePath;
            chunk.startLine = chunkStart;
            chunk.endLine = end;
            chunk.content = chunkContent;
            chunk.language = lang;
            chunk.contentHash = QCryptographicHash::hash(
                chunkContent.toUtf8(), QCryptographicHash::Md5);
            chunk.symbol = extractSymbol(chunkContent);

            m_chunks.append(chunk);
            chunkStart = i;
        }
    }
}

QString WorkspaceChunkIndex::extractSymbol(const QString &code)
{
    static const QRegularExpression rx(QStringLiteral(
        R"((?:class|struct|enum|namespace|def|function|fn|func)\s+(\w+))"));
    const auto match = rx.match(code);
    if (match.hasMatch()) return match.captured(1);

    static const QRegularExpression cppFn(QStringLiteral(
        R"((?:\w+(?:::\w+)*\s+)+(\w+)\s*\()"));
    const auto cppMatch = cppFn.match(code);
    if (cppMatch.hasMatch()) return cppMatch.captured(1);

    return {};
}

QString WorkspaceChunkIndex::languageFromExt(const QString &ext)
{
    static const QMap<QString, QString> map = {
        {QStringLiteral("cpp"), QStringLiteral("cpp")},
        {QStringLiteral("h"), QStringLiteral("cpp")},
        {QStringLiteral("hpp"), QStringLiteral("cpp")},
        {QStringLiteral("c"), QStringLiteral("c")},
        {QStringLiteral("py"), QStringLiteral("python")},
        {QStringLiteral("js"), QStringLiteral("javascript")},
        {QStringLiteral("ts"), QStringLiteral("typescript")},
        {QStringLiteral("java"), QStringLiteral("java")},
        {QStringLiteral("cs"), QStringLiteral("csharp")},
        {QStringLiteral("rb"), QStringLiteral("ruby")},
        {QStringLiteral("go"), QStringLiteral("go")},
        {QStringLiteral("rs"), QStringLiteral("rust")},
    };
    return map.value(ext, ext);
}
