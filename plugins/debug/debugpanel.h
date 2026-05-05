#pragma once

#include <QWidget>

class QTabWidget;
class QToolButton;
class QTableWidget;
class QTreeWidget;
class QTreeView;
class QPlainTextEdit;
class QLineEdit;
class QLabel;
class IDebugAdapter;
class WatchTreeModel;
class QuickWatchDialog;
class QSpinBox;
struct DebugBreakpoint;
struct DebugFrame;
struct DebugVariable;
struct DebugThread;
struct DebugWatchpoint;
enum class DebugStopReason;

/// Main debug panel containing a status row + tabs for call stack,
/// threads, locals, breakpoints, watch, and debug output. The
/// Launch/Continue/Step/Pause/Stop controls live on the main window
/// toolbar (BuildToolbar) — this dock no longer hosts a duplicate.
class DebugPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DebugPanel(QWidget *parent = nullptr);

    /// Set the active debug adapter. Connects all signals.
    void setAdapter(IDebugAdapter *adapter);

    IDebugAdapter *adapter() const { return m_adapter; }

signals:
    /// Emitted when user wants to navigate to a source location.
    void navigateToSource(const QString &filePath, int line);

    /// Emitted when user toggles a breakpoint from the breakpoints list.
    void breakpointToggled(const QString &filePath, int line, bool enabled);

    /// Emitted when user clicks Launch.
    void launchRequested();

public slots:
    /// Add a breakpoint to the breakpoints list (called by editor gutter).
    void addBreakpointEntry(const QString &filePath, int line);

    /// Remove a breakpoint entry by file+line.
    void removeBreakpointEntry(const QString &filePath, int line);

private slots:
    void onWatchInputSubmit();
    void onWatchRemoveSelected();
    void onQuickWatch();

    // Adapter signal handlers
    void onAdapterStarted();
    void onAdapterTerminated();
    void onAdapterError(const QString &msg);
    void onAdapterStopped(int threadId, DebugStopReason reason,
                          const QString &description);
    void onAdapterContinued(int threadId);
    void onThreadsReceived(const QList<DebugThread> &threads);
    void onStackTraceReceived(int threadId, const QList<DebugFrame> &frames);
    void onVariablesReceived(int ref, const QList<DebugVariable> &vars);
    void onEvaluateResult(const QString &expr, const QString &result);
    void onOutputProduced(const QString &text, const QString &category);
    void onBreakpointVerified(const DebugBreakpoint &bp);
    void onWatchpointSet(const DebugWatchpoint &wp);
    void onWatchpointRemoved(int watchpointId);

    void onCallStackDoubleClicked(int row, int col);
    void onCallStackClicked(int row, int col);
    void onThreadClicked(int row, int col);
    void onLocalsContextMenu(const QPoint &pos);
    void onWatchContextMenu(const QPoint &pos);

    void onAddWatchpointClicked();
    void onBreakpointHitCountChanged();

private:
    void setupCallStackTab();
    void setupThreadsTab();
    void setupLocalsTab();
    void setupBreakpointsTab();
    void setupOutputTab();
    void setupWatchTab();

    void setRunning(bool running, bool stopped = false);

    IDebugAdapter    *m_adapter = nullptr;

    // Status
    QLabel           *m_statusLabel;

    // Tabs
    QTabWidget       *m_tabs;

    // Call Stack tab
    QTableWidget     *m_callStackTable;

    // Threads tab
    QTableWidget     *m_threadsTable;

    // Last thread the adapter reported via *stopped — used so frame
    // selections re-issue stack/locals queries on the right thread.
    int               m_currentThread = 0;

    // Locals tab — tree view backed by WatchTreeModel
    QTreeView        *m_localsView;
    WatchTreeModel   *m_localsModel;

    // Breakpoints tab
    // Columns: 0=Kind ("Break"/"Watch"), 1=Enabled, 2=File / Expression,
    //          3=Line, 4=Hit Count
    QTableWidget     *m_breakpointsTable;
    QToolButton      *m_addWatchpointBtn = nullptr;
    QSpinBox         *m_hitCountSpin     = nullptr;
    QToolButton      *m_applyHitCountBtn = nullptr;
    // Re-entry guard: programmatic edits of the hit-count cell would
    // otherwise re-trigger the change handler → re-add → emit signal → loop.
    bool              m_updatingHitCount = false;

    // Output tab
    QPlainTextEdit   *m_outputText;

    // Watch tab — tree view backed by WatchTreeModel
    // NOTE: m_watchModel and m_localsModel are intentionally separate
    // WatchTreeModel instances so that frame-locals don't pollute the
    // user-curated watch list and vice-versa.
    QTreeView        *m_watchView;
    WatchTreeModel   *m_watchModel;
    QLineEdit        *m_watchInput;
    QToolButton      *m_watchRemoveBtn;
};
