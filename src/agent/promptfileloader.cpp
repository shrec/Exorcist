#include "promptfileloader.h"

QString PromptFileLoader::load(const QString &workspaceRoot,
                               const QString &languageId)
{
    if (workspaceRoot.isEmpty()) return {};

    QStringList parts;

    const QStringList candidates = {
        QStringLiteral(".github/copilot-instructions.md"),
        QStringLiteral(".ai-instructions.md"),
        QStringLiteral(".vscode/copilot-instructions.md"),
        QStringLiteral("AGENTS.md"),
    };

    for (const QString &rel : candidates) {
        const QString content = readFile(workspaceRoot + '/' + rel);
        if (!content.isEmpty()) {
            parts << content.trimmed();
            break;
        }
    }

    if (!languageId.isEmpty()) {
        const QString langFile = QStringLiteral(".github/%1.instructions.md")
                                 .arg(languageId.toLower());
        const QString content = readFile(workspaceRoot + '/' + langFile);
        if (!content.isEmpty())
            parts << content.trimmed();
    }

    return parts.join(QStringLiteral("\n\n"));
}

QString PromptFileLoader::loadForIntent(const QString &workspaceRoot,
                                         const QString &intentFile)
{
    if (workspaceRoot.isEmpty() || intentFile.isEmpty()) return {};

    const QStringList paths = {
        workspaceRoot + '/' + intentFile,
        workspaceRoot + QStringLiteral("/.github/") + intentFile,
    };

    for (const QString &p : paths) {
        const QString content = readFile(p);
        if (!content.isEmpty())
            return content.trimmed();
    }
    return {};
}

QString PromptFileLoader::loadCodeGenInstructions(const QString &workspaceRoot)
{
    return loadForIntent(workspaceRoot,
        QStringLiteral(".copilot-codeGeneration-instructions.md"));
}

QString PromptFileLoader::loadTestInstructions(const QString &workspaceRoot)
{
    return loadForIntent(workspaceRoot,
        QStringLiteral(".copilot-test-instructions.md"));
}

QString PromptFileLoader::loadReviewInstructions(const QString &workspaceRoot)
{
    return loadForIntent(workspaceRoot,
        QStringLiteral(".copilot-review-instructions.md"));
}

QString PromptFileLoader::loadCommitInstructions(const QString &workspaceRoot)
{
    return loadForIntent(workspaceRoot,
        QStringLiteral(".copilot-commit-message-instructions.md"));
}

bool PromptFileLoader::hasInstructions(const QString &workspaceRoot)
{
    if (workspaceRoot.isEmpty()) return false;
    const QStringList candidates = {
        QStringLiteral(".github/copilot-instructions.md"),
        QStringLiteral(".ai-instructions.md"),
        QStringLiteral(".vscode/copilot-instructions.md"),
        QStringLiteral("AGENTS.md"),
    };
    for (const QString &rel : candidates) {
        if (QFileInfo::exists(workspaceRoot + '/' + rel))
            return true;
    }
    return false;
}

QList<PromptFileLoader::PromptFile> PromptFileLoader::loadPromptFiles(
    const QString &workspaceRoot)
{
    QList<PromptFile> result;
    if (workspaceRoot.isEmpty()) return result;

    const QString promptDir = workspaceRoot + QStringLiteral("/.github/prompts");

    {
        QMutexLocker lock(&cacheMutex());
        auto &c = promptFileCache();
        if (c.root == workspaceRoot && !c.entries.isEmpty()) {
            const QFileInfo dirInfo(promptDir);
            if (dirInfo.exists() && dirInfo.lastModified() == c.dirTimestamp)
                return c.entries;
        }
    }

    QDir dir(promptDir);
    if (!dir.exists()) return result;

    const QStringList files = dir.entryList(
        {QStringLiteral("*.prompt.md")}, QDir::Files, QDir::Name);

    for (const QString &fileName : files) {
        const QString fullPath = dir.absoluteFilePath(fileName);
        const QString content = readFile(fullPath);
        if (content.isEmpty()) continue;

        PromptFile pf;
        pf.filePath = fullPath;
        pf.name = fileName;
        pf.name.chop(10); // Remove ".prompt.md"

        if (content.startsWith(QLatin1String("---"))) {
            const int endFm = content.indexOf(QLatin1String("\n---"), 3);
            if (endFm >= 0) {
                const QString fm = content.mid(4, endFm - 4);
                for (const QString &line : fm.split(QLatin1Char('\n'))) {
                    if (line.trimmed().startsWith(QLatin1String("title:"))) {
                        const QString title = line.mid(line.indexOf(':') + 1).trimmed();
                        if (!title.isEmpty())
                            pf.name = title;
                    }
                }
                pf.body = content.mid(endFm + 4).trimmed();
            } else {
                pf.body = content;
            }
        } else {
            pf.body = content;
        }

        result.append(pf);
    }

    {
        QMutexLocker lock(&cacheMutex());
        auto &c = promptFileCache();
        c.root = workspaceRoot;
        c.entries = result;
        c.dirTimestamp = QFileInfo(promptDir).lastModified();
    }

    return result;
}

void PromptFileLoader::invalidateCache()
{
    QMutexLocker lock(&cacheMutex());
    promptFileCache().entries.clear();
}

QString PromptFileLoader::interpolatePromptBody(const QString &body,
                                                 const QString &workspaceFolder,
                                                 const QString &file,
                                                 const QString &selection)
{
    QString result = body;
    result.replace(QStringLiteral("${workspaceFolder}"), workspaceFolder);
    result.replace(QStringLiteral("${file}"), file);
    result.replace(QStringLiteral("${selection}"), selection);
    return result;
}

QString PromptFileLoader::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream stream(&f);
    return stream.readAll();
}

PromptFileLoader::PromptFileCache &PromptFileLoader::promptFileCache()
{
    static PromptFileCache cache;
    return cache;
}

QMutex &PromptFileLoader::cacheMutex()
{
    static QMutex mutex;
    return mutex;
}
