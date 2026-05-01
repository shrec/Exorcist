#pragma once

#include <QHash>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;
class QTextEdit;
class QLineEdit;

class GitService;

/// Source-control side panel.
///
/// Polished UI:
///   * Top toolbar with branch dropdown (local + remote), Commit, Push, Pull, Sync, refresh.
///   * Multi-line commit message editor.
///   * Two collapsible groups: "Staged Changes" and "Unstaged Changes" with
///     status icons (A/M/D/?? letters in colored chips).
///   * Inline unified-diff preview in the lower half (split via QSplitter).
///   * Right-click menu on each file: Stage / Unstage / Discard / Open.
///   * VS Code dark-modern styling.
///
/// All git mutation goes through GitService (push/pull/sync use QProcess,
/// since GitService doesn't expose them — the backend stays untouched).
class GitPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GitPanel(GitService *git, QWidget *parent = nullptr);

    // Backwards-compatible accessor used by GitPlugin (commit-message AI).
    // Returns a QObject* that exposes a setText(QString) slot via duck-typing —
    // we expose a real QLineEdit-shim wrapper for compatibility. Actual editor
    // is QPlainTextEdit, but the plugin only calls setText().
    QPlainTextEdit *commitMessageEditor() const { return m_commitMsg; }

    // Legacy method name — GitPlugin calls commitMessageInput()->setText(msg).
    // Returns an adapter so legacy code keeps compiling.
    class CommitInputAdapter
    {
    public:
        explicit CommitInputAdapter(QPlainTextEdit *e) : m_e(e) {}
        void setText(const QString &t);
    private:
        QPlainTextEdit *m_e;
    };
    CommitInputAdapter *commitMessageInput() const { return m_commitInputAdapter; }

signals:
    void generateCommitMessageRequested();
    void resolveConflictsRequested();
    /// Emitted when the user double-clicks a file (or chooses "Open").
    void openFileRequested(const QString &absPath);

private:
    // ── Refresh / population ─────────────────────────────────────────────
    void populateLists();
    void refreshBranches();
    void refreshDiffForCurrent();

    // ── Action handlers ──────────────────────────────────────────────────
    void stageItem(QTreeWidgetItem *item);
    void unstageItem(QTreeWidgetItem *item);
    void discardItem(QTreeWidgetItem *item);
    void openItem(QTreeWidgetItem *item);
    void stageAll();
    void unstageAll();
    void commitChanges();
    void onBranchComboChanged(int index);
    void onPushClicked();
    void onPullClicked();
    void onSyncClicked();
    void showFileContextMenu(const QPoint &pos, QTreeWidget *tree, bool staged);

    // ── Helpers ──────────────────────────────────────────────────────────
    void updateCommitButtonState();
    void updateRemoteButtonsState();
    QString runGitSync(const QStringList &args, int timeoutMs = 30000) const;
    void renderDiff(const QString &absPath, bool staged);
    QString colorizeDiff(const QString &diff) const;
    static QString statusLetter(const QString &status, bool stagedSide);
    static QColor statusColor(const QString &letter);

    // ── Members ──────────────────────────────────────────────────────────
    GitService *m_git;

    // Toolbar
    QComboBox   *m_branchCombo   = nullptr;
    QToolButton *m_refreshBtn    = nullptr;
    QToolButton *m_pushBtn       = nullptr;
    QToolButton *m_pullBtn       = nullptr;
    QToolButton *m_syncBtn       = nullptr;
    QToolButton *m_newBranchBtn  = nullptr;

    // Commit area
    QPlainTextEdit *m_commitMsg     = nullptr;
    QPushButton    *m_commitBtn     = nullptr;
    QToolButton    *m_generateMsgBtn = nullptr;

    // File trees
    QTreeWidget *m_stagedTree   = nullptr;
    QTreeWidget *m_unstagedTree = nullptr;
    QPushButton *m_resolveBtn   = nullptr;

    // Diff view (lower half of splitter)
    QSplitter *m_splitter   = nullptr;
    QTextEdit *m_diffView   = nullptr;

    CommitInputAdapter *m_commitInputAdapter = nullptr;

    // Cached path → status code (from GitService::fileStatuses())
    QHash<QString, QString> m_lastStatuses;
};
