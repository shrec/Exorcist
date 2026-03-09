#include "searchindex.h"

#include <QDirIterator>
#include <QFileInfo>

static const QSet<QString> kSkipDirs = {
    ".git", ".svn", ".hg", "node_modules", "__pycache__", ".cache",
    ".vscode", ".vs", ".idea", "build", "build-llvm", "build-debug",
    "build-release", "build-ci", "cmake-build-debug", "cmake-build-release",
    "dist", "out", "bin", "obj", ".tox", ".mypy_cache", ".pytest_cache",
    "target",  // Rust/Maven
};

static const QSet<QString> kTextExtensions = {
    "c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "inl",
    "py", "pyw", "pyi",
    "js", "jsx", "ts", "tsx", "mjs", "cjs",
    "java", "kt", "kts", "scala",
    "cs", "fs", "vb",
    "rs", "go", "swift", "m", "mm",
    "rb", "php", "pl", "pm", "lua", "r",
    "sh", "bash", "zsh", "fish", "ps1", "psm1", "bat", "cmd",
    "html", "htm", "xml", "xhtml", "svg",
    "css", "scss", "sass", "less",
    "json", "yaml", "yml", "toml", "ini", "cfg", "conf",
    "md", "markdown", "rst", "txt", "log",
    "cmake", "pro", "pri", "qrc", "ui", "qml",
    "sql", "graphql", "proto", "thrift",
    "dockerfile", "makefile", "mak",
    "gitignore", "gitattributes", "editorconfig",
    "clang-format", "clang-tidy",
    "sln", "vcxproj", "csproj", "fsproj",
    "rc", "def", "asm", "s",
};

void SearchIndex::build(const QString &rootPath)
{
    m_rootPath = rootPath;
    m_allFiles.clear();

    QDirIterator it(rootPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi = it.fileInfo();

        if (fi.isDir()) {
            // DirIterator still descends; we filter in the loop
            continue;
        }

        // Skip files inside ignored directories
        const QString path = fi.absoluteFilePath();
        bool skip = false;
        for (const QString &dir : kSkipDirs) {
            if (path.contains(QLatin1Char('/') + dir + QLatin1Char('/')) ||
                path.contains(QLatin1Char('\\') + dir + QLatin1Char('\\'))) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        // Skip large files (> 1 MB)
        if (fi.size() > 1024 * 1024) continue;

        // Only index known text extensions
        const QString suffix = fi.suffix().toLower();
        const QString baseName = fi.fileName().toLower();
        if (!suffix.isEmpty() && !kTextExtensions.contains(suffix)) {
            // Check extensionless files by name
            if (!kTextExtensions.contains(baseName))
                continue;
        }

        m_allFiles.append(path);
    }
}

bool SearchIndex::shouldSkipDir(const QString &name)
{
    return kSkipDirs.contains(name.toLower());
}

bool SearchIndex::isTextFile(const QString &suffix)
{
    return kTextExtensions.contains(suffix.toLower());
}
