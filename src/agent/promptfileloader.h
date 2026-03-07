#pragma once

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStringList>
#include <QTextStream>

// ─────────────────────────────────────────────────────────────────────────────
// PromptFileLoader — reads workspace-level AI instruction files.
//
// Supported file locations (searched in this order):
//   .github/copilot-instructions.md   — GitHub Copilot compatible
//   .ai-instructions.md               — Exorcist native
//   .vscode/copilot-instructions.md   — VS Code compatible
//
// Also supports per-language instruction files:
//   .github/<lang>.instructions.md    (e.g. cpp.instructions.md)
//
// Loaded content is injected into the system prompt before each model request.
// ─────────────────────────────────────────────────────────────────────────────

class PromptFileLoader
{
public:
    // Load all instruction files from the given workspace root.
    // Returns combined content (sections separated by blank lines).
    static QString load(const QString &workspaceRoot,
                        const QString &languageId = {})
    {
        if (workspaceRoot.isEmpty()) return {};

        QStringList parts;

        // Main instruction candidates (priority order)
        const QStringList candidates = {
            QStringLiteral(".github/copilot-instructions.md"),
            QStringLiteral(".ai-instructions.md"),
            QStringLiteral(".vscode/copilot-instructions.md"),
            QStringLiteral("AGENTS.md"),                // OpenAI Codex style
        };

        for (const QString &rel : candidates) {
            const QString content = readFile(workspaceRoot + '/' + rel);
            if (!content.isEmpty()) {
                parts << content.trimmed();
                break;   // Use only the first one found
            }
        }

        // Language-specific instructions
        if (!languageId.isEmpty()) {
            const QString langFile = QStringLiteral(".github/%1.instructions.md")
                                     .arg(languageId.toLower());
            const QString content = readFile(workspaceRoot + '/' + langFile);
            if (!content.isEmpty())
                parts << content.trimmed();
        }

        return parts.join(QStringLiteral("\n\n"));
    }

    // Load intent-specific instruction files.
    // Returns content of the matching instruction file, or empty string.
    static QString loadForIntent(const QString &workspaceRoot,
                                  const QString &intentFile)
    {
        if (workspaceRoot.isEmpty() || intentFile.isEmpty()) return {};

        // Search in root and .github/
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

    // Load code generation instructions.
    static QString loadCodeGenInstructions(const QString &workspaceRoot)
    {
        return loadForIntent(workspaceRoot,
            QStringLiteral(".copilot-codeGeneration-instructions.md"));
    }

    // Load test generation instructions.
    static QString loadTestInstructions(const QString &workspaceRoot)
    {
        return loadForIntent(workspaceRoot,
            QStringLiteral(".copilot-test-instructions.md"));
    }

    // Load code review instructions.
    static QString loadReviewInstructions(const QString &workspaceRoot)
    {
        return loadForIntent(workspaceRoot,
            QStringLiteral(".copilot-review-instructions.md"));
    }

    // Load commit message instructions.
    static QString loadCommitInstructions(const QString &workspaceRoot)
    {
        return loadForIntent(workspaceRoot,
            QStringLiteral(".copilot-commit-message-instructions.md"));
    }

    // Check if any instruction file exists in the workspace.
    static bool hasInstructions(const QString &workspaceRoot)
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

    // ── Prompt file (.prompt.md) support ─────────────────────────────────

    struct PromptFile
    {
        QString name;       // Display name (from filename or YAML title)
        QString filePath;   // Absolute path
        QString body;       // Markdown body (after frontmatter)
    };

    /// Scan workspace for .prompt.md files in .github/prompts/.
    /// Returns a list of prompt files with name + body content.
    /// Results are cached and invalidated when the directory timestamp changes.
    static QList<PromptFile> loadPromptFiles(const QString &workspaceRoot)
    {
        QList<PromptFile> result;
        if (workspaceRoot.isEmpty()) return result;

        const QString promptDir = workspaceRoot + QStringLiteral("/.github/prompts");

        // Check cache validity
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

            // Strip YAML frontmatter if present
            if (content.startsWith(QLatin1String("---"))) {
                const int endFm = content.indexOf(QLatin1String("\n---"), 3);
                if (endFm >= 0) {
                    // Extract title from frontmatter if present
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

        // Store in cache
        {
            QMutexLocker lock(&cacheMutex());
            auto &c = promptFileCache();
            c.root = workspaceRoot;
            c.entries = result;
            c.dirTimestamp = QFileInfo(promptDir).lastModified();
        }

        return result;
    }

    /// Invalidate the prompt file cache (call when files are known to have changed).
    static void invalidateCache()
    {
        QMutexLocker lock(&cacheMutex());
        promptFileCache().entries.clear();
    }

    /// Interpolate prompt file variables.
    static QString interpolatePromptBody(const QString &body,
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

private:
    static QString readFile(const QString &path)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
        QTextStream stream(&f);
        return stream.readAll();
    }

    struct PromptFileCache {
        QString root;
        QList<PromptFile> entries;
        QDateTime dirTimestamp;
    };

    static PromptFileCache &promptFileCache()
    {
        static PromptFileCache cache;
        return cache;
    }

    static QMutex &cacheMutex()
    {
        static QMutex mutex;
        return mutex;
    }
};
