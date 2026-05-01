#include "debugpanel.h"
#include "sdk/idebugadapter.h"
#include "watchtreemodel.h"
#include "quickwatchdialog.h"
#include "watchpointdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

DebugPanel::DebugPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Status label
    m_statusLabel = new QLabel(tr("Not running"), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #808080; font-size: 11px;"));
    root->addWidget(m_statusLabel);

    // Tabs — VS2022 dark style
    m_tabs = new QTabWidget(this);
    m_tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { background: #1e1e1e; border: none; border-top: 1px solid #3e3e42; }"
        "QTabBar { background: #252526; }"
        "QTabBar::tab { background: #2d2d30; color: #9d9d9d; padding: 4px 12px;"
        "  border: none; border-right: 1px solid #1e1e1e; font-size: 12px; }"
        "QTabBar::tab:selected { background: #1e1e1e; color: #ffffff;"
        "  border-top: 1px solid #007acc; }"
        "QTabBar::tab:hover:!selected { background: #3e3e42; color: #cccccc; }"
    ));
    root->addWidget(m_tabs, 1);

    setupCallStackTab();
    setupThreadsTab();
    setupLocalsTab();
    setupBreakpointsTab();
    setupWatchTab();
    setupOutputTab();

    setRunning(false);

    // Watch input is disabled until a debug session is active.
    // (WatchTreeModel::addWatch already no-ops without an adapter, but
    //  disabling provides clearer UI feedback.)
    m_watchInput->setEnabled(false);
    m_watchRemoveBtn->setEnabled(false);
}

// ── Shared stylesheet for debug table views ───────────────────────────────────

static const char *kDebugTableStyle =
    "QTableWidget { background: #1e1e1e; color: #d4d4d4; border: none;"
    "  gridline-color: #2d2d30; font-size: 12px;"
    "  alternate-background-color: #252526; }"
    "QTableWidget::item { padding: 2px 4px; border: none; }"
    "QTableWidget::item:selected { background: #094771; color: #ffffff; }"
    "QHeaderView::section { background: #252526; color: #858585; border: none;"
    "  border-right: 1px solid #3e3e42; border-bottom: 1px solid #3e3e42;"
    "  padding: 3px 6px; font-size: 11px; }"
    "QScrollBar:vertical { background: #1e1e1e; width: 10px; border: none; }"
    "QScrollBar::handle:vertical { background: #424242; min-height: 20px; border-radius: 5px; }"
    "QScrollBar::handle:vertical:hover { background: #686868; }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }";

// ── Call Stack Tab ────────────────────────────────────────────────────────────

void DebugPanel::setupCallStackTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);

    m_callStackTable = new QTableWidget(0, 3, w);
    m_callStackTable->setHorizontalHeaderLabels({tr("#"), tr("Function"), tr("Location")});
    m_callStackTable->horizontalHeader()->setStretchLastSection(true);
    m_callStackTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_callStackTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_callStackTable->verticalHeader()->hide();
    m_callStackTable->setAlternatingRowColors(true);
    m_callStackTable->setColumnWidth(0, 30);
    m_callStackTable->setColumnWidth(1, 200);
    m_callStackTable->setStyleSheet(QLatin1String(kDebugTableStyle));

    connect(m_callStackTable, &QTableWidget::cellDoubleClicked,
            this, &DebugPanel::onCallStackDoubleClicked);
    connect(m_callStackTable, &QTableWidget::cellClicked,
            this, &DebugPanel::onCallStackClicked);

    lay->addWidget(m_callStackTable);
    m_tabs->addTab(w, tr("Call Stack"));
}

// ── Threads Tab ───────────────────────────────────────────────────────────────

void DebugPanel::setupThreadsTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);

    m_threadsTable = new QTableWidget(0, 3, w);
    m_threadsTable->setHorizontalHeaderLabels({tr("ID"), tr("Name"), tr("State")});
    m_threadsTable->horizontalHeader()->setStretchLastSection(true);
    m_threadsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_threadsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_threadsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_threadsTable->verticalHeader()->hide();
    m_threadsTable->setAlternatingRowColors(true);
    m_threadsTable->setColumnWidth(0, 50);
    m_threadsTable->setColumnWidth(1, 220);
    m_threadsTable->setStyleSheet(QLatin1String(kDebugTableStyle));

    // Single click switches the active thread — matches Call Stack behaviour
    // and avoids the user needing to double-click for what is conceptually a
    // navigation gesture.
    connect(m_threadsTable, &QTableWidget::cellClicked,
            this, &DebugPanel::onThreadClicked);
    connect(m_threadsTable, &QTableWidget::cellDoubleClicked,
            this, &DebugPanel::onThreadClicked);

    lay->addWidget(m_threadsTable);
    m_tabs->addTab(w, tr("Threads"));
}

// ── Locals Tab ────────────────────────────────────────────────────────────────

void DebugPanel::setupLocalsTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);

    m_localsModel = new WatchTreeModel(this);

    m_localsView = new QTreeView(w);
    m_localsView->setModel(m_localsModel);
    m_localsView->setRootIsDecorated(true);
    m_localsView->setAnimated(true);
    m_localsView->setAlternatingRowColors(true);
    m_localsView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_localsView->header()->setStretchLastSection(true);
    m_localsView->setColumnWidth(0, 150);
    m_localsView->setColumnWidth(1, 200);
    m_localsView->setStyleSheet(QStringLiteral(
        "QTreeView { background: #1e1e1e; color: #d4d4d4; border: none; "
        "font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "alternate-background-color: #252526; }"
        "QTreeView::item:selected { background: #094771; color: #ffffff; }"
        "QHeaderView::section { background: #252526; color: #858585; border: none;"
        "  border-right: 1px solid #3e3e42; border-bottom: 1px solid #3e3e42;"
        "  padding: 3px 6px; font-size: 11px; }"));

    connect(m_localsView, &QTreeView::customContextMenuRequested,
            this, &DebugPanel::onLocalsContextMenu);

    lay->addWidget(m_localsView);
    m_tabs->addTab(w, tr("Locals"));
}

// ── Breakpoints Tab ──────────────────────────────────────────────────────────

void DebugPanel::setupBreakpointsTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    // ── Top toolbar: + Watchpoint, Hit Count editor ─────────────────────────
    auto *barW = new QWidget(w);
    auto *bar = new QHBoxLayout(barW);
    bar->setContentsMargins(4, 4, 4, 4);
    bar->setSpacing(6);
    barW->setStyleSheet(QStringLiteral(
        "QWidget { background: #2d2d30; }"
        "QLabel  { color: #cccccc; font-size: 11px; }"
        "QToolButton { color: #d4d4d4; background: transparent; "
        "  padding: 3px 8px; border: 1px solid #3c3c3c; border-radius: 2px; }"
        "QToolButton:hover { background: #3e3e42; }"
        "QToolButton:pressed { background: #094771; }"
        "QToolButton:disabled { color: #6e6e6e; border-color: #2d2d30; }"
        "QSpinBox { background: #2b2b2b; color: #d4d4d4; "
        "  border: 1px solid #3c3c3c; padding: 2px 4px; min-width: 50px; }"
        "QSpinBox:focus { border: 1px solid #007acc; }"
        "QSpinBox:disabled { color: #6e6e6e; background: #252526; }"));

    m_addWatchpointBtn = new QToolButton(barW);
    m_addWatchpointBtn->setText(tr("+ Watchpoint"));
    m_addWatchpointBtn->setToolTip(tr("Add a data breakpoint (watchpoint) on a variable or memory expression"));
    bar->addWidget(m_addWatchpointBtn);

    bar->addSpacing(12);

    auto *hcLbl = new QLabel(tr("Hit Count:"), barW);
    bar->addWidget(hcLbl);

    m_hitCountSpin = new QSpinBox(barW);
    m_hitCountSpin->setRange(0, 1000000);
    m_hitCountSpin->setValue(0);
    m_hitCountSpin->setSpecialValueText(tr("always"));
    m_hitCountSpin->setToolTip(tr("Break only after this many hits (0 = always). "
                                  "Select a breakpoint then click Apply."));
    bar->addWidget(m_hitCountSpin);

    m_applyHitCountBtn = new QToolButton(barW);
    m_applyHitCountBtn->setText(tr("Apply"));
    m_applyHitCountBtn->setToolTip(tr("Apply hit-count to the selected breakpoint"));
    bar->addWidget(m_applyHitCountBtn);

    bar->addStretch();
    lay->addWidget(barW);

    // ── Table — 5 columns: Kind / Enabled / File or Expression / Line / Hit ─
    m_breakpointsTable = new QTableWidget(0, 5, w);
    m_breakpointsTable->setHorizontalHeaderLabels(
        {tr("Kind"), tr("Enabled"), tr("File / Expression"), tr("Line"), tr("Hit Count")});
    m_breakpointsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_breakpointsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_breakpointsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_breakpointsTable->verticalHeader()->hide();
    m_breakpointsTable->setAlternatingRowColors(true);
    m_breakpointsTable->setColumnWidth(0, 60);   // Kind
    m_breakpointsTable->setColumnWidth(1, 60);   // Enabled
    m_breakpointsTable->setColumnWidth(2, 280);  // File / Expression
    m_breakpointsTable->setColumnWidth(3, 60);   // Line
    m_breakpointsTable->setColumnWidth(4, 80);   // Hit Count
    m_breakpointsTable->horizontalHeader()->setStretchLastSection(true);
    m_breakpointsTable->setStyleSheet(QLatin1String(kDebugTableStyle));

    lay->addWidget(m_breakpointsTable, 1);

    connect(m_addWatchpointBtn, &QToolButton::clicked,
            this, &DebugPanel::onAddWatchpointClicked);
    connect(m_applyHitCountBtn, &QToolButton::clicked,
            this, &DebugPanel::onBreakpointHitCountChanged);

    // Reflect the selected row's existing hit-count into the spinner so
    // the user has a sensible starting value before editing.
    connect(m_breakpointsTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        const int row = m_breakpointsTable->currentRow();
        if (row < 0 || !m_hitCountSpin) return;
        const auto *hcItem = m_breakpointsTable->item(row, 4);
        if (!hcItem) return;
        QSignalBlocker b(m_hitCountSpin);
        m_hitCountSpin->setValue(hcItem->text().toInt());
    });

    m_tabs->addTab(w, tr("Breakpoints"));
}

// ── Watch Tab ─────────────────────────────────────────────────────────────────

void DebugPanel::setupWatchTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    // Input row: line edit + remove button
    auto *inputLay = new QHBoxLayout;
    inputLay->setContentsMargins(0, 0, 0, 0);
    inputLay->setSpacing(4);

    m_watchInput = new QLineEdit(w);
    m_watchInput->setPlaceholderText(tr("Add expression to watch (Enter)..."));
    m_watchInput->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #2b2b2b; color: #d4d4d4; border: 1px solid #3c3c3c; "
        "padding: 4px; font-family: 'Consolas','Courier New',monospace; }"
        "QLineEdit:focus { border: 1px solid #007acc; }"
        "QLineEdit:disabled { color: #6e6e6e; background: #252526; }"));
    inputLay->addWidget(m_watchInput, 1);

    m_watchRemoveBtn = new QToolButton(w);
    m_watchRemoveBtn->setText(QStringLiteral("✕")); // ✕
    m_watchRemoveBtn->setToolTip(tr("Remove selected watch"));
    m_watchRemoveBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #d4d4d4; background: #2b2b2b; "
        "border: 1px solid #3c3c3c; padding: 3px 8px; }"
        "QToolButton:hover { background: #3e3e42; }"
        "QToolButton:pressed { background: #094771; }"
        "QToolButton:disabled { color: #6e6e6e; background: #252526; }"));
    inputLay->addWidget(m_watchRemoveBtn);

    connect(m_watchInput, &QLineEdit::returnPressed,
            this, &DebugPanel::onWatchInputSubmit);
    connect(m_watchRemoveBtn, &QToolButton::clicked,
            this, &DebugPanel::onWatchRemoveSelected);

    lay->addLayout(inputLay);

    // Tree view for watches
    m_watchModel = new WatchTreeModel(this);

    m_watchView = new QTreeView(w);
    m_watchView->setModel(m_watchModel);
    m_watchView->setRootIsDecorated(true);
    m_watchView->setAnimated(true);
    m_watchView->setAlternatingRowColors(true);
    m_watchView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_watchView->header()->setStretchLastSection(true);
    m_watchView->setColumnWidth(0, 150);
    m_watchView->setColumnWidth(1, 200);
    m_watchView->setStyleSheet(QStringLiteral(
        "QTreeView { background: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; "
        "font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "alternate-background-color: #252526; }"
        "QTreeView::item:selected { background: #094771; color: #ffffff; }"
        "QHeaderView::section { background: #252526; color: #858585; border: none;"
        "  border-right: 1px solid #3e3e42; border-bottom: 1px solid #3e3e42;"
        "  padding: 3px 6px; font-size: 11px; }"));

    connect(m_watchView, &QTreeView::customContextMenuRequested,
            this, &DebugPanel::onWatchContextMenu);

    lay->addWidget(m_watchView, 1);

    m_tabs->addTab(w, tr("Watch"));
}

// ── Output Tab ────────────────────────────────────────────────────────────────

void DebugPanel::setupOutputTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(0, 0, 0, 0);

    m_outputText = new QPlainTextEdit(w);
    m_outputText->setReadOnly(true);
    m_outputText->setMaximumBlockCount(10000);
    m_outputText->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background: #1e1e1e; color: #d4d4d4; "
        "font-family: 'Consolas','Courier New',monospace; font-size: 12px; }"));
    lay->addWidget(m_outputText);

    m_tabs->addTab(w, tr("Debug Output"));
}

// ── Adapter wiring ────────────────────────────────────────────────────────────

void DebugPanel::setAdapter(IDebugAdapter *adapter)
{
    if (m_adapter) {
        disconnect(m_adapter, nullptr, this, nullptr);
    }

    m_adapter = adapter;
    if (!m_adapter) return;

    // Wire watch/locals models to the same adapter
    m_watchModel->setAdapter(m_adapter);
    m_localsModel->setAdapter(m_adapter);

    // All connections use SIGNAL/SLOT string syntax — PMF connect silently
    // fails because IDebugAdapter MOC lives in the exe while DebugPanel
    // is compiled into libdebug.dll (IndexOfMethod compares function pointers
    // across DLL boundaries, which never match).
    connect(m_adapter, SIGNAL(started()),
            this, SLOT(onAdapterStarted()));
    connect(m_adapter, SIGNAL(terminated()),
            this, SLOT(onAdapterTerminated()));
    connect(m_adapter, SIGNAL(error(QString)),
            this, SLOT(onAdapterError(QString)));
    connect(m_adapter, SIGNAL(stopped(int,DebugStopReason,QString)),
            this, SLOT(onAdapterStopped(int,DebugStopReason,QString)));
    connect(m_adapter, SIGNAL(continued(int)),
            this, SLOT(onAdapterContinued(int)));
    connect(m_adapter, SIGNAL(threadsReceived(QList<DebugThread>)),
            this, SLOT(onThreadsReceived(QList<DebugThread>)));
    connect(m_adapter, SIGNAL(stackTraceReceived(int,QList<DebugFrame>)),
            this, SLOT(onStackTraceReceived(int,QList<DebugFrame>)));
    connect(m_adapter, SIGNAL(variablesReceived(int,QList<DebugVariable>)),
            this, SLOT(onVariablesReceived(int,QList<DebugVariable>)));
    connect(m_adapter, SIGNAL(evaluateResult(QString,QString)),
            this, SLOT(onEvaluateResult(QString,QString)));
    connect(m_adapter, SIGNAL(outputProduced(QString,QString)),
            this, SLOT(onOutputProduced(QString,QString)));
    connect(m_adapter, SIGNAL(breakpointSet(DebugBreakpoint)),
            this, SLOT(onBreakpointVerified(DebugBreakpoint)));
    // Watchpoint signals — same SIGNAL/SLOT cross-DLL pattern.
    connect(m_adapter, SIGNAL(watchpointSet(DebugWatchpoint)),
            this, SLOT(onWatchpointSet(DebugWatchpoint)));
    connect(m_adapter, SIGNAL(watchpointRemoved(int)),
            this, SLOT(onWatchpointRemoved(int)));
}

// ── Watch input handlers ──────────────────────────────────────────────────────

void DebugPanel::onWatchInputSubmit()
{
    const QString expr = m_watchInput->text().trimmed();
    if (expr.isEmpty()) return;
    m_watchModel->addWatch(expr);  // creates a persistent GDB var-object, not a one-shot evaluate
    m_watchInput->clear();
}

void DebugPanel::onWatchRemoveSelected()
{
    const QModelIndex idx = m_watchView->currentIndex();
    if (!idx.isValid()) return;

    // Only top-level rows correspond to user-added watch expressions;
    // children are sub-fields populated via GDB var-object children.
    QModelIndex top = idx;
    while (top.parent().isValid())
        top = top.parent();

    const QString expr = top.sibling(top.row(), 0).data().toString();
    if (expr.isEmpty()) return;
    m_watchModel->removeWatch(expr);
}

// ── Adapter signal handlers ───────────────────────────────────────────────────

void DebugPanel::onAdapterStarted()
{
    setRunning(true, false);
    m_statusLabel->setText(tr("Running"));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #89d185; font-size: 11px;"));
    m_outputText->clear();
    m_outputText->appendPlainText(tr("--- Debug session started ---"));
    m_watchInput->setEnabled(true);
    m_watchRemoveBtn->setEnabled(true);
}

void DebugPanel::onAdapterTerminated()
{
    setRunning(false);
    m_statusLabel->setText(tr("Terminated"));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #808080; font-size: 11px;"));
    m_outputText->appendPlainText(tr("--- Debug session ended ---"));
    m_watchInput->setEnabled(false);
    m_watchRemoveBtn->setEnabled(false);
    m_threadsTable->setRowCount(0);
}

void DebugPanel::onAdapterError(const QString &msg)
{
    m_outputText->appendPlainText(tr("[ERROR] %1").arg(msg));
}

void DebugPanel::onAdapterStopped(int threadId, DebugStopReason reason,
                                   const QString &description)
{
    Q_UNUSED(reason)
    m_currentThread = threadId;
    setRunning(true, true);
    m_statusLabel->setText(tr("Paused: %1 (thread %2)").arg(description).arg(threadId));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #dcdcaa; font-size: 11px;"));

    // Auto-request stack trace and locals
    if (m_adapter) {
        m_adapter->requestThreads();
        m_adapter->requestStackTrace(threadId);
        m_adapter->requestScopes(0);
    }

    // Refresh watch expressions after each stop (picks up changed values)
    m_watchModel->onDebuggerStopped();
}

void DebugPanel::onAdapterContinued(int /*threadId*/)
{
    setRunning(true, false);
    m_statusLabel->setText(tr("Running"));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #89d185; font-size: 11px;"));
    m_callStackTable->setRowCount(0);
    m_localsModel->clearAll();
}

void DebugPanel::onThreadsReceived(const QList<DebugThread> &threads)
{
    m_threadsTable->setRowCount(threads.size());
    int activeRow = -1;
    for (int i = 0; i < threads.size(); ++i) {
        const auto &t = threads[i];

        auto *idItem    = new QTableWidgetItem(QString::number(t.id));
        auto *nameItem  = new QTableWidgetItem(t.name);
        auto *stateItem = new QTableWidgetItem(t.stopped ? tr("Stopped") : tr("Running"));

        // Stash the thread id on the row so the click handler does not have
        // to re-parse the displayed text (which is locale-formatted).
        idItem->setData(Qt::UserRole, t.id);

        m_threadsTable->setItem(i, 0, idItem);
        m_threadsTable->setItem(i, 1, nameItem);
        m_threadsTable->setItem(i, 2, stateItem);

        if (t.id == m_currentThread)
            activeRow = i;
    }

    if (activeRow >= 0)
        m_threadsTable->selectRow(activeRow);
}

void DebugPanel::onStackTraceReceived(int /*threadId*/, const QList<DebugFrame> &frames)
{
    m_callStackTable->setRowCount(frames.size());
    for (int i = 0; i < frames.size(); ++i) {
        const auto &f = frames[i];
        m_callStackTable->setItem(i, 0, new QTableWidgetItem(QString::number(f.id)));
        m_callStackTable->setItem(i, 1, new QTableWidgetItem(f.name));

        QString loc;
        if (!f.filePath.isEmpty())
            loc = QStringLiteral("%1:%2").arg(f.filePath).arg(f.line);
        m_callStackTable->setItem(i, 2, new QTableWidgetItem(loc));

        // Store frame data
        m_callStackTable->item(i, 2)->setData(Qt::UserRole, f.filePath);
        m_callStackTable->item(i, 2)->setData(Qt::UserRole + 1, f.line);
    }

    // Navigate to top frame
    if (!frames.isEmpty() && !frames.first().filePath.isEmpty()) {
        emit navigateToSource(frames.first().filePath, frames.first().line);
    }
}

void DebugPanel::onVariablesReceived(int /*ref*/, const QList<DebugVariable> &vars)
{
    // Populate Locals tree directly from the snapshot (synchronous). The
    // previous clearAll() + addWatch() approach left the tree empty until
    // async var-object creates returned, which made Locals appear blank
    // and would then race with onAdapterContinued()'s clearAll() on resume.
    m_localsModel->setLocals(vars);
}

void DebugPanel::onEvaluateResult(const QString &expr, const QString &result)
{
    if (!expr.isEmpty())
        m_outputText->appendPlainText(QStringLiteral("%1 = %2").arg(expr, result));
    else
        m_outputText->appendPlainText(result);
}

void DebugPanel::onOutputProduced(const QString &text, const QString &category)
{
    Q_UNUSED(category)
    m_outputText->appendPlainText(text);
}

void DebugPanel::onBreakpointVerified(const DebugBreakpoint &bp)
{
    for (int r = 0; r < m_breakpointsTable->rowCount(); ++r) {
        const auto *kindItem = m_breakpointsTable->item(r, 0);
        if (kindItem && kindItem->text() != tr("Break"))
            continue; // skip watchpoint rows
        const auto *fileItem = m_breakpointsTable->item(r, 2);
        const auto *lineItem = m_breakpointsTable->item(r, 3);
        if (fileItem && lineItem
            && fileItem->text() == bp.filePath
            && lineItem->text().toInt() == bp.line) {
            m_breakpointsTable->item(r, 1)->setText(
                bp.verified ? tr("\u2714") : tr("\u25CB"));
            // Persist adapter-assigned id and hit-count for later edits.
            m_breakpointsTable->item(r, 0)->setData(Qt::UserRole, bp.id);
            if (auto *hcItem = m_breakpointsTable->item(r, 4)) {
                hcItem->setText(QString::number(bp.hitCount));
            }
            break;
        }
    }
}

void DebugPanel::onWatchpointSet(const DebugWatchpoint &wp)
{
    // First try to update an existing pending row matching this expression.
    for (int r = 0; r < m_breakpointsTable->rowCount(); ++r) {
        const auto *kindItem = m_breakpointsTable->item(r, 0);
        if (!kindItem || kindItem->text() != tr("Watch"))
            continue;
        const auto *exprItem = m_breakpointsTable->item(r, 2);
        if (exprItem && exprItem->text() == wp.expression
            && m_breakpointsTable->item(r, 0)->data(Qt::UserRole).toInt() <= 0) {
            m_breakpointsTable->item(r, 0)->setData(Qt::UserRole, wp.id);
            m_breakpointsTable->item(r, 1)->setText(
                wp.verified ? tr("\u2714") : tr("\u25CB"));
            return;
        }
    }

    // Otherwise insert a fresh row (e.g. created from elsewhere).
    const int row = m_breakpointsTable->rowCount();
    m_breakpointsTable->insertRow(row);
    auto *kindItem = new QTableWidgetItem(tr("Watch"));
    kindItem->setData(Qt::UserRole, wp.id);
    // type stored in UserRole+1 so a future edit dialog can re-show it.
    kindItem->setData(Qt::UserRole + 1, static_cast<int>(wp.type));
    m_breakpointsTable->setItem(row, 0, kindItem);
    m_breakpointsTable->setItem(row, 1,
        new QTableWidgetItem(wp.verified ? tr("\u2714") : tr("\u25CB")));
    m_breakpointsTable->setItem(row, 2, new QTableWidgetItem(wp.expression));
    m_breakpointsTable->setItem(row, 3, new QTableWidgetItem(QString())); // no line
    m_breakpointsTable->setItem(row, 4, new QTableWidgetItem(QString())); // no hit-count
}

void DebugPanel::onWatchpointRemoved(int watchpointId)
{
    for (int r = 0; r < m_breakpointsTable->rowCount(); ++r) {
        const auto *kindItem = m_breakpointsTable->item(r, 0);
        if (!kindItem || kindItem->text() != tr("Watch"))
            continue;
        if (kindItem->data(Qt::UserRole).toInt() == watchpointId) {
            m_breakpointsTable->removeRow(r);
            return;
        }
    }
}

void DebugPanel::onCallStackDoubleClicked(int row, int col)
{
    // Same behaviour as a single click — kept for users who instinctively
    // double-click rows in tables.
    onCallStackClicked(row, col);
}

void DebugPanel::onCallStackClicked(int row, int /*col*/)
{
    if (row < 0 || row >= m_callStackTable->rowCount()) return;

    auto *idItem  = m_callStackTable->item(row, 0);
    auto *locItem = m_callStackTable->item(row, 2);
    if (!idItem || !locItem) return;

    // Highlight the selected row so the user sees which frame is active.
    m_callStackTable->selectRow(row);

    // Tell GDB to switch context to this frame so subsequent locals/eval
    // commands run against it. The adapter also re-requests the stack
    // and locals so the Locals tab refreshes for the new frame.
    bool ok = false;
    const int frameId = idItem->text().toInt(&ok);
    if (ok && m_adapter)
        m_adapter->stackSelectFrame(frameId);

    // Move the editor cursor to the source location of the selected frame.
    const QString filePath = locItem->data(Qt::UserRole).toString();
    const int line = locItem->data(Qt::UserRole + 1).toInt();
    if (!filePath.isEmpty() && line > 0)
        emit navigateToSource(filePath, line);
}

void DebugPanel::onThreadClicked(int row, int /*col*/)
{
    if (row < 0 || row >= m_threadsTable->rowCount()) return;

    auto *idItem = m_threadsTable->item(row, 0);
    if (!idItem) return;

    bool ok = false;
    // Prefer the int stashed in UserRole; fall back to the displayed text.
    int threadId = idItem->data(Qt::UserRole).toInt(&ok);
    if (!ok || threadId == 0)
        threadId = idItem->text().toInt(&ok);
    if (!ok) return;

    m_threadsTable->selectRow(row);
    m_currentThread = threadId;

    // Re-issue the stack trace for the newly selected thread; the resulting
    // onStackTraceReceived() will populate the Call Stack tab and navigate
    // the editor to the top frame.
    if (m_adapter)
        m_adapter->requestStackTrace(threadId);
}

// ── Breakpoint entries ────────────────────────────────────────────────────────

void DebugPanel::addBreakpointEntry(const QString &filePath, int line)
{
    // Check for duplicate (Break rows only — Watch rows have no file/line)
    for (int r = 0; r < m_breakpointsTable->rowCount(); ++r) {
        const auto *kindItem = m_breakpointsTable->item(r, 0);
        if (kindItem && kindItem->text() != tr("Break")) continue;
        if (m_breakpointsTable->item(r, 2)->text() == filePath
            && m_breakpointsTable->item(r, 3)->text().toInt() == line)
            return; // already exists
    }

    const int row = m_breakpointsTable->rowCount();
    m_breakpointsTable->insertRow(row);
    auto *kindItem = new QTableWidgetItem(tr("Break"));
    kindItem->setData(Qt::UserRole, -1); // adapter-id unset
    m_breakpointsTable->setItem(row, 0, kindItem);
    m_breakpointsTable->setItem(row, 1, new QTableWidgetItem(tr("\u25CB"))); // pending
    m_breakpointsTable->setItem(row, 2, new QTableWidgetItem(filePath));
    m_breakpointsTable->setItem(row, 3, new QTableWidgetItem(QString::number(line)));
    m_breakpointsTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("0")));

    // Send to adapter if active
    if (m_adapter && m_adapter->isRunning()) {
        DebugBreakpoint bp;
        bp.filePath = filePath;
        bp.line = line;
        m_adapter->addBreakpoint(bp);
    }
}

void DebugPanel::removeBreakpointEntry(const QString &filePath, int line)
{
    for (int r = 0; r < m_breakpointsTable->rowCount(); ++r) {
        const auto *kindItem = m_breakpointsTable->item(r, 0);
        if (kindItem && kindItem->text() != tr("Break")) continue;
        if (m_breakpointsTable->item(r, 2)->text() == filePath
            && m_breakpointsTable->item(r, 3)->text().toInt() == line) {
            m_breakpointsTable->removeRow(r);
            break;
        }
    }
}

// ── State management ──────────────────────────────────────────────────────────

void DebugPanel::setRunning(bool running, bool stopped)
{
    // The embedded toolbar (with Launch/Continue/Step/Pause/Stop) was removed;
    // those controls now live exclusively on the main window's debug toolbar,
    // which manages its own enabled-state via the build/debug plugin. The
    // dock-internal status label is updated by the adapter signal handlers.
    Q_UNUSED(running);
    Q_UNUSED(stopped);
}

void DebugPanel::onQuickWatch()
{
    if (!m_adapter) return;

    auto *dlg = new QuickWatchDialog(m_adapter, this);
    connect(dlg, &QuickWatchDialog::addToWatch, this, [this](const QString &expr) {
        m_watchModel->addWatch(expr);
    });

    // Pre-fill from selected locals item if available
    const QModelIndex idx = m_localsView->currentIndex();
    if (idx.isValid()) {
        const QString expr = idx.sibling(idx.row(), 0).data().toString();
        dlg->setExpression(expr);
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

void DebugPanel::onLocalsContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_localsView->indexAt(pos);
    if (!idx.isValid()) return;

    const QString name  = idx.sibling(idx.row(), 0).data().toString();
    const QString value = idx.sibling(idx.row(), 1).data().toString();

    QMenu menu(this);

    menu.addAction(tr("Copy Name"), this, [name]() {
        QApplication::clipboard()->setText(name);
    });
    menu.addAction(tr("Copy Value"), this, [value]() {
        QApplication::clipboard()->setText(value);
    });
    menu.addAction(tr("Copy Name = Value"), this, [name, value]() {
        QApplication::clipboard()->setText(
            QStringLiteral("%1 = %2").arg(name, value));
    });
    menu.addSeparator();
    menu.addAction(tr("Add to Watch"), this, [this, name]() {
        m_watchModel->addWatch(name);
    });

    menu.exec(m_localsView->viewport()->mapToGlobal(pos));
}

void DebugPanel::onWatchContextMenu(const QPoint &pos)
{
    const QModelIndex idx = m_watchView->indexAt(pos);

    QMenu menu(this);

    if (idx.isValid()) {
        const QString name  = idx.sibling(idx.row(), 0).data().toString();
        const QString value = idx.sibling(idx.row(), 1).data().toString();

        menu.addAction(tr("Copy Name"), this, [name]() {
            QApplication::clipboard()->setText(name);
        });
        menu.addAction(tr("Copy Value"), this, [value]() {
            QApplication::clipboard()->setText(value);
        });
        menu.addSeparator();
        menu.addAction(tr("Remove Watch"), this, [this, name]() {
            m_watchModel->removeWatch(name);
        });
    }

    menu.addSeparator();
    menu.addAction(tr("Remove All Watches"), this, [this]() {
        m_watchModel->clearAll();
    });

    menu.exec(m_watchView->viewport()->mapToGlobal(pos));
}

// ── Watchpoint dialog launcher ───────────────────────────────────────────────

void DebugPanel::onAddWatchpointClicked()
{
    auto *dlg = new WatchpointDialog(this);
    connect(dlg, &WatchpointDialog::watchpointConfigured,
            this, [this](const QString &expr, int type) {
        // Insert a placeholder row immediately for instant feedback;
        // onWatchpointSet() will fill in the adapter-assigned id once
        // the backend confirms.
        const int row = m_breakpointsTable->rowCount();
        m_breakpointsTable->insertRow(row);
        auto *kindItem = new QTableWidgetItem(tr("Watch"));
        kindItem->setData(Qt::UserRole, -1);            // pending id
        kindItem->setData(Qt::UserRole + 1, type);      // store type for re-edit
        m_breakpointsTable->setItem(row, 0, kindItem);
        m_breakpointsTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("○")));
        m_breakpointsTable->setItem(row, 2, new QTableWidgetItem(expr));
        m_breakpointsTable->setItem(row, 3, new QTableWidgetItem(QString()));
        m_breakpointsTable->setItem(row, 4, new QTableWidgetItem(QString()));

        if (!m_adapter) {
            m_breakpointsTable->item(row, 1)->setText(QStringLiteral("?"));
            return;
        }
        DebugWatchpoint wp;
        wp.expression = expr;
        wp.type = static_cast<DebugWatchpoint::Type>(type);
        m_adapter->addWatchpoint(wp);
    });
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// ── Hit-count edit on selected breakpoint ────────────────────────────────────

void DebugPanel::onBreakpointHitCountChanged()
{
    if (m_updatingHitCount) return;
    if (!m_breakpointsTable || !m_hitCountSpin) return;

    const int row = m_breakpointsTable->currentRow();
    if (row < 0) return;

    const auto *kindItem = m_breakpointsTable->item(row, 0);
    if (!kindItem || kindItem->text() != tr("Break")) return;

    const auto *fileItem = m_breakpointsTable->item(row, 2);
    const auto *lineItem = m_breakpointsTable->item(row, 3);
    if (!fileItem || !lineItem) return;

    const int newHitCount = m_hitCountSpin->value();
    const int oldId = kindItem->data(Qt::UserRole).toInt();

    // Update the table cell now; backend confirmation will arrive via
    // onBreakpointVerified and re-write the cell to the same value.
    m_updatingHitCount = true;
    m_breakpointsTable->item(row, 4)->setText(QString::number(newHitCount));
    m_updatingHitCount = false;

    if (!m_adapter || !m_adapter->isRunning()) return;

    // Re-create the breakpoint with the new hit-count. Adapters that
    // don't support live -break-after editing handle this via the
    // remove/add pair (mirrors the existing condition flow).
    if (oldId > 0)
        m_adapter->removeBreakpoint(oldId);

    DebugBreakpoint bp;
    bp.filePath = fileItem->text();
    bp.line     = lineItem->text().toInt();
    bp.hitCount = newHitCount;
    m_adapter->addBreakpoint(bp);
}
