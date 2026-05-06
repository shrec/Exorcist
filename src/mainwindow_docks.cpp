// MainWindow::createDockWidgets — extracted from mainwindow.cpp.
//
// All dock widget construction + sidebar wiring lives here.  Methods stay
// on MainWindow class (Qt allows method bodies of one class across multiple
// .cpp files).  A future pass will extract this into a true DockBootstrap
// class with friend access to MainWindow.

#include "mainwindow.h"

#include "agent/agentplatformbootstrap.h"
#include "agent/chat/chatpanelwidget.h"
#include "agent/inlinecompletionengine.h"
#include "agent/memorybrowserpanel.h"
#include "agent/memorysuggestionengine.h"
#include "agent/projectbrainservice.h"
#include "lsp/referencespanel.h"
#include "agent/requestlogpanel.h"
#include "agent/settingspanel.h"
#include "agent/trajectorypanel.h"
#include "bootstrap/aiservicesbootstrap.h"
#include "bootstrap/dockbootstrap.h"
#include "bootstrap/postpluginbootstrap.h"
#include "bootstrap/workbenchservicesbootstrap.h"
#include "core/idockmanager.h"
#include "editor/breadcrumbbar.h"
#include "editor/diffviewerpanel.h"
#include "editor/editormanager.h"
#include "editor/proposededitpanel.h"
#include "editor/symboloutlinepanel.h"
#include "git/gitservice.h"
#include "lsp/lspclient.h"
#include "mcp/mcpclient.h"
#include "mcp/mcppanel.h"
#include "pluginmanager.h"
#include "problems/problemspanel.h"
#include "project/projectmanager.h"
#include "sdk/contributionregistry.h"
#include "sdk/hostservices.h"
#include "serviceregistry.h"
#include "ui/dock/DockManager.h"
#include "ui/dock/ExDockWidget.h"
#include "ui/markdownpreviewpanel.h"
#include "agent/networkmonitor.h"
#include "plugin/pluginmarketplaceservice.h"
#include "ui/themegallerypanel.h"

#include "project/solutiontreemodel.h"
#include "editor/editorview.h"
#include "editor/filewatchservice.h"
#include "agent/agentorchestrator.h"
#include "agent/authmanager.h"
#include "agent/contextbuilder.h"
#include "agent/oauthmanager.h"
#include "process/bridgeclient.h"
#include "agent/testgenerator.h"
#include "mcp/mcptooladapter.h"
#include "bootstrap/statusbarmanager.h"
#include "editor/inlinereviewwidget.h"
#include "ui/notificationtoast.h"
#include "thememanager.h"

#include <QMessageBox>
#include <QStatusBar>
#include <QTextBlock>
#include <QTextCursor>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
void MainWindow::createDockWidgets()
{
    // ── Create DockManager (takes over centralWidget) ─────────────────────
    m_dockManager = new exdock::DockManager(this, this);
    // Start with dock infrastructure hidden — only the Welcome page shows.
    m_dockManager->setDockLayoutVisible(false);


    // (ThemeManager→dockManager signal wired in createDockWidgets once dockManager exists.)

    // Build the central area as a QStackedWidget:
    //   page 0 = welcome page    (shown when no project is open)
    //   page 1 = editor container (breadcrumb + tab widget)
    //
    // The welcome page is owned by the start-page plugin (rule L3).  We
    // start with an empty placeholder; deferredInit() queries the
    // "welcomeWidget" service after plugins load and swaps it into page 0.
    m_centralStack = new QStackedWidget(this);
    m_editorMgr->setCentralStack(m_centralStack);

    auto *welcomePlaceholder = new QWidget(this);
    welcomePlaceholder->setObjectName(QStringLiteral("welcomePlaceholder"));
    m_centralStack->addWidget(welcomePlaceholder);  // page 0

    auto *editorContainer = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);
    editorLayout->addWidget(m_editorMgr->breadcrumb());
    editorLayout->addWidget(m_editorMgr->tabs());
    m_centralStack->addWidget(editorContainer);  // page 1

    m_centralStack->setCurrentIndex(0);  // start on welcome
    updateMenuBarVisibility(false);

    // Toggle dock infrastructure and toolbar with welcome ↔ editor transitions.
    // Order matters: show toolbars and update menu bar BEFORE repositioning the
    // dock sidebars, so repositionSideBars() sees the final menu+toolbar heights
    // and doesn't place sidebars at Y=0 (which would obscure menu bar items).
    connect(m_centralStack, &QStackedWidget::currentChanged, this, [this](int index) {
        const bool editing = (index != 0);
        // Hide/show all toolbars on welcome page
        for (auto *tb : findChildren<QToolBar *>())
            tb->setVisible(editing);
        updateMenuBarVisibility(editing);
        // Defer reposition to next event loop tick so menu+toolbar geometry is
        // settled when DockManager reads their heights.
        QTimer::singleShot(0, this, [this, editing]() {
            m_dockManager->setDockLayoutVisible(editing);
        });
    });

    // Welcome widget signals are routed via the command service by the
    // start-page plugin.  Here we register the matching command handlers
    // (rule L4 — dispatcher infra is plugin-agnostic, command handlers
    // route into MainWindow's existing methods).
    if (auto *cmdSvc = m_hostServices ? m_hostServices->commands() : nullptr) {
        cmdSvc->registerCommand(QStringLiteral("file.openFolder"),
                                tr("Open Folder..."),
                                [this]() { openFolder(); });
        cmdSvc->registerCommand(QStringLiteral("file.openFolderPath"),
                                tr("Open Recent Folder"),
                                [this]() {
            const QString path = QSettings(QStringLiteral("Exorcist"),
                                           QStringLiteral("Exorcist"))
                                     .value(QStringLiteral("startPage/openFolderPath"))
                                     .toString();
            if (!path.isEmpty()) openFolder(path);
        });
        cmdSvc->registerCommand(QStringLiteral("file.newProject"),
                                tr("New Project..."),
                                [this]() { newSolution(); });
        cmdSvc->registerCommand(QStringLiteral("file.newTab"),
                                tr("New Tab"),
                                [this]() { openNewTab(); });
        cmdSvc->registerCommand(QStringLiteral("file.openFile"),
                                tr("Open File..."),
                                [this]() {
            const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
            if (!path.isEmpty()) openFile(path);
        });
    }

    m_dockManager->setCentralContent(m_centralStack);


    // ── Project tree ──────────────────────────────────────────────────────
    m_editorMgr->setTreeModel(new SolutionTreeModel(m_editorMgr->projectManager(), m_gitService, this));

    m_editorMgr->setFileTree(new QTreeView);
    m_editorMgr->fileTree()->setModel(m_editorMgr->treeModel());
    m_editorMgr->fileTree()->setHeaderHidden(true);
    m_editorMgr->fileTree()->setIndentation(14);
    m_editorMgr->fileTree()->setContextMenuPolicy(Qt::CustomContextMenu);
    m_editorMgr->fileTree()->setAnimated(true);
    m_editorMgr->fileTree()->setUniformRowHeights(true);
    m_editorMgr->fileTree()->setIconSize(QSize(16, 16));
    m_editorMgr->fileTree()->setStyleSheet(QStringLiteral(
        "QTreeView {"
        "  background: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: none;"
        "  font-size: 13px;"
        "}"
        "QTreeView::item {"
        "  padding: 2px 0;"
        "  border: none;"
        "}"
        "QTreeView::item:hover {"
        "  background: #2a2d2e;"
        "}"
        "QTreeView::item:selected {"
        "  background: #094771;"
        "  color: #ffffff;"
        "}"
        "QTreeView::item:selected:!active {"
        "  background: #37373d;"
        "  color: #d4d4d4;"
        "}"
        "QTreeView::branch {"
        "  background: #1e1e1e;"
        "}"
        "QTreeView::branch:hover {"
        "  background: #2a2d2e;"
        "}"
        "QTreeView::branch:selected {"
        "  background: #094771;"
        "}"
        "QTreeView::branch:has-children:!has-siblings:closed,"
        "QTreeView::branch:closed:has-children:has-siblings {"
        "  image: url(:/icons/branch-closed.png);"
        "}"
        "QTreeView::branch:open:has-children:!has-siblings,"
        "QTreeView::branch:open:has-children:has-siblings {"
        "  image: url(:/icons/branch-open.png);"
        "}"
        "QScrollBar:vertical {"
        "  background: #1e1e1e;"
        "  width: 10px;"
        "  border: none;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: #424242;"
        "  min-height: 20px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "  background: #686868;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0;"
        "}"
        "QScrollBar:horizontal {"
        "  background: #1e1e1e;"
        "  height: 10px;"
        "  border: none;"
        "}"
        "QScrollBar::handle:horizontal {"
        "  background: #424242;"
        "  min-width: 20px;"
        "  border-radius: 5px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
        "  background: #686868;"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
        "  width: 0;"
        "}"
    ));
    connect(m_editorMgr->fileTree(), &QTreeView::doubleClicked, this, &MainWindow::openFileFromIndex);
    connect(m_editorMgr->fileTree(), &QTreeView::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);

    registerShellDock(QStringLiteral("ProjectDock"), tr("Project"),
                      m_editorMgr->fileTree(), IDockManager::Left);

    // ── Search — contributed by search plugin (plugins/search/) ────────
    // SearchDock, ISearchService ("searchService"), SymbolIndex ("symbolIndex"),
    // and WorkspaceIndexer ("workspaceIndexer") are registered by the plugin.
    // Post-plugin wiring connects ISearchService::resultActivated in loadPlugins().

    // ── Git panel — contributed by git plugin (plugins/git/) ──────────────
    // GitDock, DiffExplorerDock, and MergeEditorDock are registered by the
    // plugin. Post-plugin wiring connects GitService signals in loadPlugins().

    // ── Terminal — contributed by terminal plugin (plugins/terminal/) ─────
    // TerminalDock and ITerminalService ("terminalService") are registered
    // by the plugin via IViewContributor and ServiceRegistry.


    // ── DockBootstrap — AI, Outline, References, Log, Settings, Memory, ──
    // ── Theme Gallery, Diff, Proposed Edit, MCP, Plugin Gallery ──────────
    {
        DockBootstrap::Deps d;
        d.parent      = this;
        d.services    = m_services.get();
        d.dockManager = m_dockManager;
        d.themeManager = m_workbenchServices->themeManager();
        d.orchestrator = m_agentOrchestrator;
        if (m_services) {
            d.bridgeClient = m_services->service<BridgeClient>(
                QStringLiteral("bridgeClient"));
        }
        m_dockBootstrap = new DockBootstrap(this);
        m_dockBootstrap->initialize(d);
    }

    // Cache eagerly-created panel pointers (ChatPanel, SymbolOutline,
    // References, Settings are created in DockBootstrap::initialize).
    // Other panels are lazy — access via m_dockBootstrap->panel() on demand.
    m_chatPanel        = m_dockBootstrap->chatPanel();
    m_symbolPanel      = m_dockBootstrap->symbolPanel();
    m_referencesPanel  = m_dockBootstrap->referencesPanel();
    m_settingsPanel    = m_dockBootstrap->settingsPanel();

    // ── Wire signals that need MainWindow state ───────────────────────────

    // Manifesto #9: start network monitor only when user opens AI panel
    connect(dock(QStringLiteral("AIDock")), &exdock::ExDockWidget::stateChanged,
            this, [this](exdock::DockState state) {
        if (state != exdock::DockState::Closed && m_aiServices && m_aiServices->networkMonitor())
            m_aiServices->networkMonitor()->start();
    });

    // Symbol outline → navigate in current editor
    connect(m_symbolPanel, &SymbolOutlinePanel::symbolActivated,
            this, [this](int line, int col) {
        auto *editor = currentEditor();
        if (!editor) return;
        const QTextBlock block = editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cur(block);
            cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                             qMin(col, block.length() - 1));
            editor->setTextCursor(cur);
            editor->centerCursor();
        }
        editor->setFocus();
    });

    connect(m_referencesPanel, &ReferencesPanel::referenceActivated,
            this, &MainWindow::navigateToLocation);

    // Proposed edit → write file + update open editor
    connect(m_dockBootstrap->proposedEditPanel(), &ProposedEditPanel::editAccepted,
            this, [this](const QString &filePath, const QString &newText) {
        QFile f(filePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            f.write(newText.toUtf8());
            f.close();
        }
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *editor = m_editorMgr->editorAt(i);
            if (editor && editor->property("filePath").toString() == filePath) {
                editor->setPlainText(newText);
                break;
            }
        }
    });

    // ── Debug Panel — contributed by debug plugin (plugins/debug/) ───────
    // ── Theme Manager → dock stylesheet ──────────────────────────────────
    connect(m_workbenchServices->themeManager(), &ThemeManager::themeChanged,
            m_dockManager, &exdock::DockManager::applyDockStyleSheet);

    // ── Output / Build / Run panels — contributed by build plugin ──────────
    // Created by plugins/build/ via IViewContributor (OutputDock, RunDock).
    // Wiring happens in loadPlugins() after plugin initialization.

    // ── Test Explorer panel — contributed by testing plugin ───────────────
    // Created by plugins/testing/ via IViewContributor (TestExplorerDock).
    // TestRunnerService registered as "testRunner" service by the plugin.

    // ── Problems panel ────────────────────────────────────────────────────
    {
        auto *problemsPanel = new ProblemsPanel(this);
        if (m_hostServices)
            problemsPanel->setDiagnosticsService(m_hostServices->diagnostics());
        // OutputPanel wiring deferred to loadPlugins() (contributed by build plugin)
        m_services->registerService(QStringLiteral("problemsPanel"), problemsPanel);
        registerShellDock(QStringLiteral("ProblemsDock"), tr("Problems"),
                          problemsPanel, IDockManager::Bottom);

        connect(problemsPanel, &ProblemsPanel::navigateToFile,
                this, &MainWindow::navigateToLocation);
    }

    // ── Markdown Preview panel ────────────────────────────────────────────
    // Renders the active editor's .md/.markdown buffer as HTML on the right.
    {
        auto *mdPanel = new MarkdownPreviewPanel(this);
        m_services->registerService(QStringLiteral("markdownPreviewPanel"), mdPanel);
        registerShellDock(QStringLiteral("MarkdownPreviewDock"),
                          tr("Markdown Preview"),
                          mdPanel, IDockManager::Right);

        // Debounced refresh: any time the active document changes, reschedule
        // a render after 250 ms of quiet — avoids re-parsing on every keystroke.
        auto *debounce = new QTimer(this);
        debounce->setSingleShot(true);
        debounce->setInterval(250);

        auto refreshNow = [this, mdPanel]() {
            EditorView *editor = currentEditor();
            if (!editor) {
                mdPanel->setMarkdown(QString());
                return;
            }
            const QString path = editor->property("filePath").toString();
            if (!MarkdownPreviewPanel::isMarkdownPath(path)) {
                mdPanel->setMarkdown(QString());
                return;
            }
            mdPanel->setMarkdown(editor->toPlainText());
        };
        connect(debounce, &QTimer::timeout, this, refreshNow);

        // Wire whenever a new editor opens — track its document for changes,
        // and trigger an immediate refresh if it's the active tab.
        connect(m_editorMgr, &EditorManager::editorOpened, this,
                [this, debounce](EditorView *editor, const QString &path) {
            if (!editor) return;
            if (auto *doc = editor->document()) {
                connect(doc, &QTextDocument::contentsChange, this,
                        [debounce]() { debounce->start(); });
            }
            if (MarkdownPreviewPanel::isMarkdownPath(path))
                debounce->start(0);
        });

        // Also refresh when the user switches tabs.
        connect(m_editorMgr->tabs(), &QTabWidget::currentChanged, this,
                [debounce](int) { debounce->start(0); });
    }

    // Qt Help dock + F1 lookup are contributed by the qt-tools plugin via
    // IDockManager::addPanel("QtHelpDock", ...) in QtToolsPlugin::initializePlugin().

    // ── Diff Explorer + Merge Editor — contributed by git plugin ─────────
    // DiffExplorerDock and MergeEditorDock are registered by plugins/git/.
    // GitDock is also plugin-contributed; see createDockWidgets comment above.

    // ── File Watch Service ────────────────────────────────────────────────
    // Owned by WorkbenchServicesBootstrap. Wire the reload signal here so
    // the lambda can access editor tabs (MainWindow state).
    connect(m_workbenchServices->fileWatcher(), &FileWatchService::fileChangedExternally,
            this, [this](const QString &path) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *editor = m_editorMgr->editorAt(i);
            if (!editor) continue;
            if (editor->property("filePath").toString() == path) {
                QFile f(path);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString content = QString::fromUtf8(f.readAll());
                    if (content != editor->toPlainText()) {
                        auto answer = QMessageBox::question(
                            this, tr("File Changed"),
                            tr("'%1' has been modified externally.\nReload?")
                                .arg(QFileInfo(path).fileName()),
                            QMessageBox::Yes | QMessageBox::No);
                        if (answer == QMessageBox::Yes)
                            editor->setPlainText(content);
                    }
                }
                break;
            }
        }
    });

    // ── AI Services ────────────────────────────────────────────────────────
    m_aiServices = new AIServicesBootstrap(this);
    m_aiServices->initialize(statusBar());

    // Wire AI services signals to MainWindow
    connect(m_aiServices->oauthManager(), &OAuthManager::loginSucceeded, this,
            [this](const QString &) {
        statusBar()->showMessage(tr("Signed in successfully"), 5000);
    });
    connect(m_aiServices->oauthManager(), &OAuthManager::loginFailed, this,
            [this](const QString &err) {
        statusBar()->showMessage(tr("Sign-in failed: %1").arg(err), 5000);
    });
    connect(m_aiServices, &AIServicesBootstrap::indexingStarted, this,
            [this]() {
        m_statusBarMgr->indexLabel()->setText(tr("Indexing..."));
    });
    connect(m_aiServices, &AIServicesBootstrap::indexProgress, this,
            [this](int done, int total) {
        m_statusBarMgr->indexLabel()->setText(tr("Indexing %1/%2").arg(done).arg(total));
    });
    connect(m_aiServices, &AIServicesBootstrap::indexingFinished, this,
            [this](int count) {
        m_statusBarMgr->indexLabel()->setText(tr("%1 chunks indexed").arg(count));
    });
    // Network offline notification
    connect(m_aiServices->networkMonitor(), &NetworkMonitor::wentOffline, this, [this]() {
        NotificationToast::show(this, tr("Network connectivity issue detected"),
                                NotificationToast::Warning, 4000);
    });
    // Test generator → open file
    connect(m_aiServices->testGenerator(), &TestGenerator::testsGenerated, this,
            [this](const QString &path, const QString &content) {
        Q_UNUSED(content);
        openFile(path);
        statusBar()->showMessage(tr("Tests generated: %1").arg(path), 5000);
    });
    // Inline review → navigate to line
    connect(m_aiServices->inlineReview(), &InlineReviewWidget::navigateToLine, this, [this](int line) {
        auto *editor = currentEditor();
        if (editor) {
            QTextCursor cursor(editor->document()->findBlockByLineNumber(line - 1));
            editor->setTextCursor(cursor);
        }
    });
    // AuthManager → copilot status label
    connect(m_aiServices->authManager(), &AuthManager::statusChanged, this, [this]() {
        m_statusBarMgr->copilotStatusLabel()->setText(
            m_aiServices->authManager()->statusIcon() + QStringLiteral(" ") + m_aiServices->authManager()->statusText());
    });

    // ── LSP signal wiring — deferred to post-plugin wiring in loadPlugins() ──

    // When MCP discovers new tools, register them in the ToolRegistry.
    // McpPanel is lazy — connect when the dock is first opened.
    connect(dock(QStringLiteral("McpDock")), &exdock::ExDockWidget::stateChanged,
            this, [this](exdock::DockState state) {
        if (state == exdock::DockState::Closed) return;
        auto *mp = m_dockBootstrap->mcpPanel();
        // Connect only once (disconnect check)
        connect(mp, &McpPanel::toolsChanged, this, [this]() {
            auto *mc = m_dockBootstrap->mcpClient();
            const QList<McpToolInfo> tools = mc->allTools();
            for (const McpToolInfo &t : tools) {
                const QString regName = QStringLiteral("mcp_%1_%2").arg(t.serverName, t.name);
                if (!m_agentPlatform->toolRegistry()->hasTool(regName))
                    m_agentPlatform->toolRegistry()->registerTool(std::make_unique<McpToolAdapter>(mc, t));
            }
        });
    });

    // Wire settings to agent controller
    connect(m_settingsPanel, &SettingsPanel::settingsChanged, this, [this]() {
        if (auto *ctrl = m_agentPlatform->agentController())
            ctrl->setMaxStepsPerTurn(m_settingsPanel->maxSteps());
        if (auto *cb = m_agentPlatform->contextBuilder())
            cb->setMaxContextChars(m_settingsPanel->contextTokenLimit() * 4);
        // Update disabled languages for inline completions
        if (m_inlineEngine) {
            const QStringList langs = m_settingsPanel->disabledCompletionLanguages();
            m_inlineEngine->setDisabledLanguages(QSet<QString>(langs.begin(), langs.end()));
            m_inlineEngine->setCompletionModel(m_settingsPanel->completionModel());
        }
        // Configure BYOK provider (loaded as plugin)
        const QString endpoint = m_settingsPanel->customEndpoint();
        const QString apiKey   = m_settingsPanel->customApiKey();
        if (!endpoint.isEmpty() && !apiKey.isEmpty()) {
            for (IAgentProvider *p : m_agentOrchestrator->providers()) {
                if (p->id() == QLatin1String("custom")) {
                    QMetaObject::invokeMethod(p, "configure",
                        Q_ARG(QString, endpoint), Q_ARG(QString, apiKey));
                    break;
                }
            }
        }
        // Update disabled tools
        if (auto *tr = m_agentPlatform->toolRegistry()) {
            const QStringList disabled = m_settingsPanel->disabledTools();
            tr->setDisabledTools(QSet<QString>(disabled.begin(), disabled.end()));
        }
    });

}

