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
    static QString load(const QString &workspaceRoot,
                        const QString &languageId = {});

    static QString loadForIntent(const QString &workspaceRoot,
                                  const QString &intentFile);

    static QString loadCodeGenInstructions(const QString &workspaceRoot);
    static QString loadTestInstructions(const QString &workspaceRoot);
    static QString loadReviewInstructions(const QString &workspaceRoot);
    static QString loadCommitInstructions(const QString &workspaceRoot);

    static bool hasInstructions(const QString &workspaceRoot);

    // ── Prompt file (.prompt.md) support ─────────────────────────────────

    struct PromptFile
    {
        QString name;
        QString filePath;
        QString body;
    };

    static QList<PromptFile> loadPromptFiles(const QString &workspaceRoot);
    static void invalidateCache();

    static QString interpolatePromptBody(const QString &body,
                                          const QString &workspaceFolder,
                                          const QString &file,
                                          const QString &selection);

private:
    static QString readFile(const QString &path);

    struct PromptFileCache {
        QString root;
        QList<PromptFile> entries;
        QDateTime dirTimestamp;
    };

    static PromptFileCache &promptFileCache();
    static QMutex &cacheMutex();
};
