#pragma once

#include "icontextprovider.h"
#include "../aiinterface.h"

#include <QObject>
#include <QSet>
#include <functional>

class IFileSystem;
class GitService;

// ── ContextBuilder ────────────────────────────────────────────────────────────
//
// Concrete IContextProvider that gathers context from IDE services:
//   - Active file content
//   - Selection
//   - Open files list
//   - Git status/diff
//   - Diagnostics
//
// Wired to IDE services via callbacks (no direct dependency on MainWindow).

class ContextBuilder : public QObject, public IContextProvider
{
    Q_OBJECT

public:
    explicit ContextBuilder(QObject *parent = nullptr);

    // ── Configuration setters ─────────────────────────────────────────────

    void setFileSystem(IFileSystem *fs) { m_fs = fs; }
    void setWorkspaceRoot(const QString &root) { m_workspaceRoot = root; }

    // Callbacks for IDE state (avoids hard dependency on MainWindow)
    using OpenFilesGetter    = std::function<QStringList()>;
    using DiagnosticsGetter  = std::function<QList<AgentDiagnostic>()>;
    using GitStatusGetter    = std::function<QString()>;
    using GitDiffGetter        = std::function<QString()>;
    using TerminalOutputGetter = std::function<QString()>;

    void setOpenFilesGetter(OpenFilesGetter fn)           { m_openFilesGetter = std::move(fn); }
    void setDiagnosticsGetter(DiagnosticsGetter fn)       { m_diagnosticsGetter = std::move(fn); }
    void setGitStatusGetter(GitStatusGetter fn)           { m_gitStatusGetter = std::move(fn); }
    void setGitDiffGetter(GitDiffGetter fn)               { m_gitDiffGetter = std::move(fn); }
    void setTerminalOutputGetter(TerminalOutputGetter fn) { m_terminalOutputGetter = std::move(fn); }

    // Custom instructions from .github/copilot-instructions.md etc.
    void setCustomInstructions(const QString &s) { m_customInstructions = s; }
    QString customInstructions() const           { return m_customInstructions; }

    /// Disable a context item type (won't be included in buildContext).
    void disableContextType(ContextItem::Type type) { m_disabledTypes.insert(type); }
    /// Re-enable a context item type.
    void enableContextType(ContextItem::Type type) { m_disabledTypes.remove(type); }
    /// Check if a context type is enabled.
    bool isContextTypeEnabled(ContextItem::Type type) const { return !m_disabledTypes.contains(type); }

    /// Pin a context type so it's never pruned.
    void pinContextType(ContextItem::Type type) { m_pinnedTypes.insert(type); }
    /// Unpin a context type.
    void unpinContextType(ContextItem::Type type) { m_pinnedTypes.remove(type); }
    /// Check if a context type is pinned.
    bool isContextTypePinned(ContextItem::Type type) const { return m_pinnedTypes.contains(type); }

    // ── IContextProvider ──────────────────────────────────────────────────

    ContextSnapshot buildContext() override;

    ContextSnapshot buildContext(const QString &userPrompt,
                                 const QString &activeFilePath,
                                 const QString &selectedText,
                                 const QString &languageId) override;

    /// Set maximum total context size in characters (default: 120000 ≈ 30k tokens).
    void setMaxContextChars(int chars) { m_maxContextChars = chars; }
    int maxContextChars() const { return m_maxContextChars; }

    /// Prune a context snapshot to fit within maxContextChars.
    /// Removes lowest-priority items first, then truncates remaining items.
    static void pruneContext(ContextSnapshot &ctx, int maxChars);

private:
    void addFileContext(ContextSnapshot &ctx, const QString &filePath);
    void addSelectionContext(ContextSnapshot &ctx, const QString &selectedText);
    void addOpenFilesContext(ContextSnapshot &ctx);
    void addDiagnosticsContext(ContextSnapshot &ctx);
    void addGitContext(ContextSnapshot &ctx);
    void addTerminalContext(ContextSnapshot &ctx);

    IFileSystem *m_fs = nullptr;
    QString      m_workspaceRoot;
    QString      m_customInstructions;

    OpenFilesGetter   m_openFilesGetter;
    DiagnosticsGetter m_diagnosticsGetter;
    GitStatusGetter   m_gitStatusGetter;
    GitDiffGetter        m_gitDiffGetter;
    TerminalOutputGetter m_terminalOutputGetter;
    QSet<ContextItem::Type> m_disabledTypes;
    QSet<ContextItem::Type> m_pinnedTypes;
    int m_maxContextChars = 120000;
};
