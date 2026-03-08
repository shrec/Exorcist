#pragma once

#include <QFuture>
#include <QJsonObject>
#include <QString>
#include <QtConcurrent>

// ── IContextProvider ──────────────────────────────────────────────────────────
//
// Provides contextual information about the IDE state to the agent.
// The AgentController calls buildContext() before each model request
// to populate the system prompt and available context items.
//
// Context items include:
//   - Active file path + content
//   - Selected text
//   - Diagnostics (errors/warnings)
//   - Git status / diff
//   - Open files list
//   - Workspace search results
//   - Symbol information

struct ContextItem
{
    enum class Type
    {
        ActiveFile,
        Selection,
        OpenFiles,
        Diagnostics,
        GitDiff,
        GitStatus,
        WorkspaceInfo,
        SearchResults,
        SymbolInfo,
        Custom,
    };

    Type    type;
    QString label;    // human-readable label
    QString content;  // the actual content
    QString filePath; // optional – associated file
    int     priority = 0; // higher = more important, used for truncation
    bool    pinned   = false; // pinned items are never pruned
};

struct ContextSnapshot
{
    QString              workspaceRoot;
    QString              activeFilePath;
    QString              languageId;
    QString              selectedText;
    int                  cursorLine   = -1;
    int                  cursorColumn = -1;
    QList<ContextItem>   items;

    // Total estimated token count for context (rough estimate)
    int estimatedTokens() const
    {
        int total = 0;
        for (const auto &item : items)
            total += item.content.size() / 4; // ~4 chars per token
        return total;
    }
};

class IContextProvider
{
public:
    virtual ~IContextProvider() = default;

    /// Build a full context snapshot from the current IDE state.
    virtual ContextSnapshot buildContext() = 0;

    /// Build context for a specific request (includes request-specific data).
    virtual ContextSnapshot buildContext(const QString &userPrompt,
                                        const QString &activeFilePath,
                                        const QString &selectedText,
                                        const QString &languageId) = 0;

    /// Async variant — builds context on a worker thread.
    /// Default implementation wraps the synchronous buildContext().
    virtual QFuture<ContextSnapshot> buildContextAsync(
        const QString &userPrompt,
        const QString &activeFilePath,
        const QString &selectedText,
        const QString &languageId)
    {
        return QtConcurrent::run([this, userPrompt, activeFilePath,
                                  selectedText, languageId]() {
            return buildContext(userPrompt, activeFilePath,
                               selectedText, languageId);
        });
    }
};
