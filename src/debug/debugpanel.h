#pragma once

#include <QWidget>

class QTabWidget;
class QToolBar;
class QAction;
class QTableWidget;
class QTreeWidget;
class QTreeView;
class QPlainTextEdit;
class QLineEdit;
class QLabel;
class IDebugAdapter;
class WatchTreeModel;
class QuickWatchDialog;
struct DebugBreakpoint;
struct DebugFrame;
struct DebugVariable;
struct DebugThread;
enum class DebugStopReason;

/// Main debug panel containing control bar + tabs for
/// call stack, locals, breakpoints, and output.
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
    void onLaunchClicked();
    void onContinueClicked();
    void onStepOverClicked();
    void onStepIntoClicked();
    void onStepOutClicked();
    void onPauseClicked();
    void onStopClicked();
    void onWatchInputSubmit();
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

    void onCallStackDoubleClicked(int row, int col);
    void onLocalsContextMenu(const QPoint &pos);
    void onWatchContextMenu(const QPoint &pos);

private:
    void setupToolBar();
    void setupCallStackTab();
    void setupLocalsTab();
    void setupBreakpointsTab();
    void setupOutputTab();
    void setupWatchTab();

    void setRunning(bool running, bool stopped = false);

    IDebugAdapter    *m_adapter = nullptr;

    // Toolbar
    QToolBar         *m_toolbar;
    QAction          *m_actLaunch;
    QAction          *m_actContinue;
    QAction          *m_actStepOver;
    QAction          *m_actStepInto;
    QAction          *m_actStepOut;
    QAction          *m_actPause;
    QAction          *m_actStop;

    // Status
    QLabel           *m_statusLabel;

    // Tabs
    QTabWidget       *m_tabs;

    // Call Stack tab
    QTableWidget     *m_callStackTable;

    // Locals tab — tree view backed by WatchTreeModel
    QTreeView        *m_localsView;
    WatchTreeModel   *m_localsModel;

    // Breakpoints tab
    QTableWidget     *m_breakpointsTable;

    // Output tab
    QPlainTextEdit   *m_outputText;

    // Watch tab — tree view backed by WatchTreeModel
    QTreeView        *m_watchView;
    WatchTreeModel   *m_watchModel;
    QLineEdit        *m_watchInput;
};
