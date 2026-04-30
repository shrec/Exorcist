#include "debugpanel.h"
#include "sdk/idebugadapter.h"
#include "watchtreemodel.h"
#include "quickwatchdialog.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>

DebugPanel::DebugPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    setupToolBar();
    root->addWidget(m_toolbar);

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
    setupLocalsTab();
    setupBreakpointsTab();
    setupWatchTab();
    setupOutputTab();

    setRunning(false);
}

// ── Toolbar ───────────────────────────────────────────────────────────────────

void DebugPanel::setupToolBar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);
    m_toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { background: #2d2d30; border-bottom: 1px solid #3e3e42; "
        "spacing: 2px; padding: 2px; }"
        "QToolButton { color: #d4d4d4; padding: 3px 8px; border: none; "
        "border-radius: 2px; font-size: 13px; }"
        "QToolButton:hover { background: #3e3e42; }"
        "QToolButton:pressed { background: #094771; }"
        "QToolButton:disabled { color: #555558; }"));

    m_actLaunch   = m_toolbar->addAction(tr("\u25B6 Launch"));
    m_actContinue = m_toolbar->addAction(tr("\u25B6 Continue"));
    m_actStepOver = m_toolbar->addAction(tr("\u23ED Step Over"));
    m_actStepInto = m_toolbar->addAction(tr("\u2B07 Step Into"));
    m_actStepOut  = m_toolbar->addAction(tr("\u2B06 Step Out"));
    m_actPause    = m_toolbar->addAction(tr("\u23F8 Pause"));
    m_actStop     = m_toolbar->addAction(tr("\u25A0 Stop"));

    connect(m_actLaunch,   &QAction::triggered, this, &DebugPanel::onLaunchClicked);
    connect(m_actContinue, &QAction::triggered, this, &DebugPanel::onContinueClicked);
    connect(m_actStepOver, &QAction::triggered, this, &DebugPanel::onStepOverClicked);
    connect(m_actStepInto, &QAction::triggered, this, &DebugPanel::onStepIntoClicked);
    connect(m_actStepOut,  &QAction::triggered, this, &DebugPanel::onStepOutClicked);
    connect(m_actPause,    &QAction::triggered, this, &DebugPanel::onPauseClicked);
    connect(m_actStop,     &QAction::triggered, this, &DebugPanel::onStopClicked);
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

    lay->addWidget(m_callStackTable);
    m_tabs->addTab(w, tr("Call Stack"));
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

    m_breakpointsTable = new QTableWidget(0, 3, w);
    m_breakpointsTable->setHorizontalHeaderLabels({tr("Enabled"), tr("File"), tr("Line")});
    m_breakpointsTable->horizontalHeader()->setStretchLastSection(true);
    m_breakpointsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_breakpointsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_breakpointsTable->verticalHeader()->hide();
    m_breakpointsTable->setAlternatingRowColors(true);
    m_breakpointsTable->setColumnWidth(0, 60);
    m_breakpointsTable->setColumnWidth(1, 250);
    m_breakpointsTable->setStyleSheet(QLatin1String(kDebugTableStyle));

    lay->addWidget(m_breakpointsTable);
    m_tabs->addTab(w, tr("Breakpoints"));
}

// ── Watch Tab ─────────────────────────────────────────────────────────────────

void DebugPanel::setupWatchTab()
{
    auto *w = new QWidget(this);
    auto *lay = new QVBoxLayout(w);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    // Input row
    auto *inputLay = new QHBoxLayout;
    m_watchInput = new QLineEdit(w);
    m_watchInput->setPlaceholderText(tr("Add watch expression..."));
    m_watchInput->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #2b2b2b; color: #d4d4d4; border: 1px solid #3c3c3c; "
        "padding: 4px; font-family: 'Consolas','Courier New',monospace; }"));
    inputLay->addWidget(m_watchInput);

    connect(m_watchInput, &QLineEdit::returnPressed,
            this, &DebugPanel::onWatchInputSubmit);

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
}

// ── Toolbar actions ───────────────────────────────────────────────────────────

void DebugPanel::onLaunchClicked()
{
    emit launchRequested();
}

void DebugPanel::onContinueClicked()
{
    if (m_adapter) m_adapter->continueExecution();
}

void DebugPanel::onStepOverClicked()
{
    if (m_adapter) m_adapter->stepOver();
}

void DebugPanel::onStepIntoClicked()
{
    if (m_adapter) m_adapter->stepInto();
}

void DebugPanel::onStepOutClicked()
{
    if (m_adapter) m_adapter->stepOut();
}

void DebugPanel::onPauseClicked()
{
    if (m_adapter) m_adapter->pause();
}

void DebugPanel::onStopClicked()
{
    if (m_adapter) m_adapter->terminate();
}

void DebugPanel::onWatchInputSubmit()
{
    const QString expr = m_watchInput->text().trimmed();
    if (expr.isEmpty()) return;
    m_watchModel->addWatch(expr);  // creates a persistent GDB var-object, not a one-shot evaluate
    m_watchInput->clear();
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
}

void DebugPanel::onAdapterTerminated()
{
    setRunning(false);
    m_statusLabel->setText(tr("Terminated"));
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #808080; font-size: 11px;"));
    m_outputText->appendPlainText(tr("--- Debug session ended ---"));
}

void DebugPanel::onAdapterError(const QString &msg)
{
    m_outputText->appendPlainText(tr("[ERROR] %1").arg(msg));
}

void DebugPanel::onAdapterStopped(int threadId, DebugStopReason reason,
                                   const QString &description)
{
    Q_UNUSED(reason)
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

void DebugPanel::onThreadsReceived(const QList<DebugThread> &/*threads*/)
{
    // Could show in a threads sub-tab; for now we focus on the stopped thread
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
        const auto *fileItem = m_breakpointsTable->item(r, 1);
        const auto *lineItem = m_breakpointsTable->item(r, 2);
        if (fileItem && lineItem
            && fileItem->text() == bp.filePath
            && lineItem->text().toInt() == bp.line) {
            m_breakpointsTable->item(r, 0)->setText(
                bp.verified ? tr("\u2714") : tr("\u25CB"));
            break;
        }
    }
}

void DebugPanel::onCallStackDoubleClicked(int row, int /*col*/)
{
    if (row < 0 || row >= m_callStackTable->rowCount()) return;

    auto *locItem = m_callStackTable->item(row, 2);
    if (!locItem) return;

    const QString filePath = locItem->data(Qt::UserRole).toString();
    const int line = locItem->data(Qt::UserRole + 1).toInt();
    if (!filePath.isEmpty() && line > 0)
        emit navigateToSource(filePath, line);
}

// ── Breakpoint entries ────────────────────────────────────────────────────────

void DebugPanel::addBreakpointEntry(const QString &filePath, int line)
{
    // Check for duplicate
    for (int r = 0; r < m_breakpointsTable->rowCount(); ++r) {
        if (m_breakpointsTable->item(r, 1)->text() == filePath
            && m_breakpointsTable->item(r, 2)->text().toInt() == line)
            return; // already exists
    }

    const int row = m_breakpointsTable->rowCount();
    m_breakpointsTable->insertRow(row);
    m_breakpointsTable->setItem(row, 0, new QTableWidgetItem(tr("\u25CB"))); // pending
    m_breakpointsTable->setItem(row, 1, new QTableWidgetItem(filePath));
    m_breakpointsTable->setItem(row, 2, new QTableWidgetItem(QString::number(line)));

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
        if (m_breakpointsTable->item(r, 1)->text() == filePath
            && m_breakpointsTable->item(r, 2)->text().toInt() == line) {
            m_breakpointsTable->removeRow(r);
            break;
        }
    }
}

// ── State management ──────────────────────────────────────────────────────────

void DebugPanel::setRunning(bool running, bool stopped)
{
    m_actLaunch->setEnabled(!running);
    m_actContinue->setEnabled(running && stopped);
    m_actStepOver->setEnabled(running && stopped);
    m_actStepInto->setEnabled(running && stopped);
    m_actStepOut->setEnabled(running && stopped);
    m_actPause->setEnabled(running && !stopped);
    m_actStop->setEnabled(running);
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
