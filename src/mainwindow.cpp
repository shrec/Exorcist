#include "mainwindow.h"

#include "editor/codefoldingengine.h"
#include "startupprofiler.h"

#include <QAction>
#include <QDialog>
#include <QDirIterator>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QFileDialog>
#include <QStandardPaths>
#include <QKeySequence>
#include <QLabel>
#include <QStandardPaths>
#include <QTextCursor>
#include <QMenu>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QApplication>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QAbstractItemView>
#include <QLineEdit>
#include <QStatusBar>
#include <QTextEdit>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QUuid>
#include <QInputDialog>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QMessageBox>
#include <QScrollBar>
#include <QTextDocument>
#include <QTimer>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QRegularExpression>

#include <atomic>
#include <memory>

#ifdef Q_OS_UNIX
#  include <unistd.h>   // sysconf, _SC_PAGESIZE
#endif

#include "commandpalette.h"
#include "sdk/hostservices.h"
#include "sdk/contributionregistry.h"
#include "editor/editorview.h"
#include "editor/largefileloader.h"
#include "editor/syntaxhighlighter.h"
#include "editor/highlighterfactory.h"
#include "agent/inlinecompletionengine.h"
#include "agent/nexteditengine.h"
#include "agent/agentorchestrator.h"
#include "agent/agentproviderregistry.h"
#include "agent/agentrequestrouter.h"
#include "agent/memorysuggestionengine.h"
#include "agent/treesitteragenthelper.h"
#include "agent/diagnosticsnotifier.h"
#include "sdk/idebugadapter.h"
#include "sdk/idebugservice.h"
#include "agent/chat/chatpanelwidget.h"
#include "agent/ui/agentdashboardpanel.h"
#include "agent/ui/agentuibus.h"
#include "agent/agentcontroller.h"
#include "agent/agentmodes.h"
#include "agent/agentplatformbootstrap.h"
#include "agent/projectbrainservice.h"
#include "agent/contextbuilder.h"
#include "agent/quickchatdialog.h"
#include "agent/inlinechat/inlinechatwidget.h"
#include "agent/promptvariables.h"
#include "agent/promptfileloader.h"
#include "agent/sessionstore.h"
#include "agent/itool.h"
#include "agent/tools/filesystemtools.h"
#include "agent/tools/searchworkspacetool.h"
#include "agent/tools/semanticsearchtool.h"
#include "agent/modelpickerdialog.h"
#include "agent/tools/runcommandtool.h"
#include "agent/tools/applypatchtool.h"
#include "agent/tools/idetools.h"
#include "agent/tools/moretools.h"
#include "agent/tools/advancedtools.h"
#include "agent/tools/memorytool.h"
#include "agent/requestlogpanel.h"
#include "agent/trajectorypanel.h"
#include "sdk/luajit/luascriptengine.h"

#include <QDockWidget>
#include <QGroupBox>
#include <QPlainTextEdit>

#include "mcp/mcpclient.h"
#include "mcp/mcppanel.h"
#include "mcp/mcptooladapter.h"
#include "plugin/plugingallerypanel.h"
#include "plugin/pluginmarketplaceservice.h"
#include "bootstrap/workbenchservicesbootstrap.h"
#include "thememanager.h"
#include "editor/filewatchservice.h"
#include "editor/diffviewerpanel.h"
#include "editor/proposededitpanel.h"
#include "editor/breadcrumbbar.h"
#include "agent/settingspanel.h"
#include "agent/memorybrowserpanel.h"
#include "agent/iagentplugin.h"
#include "ui/notificationtoast.h"
#include "ui/settingsdialog.h"
#include "ui/extrasettingspages.h"
#include "ui/aboutdialog.h"
#include "ui/recentfilesmanager.h"
#include "ui/themegallerypanel.h"
#include "ui/keymapdialog.h"
#include "ui/workspacesymbolsdialog.h"
#include "ui/gotolinedialog.h"
// kitmanagerdialog moved to plugins/qt-tools/ — accessed via "qt.manageKits" command
#include "ui/quickopendialog.h"
#include "ui/dock/DockManager.h"
#include "ui/dock/ExDockWidget.h"
#include "pluginmanager.h"
#include "serviceregistry.h"
#include "profile/profilemanager.h"
#include "core/qtfilesystem.h"
#include "core/qtprocess.h"
#include "sdk/isearchservice.h"
#include "search/symbolindex.h"
#include "search/workspacechunkindex.h"
#include "lsp/lspclient.h"
#include "lsp/lspeditorbridge.h"
#include "lsp/referencespanel.h"
#include "sdk/ilspservice.h"
#include "sdk/ibuildsystem.h"
#include "sdk/ilaunchservice.h"
#include "build/kit.h"
#include "bootstrap/bridgebootstrap.h"
#include "bootstrap/sharedservicesbootstrap.h"
#include "bootstrap/templateservicesbootstrap.h"
#include "process/bridgeclient.h"
#include "bootstrap/statusbarmanager.h"
#include "bootstrap/aiservicesbootstrap.h"
#include "bootstrap/dockbootstrap.h"
#include "bootstrap/workbenchcommandbootstrap.h"
#include "bootstrap/workbenchstatebootstrap.h"
#include "bootstrap/postpluginbootstrap.h"
#include "bootstrap/autosavemanager.h"
#include "bootstrap/globalshortcutdispatcher.h"
#include "editor/symboloutlinepanel.h"
#include "sdk/iterminalservice.h"
#include "project/projectmanager.h"
#include "project/projecttemplateregistry.h"
#include "project/newprojectwizard.h"
#include "project/newpluginwizard.h"
// newqtclasswizard moved to plugins/qt-tools/ — accessed via "qt.newClass" command
#include "project/filetemplatedialog.h"
#include "project/solutiontreemodel.h"
#include "git/gitservice.h"
#include "build/outputpanel.h"
#include "problems/problemspanel.h"
#include "settings/workspacesettings.h"
#include "settings/scopedsettings.h"
#include "settings/languageprofile.h"
#include "git/diffexplorerpanel.h"
#include "git/mergeeditor.h"
#include "build/runlaunchpanel.h"
#include "agent/authmanager.h"
#include "agent/securekeystorage.h"
#include "agent/tools/websearchtool.h"
#include "agent/promptfilemanager.h"
#include "editor/inlinereviewwidget.h"
#include "agent/oauthmanager.h"
#include "git/commitmessagegenerator.h"
#include "agent/testgenerator.h"
#include "agent/networkmonitor.h"
#include "agent/citationmanager.h"
#include "agent/modelconfigwidget.h"
#include "agent/contextinspector.h"
#include "agent/toolcalltrace.h"
#include "agent/trajectoryreplaywidget.h"
#include "agent/promptarchiveexporter.h"
#include "agent/memoryfileeditor.h"
#include "ui/welcomewidget.h"
#include "ui/keymapmanager.h"
#include "ui/markdownpreviewpanel.h"
// qthelpdock moved to plugins/qt-tools/ — registered as dock by qt-tools plugin
#include <QShortcut>

#include <QStackedWidget>
#include "agent/backgroundcompactor.h"
#include "agent/errorstatewidget.h"
#include "agent/setuptestswizard.h"
#include "agent/renameintegration.h"
#include "agent/accessibilityhelper.h"
#include "agent/tooltogglemanager.h"
#include "agent/contextscopeconfig.h"
#include "agent/tools/notebooktools.h"

#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMouseEvent>
#include <QProcess>
#include <QSet>
#include <QTabBar>
#include <QUrl>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_editorMgr(nullptr),
      m_gitService(nullptr),
      m_agentOrchestrator(nullptr),
      m_chatPanel(nullptr),
    m_promptResolver(nullptr),
      m_referencesPanel(nullptr),
      m_symbolPanel(nullptr),
      m_inlineEngine(nullptr)
{
    setWindowTitle(tr("Exorcist"));

    // EditorManager owns tab/project/tree state
    m_editorMgr = new EditorManager(this);

    // Periodic auto-save of dirty editors (configurable via QSettings).
    auto *autoSave = new AutoSaveManager(m_editorMgr, this);
    Q_UNUSED(autoSave);

    // Services that dock widgets depend on must exist before setupUi().
    m_agentOrchestrator = new AgentOrchestrator(this);
    m_editorMgr->setProjectManager(new ProjectManager(this));
    m_gitService        = new GitService(this);

    // ServiceRegistry must exist before setupUi (createDockWidgets uses it).
    m_services = std::make_unique<ServiceRegistry>();

    // Workbench services are used while setupUi() builds docks.
    m_workbenchServices = new WorkbenchServicesBootstrap(this);
    m_workbenchServices->initialize(m_services.get());

    // LSP — contributed by cpp-language plugin (plugins/cpp-language/).
    // LspClient registered as "lspClient", ILspService as "lspService".

    // Debug subsystem — contributed by debug plugin (plugins/debug/).
    // IDebugAdapter registered as "debugAdapter", IDebugService as "debugService".

    setupToolBar();
    setupUi();
    setupMenus();

    m_statusBarMgr = new StatusBarManager(statusBar(), this);
    m_statusBarMgr->initialize();
    connect(m_statusBarMgr, &StatusBarManager::copilotStatusClicked,
            this, [this]() { m_chatPanel->focusInput(); });

    StartupProfiler::instance().mark(QStringLiteral("UI setup"));

    // ── Keymap actions registration ────────────────────────────────────────
    {
        auto *km = m_workbenchServices->keymapManager();
        // Auto-register all actions that have shortcuts
        for (QAction *action : actions()) {
            if (!action->shortcut().isEmpty()) {
                QString id = action->text().remove(QLatin1Char('&')).trimmed();
                if (!id.isEmpty())
                    km->registerAction(id, action, action->shortcut());
            }
        }
        // Also register menu actions
        for (QAction *menuAction : menuBar()->actions()) {
            if (auto *menu = menuAction->menu()) {
                for (QAction *action : menu->actions()) {
                    if (!action->shortcut().isEmpty()) {
                        QString id = action->text().remove(QLatin1Char('&')).trimmed();
                        if (!id.isEmpty() && !km->actionIds().contains(id))
                            km->registerAction(id, action, action->shortcut());
                    }
                }
            }
        }
        km->load();  // Apply saved overrides
    }

    m_services->registerService("mainwindow", this);
    m_services->registerService(QStringLiteral("editorManager"), m_editorMgr);
    m_services->registerService("agentOrchestrator", m_agentOrchestrator);
    m_services->registerService(QStringLiteral("gitService"), m_gitService);
    if (m_aiServices && m_aiServices->keyStorage())
        m_services->registerService(QStringLiteral("secureKeyStorage"), m_aiServices->keyStorage());

    // ── Build System — registered by build plugin (plugins/build/) ──────
    // Debug adapter — registered by debug plugin (plugins/debug/)

    m_fileSystem = std::make_unique<QtFileSystem>();
    m_editorMgr->setFileSystem(m_fileSystem.get());

    // ── ExoBridge bootstrap ─────────────────────────────────────────────
    {
        auto *bridgeBoot = new BridgeBootstrap(this);
        bridgeBoot->initialize(m_services.get());
        connect(bridgeBoot, &BridgeBootstrap::bridgeCrashed, this, [this]() {
            NotificationToast::show(
                this,
                tr("ExoBridge daemon crashed — restarting automatically"),
                NotificationToast::Warning, 5000);
        });
    }

    // Wire GitService → ExoBridge for shared git watching
    if (auto *bc = m_services->service<BridgeClient>(QStringLiteral("bridgeClient")))
        m_gitService->setBridgeClient(bc);

    // ── Agent core bootstrap ──────────────────────────────────────────────
    m_agentPlatform = new AgentPlatformBootstrap(
        m_agentOrchestrator, m_services.get(), m_fileSystem.get(), this);

    auto agentCallbacks = buildAgentCallbacks();


    m_agentPlatform->initialize(agentCallbacks);
    m_agentPlatform->registerCoreTools(m_editorMgr->currentFolder());

    // ── Wire LSP diagnostics push to agent (deferred to post-plugin wiring) ──

    // Deferred from createDockWidgets — m_agentPlatform was null there.
    // memoryBrowser() is lazy — calling it here would create it early.
    // Instead, defer setBrainService to when the panel is first opened.
    if (m_dockBootstrap) {
        auto *db = m_dockBootstrap;
        auto *brain = m_agentPlatform->brainService();
        connect(dock(QStringLiteral("MemoryDock")), &exdock::ExDockWidget::stateChanged,
                this, [db, brain](exdock::DockState state) {
            if (state != exdock::DockState::Closed)
                db->memoryBrowser()->setBrainService(brain);
        });
    }

    // Wire the orchestrator facade to delegate to the new platform services
    m_agentOrchestrator->setRegistry(m_agentPlatform->providerRegistry());
    m_agentOrchestrator->setRouter(m_agentPlatform->requestRouter());

    // Wire AI status indicator
    auto *ac = m_agentPlatform->agentController();
    connect(ac, &AgentController::turnStarted,
            this, [this]() {
        m_statusBarMgr->copilotStatusLabel()->setText(tr("\u25CF Working..."));
        m_statusBarMgr->copilotStatusLabel()->setStyleSheet(
            QStringLiteral("padding: 0 8px; color:#dcdcaa;"));
    });
    connect(ac, &AgentController::turnFinished,
            this, [this]() {
        m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2713 AI Ready"));
        m_statusBarMgr->copilotStatusLabel()->setStyleSheet(
            QStringLiteral("padding: 0 8px; color:#89d185;"));
    });
    connect(ac, &AgentController::turnError,
            this, [this]() {
        m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2717 AI Error"));
        m_statusBarMgr->copilotStatusLabel()->setStyleSheet(
            QStringLiteral("padding: 0 8px; color:#f14c4c;"));
    });

    // Wire request log panel
    connect(ac, &AgentController::turnStarted,
            this, [this](const QString &msg) {
        m_dockBootstrap->requestLogPanel()->logRequest(QStringLiteral("turn"), QStringLiteral("-"),
                                       1, 0);
    });
    connect(ac, &AgentController::turnFinished,
            this, [this](const AgentTurn &turn) {
        const QString text = turn.steps.isEmpty() ? QString() : turn.steps.last().finalText;
        m_dockBootstrap->requestLogPanel()->logResponse(QStringLiteral("turn"), text.left(500));
    });
    connect(ac, &AgentController::turnError,
            this, [this](const QString &err) {
        m_dockBootstrap->requestLogPanel()->logError(QStringLiteral("turn"), err);
        NotificationToast::show(this, tr("AI Error: %1").arg(err.left(100)),
                                NotificationToast::Error, 5000);
    });
    connect(ac, &AgentController::toolCallStarted,
            this, [this](const QString &name, const QJsonObject &args) {
        m_dockBootstrap->requestLogPanel()->logToolCall(name, args, true, QStringLiteral("started"));
    });
    connect(ac, &AgentController::toolCallFinished,
            this, [this](const QString &name, const ToolExecResult &result) {
        m_dockBootstrap->requestLogPanel()->logToolCall(name, {}, result.ok,
            result.ok ? result.textContent.left(200) : result.error);
    });

    // Wire trajectory panel
    connect(ac, &AgentController::turnStarted,
            this, [this](const QString &) {
        m_dockBootstrap->trajectoryPanel()->clear();
    });
    connect(ac, &AgentController::turnFinished,
            this, [this](const AgentTurn &turn) {
        m_dockBootstrap->trajectoryPanel()->setTurn(turn);
    });

    // Wire memory suggestion engine — suggest facts after each turn
    if (auto *engine = m_agentPlatform->memorySuggestionEngine()) {
        connect(ac, &AgentController::turnFinished,
                this, [this, engine](const AgentTurn &turn) {
            // Collect touched files from tool call arguments
            QStringList touchedFiles;
            for (const auto &step : turn.steps) {
                if (step.type == AgentStep::Type::ToolCall) {
                    const auto args = QJsonDocument::fromJson(
                        step.toolCall.arguments.toUtf8()).object();
                    const QString fp = args.value(QStringLiteral("filePath")).toString();
                    if (!fp.isEmpty() && !touchedFiles.contains(fp))
                        touchedFiles.append(fp);
                }
            }
            // Gather response text
            QString response;
            for (const auto &step : turn.steps) {
                if (!step.finalText.isEmpty())
                    response += step.finalText;
            }
            engine->suggestFacts(turn.userMessage, response, touchedFiles);
        });
        connect(engine, &MemorySuggestionEngine::factSuggested,
                this, [this](const MemoryFact &fact) {
            NotificationToast::showWithAction(
                this,
                tr("Memory suggestion: %1").arg(fact.value.left(80)),
                tr("Save"),
                [this, fact]() {
                    if (auto *brain = m_agentPlatform->brainService()) {
                        MemoryFact accepted = fact;
                        accepted.confidence = 1.0;
                        brain->addFact(accepted);
                    }
                },
                NotificationToast::Info, 10000);
        });
    }
    connect(ac, &AgentController::toolCallStarted,
            this, [this](const QString &name, const QJsonObject &args) {
        AgentStep step;
        step.type = AgentStep::Type::ToolCall;
        step.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        step.toolCall.name = name;
        step.toolCall.arguments = QString::fromUtf8(
            QJsonDocument(args).toJson(QJsonDocument::Compact));
        step.toolResult = tr("running...");
        m_dockBootstrap->trajectoryPanel()->appendStep(step);
    });
    // Wire chat panel now that the controller exists
    if (m_chatPanel) {
        m_chatPanel->setAgentController(ac);
        m_chatPanel->setSessionStore(m_agentPlatform->sessionStore());
        m_chatPanel->setChatSessionService(m_agentPlatform->chatSessionService());
        m_chatPanel->setContextBuilder(m_agentPlatform->contextBuilder());
        if (m_aiServices && m_aiServices->modelRegistry())
            m_chatPanel->setModelRegistry(m_aiServices->modelRegistry());
        if (m_agentPlatform->toolRegistry())
            m_chatPanel->setToolCount(m_agentPlatform->toolRegistry()->toolNames().size());

        // Wire review annotations → apply to active editor
        connect(m_chatPanel, &ChatPanelWidget::reviewAnnotationsReady,
                this, [this](const QString &filePath,
                             const QList<QPair<int, QString>> &annotations) {
            // Find open editor tab for the file
            for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                auto *ed = m_editorMgr->editorAt(i);
                if (!ed) continue;
                if (ed->property("filePath").toString() == filePath) {
                    QList<EditorView::ReviewAnnotation> anns;
                    for (const auto &[line, comment] : annotations)
                        anns.append({line, comment});
                    ed->setReviewAnnotations(anns);
                    m_editorMgr->tabs()->setCurrentIndex(i);
                    break;
                }
            }
        });

        // Wire test scaffold → open file
        connect(m_chatPanel, &ChatPanelWidget::openFileRequested,
                this, [this](const QString &filePath) {
            openFile(filePath);
        });

        // Wire gear icon → settings dialog
        connect(m_chatPanel, &ChatPanelWidget::settingsRequested,
                this, [this]() {
            if (!m_settingsDialog) {
                m_settingsDialog = new QDialog(this);
                m_settingsDialog->setWindowTitle(tr("AI Settings"));
                m_settingsDialog->setMinimumSize(420, 480);
                auto *lay = new QVBoxLayout(m_settingsDialog);
                lay->setContentsMargins(0, 0, 0, 0);
                m_settingsPanel->setParent(m_settingsDialog);
                lay->addWidget(m_settingsPanel);
                m_settingsPanel->show();
            }
            m_settingsDialog->show();
            m_settingsDialog->raise();
            m_settingsDialog->activateWindow();
        });

        // Auto-restore last session on startup
        if (auto *ss = m_agentPlatform->sessionStore()) {
            const auto last = ss->loadLastSession();
            if (!last.isEmpty())
                m_chatPanel->restoreSession(
                    last.sessionId, last.completeTurns,
                    last.mode, last.title,
                    last.modelId, last.providerId);
        }
    }

    // Wire agent dashboard panel to UI bus
    if (m_agentPlatform && m_agentPlatform->uiBus() && m_services) {
        auto *dashPanel = qobject_cast<AgentDashboardPanel *>(
            m_services->service(QStringLiteral("agentDashboardPanel")));
        if (dashPanel) {
            m_agentPlatform->uiBus()->addRenderer(dashPanel);
            connect(dashPanel, &AgentDashboardPanel::openFileRequested,
                    this, [this](const QString &path) { openFile(path); });
        }
    }

    // Inline completion engine — wired after plugins load
    m_inlineEngine = new InlineCompletionEngine(this);

    // Next Edit Suggestion engine — predicts next edit after ghost text acceptance
    m_nesEngine = new NextEditEngine(this);
    connect(m_nesEngine, &NextEditEngine::suggestionReady, this, [this](int line) {
        auto *ed = currentEditor();
        if (ed)
            ed->setNesLine(line);
    });
    connect(m_nesEngine, &NextEditEngine::suggestionDismissed, this, [this] {
        auto *ed = currentEditor();
        if (ed)
            ed->clearNesLine();
    });
    connect(m_nesEngine, &NextEditEngine::navigateToFile, this,
            [this](const QString &path, int line, int) {
        openFile(path);
        // After opening, jump to line
        auto *ed = currentEditor();
        if (ed) {
            QTextBlock block = ed->document()->findBlockByNumber(line);
            if (block.isValid()) {
                QTextCursor cur(block);
                ed->setTextCursor(cur);
                ed->centerCursor();
            }
        }
    });

    // Prompt variable resolver used by chat panel before send.
    m_promptResolver = new PromptVariableResolver(this);
    m_promptResolver->registerVariable({
        QStringLiteral("file"),
        tr("Active file context"),
        [this]() -> QString {
            auto *ed = currentEditor();
            if (!ed) return tr("No active file.");
            const QString path = ed->property("filePath").toString();
            if (path.isEmpty()) return tr("No active file.");
            return tr("Active file: %1\n\n%2")
                .arg(path, ed->toPlainText().left(8000));
        }
    });
    m_promptResolver->registerVariable({
        QStringLiteral("selection"),
        tr("Selected editor text"),
        [this]() -> QString {
            auto *ed = currentEditor();
            if (!ed) return tr("No active editor selection.");
            const QString sel = ed->textCursor().selectedText();
            return sel.trimmed().isEmpty() ? tr("No selected text.") : sel;
        }
    });
    m_promptResolver->registerVariable({
        QStringLiteral("problems"),
        tr("Current diagnostics"),
        [this]() -> QString {
            auto *ed = currentEditor();
            if (!ed) return tr("No diagnostics available.");
            const auto marks = ed->diagnosticMarks();
            if (marks.isEmpty()) return tr("No diagnostics available.");
            QStringList lines;
            for (const auto &d : marks) {
                QString sev = tr("info");
                if (d.severity == DiagSeverity::Error) sev = tr("error");
                else if (d.severity == DiagSeverity::Warning) sev = tr("warning");
                lines.append(QStringLiteral("%1:%2 %3: %4")
                                .arg(d.line + 1)
                                .arg(d.startChar + 1)
                                .arg(sev, d.message));
            }
            return lines.join(QLatin1Char('\n'));
        }
    });
    m_promptResolver->registerVariable({
        QStringLiteral("terminal"),
        tr("Recent terminal output"),
        [this]() -> QString {
            auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService")));
            if (!ts) return tr("No terminal output.");
            const QString out = ts->recentOutput(120);
            return out.trimmed().isEmpty() ? tr("No terminal output.") : out;
        }
    });

    if (m_chatPanel) {
        m_chatPanel->setVariableResolver(m_promptResolver);
        m_chatPanel->setInsertAtCursorCallback([this](const QString &code) {
            if (auto *ed = currentEditor()) {
                QTextCursor c = ed->textCursor();
                if (c.hasSelection())
                    c.insertText(code);
                else
                    c.insertText(code);
                ed->setFocus();
            }
        });
        m_chatPanel->setRunInTerminalCallback([this](const QString &code) {
            auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService")));
            if (!ts) return;
            m_dockManager->showDock(dock(QStringLiteral("TerminalDock")), exdock::SideBarArea::Bottom);
            ts->sendInput(code);
            ts->sendInput(QStringLiteral("\r\n"));
        });
        m_chatPanel->setWorkspaceFileProvider([this]() -> QStringList {
            QStringList result;
            if (m_editorMgr->currentFolder().isEmpty()) return result;
            QDirIterator it(m_editorMgr->currentFolder(),
                            QDir::Files | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            const QDir root(m_editorMgr->currentFolder());
            while (it.hasNext()) {
                it.next();
                const QString rel = root.relativeFilePath(it.filePath());
                if (rel.startsWith('.') || rel.contains(QStringLiteral("/."))
                    || rel.contains(QStringLiteral("\\.")))
                    continue;
                if (rel.startsWith(QStringLiteral("build"))) continue;
                result.append(rel);
                if (result.size() >= 500) break;
            }
            return result;
        });
    }

    connect(m_gitService, &GitService::statusRefreshed,
            m_editorMgr->treeModel(), &SolutionTreeModel::refreshGitOverlays);
    connect(m_gitService, &GitService::statusRefreshed, this, [this] {
        QTimer::singleShot(0, this, [this] { updateDiffRanges(currentEditor()); });
    });
    connect(m_gitService, &GitService::branchChanged, this, [this](const QString &branch) {
        m_statusBarMgr->branchLabel()->setText(branch.isEmpty() ? QString() : QString(" ") + branch);
    });

    // Restore window geometry immediately (cheap, needed before show).
    {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        const QByteArray geometry = s.value(QStringLiteral("geometry")).toByteArray();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
    }

    statusBar()->showMessage(tr("Ready"), 3000);
}

void MainWindow::deferredInit()
{
    loadPlugins();
    StartupProfiler::instance().mark(QStringLiteral("plugins loaded"));

    // On startup we always land on the welcome page (centralStack index 0).
    // Restoring the saved dock layout here only to close all docks a few lines
    // later is a no-op at best and a crash on Windows at worst: restoreState
    // calls pinDock which creates native HWNDs, the force-close below destroys
    // them via setParent(mainWindow), and the next pinDock (when a project is
    // opened) triggers a second HWND reconstruct that corrupts Qt's internal
    // widget state (RBP=1 access violation in Qt6Widgets.dll).
    //
    // Solution: only restore dock state when NOT on the welcome page.  In
    // practice deferredInit always runs on the welcome page, so restoreState
    // is normally skipped here.  Dock positions for a reopened project are
    // applied separately when the project loads.
    const bool onWelcomePage = (m_centralStack && m_centralStack->currentIndex() == 0);

    {
        constexpr int kLayoutVersion = 4;
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        const int savedVersion = s.value(QStringLiteral("dockLayoutVersion"), 0).toInt();
        if (!onWelcomePage && savedVersion >= kLayoutVersion) {
            const QString dockJson = s.value(QStringLiteral("dockState")).toString();
            if (!dockJson.isEmpty()) {
                const QJsonObject obj = QJsonDocument::fromJson(dockJson.toUtf8()).object();
                if (!obj.isEmpty())
                    m_dockManager->restoreState(obj);
            }
        } else if (savedVersion < kLayoutVersion) {
            // Outdated or missing layout — save new version
            s.setValue(QStringLiteral("dockLayoutVersion"), kLayoutVersion);
        }
    }

    // All docks were already closed by setupDocks().  On the welcome page we
    // never called restoreState so nothing was pinned; this loop is a safe
    // no-op that also guards against any future code that might open a dock
    // before deferredInit runs.
    if (onWelcomePage) {
        for (auto *dw : m_dockManager->dockWidgets())
            m_dockManager->closeDock(dw);
    }

    // Refresh the welcome page's recent list after plugins have loaded.
    if (m_welcome)
        m_welcome->refreshRecent();
    StartupProfiler::instance().mark(QStringLiteral("deferred init done"));
}

// ── UI setup ────────────────────────────────────────────────────────────────

void MainWindow::setupUi()
{
    m_editorMgr->setTabs(new QTabWidget(this));
    m_editorMgr->tabs()->setDocumentMode(true);
    m_editorMgr->tabs()->setTabsClosable(true);
    m_editorMgr->tabs()->setMovable(true);
    m_editorMgr->tabs()->setStyleSheet(QStringLiteral(
        // Tab bar — VS2022 dark document tab style
        "QTabBar { background: #2d2d30; }"
        "QTabBar::tab {"
        "  background: #2d2d30; color: #9d9d9d;"
        "  padding: 5px 10px 5px 12px;"
        "  border: none; border-right: 1px solid #1e1e1e;"
        "  min-width: 80px; max-width: 220px; font-size: 12px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: #1e1e1e; color: #ffffff;"
        "  border-top: 1px solid #007acc;"
        "}"
        "QTabBar::tab:hover:!selected { background: #3e3e42; color: #cccccc; }"
        "QTabBar::tab:!selected { margin-top: 1px; }"
        // Close button
        "QTabBar::close-button { subcontrol-position: right; margin-right: 2px; }"
        // Pane — just a seamless dark bg
        "QTabWidget::pane { background: #1e1e1e; border: none; }"
    ));

    // Middle-click closes tab
    m_editorMgr->tabs()->tabBar()->installEventFilter(this);

    m_editorMgr->setBreadcrumb(new BreadcrumbBar(this));

    connect(m_editorMgr->breadcrumb(), &BreadcrumbBar::symbolActivated,
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

    // Tab close button → EditorManager; Lua event via tabClosed signal
    connect(m_editorMgr->tabs(), &QTabWidget::tabCloseRequested,
            m_editorMgr, &EditorManager::closeTab);
    connect(m_editorMgr, &EditorManager::tabClosed, this, [this](const QString &path) {
        if (!path.isEmpty() && m_pluginManager)
            m_pluginManager->fireLuaEvent(QStringLiteral("editor.close"), {path});
    });

    // Status messages from EditorManager → status bar
    connect(m_editorMgr, &EditorManager::statusMessage, this,
            [this](const QString &msg, int ms) { statusBar()->showMessage(msg, ms); });

    // Per-editor wiring: called once each time a new file tab is created
    connect(m_editorMgr, &EditorManager::editorOpened, this,
            [this](EditorView *editor, const QString &path) {
        // Recent files: tracked by RecentFilesManager (dedupe + persist via
        // QSettings under "recentFiles"). After the manager saves, enforce a
        // 10-entry cap on the persisted list (manager's internal cap is 15).
        if (auto *rfm = findChild<RecentFilesManager *>(
                QStringLiteral("recentFilesManager"))) {
            rfm->addFile(path);
            QSettings rs(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
            QStringList recent = rs.value(QStringLiteral("recentFiles")).toStringList();
            constexpr int kRecentFilesCap = 10;
            if (recent.size() > kRecentFilesCap) {
                while (recent.size() > kRecentFilesCap)
                    recent.removeLast();
                rs.setValue(QStringLiteral("recentFiles"), recent);
            }
        }

        // Language-profile: activate language-specific plugin if profile is active
        if (m_pluginManager) {
            const QString langId = editor->languageId();
            if (auto *lpm = findChild<LanguageProfileManager *>(
                    QStringLiteral("languageProfileManager"))) {
                if (lpm->isActive(langId))
                    m_pluginManager->activateByLanguageProfile(langId);
            }
        }

        // Ctrl+I inline chat
        connect(editor, &EditorView::inlineChatRequested,
                this, [this, editor, path](const QString &sel, const QString &lang) {
            showInlineChat(editor, sel, path, lang);
        });

        // AI context menu — five actions wired to the chat panel
        auto wireAiAction = [this](const QString &sel, const QString &fp,
                                   const QString &lang, const QString &cmd) {
            m_chatPanel->setEditorContext(fp, sel, lang);
            m_chatPanel->focusInput();
            const QString prompt = sel.isEmpty()
                ? cmd : QStringLiteral("%1 %2").arg(cmd, sel.left(200));
            m_chatPanel->setInputText(prompt);
        };
        connect(editor, &EditorView::aiExplainRequested, this,
                [wireAiAction](const QString &s, const QString &f, const QString &l) {
            wireAiAction(s, f, l, QStringLiteral("/explain")); });
        connect(editor, &EditorView::aiReviewRequested, this,
                [wireAiAction](const QString &s, const QString &f, const QString &l) {
            wireAiAction(s, f, l, QStringLiteral("/review")); });
        connect(editor, &EditorView::aiFixRequested, this,
                [wireAiAction](const QString &s, const QString &f, const QString &l) {
            wireAiAction(s, f, l, QStringLiteral("/fix")); });
        connect(editor, &EditorView::aiTestRequested, this,
                [wireAiAction](const QString &s, const QString &f, const QString &l) {
            wireAiAction(s, f, l, QStringLiteral("/test")); });
        connect(editor, &EditorView::aiDocRequested, this,
                [wireAiAction](const QString &s, const QString &f, const QString &l) {
            wireAiAction(s, f, l, QStringLiteral("/doc")); });

        // Alt+\ manual completion trigger
        connect(editor, &EditorView::manualCompletionRequested, this, [this]() {
            if (m_inlineEngine) m_inlineEngine->triggerCompletion();
        });

        // Breakpoint gutter → debug service
        connect(editor, &EditorView::breakpointToggled, this,
                [this](const QString &fp, int ln, bool added) {
            auto *debugSvc = m_services->service<IDebugService>(
                QStringLiteral("debugService"));
            auto *adapter = m_services->service<IDebugAdapter>(
                QStringLiteral("debugAdapter"));
            if (added) {
                if (debugSvc) debugSvc->addBreakpointEntry(fp, ln);
                // Always send to adapter — if not yet launched, it queues for next run
                if (adapter) {
                    DebugBreakpoint bp;
                    bp.filePath = fp;
                    bp.line = ln;
                    adapter->addBreakpoint(bp);
                }
            } else {
                if (debugSvc) {
                    // Remove from adapter using the confirmed breakpoint ID
                    const int bpId = debugSvc->breakpointIdForLocation(fp, ln);
                    if (adapter && adapter->isRunning() && bpId >= 0)
                        adapter->removeBreakpoint(bpId);
                    debugSvc->removeBreakpointEntry(fp, ln);
                }
            }
        });

        // LSP bridge — sets up completions, hover, go-to-def, symbols, etc.
        createLspBridge(editor, path);

        // File watcher
        if (m_workbenchServices && m_workbenchServices->fileWatcher() && !path.isEmpty())
            m_workbenchServices->fileWatcher()->watchFile(path);

        // Lua event
        if (m_pluginManager)
            m_pluginManager->fireLuaEvent(QStringLiteral("editor.open"), {path});
    });

    connect(m_editorMgr->tabs(), &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    createDockWidgets();
    // No openNewTab() here — welcome page shows first until a project is opened.
}

void MainWindow::setupMenus()
{
    // ── File ──────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *newTabAction = fileMenu->addAction(tr("New &Tab"));
    newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));

    QAction *newFromTemplateAction = fileMenu->addAction(tr("New From &Template..."));
    newFromTemplateAction->setShortcut(QKeySequence::New);

    QAction *newPluginAction = fileMenu->addAction(tr("New &Plugin..."));
    // "New Qt Class" action is contributed by the qt-tools plugin via
    // IMenuManager::File menu location (command: qt.newClass).

    QAction *openAction = fileMenu->addAction(tr("&Open File..."));
    openAction->setShortcut(QKeySequence::Open);

    QAction *openFolderAction = fileMenu->addAction(tr("Open &Folder..."));
    openFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    QAction *quickOpenAction = fileMenu->addAction(tr("Quick &Open File..."));
    quickOpenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(quickOpenAction, &QAction::triggered, this, [this]() {
        const QString root = m_editorMgr ? m_editorMgr->currentFolder() : QString();
        if (root.isEmpty()) return;
        QuickOpenDialog dlg(root, this);
        connect(&dlg, &QuickOpenDialog::fileSelected,
                this, &MainWindow::openFile);
        dlg.exec();
    });

    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);

    QAction *saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();

    auto *recentMgr = new RecentFilesManager(this);
    recentMgr->setObjectName(QStringLiteral("recentFilesManager"));

    auto *langProfileMgr = new LanguageProfileManager(this);
    langProfileMgr->setObjectName(QStringLiteral("languageProfileManager"));
    m_editorMgr->setLanguageProfileManager(langProfileMgr);
    QMenu *recentMenu = fileMenu->addMenu(tr("Recent &Files"));
    recentMgr->attachMenu(recentMenu);
    connect(recentMgr, &RecentFilesManager::fileSelected,
            this, [this](const QString &path) { openFile(path); });

    fileMenu->addSeparator();

    QMenu *solutionMenu = fileMenu->addMenu(tr("Solution"));
    QAction *newSolutionAction = solutionMenu->addAction(tr("New Solution..."));
    QAction *openSolutionAction = solutionMenu->addAction(tr("Open Solution (.exsln)..."));
    solutionMenu->addSeparator();
    QAction *addProjectAction = solutionMenu->addAction(tr("Add Folder to Solution..."));
    QAction *saveSolutionAction = solutionMenu->addAction(tr("Save Solution"));
    solutionMenu->addSeparator();
    QAction *closeSolutionAction = solutionMenu->addAction(tr("Close Solution"));

    QAction *closeFolderAction = fileMenu->addAction(tr("Close Folder"));

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);

    connect(newTabAction, &QAction::triggered, this, &MainWindow::openNewTab);
    connect(newFromTemplateAction, &QAction::triggered, this, [this]() {
        QString defaultFolder;
        if (m_editorMgr)
            defaultFolder = m_editorMgr->currentFolder();
        if (defaultFolder.isEmpty())
            defaultFolder = QDir::homePath();
        FileTemplateDialog dlg(defaultFolder, this);
        connect(&dlg, &FileTemplateDialog::templateSelected,
                this, [this](const QString &filePath, const QString & /*content*/) {
            openFile(filePath);
        });
        dlg.exec();
    });
    connect(newPluginAction, &QAction::triggered, this, [this]() {
        NewPluginWizard dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString primary = dlg.primarySourceFile();
            if (!primary.isEmpty() && QFileInfo::exists(primary))
                openFile(primary);
        }
    });
    // newQtClassAction handler is in qt-tools plugin (command: qt.newClass).
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    connect(openFolderAction, &QAction::triggered, this, [this]() { openFolder(); });
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentTab);
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        const QString path = QFileDialog::getSaveFileName(this, tr("Save File As"));
        if (path.isEmpty()) return;
        editor->setProperty("filePath", path);
        saveCurrentTab();
    });
    connect(newSolutionAction, &QAction::triggered, this, &MainWindow::newSolution);
    connect(openSolutionAction, &QAction::triggered, this, &MainWindow::openSolutionFile);
    connect(addProjectAction, &QAction::triggered, this, &MainWindow::addProjectToSolution);
    connect(saveSolutionAction, &QAction::triggered, this, [this]() {
        m_editorMgr->projectManager()->saveSolution();
    });
    auto closeWorkspace = [this](const QString &statusText, bool unwatchFiles) {
        m_editorMgr->projectManager()->closeSolution();
        m_editorMgr->closeAllTabs();
        m_editorMgr->setCurrentFolder(QString());
        ScopedSettings::instance().setWorkspaceRoot({});
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->stopServer();
        m_gitService->setWorkingDirectory({});
        if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
            srch->setRootPath({});
        if (auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService"))))
            ts->setWorkingDirectory({});
        if (unwatchFiles && m_workbenchServices && m_workbenchServices->fileWatcher())
            m_workbenchServices->fileWatcher()->unwatchAll();
        // Switch back to the welcome page (index 0) and refresh recent list.
        if (m_centralStack)
            m_centralStack->setCurrentIndex(0);
        if (m_welcome)
            m_welcome->refreshRecent();
        setWindowTitle(tr("Exorcist"));
        statusBar()->showMessage(statusText, 3000);
    };
    connect(closeSolutionAction, &QAction::triggered, this, [closeWorkspace]() {
        closeWorkspace(tr("Solution closed"), false);
    });
    connect(closeFolderAction, &QAction::triggered, this, [closeWorkspace]() {
        closeWorkspace(tr("Folder closed"), true);
    });
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // ── Edit ──────────────────────────────────────────────────────────────
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    // Helper: dispatch edit actions to whichever text widget currently has focus.
    // Non-EditorView text widgets (output panels, search boxes, chat input etc.) must
    // receive Ctrl+C / Ctrl+A / Ctrl+X / Ctrl+V rather than the code editor.
    auto dispatchEdit = [this](auto editorCall, auto pteFn, auto teFn, auto leFn,
                               QKeySequence::StandardKey stdKey) {
        QWidget *fw = QApplication::focusWidget();
        if (fw && !qobject_cast<EditorView *>(fw)) {
            if (auto *pte = qobject_cast<QPlainTextEdit *>(fw)) { (pte->*pteFn)(); return; }
            if (auto *te  = qobject_cast<QTextEdit *>(fw))      { (te->*teFn)();  return; }
            if (auto *le  = qobject_cast<QLineEdit *>(fw))      { (le->*leFn)();  return; }
            // Tree/table/list views — let the widget's own handler process it.
            // Re-deliver the key sequence as a synthetic event since our menu
            // shortcut already consumed the original one.
            if (qobject_cast<QAbstractItemView *>(fw)
                || fw->inherits("QWebEngineView")
                || fw->inherits("ChatTranscriptView")) {
                const QKeySequence seq = QKeySequence(stdKey);
                if (!seq.isEmpty()) {
                    const int k = seq[0].toCombined();
                    QKeyEvent press(QEvent::KeyPress, k & ~Qt::KeyboardModifierMask,
                                    Qt::KeyboardModifiers(k & Qt::KeyboardModifierMask));
                    QApplication::sendEvent(fw, &press);
                }
                return;
            }
        }
        if (auto *e = m_editorMgr->currentEditor())
            editorCall(e);
    };

    QAction *undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->undo(); },
                     &QPlainTextEdit::undo, &QTextEdit::undo, &QLineEdit::undo,
                     QKeySequence::Undo);
    });

    QAction *redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->redo(); },
                     &QPlainTextEdit::redo, &QTextEdit::redo, &QLineEdit::redo,
                     QKeySequence::Redo);
    });

    editMenu->addSeparator();

    QAction *cutAction = editMenu->addAction(tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->cut(); },
                     &QPlainTextEdit::cut, &QTextEdit::cut, &QLineEdit::cut,
                     QKeySequence::Cut);
    });

    QAction *copyAction = editMenu->addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->copy(); },
                     &QPlainTextEdit::copy, &QTextEdit::copy, &QLineEdit::copy,
                     QKeySequence::Copy);
    });

    QAction *pasteAction = editMenu->addAction(tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->paste(); },
                     &QPlainTextEdit::paste, &QTextEdit::paste, &QLineEdit::paste,
                     QKeySequence::Paste);
    });

    editMenu->addSeparator();

    QAction *selectAllAction = editMenu->addAction(tr("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->selectAll(); },
                     &QPlainTextEdit::selectAll, &QTextEdit::selectAll, &QLineEdit::selectAll,
                     QKeySequence::SelectAll);
    });

    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction(tr("&Find..."));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() {
        if (auto *e = m_editorMgr->currentEditor())
            e->showFindBar();
    });

    QAction *findReplAction = editMenu->addAction(tr("Find && &Replace..."));
    findReplAction->setShortcut(QKeySequence::Replace);
    connect(findReplAction, &QAction::triggered, this, [this]() {
        if (auto *e = m_editorMgr->currentEditor())
            e->showFindBar();
    });

    QAction *gotoLineAction = editMenu->addAction(tr("&Go to Line..."));
    gotoLineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(gotoLineAction, &QAction::triggered, this, [this]() {
        auto *editor = m_editorMgr->currentEditor();
        if (!editor) return;
        const int curLine = editor->textCursor().blockNumber() + 1;
        const int total   = editor->document()->blockCount();
        GoToLineDialog dlg(curLine, total, this);
        connect(&dlg, &GoToLineDialog::lineSelected, this, [editor](int line, int col) {
            QTextCursor cur(editor->document()->findBlockByNumber(line - 1));
            if (col > 0) cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col - 1);
            editor->setTextCursor(cur);
            editor->centerCursor();
        });
        dlg.exec();
    });

    QAction *gotoSymbolAction = editMenu->addAction(tr("Go to Symbol in &Workspace..."));
    gotoSymbolAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(gotoSymbolAction, &QAction::triggered, this, [this]() {
        WorkspaceSymbolsDialog dlg(this);
        dlg.setLspClient(m_services->service<LspClient>(QStringLiteral("lspClient")));
        connect(&dlg, &WorkspaceSymbolsDialog::symbolActivated, this,
                [this](const QString &fp, int line, int ch) {
            navigateToLocation(fp, line, ch);
        });
        dlg.exec();
    });

    editMenu->addSeparator();

    QAction *prefsAction = editMenu->addAction(tr("&Preferences..."));
    prefsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(prefsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(m_workbenchServices->themeManager(), this);

        // Add Language and Terminal settings pages
        auto *langMgr = findChild<LanguageProfileManager *>(
            QStringLiteral("languageProfileManager"));
        if (langMgr) {
            dlg.addPage(new LanguageSettingsPage(langMgr, &dlg));
        }
        dlg.addPage(new TerminalSettingsPage(&dlg));

        connect(&dlg, &SettingsDialog::settingsApplied, this, [this]() {
            // Notify WorkspaceSettings so it re-resolves global → workspace hierarchy
            if (auto *wss = m_services->service<WorkspaceSettings>(
                    QStringLiteral("workspaceSettings"))) {
                emit wss->settingsChanged();
            } else {
                // Fallback: apply editor settings directly from QSettings
                QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
                s.beginGroup(QStringLiteral("editor"));
                const QFont font(s.value(QStringLiteral("fontFamily"), QStringLiteral("Consolas")).toString(),
                                 s.value(QStringLiteral("fontSize"), 11).toInt());
                const int tabSize = s.value(QStringLiteral("tabSize"), 4).toInt();
                const bool wrap = s.value(QStringLiteral("wordWrap"), false).toBool();
                const bool minimap = s.value(QStringLiteral("showMinimap"), false).toBool();
                s.endGroup();

                for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                    if (auto *e = m_editorMgr->editorAt(i)) {
                        e->setFont(font);
                        e->setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * tabSize);
                        e->setWordWrapMode(wrap ? QTextOption::WordWrap : QTextOption::NoWrap);
                        e->setMinimapVisible(minimap);
                    }
                }
            }

            // Re-index workspace if indexer settings changed
            if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
                srch->indexWorkspace(m_editorMgr->currentFolder());

            // Sync language profiles → plugin activation
            if (auto *lpm2 = findChild<LanguageProfileManager *>(
                    QStringLiteral("languageProfileManager"))) {
                const QSet<QString> active = lpm2->activeLanguageIds();
                m_pluginManager->setActiveLanguageProfiles(active);
                // Activate any deferred plugins whose profiles just became active
                for (const QString &lid : active)
                    m_pluginManager->activateByLanguageProfile(lid);
            }
        });
        dlg.exec();
    });

    QAction *keymapAction = editMenu->addAction(tr("&Keyboard Shortcuts..."));
    keymapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_S));
    connect(keymapAction, &QAction::triggered, this, [this]() {
        KeymapDialog dlg(m_workbenchServices->keymapManager(), this);
        dlg.exec();
    });

    // ── View ──────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QAction *cmdPaletteAction = viewMenu->addAction(tr("&Command Palette"));
    cmdPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

    // Note: Ctrl+P is now bound to the new "Quick Open File..." action under
    // File menu (QuickOpenDialog). The legacy palette stays accessible from
    // the View menu without a shortcut.
    QAction *filePaletteAction = viewMenu->addAction(tr("&Go to File (legacy)..."));
QAction *symbolPaletteAction = viewMenu->addAction(tr("Go to &Symbol..."));
    symbolPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));

    
    viewMenu->addSeparator();

    // Dock toggle actions.
    // They are added after createDockWidgets() populates the dock pointers, so
    // we use a lambda that defers the addAction until after the constructor.
    QAction *toggleProjectAction   = viewMenu->addAction(tr("&Project panel"));
    QAction *toggleSearchAction    = viewMenu->addAction(tr("&Search panel"));
    toggleSearchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    QAction *toggleGitAction       = viewMenu->addAction(tr("&Git panel"));
    QAction *toggleTerminalAction  = viewMenu->addAction(tr("&Terminal panel"));
    QAction *toggleAiAction        = viewMenu->addAction(tr("&AI panel"));
    QAction *toggleOutlineAction   = viewMenu->addAction(tr("&Outline panel"));
    QAction *toggleRefsAction      = viewMenu->addAction(tr("&References panel"));
    viewMenu->addSeparator();
    QAction *toggleLogAction       = viewMenu->addAction(tr("Request &Log"));
    QAction *toggleTrajectoryAction = viewMenu->addAction(tr("&Trajectory"));
    QAction *toggleSettingsAction  = viewMenu->addAction(tr("AI &Settings"));
    QAction *toggleMemoryAction    = viewMenu->addAction(tr("AI &Memory"));
    QAction *toggleMcpAction       = viewMenu->addAction(tr("&MCP Servers"));
    QAction *togglePluginAction    = viewMenu->addAction(tr("E&xtensions"));
    QAction *toggleThemeAction     = viewMenu->addAction(tr("&Themes"));
    QAction *toggleOutputAction    = viewMenu->addAction(tr("&Output / Build"));
    QAction *toggleRunAction       = viewMenu->addAction(tr("&Run panel"));
    QAction *toggleDebugAction     = viewMenu->addAction(tr("&Debug panel"));
    QAction *toggleProblemsAction  = viewMenu->addAction(tr("&Problems panel"));
    QAction *toggleMarkdownPreviewAction = viewMenu->addAction(tr("Toggle &Markdown Preview"));
    // "Toggle Qt Help" view action: contributed by the qt-tools plugin via
    // IDockManager::panelToggleAction("QtHelpDock").
    viewMenu->addSeparator();
    QAction *toggleBlameAction     = viewMenu->addAction(tr("Toggle Git &Blame"));
    toggleBlameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
    toggleBlameAction->setCheckable(true);
    toggleBlameAction->setChecked(false);

    QAction *themeToggleAction = viewMenu->addAction(tr("Toggle &Dark/Light Theme"));
    themeToggleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F10));
    connect(themeToggleAction, &QAction::triggered, this, [this]() {
        m_workbenchServices->themeManager()->toggle();
    });

    QAction *minimapAction = viewMenu->addAction(tr("Toggle &Minimap"));
    minimapAction->setCheckable(true);
    minimapAction->setChecked(true);
    connect(minimapAction, &QAction::toggled, this, [this](bool on) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            if (auto *ev = m_editorMgr->editorAt(i))
                ev->setMinimapVisible(on);
        }
    });

    QAction *indentGuidesAction = viewMenu->addAction(tr("Toggle Indent &Guides"));
    indentGuidesAction->setCheckable(true);
    indentGuidesAction->setChecked(true);
    connect(indentGuidesAction, &QAction::toggled, this, [this](bool on) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            if (auto *ev = m_editorMgr->editorAt(i))
                ev->setIndentGuidesVisible(on);
        }
    });

    QAction *unfoldAllAction = viewMenu->addAction(tr("Unfold &All"));
    unfoldAllAction->setShortcut(QKeySequence(tr("Ctrl+Shift+]")));
    connect(unfoldAllAction, &QAction::triggered, this, [this]() {
        if (auto *ev = currentEditor())
            ev->foldingEngine()->unfoldAll();
    });

    connect(toggleBlameAction, &QAction::toggled, this, [this](bool checked) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        if (checked && m_gitService && m_gitService->isGitRepo()) {
            const QString path = editor->property("filePath").toString();
            if (path.isEmpty()) return;
            // Async blame — Manifesto #2: never block UI thread
            m_gitService->blameAsync(path);
        } else {
            editor->clearBlameData();
        }
    });
    connect(m_gitService, &GitService::blameReady, this,
            [this](const QString &filePath, const QList<GitService::BlameEntry> &entries) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        const QString editorPath = editor->property("filePath").toString();
        if (editorPath != filePath) return;
        QHash<int, EditorView::BlameInfo> blameMap;
        for (const auto &e : entries) {
            EditorView::BlameInfo bi;
            bi.author  = e.author;
            bi.hash    = e.commitHash;
            bi.summary = e.summary;
            blameMap.insert(e.line, bi);
        }
        editor->setBlameData(blameMap);
        editor->setBlameVisible(true);
    });

    QAction *compareHeadAction = viewMenu->addAction(tr("Compare with &HEAD"));
    compareHeadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(compareHeadAction, &QAction::triggered, this, [this]() {
        EditorView *editor = currentEditor();
        if (!editor || !m_gitService || !m_gitService->isGitRepo()) return;
        const QString path = editor->property("filePath").toString();
        if (path.isEmpty()) return;
        // Async — Manifesto #2: never block UI thread
        m_gitService->showAtHeadAsync(path);
    });
    connect(m_gitService, &GitService::showAtHeadReady, this,
            [this](const QString &filePath, const QString &headText) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        const QString currentText = editor->toPlainText();
        m_dockBootstrap->diffViewer()->showDiff(filePath, headText, currentText);
        m_dockManager->showDock(dock(QStringLiteral("DiffDock")), exdock::SideBarArea::Bottom);
    });

    for (QAction *a : {toggleProjectAction, toggleSearchAction, toggleGitAction,
                       toggleTerminalAction, toggleAiAction,
                       toggleOutlineAction,  toggleRefsAction}) {
        a->setCheckable(true);
        a->setChecked(true);
    }
    toggleLogAction->setCheckable(true);
    toggleLogAction->setChecked(false);
    toggleTrajectoryAction->setCheckable(true);
    toggleTrajectoryAction->setChecked(false);
    toggleMemoryAction->setCheckable(true);
    toggleMemoryAction->setChecked(false);
    toggleMcpAction->setCheckable(true);
    toggleMcpAction->setChecked(false);
    togglePluginAction->setCheckable(true);
    togglePluginAction->setChecked(false);
    toggleThemeAction->setCheckable(true);
    toggleThemeAction->setChecked(false);
    toggleOutputAction->setCheckable(true);
    toggleOutputAction->setChecked(false);
    toggleRunAction->setCheckable(true);
    toggleRunAction->setChecked(false);
    toggleDebugAction->setCheckable(true);
    toggleDebugAction->setChecked(false);
    toggleProblemsAction->setCheckable(true);
    toggleProblemsAction->setChecked(false);
    toggleMarkdownPreviewAction->setCheckable(true);
    toggleMarkdownPreviewAction->setChecked(false);

    // ── Focus / Zen Mode ──────────────────────────────────────────────────
    // Hides toolbars + dock sidebars + status bar — distraction-free editing.
    // Persisted via QSettings("Exorcist","Exorcist") key "focusMode".
    viewMenu->addSeparator();
    QAction *focusModeAction = viewMenu->addAction(tr("Toggle Focus &Mode"));
    focusModeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K, Z")));
    focusModeAction->setShortcutContext(Qt::ApplicationShortcut);
    focusModeAction->setCheckable(true);
    focusModeAction->setToolTip(tr("Toggle Focus Mode (Ctrl+K, Z) — hide all panels and toolbars"));
    connect(focusModeAction, &QAction::triggered, this, [this, focusModeAction](bool checked) {
        m_focusMode = checked;
        if (statusBar()) statusBar()->setVisible(!m_focusMode);
        for (QToolBar *tb : findChildren<QToolBar *>())
            tb->setVisible(!m_focusMode);
        if (m_dockManager) m_dockManager->setDockLayoutVisible(!m_focusMode);
        QSettings(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"))
            .setValue(QStringLiteral("focusMode"), m_focusMode);
        if (statusBar())
            statusBar()->showMessage(m_focusMode
                ? tr("Focus Mode On — Ctrl+K Z to exit")
                : tr("Focus Mode Off"), 3000);
    });

    // ── Window ────────────────────────────────────────────────────────────
    QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));

    QAction *closeTabAction = windowMenu->addAction(tr("&Close Tab"));
    closeTabAction->setShortcut(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, this, [this]() {
        if (m_editorMgr && m_editorMgr->tabs())
            m_editorMgr->closeTab(m_editorMgr->tabs()->currentIndex());
    });

    QAction *closeAllAction = windowMenu->addAction(tr("Close &All Tabs"));
    connect(closeAllAction, &QAction::triggered, this, [this]() {
        if (m_editorMgr) m_editorMgr->closeAllTabs();
    });

    QAction *closeOthersAction = windowMenu->addAction(tr("Close &Other Tabs"));
    connect(closeOthersAction, &QAction::triggered, this, [this]() {
        if (m_editorMgr && m_editorMgr->tabs())
            m_editorMgr->closeOtherTabs(m_editorMgr->tabs()->currentIndex());
    });

    windowMenu->addSeparator();

    QAction *nextTab = windowMenu->addAction(tr("&Next Tab"));
    nextTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
    connect(nextTab, &QAction::triggered, this, [this]() {
        if (!m_editorMgr || !m_editorMgr->tabs()) return;
        auto *tabs = m_editorMgr->tabs();
        if (tabs->count() == 0) return;
        int next = (tabs->currentIndex() + 1) % tabs->count();
        tabs->setCurrentIndex(next);
    });

    QAction *prevTab = windowMenu->addAction(tr("&Previous Tab"));
    prevTab->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTab, &QAction::triggered, this, [this]() {
        if (!m_editorMgr || !m_editorMgr->tabs()) return;
        auto *tabs = m_editorMgr->tabs();
        if (tabs->count() == 0) return;
        int prev = (tabs->currentIndex() - 1 + tabs->count()) % tabs->count();
        tabs->setCurrentIndex(prev);
    });

    windowMenu->addSeparator();

    // Dynamic list of currently-open editor tabs. Rebuilt on each menu show.
    connect(windowMenu, &QMenu::aboutToShow, this, [this, windowMenu]() {
        // Remove dynamic actions added on previous show.
        const auto actions = windowMenu->actions();
        for (auto *a : actions) {
            if (a->property("dynamic").toBool()) {
                windowMenu->removeAction(a);
                a->deleteLater();
            }
        }
        if (!m_editorMgr || !m_editorMgr->tabs()) return;
        auto *tabs = m_editorMgr->tabs();
        for (int i = 0; i < tabs->count(); ++i) {
            const QString title = tabs->tabText(i);
            QAction *a = windowMenu->addAction(QStringLiteral("%1 %2").arg(i + 1).arg(title));
            a->setProperty("dynamic", true);
            a->setCheckable(true);
            a->setChecked(i == tabs->currentIndex());
            connect(a, &QAction::triggered, this, [tabs, i]() { tabs->setCurrentIndex(i); });
        }
    });

    // ── Tools ─────────────────────────────────────────────────────────────
    // Container creates the empty Tools menu; plugins (qt-tools, etc.)
    // contribute actions via IMenuManager::Tools.
    menuBar()->addMenu(tr("&Tools"));

    // ── Help ──────────────────────────────────────────────────────────────
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    auto showAboutDialog = [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    };

    QAction *aboutAction = helpMenu->addAction(tr("&About Exorcist..."));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, showAboutDialog);

    // Status bar quick-access action: clickable "About" entry on the right side
    // of the status bar, surfaces version + build info without opening the menu.
    auto *aboutStatusBtn = new QToolButton(this);
    aboutStatusBtn->setText(tr("About"));
    aboutStatusBtn->setAutoRaise(true);
    aboutStatusBtn->setCursor(Qt::PointingHandCursor);
    aboutStatusBtn->setToolTip(tr("About Exorcist"));
    aboutStatusBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #9cdcfe; border: none; padding: 0 8px; }"
        "QToolButton:hover { color: #ffffff; }"));
    connect(aboutStatusBtn, &QToolButton::clicked, this, showAboutDialog);
    statusBar()->addPermanentWidget(aboutStatusBtn);

    // ── AI shortcuts ──────────────────────────────────────────────────────
    auto *focusChatAct = new QAction(tr("Focus AI Chat"), this);
    focusChatAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(focusChatAct, &QAction::triggered, this, [this]() {
        m_chatPanel->focusInput();
    });
    addAction(focusChatAct);

    auto *toggleChatAct = new QAction(tr("Toggle AI Panel"), this);
    toggleChatAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(toggleChatAct, &QAction::triggered, this, [this]() {
        m_dockManager->toggleDock(dock(QStringLiteral("AIDock")));
        if (m_dockManager->isPinned(dock(QStringLiteral("AIDock"))))
            m_chatPanel->focusInput();
    });
    addAction(toggleChatAct);

    auto *quickChatAct = new QAction(tr("Quick Chat"), this);
    quickChatAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_L));
    connect(quickChatAct, &QAction::triggered, this, [this]() {
        if (!m_quickChat)
            m_quickChat = new QuickChatDialog(m_agentOrchestrator, this);
        auto *ed = currentEditor();
        if (ed) {
            m_quickChat->setEditorContext(
                ed->property("filePath").toString(),
                ed->textCursor().selectedText(),
                ed->property("languageId").toString());
        }
        m_quickChat->show();
        m_quickChat->raise();
        m_quickChat->activateWindow();
    });
    addAction(quickChatAct);

    auto *selectModelAct = new QAction(tr("Select AI Model"), this);
    selectModelAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(selectModelAct, &QAction::triggered, this, [this]() {
        ModelPickerDialog dlg(m_agentOrchestrator, this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString provId = dlg.selectedProviderId();
            const QString model  = dlg.selectedModel();
            if (!provId.isEmpty())
                m_agentOrchestrator->setActiveProvider(provId);
            auto *prov = m_agentOrchestrator->activeProvider();
            if (prov && !model.isEmpty())
                prov->setModel(model);
            statusBar()->showMessage(
                tr("Model: %1 (%2)").arg(model, provId), 3000);
        }
    });
    connect(symbolPaletteAction, &QAction::triggered, this, &MainWindow::showSymbolPalette);
    addAction(selectModelAct);

    connect(cmdPaletteAction, &QAction::triggered, this, &MainWindow::showCommandPalette);
    connect(filePaletteAction, &QAction::triggered, this, &MainWindow::showFilePalette);

    // Wire dock toggles via DockManager.
    auto dockToggle = [this](const QString &dockId) {
        return [this, dockId](bool on) {
            auto *d = dock(dockId);
            if (!d) return;
            if (on)
                m_dockManager->showDock(d, m_dockManager->inferSide(d));
            else
                m_dockManager->closeDock(d);
        };
    };
    connect(toggleProjectAction,  &QAction::toggled, this, dockToggle(QStringLiteral("ProjectDock")));
    connect(toggleSearchAction,   &QAction::toggled, this, [this](bool on) {
        if (on) {
            m_dockManager->showDock(dock(QStringLiteral("SearchDock")), m_dockManager->inferSide(dock(QStringLiteral("SearchDock"))));
            if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
                srch->activateSearch();
        } else {
            m_dockManager->closeDock(dock(QStringLiteral("SearchDock")));
        }
    });
    connect(toggleGitAction,      &QAction::toggled, this, dockToggle(QStringLiteral("GitDock")));
    connect(toggleTerminalAction, &QAction::toggled, this, dockToggle(QStringLiteral("TerminalDock")));
    connect(toggleAiAction,       &QAction::toggled, this, dockToggle(QStringLiteral("AIDock")));
    connect(toggleOutlineAction,  &QAction::toggled, this, dockToggle(QStringLiteral("OutlineDock")));
    connect(toggleRefsAction,     &QAction::toggled, this, dockToggle(QStringLiteral("ReferencesDock")));
    connect(toggleLogAction,      &QAction::toggled, this, dockToggle(QStringLiteral("RequestLogDock")));
    connect(toggleTrajectoryAction, &QAction::toggled, this, dockToggle(QStringLiteral("TrajectoryDock")));
    connect(toggleSettingsAction, &QAction::triggered, this, [this]() {
        if (!m_settingsDialog) {
            m_settingsDialog = new QDialog(this);
            m_settingsDialog->setWindowTitle(tr("AI Settings"));
            m_settingsDialog->setMinimumSize(420, 480);
            auto *lay = new QVBoxLayout(m_settingsDialog);
            lay->setContentsMargins(0, 0, 0, 0);
            m_settingsPanel->setParent(m_settingsDialog);
            lay->addWidget(m_settingsPanel);
        }
        m_settingsDialog->show();
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
    });
    connect(toggleMemoryAction,   &QAction::toggled, this, [this](bool on) {
        if (on)
            m_dockManager->showDock(dock(QStringLiteral("MemoryDock")), exdock::SideBarArea::Right);
        else
            m_dockManager->closeDock(dock(QStringLiteral("MemoryDock")));
        if (on) m_dockBootstrap->memoryBrowser()->refresh();
    });
    connect(toggleMcpAction,    &QAction::toggled, this, dockToggle(QStringLiteral("McpDock")));
    connect(togglePluginAction, &QAction::toggled, this, dockToggle(QStringLiteral("PluginDock")));
    connect(toggleThemeAction,  &QAction::toggled, this, dockToggle(QStringLiteral("ThemeDock")));
    connect(toggleOutputAction, &QAction::toggled, this, dockToggle(QStringLiteral("OutputDock")));
    connect(toggleRunAction,    &QAction::toggled, this, dockToggle(QStringLiteral("RunDock")));
    connect(toggleDebugAction,  &QAction::toggled, this, dockToggle(QStringLiteral("DebugDock")));
    connect(toggleProblemsAction, &QAction::toggled, this, dockToggle(QStringLiteral("ProblemsDock")));
    connect(toggleMarkdownPreviewAction, &QAction::toggled, this, dockToggle(QStringLiteral("MarkdownPreviewDock")));

    // Sync View-menu checkbox with dock state changes.
    // Use QSignalBlocker to prevent setChecked from firing toggled,
    // which would re-enter showDock/closeDock via dockToggle.
    auto syncAction = [](QAction *action, exdock::ExDockWidget *dock) {
        if (!dock) return; // Plugin-contributed docks may not exist yet
        QObject::connect(dock, &exdock::ExDockWidget::stateChanged,
                         action, [action](exdock::DockState s) {
            QSignalBlocker blocker(action);
            action->setChecked(s == exdock::DockState::Docked);
        });
    };
    syncAction(toggleProjectAction,    dock(QStringLiteral("ProjectDock")));
    syncAction(toggleSearchAction,     dock(QStringLiteral("SearchDock")));
    syncAction(toggleGitAction,        dock(QStringLiteral("GitDock")));
    syncAction(toggleTerminalAction,   dock(QStringLiteral("TerminalDock")));
    syncAction(toggleAiAction,         dock(QStringLiteral("AIDock")));
    syncAction(toggleOutlineAction,    dock(QStringLiteral("OutlineDock")));
    syncAction(toggleRefsAction,       dock(QStringLiteral("ReferencesDock")));
    syncAction(toggleLogAction,        dock(QStringLiteral("RequestLogDock")));
    syncAction(toggleTrajectoryAction, dock(QStringLiteral("TrajectoryDock")));
    syncAction(toggleMemoryAction,     dock(QStringLiteral("MemoryDock")));
    syncAction(toggleMcpAction,        dock(QStringLiteral("McpDock")));
    syncAction(togglePluginAction,     dock(QStringLiteral("PluginDock")));
    syncAction(toggleThemeAction,      dock(QStringLiteral("ThemeDock")));
    syncAction(toggleOutputAction,     dock(QStringLiteral("OutputDock")));
    syncAction(toggleRunAction,        dock(QStringLiteral("RunDock")));
    syncAction(toggleDebugAction,      dock(QStringLiteral("DebugDock")));
    syncAction(toggleProblemsAction,   dock(QStringLiteral("ProblemsDock")));
    syncAction(toggleMarkdownPreviewAction, dock(QStringLiteral("MarkdownPreviewDock")));
}

void MainWindow::setupToolBar()
{
    // Core shell does not own a default toolbar.
    // All top-level toolbars must be contributed by plugins through
    // IToolBarManager so each subsystem owns its own actions and lifecycle.
}

void MainWindow::updateMenuBarVisibility(bool editing)
{
    auto isWelcomeMenu = [](QAction *action) {
        if (!action)
            return false;

        const QString text = action->text().remove(QLatin1Char('&')).trimmed();
        return text == QLatin1String("File")
            || text == QLatin1String("Edit")
            || text == QLatin1String("View")
            || text == QLatin1String("Help");
    };

    for (QAction *action : menuBar()->actions()) {
        if (!action)
            continue;
        const bool visible = editing || isWelcomeMenu(action);
        action->setVisible(visible);
        if (auto *menu = action->menu()) {
            menu->menuAction()->setVisible(visible);
            menu->setEnabled(visible);
        }
    }

    menuBar()->updateGeometry();
    menuBar()->update();
    menuBar()->repaint();
}

void MainWindow::registerShellDock(const QString &id, const QString &title,
                                   QWidget *widget, IDockManager::Area area,
                                   bool startVisible)
{
    if (!m_dockManager || !widget)
        return;

    exdock::SideBarArea side = exdock::SideBarArea::Left;
    switch (area) {
    case IDockManager::Left:
        side = exdock::SideBarArea::Left;
        break;
    case IDockManager::Right:
        side = exdock::SideBarArea::Right;
        break;
    case IDockManager::Bottom:
        side = exdock::SideBarArea::Bottom;
        break;
    case IDockManager::Center:
        side = exdock::SideBarArea::Left;
        break;
    }

    auto *dockWidget = new exdock::ExDockWidget(title, this);
    dockWidget->setDockId(id);
    dockWidget->setContentWidget(widget);
    m_dockManager->addDockWidget(dockWidget, side, startVisible);
    if (!startVisible)
        m_dockManager->closeDock(dockWidget);
}

void MainWindow::setShellDockVisible(const QString &id, bool visible)
{
    auto *dockWidget = dock(id);
    if (!dockWidget || !m_dockManager)
        return;

    if (visible)
        m_dockManager->showDock(dockWidget, m_dockManager->inferSide(dockWidget));
    else
        m_dockManager->closeDock(dockWidget);
}

void MainWindow::createDockWidgets()
{
    // ── Create DockManager (takes over centralWidget) ─────────────────────
    m_dockManager = new exdock::DockManager(this, this);
    // Start with dock infrastructure hidden — only the Welcome page shows.
    m_dockManager->setDockLayoutVisible(false);


    // (ThemeManager→dockManager signal wired in createDockWidgets once dockManager exists.)

    // Build the central area as a QStackedWidget:
    //   page 0 = WelcomeWidget   (shown when no project is open)
    //   page 1 = editor container (breadcrumb + tab widget)
    m_centralStack = new QStackedWidget(this);
    m_editorMgr->setCentralStack(m_centralStack);

    m_welcome = new WelcomeWidget(this);
    m_centralStack->addWidget(m_welcome);  // page 0

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

    // Wire welcome signals
    connect(m_welcome, &WelcomeWidget::openFolderBrowseRequested,
            this, [this]() { openFolder(); });
    connect(m_welcome, &WelcomeWidget::openFolderRequested,
            this, &MainWindow::openFolder);
    connect(m_welcome, &WelcomeWidget::newProjectRequested,
            this, &MainWindow::newSolution);
    connect(m_welcome, &WelcomeWidget::newFileRequested,
            this, [this]() { openNewTab(); });
    connect(m_welcome, &WelcomeWidget::openFileRequested,
            this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });

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

// ── Tabs / editor ────────────────────────────────────────────────────────────

void MainWindow::openNewTab()
{
    // Switch from welcome page to editor view
    if (m_centralStack && m_centralStack->currentIndex() == 0)
        m_centralStack->setCurrentIndex(1);

    auto *editor = new EditorView();
    int index = m_editorMgr->tabs()->addTab(editor, tr("Untitled"));
    m_editorMgr->tabs()->setCurrentIndex(index);
    updateEditorStatus(editor);
}


void MainWindow::onTabChanged(int /*index*/)
{
    EditorView *editor = currentEditor();
    updateEditorStatus(editor);

    // Update breadcrumb
    if (editor) {
        const QString fp = editor->property("filePath").toString();
        const QString rt = m_editorMgr->projectManager()
            ? m_editorMgr->projectManager()->activeSolutionDir() : QString();
        m_editorMgr->breadcrumb()->setFilePath(fp, rt);
    } else {
        m_editorMgr->breadcrumb()->clear();
        m_editorMgr->breadcrumb()->clearSymbols();
    }

    if (editor) {
        auto *bridge = editor->findChild<LspEditorBridge *>();
        if (bridge) {
            bridge->requestSymbols();
        } else {
            m_symbolPanel->clear();
            m_editorMgr->breadcrumb()->clearSymbols();
        }

        // Attach inline completion engine to the active editor
        if (m_inlineEngine) {
            const QString path = editor->property("filePath").toString();
            const QString lang = editor->property("languageId").toString();
            m_inlineEngine->attachEditor(editor, path, lang);
        }

        // Switch language-specific plugins when the active editor language changes
        {
            const QString lang = editor->property("languageId").toString();
            if (!lang.isEmpty() && m_pluginManager)
                m_pluginManager->switchActiveLanguage(lang);
        }

        // Update inline diff gutter for the active editor
        updateDiffRanges(editor);

        // Attach NES engine and wire ghost text acceptance
        if (m_nesEngine) {
            const QString path = editor->property("filePath").toString();
            m_nesEngine->attachEditor(editor, path);
            // Disconnect any previous NES connection to avoid duplicates
            disconnect(editor, &EditorView::ghostTextAccepted, this, nullptr);
            connect(editor, &EditorView::ghostTextAccepted, this, [this, editor] {
                if (!m_nesEngine) return;
                QTextCursor cur = editor->textCursor();
                // The ghost text was just inserted — the cursor is at end of insertion
                m_nesEngine->onEditAccepted(
                    editor->toPlainText().mid(
                        qMax(0, cur.position() - 200), 200),
                    cur.blockNumber(), cur.columnNumber());
            });
        }
    } else {
        m_symbolPanel->clear();
        if (m_inlineEngine)
            m_inlineEngine->detachEditor();
        if (m_nesEngine)
            m_nesEngine->detachEditor();
    }
}

void MainWindow::updateDiffRanges(EditorView *editor)
{
    if (!editor || !m_gitService || !m_gitService->isGitRepo())
        return;
    const QString path = editor->property("filePath").toString();
    if (path.isEmpty()) {
        editor->clearDiffRanges();
        return;
    }
    const auto changes = m_gitService->lineChanges(path);
    QList<EditorView::DiffRange> ranges;
    ranges.reserve(changes.size());
    for (const auto &c : changes) {
        EditorView::DiffRange dr;
        dr.startLine = c.startLine;
        dr.lineCount = c.lineCount;
        dr.type = static_cast<EditorView::DiffType>(static_cast<int>(c.type));
        ranges.append(dr);
    }
    editor->setDiffRanges(ranges);
}

void MainWindow::updateEditorStatus(EditorView *editor)
{
    // Disconnect previous editor's cursor signal before wiring a new one.
    disconnect(m_editorMgr->cursorConn());

    if (!m_statusBarMgr) return;   // called before status bar init

    if (!editor) {
        m_statusBarMgr->posLabel()->setText(tr("—"));
        return;
    }

    auto update = [this, editor]() {
        const QTextCursor c = editor->textCursor();
        m_statusBarMgr->posLabel()->setText(tr("Ln %1, Col %2")
            .arg(c.blockNumber() + 1)
            .arg(c.columnNumber() + 1));
    };

    update();
    m_editorMgr->setCursorConn(connect(editor, &QPlainTextEdit::cursorPositionChanged, this, update));
}

// ── File operations ──────────────────────────────────────────────────────────

void MainWindow::openFileFromIndex(const QModelIndex &index)
{
    if (!index.isValid() || m_editorMgr->treeModel()->isDir(index)) return;
    openFile(m_editorMgr->treeModel()->filePath(index));
}

void MainWindow::openFile(const QString &path)
{
    m_editorMgr->openFile(path);
}

void MainWindow::openFolder(const QString &path)
{
    QString folder = path;
    if (folder.isEmpty()) {
        folder = QFileDialog::getExistingDirectory(this, tr("Open Folder"));
    }
    if (folder.isEmpty()) return;

    // Switch from welcome page to editor view
    if (m_centralStack && m_centralStack->currentIndex() == 0) {
        m_centralStack->setCurrentIndex(1);
        // Open a blank tab if no tabs exist yet
        if (m_editorMgr->tabs()->count() == 0)
            openNewTab();
    }

    // Save to recent folders list (MRU, max 10)
    {
        QSettings rs(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        QStringList recent = rs.value(QStringLiteral("recentFolders")).toStringList();
        recent.removeAll(folder);
        recent.prepend(folder);
        while (recent.size() > 10)
            recent.removeLast();
        rs.setValue(QStringLiteral("recentFolders"), recent);
    }

    const bool hasSolution = m_editorMgr->projectManager()->openFolder(folder);
    m_gitService->setWorkingDirectory(folder);
    const QString root = m_editorMgr->projectManager()->activeSolutionDir();
    if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
        srch->setRootPath(root);
    setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
    statusBar()->showMessage(tr("Opened: %1").arg(root), 4000);
    if (hasSolution) {
        statusBar()->showMessage(tr("Solution: %1")
            .arg(QFileInfo(m_editorMgr->projectManager()->solution().filePath).fileName()), 4000);
    }

    m_editorMgr->setCurrentFolder(root);

    // Apply workspace-level settings (.exorcist/settings.json)
    if (auto *wss = m_services->service<WorkspaceSettings>(QStringLiteral("workspaceSettings")))
        wss->setWorkspaceRoot(root);

    // Activate workspace overlay for ScopedSettings (User vs Workspace scope).
    ScopedSettings::instance().setWorkspaceRoot(root);

    // Activate plugins with workspaceContains activation events,
    // then notify all active plugins of the new workspace root.
    // Each plugin owns its own workspace-change response (cmake setup,
    // terminal cwd, config reload, etc.) — MainWindow knows nothing about them.
    if (m_pluginManager) {
        m_pluginManager->activateByWorkspace(root);
        m_pluginManager->notifyWorkspaceChanged(root);
    }

    // Wire LSP service signals that loadPlugins() post-wiring may have
    // missed for deferred plugins (e.g. CppLanguagePlugin activated above).
    if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService"))) {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        qWarning() << "[LSP-DIAG] openFolder deferred wiring: lspSvc found, lspClient="
                   << (lspClient ? "yes" : "null");
        if (lspClient) {
            connect(lspClient, &LspClient::initialized,
                    this, &MainWindow::onLspInitialized, Qt::UniqueConnection);
        }
        connect(lspSvc, &ILspService::navigateToLocation,
                this, &MainWindow::navigateToLocation, Qt::UniqueConnection);
        // Qt::UniqueConnection requires a pointer-to-member-function slot
        // (lambdas can't be deduplicated and Qt asserts at runtime — fatal
        // on the second openFolder call).  Route these through dedicated
        // member methods instead.
        connect(lspSvc, &ILspService::referencesReady,
                this, &MainWindow::onLspReferencesReady, Qt::UniqueConnection);
        connect(lspSvc, &ILspService::workspaceEditRequested,
                this, &MainWindow::applyWorkspaceEdit, Qt::UniqueConnection);
        connect(lspSvc, &ILspService::statusMessage,
                this, &MainWindow::onLspStatusMessage, Qt::UniqueConnection);
        if (lspClient && m_agentPlatform) {
            if (auto *notifier = m_agentPlatform->diagnosticsNotifier()) {
                connect(lspClient, &LspClient::diagnosticsPublished,
                        notifier, &DiagnosticsNotifier::onDiagnosticsPublished,
                        Qt::UniqueConnection);
            }
        }
        if (lspClient && m_hostServices) {
            connect(lspClient, &LspClient::diagnosticsPublished,
                    m_hostServices->diagnosticsService(),
                    &DiagnosticsServiceImpl::onDiagnosticsPublished,
                    Qt::UniqueConnection);
        }
    }

    if (auto *profileMgr = qobject_cast<ProfileManager *>(
            m_services->service(QStringLiteral("profileManager")))) {
        const QString detectedProfile = profileMgr->autoDetectEnabled()
            ? profileMgr->detectProfile(root)
            : profileMgr->activeProfile();
        if (!detectedProfile.isEmpty())
            profileMgr->activateProfile(detectedProfile);
    }

    // ── Contextual dock activation ─────────────────────────────────────
    // Show core panels relevant to the opened workspace.
    // Plugin-contributed docks (Output, Run, Debug) are shown by their
    // owning plugins, NOT by MainWindow — see plugin-first extension model.
    //
    // Deferred via singleShot(0) so the editor tab (and its LineNumberArea)
    // are fully laid out before the dock resize cascade fires. Calling
    // showDock synchronously here triggers Qt layout reflows that reach
    // EditorView::resizeEvent while the widget is still being initialised,
    // resulting in an access violation inside Qt6Widgets.dll.
    QTimer::singleShot(0, this, [this]() {
        m_dockManager->showDock(dock(QStringLiteral("ProjectDock")), exdock::SideBarArea::Left);
        if (m_gitService && m_gitService->isGitRepo())
            m_dockManager->showDock(dock(QStringLiteral("GitDock")), exdock::SideBarArea::Left);
        if (dock(QStringLiteral("AIDock")))
            m_dockManager->showDock(dock(QStringLiteral("AIDock")), exdock::SideBarArea::Right);
        m_dockManager->showDock(dock(QStringLiteral("TerminalDock")), exdock::SideBarArea::Bottom);
        if (dock(QStringLiteral("ProblemsDock")))
            m_dockManager->showDock(dock(QStringLiteral("ProblemsDock")), exdock::SideBarArea::Bottom);
    });

    // Auto-detect workspace languages and activate their profiles.
    // Two-pass approach:
    //   1. Marker files (build system indicators) — instant, top-level only
    //   2. File extension scan (depth-limited) — runs async to not block startup
    if (m_pluginManager) {
        // Pass 1: project file markers (instant)
        static const struct { const char *file; const char *lang; } markers[] = {
            {"CMakeLists.txt", "cpp"}, {"Cargo.toml", "rust"},
            {"package.json", "javascript"}, {"tsconfig.json", "typescript"},
            {"pyproject.toml", "python"}, {"setup.py", "python"},
            {"go.mod", "go"}, {"pom.xml", "java"},
            {"build.gradle", "java"}, {"*.sln", "csharp"},
        };
        QDir ws(root);
        for (const auto &m : markers) {
            if (!ws.entryList(QStringList{QString::fromLatin1(m.file)},
                              QDir::Files).isEmpty()) {
                const QString lang = QString::fromLatin1(m.lang);
                m_pluginManager->addWorkspaceLanguage(lang);
                m_pluginManager->activateByLanguageProfile(lang);
            }
        }

        // Pass 2: extension scan (async, depth-limited)
        // Scans actual files to discover languages not covered by markers
        QTimer::singleShot(100, this, [this, root]() {
            if (!m_pluginManager)
                return;

            // Extension → language ID mapping (subset of LspClient::languageIdForPath)
            static const QHash<QString, QString> extMap = {
                {"cpp", "cpp"}, {"cxx", "cpp"}, {"cc", "cpp"}, {"c", "cpp"},
                {"h", "cpp"}, {"hpp", "cpp"}, {"hxx", "cpp"},
                {"py", "python"}, {"pyw", "python"},
                {"js", "javascript"}, {"mjs", "javascript"},
                {"ts", "typescript"}, {"tsx", "typescript"},
                {"rs", "rust"}, {"go", "go"}, {"java", "java"},
                {"kt", "kotlin"}, {"kts", "kotlin"},
                {"swift", "swift"}, {"dart", "dart"},
                {"rb", "ruby"}, {"php", "php"}, {"lua", "lua"},
                {"scala", "scala"}, {"cs", "csharp"},
            };

            QSet<QString> detected;
            // Depth-limited BFS (max depth 3, max 2000 entries)
            struct Level { QString path; int depth; };
            QVector<Level> queue;
            queue.append({root, 0});
            int scanned = 0;
            constexpr int kMaxDepth = 3;
            constexpr int kMaxEntries = 2000;

            while (!queue.isEmpty() && scanned < kMaxEntries) {
                const auto cur = queue.takeFirst();
                QDir dir(cur.path);
                const auto entries = dir.entryInfoList(
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &fi : entries) {
                    if (++scanned > kMaxEntries)
                        break;
                    if (fi.isDir()) {
                        // Skip hidden dirs, build dirs, node_modules, .git
                        const QString name = fi.fileName();
                        if (name.startsWith(QLatin1Char('.'))
                            || name == QLatin1String("node_modules")
                            || name == QLatin1String("build")
                            || name == QLatin1String("dist")
                            || name == QLatin1String("__pycache__")
                            || name.startsWith(QLatin1String("build-")))
                            continue;
                        if (cur.depth < kMaxDepth)
                            queue.append({fi.absoluteFilePath(), cur.depth + 1});
                    } else {
                        const QString ext = fi.suffix().toLower();
                        auto it = extMap.find(ext);
                        if (it != extMap.end())
                            detected.insert(it.value());
                    }
                }
            }

            // Activate detected languages as workspace-level profiles
            for (const QString &lang : detected) {
                if (!m_pluginManager->activeLanguageProfiles().contains(lang)) {
                    m_pluginManager->addWorkspaceLanguage(lang);
                    m_pluginManager->activateByLanguageProfile(lang);
                }
            }

            if (!detected.isEmpty())
                qInfo("Workspace language scan: detected %lld language(s): %s",
                      static_cast<long long>(detected.size()),
                      qUtf8Printable(QStringList(detected.begin(), detected.end()).join(QStringLiteral(", "))));
        });
    }

    // ── Toolchain & CMake detection (via build plugin service) ──────────
    // Clangd start is deferred until after CMake detection so we can pass
    // --compile-commands-dir if compile_commands.json exists in a build dir.
    QTimer::singleShot(0, this, [this, root]() {
        auto *buildSys = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));

        QStringList clangdArgs;
        if (buildSys && buildSys->hasProject()) {
            // Find compile_commands.json — try build service first, then scan
            QString ccjson = buildSys->compileCommandsPath();
            if (ccjson.isEmpty()) {
                // Scan common build dirs for compile_commands.json
                const QStringList candidates = {
                    QStringLiteral("build-llvm"), QStringLiteral("build"),
                    QStringLiteral("build-debug"), QStringLiteral("build-release"),
                    QStringLiteral("build-ci"), QStringLiteral("cmake-build-debug"),
                };
                for (const auto &d : candidates) {
                    const QString p = root + QLatin1Char('/') + d
                                    + QStringLiteral("/compile_commands.json");
                    if (QFileInfo::exists(p)) { ccjson = p; break; }
                }
            }
            if (!ccjson.isEmpty()) {
                // Point clangd to the directory containing compile_commands.json
                const QString ccDir = QFileInfo(ccjson).absolutePath();
                clangdArgs << QStringLiteral("--compile-commands-dir=") + ccDir;

                // Extract include paths for #include file opening
                QFile ccFile(ccjson);
                if (ccFile.open(QIODevice::ReadOnly)) {
                    const QJsonArray arr = QJsonDocument::fromJson(ccFile.readAll()).array();
                    if (!arr.isEmpty()) {
                        const QString cmd = arr[0].toObject()[QStringLiteral("command")].toString();
                        QSet<QString> seen;
                        // Match -I<path>, -I <path>, -isystem <path>
                        const auto parts = cmd.split(QLatin1Char(' '));
                        for (int i = 0; i < parts.size(); ++i) {
                            QString dir;
                            if (parts[i].startsWith(QStringLiteral("-isystem")) && i + 1 < parts.size()) {
                                dir = parts[i + 1];
                                ++i;
                            } else if (parts[i].startsWith(QStringLiteral("-I"))) {
                                dir = parts[i].mid(2);
                                if (dir.isEmpty() && i + 1 < parts.size()) {
                                    dir = parts[++i];
                                }
                            }
                            if (!dir.isEmpty() && !seen.contains(dir)) {
                                seen.insert(dir);
                                m_editorMgr->addIncludePath(QDir::cleanPath(dir));
                            }
                        }
                        qWarning() << "Loaded" << m_editorMgr->includePaths().size() << "include paths from compile_commands.json";
                    }
                }
            } else {
                // No compile_commands.json yet — auto-configure if the active profile
                // requests it or if the project has CMakePresets.json.
                bool shouldAutoConfigure = false;
                if (auto *wss = m_services->service<WorkspaceSettings>(
                        QStringLiteral("workspaceSettings"))) {
                    shouldAutoConfigure = wss->value(QStringLiteral("build"),
                                                     QStringLiteral("autoConfigure"),
                                                     false).toBool();
                }

                const QString presetsPath = root + QStringLiteral("/CMakePresets.json");
                if (shouldAutoConfigure || QFileInfo::exists(presetsPath)) {
                    statusBar()->showMessage(
                        tr("CMake project detected \u2014 auto-configuring..."), 4000);

                    // Defer clangd start until configure completes
                    auto conn = std::make_shared<QMetaObject::Connection>();
                    *conn = connect(buildSys, &IBuildSystem::configureFinished,
                                    this, [this, root, buildSys, conn](bool ok, const QString &) {
                        QObject::disconnect(*conn);
                        if (!ok) return;

                        // After configure, compile_commands.json should exist
                        QString ccPath = buildSys->compileCommandsPath();
                        QStringList args;
                        if (!ccPath.isEmpty())
                            args << QStringLiteral("--compile-commands-dir=")
                                    + QFileInfo(ccPath).absolutePath();

                        if (auto *lsp = m_services->service<ILspService>(QStringLiteral("lspService")))
                            lsp->startServer(root, args);

                        statusBar()->showMessage(tr("CMake configured — LSP started"), 4000);
                    });

                    buildSys->configure();
                    return; // clangd will start from configureFinished callback
                } else {
                    statusBar()->showMessage(
                        tr("CMake project detected \u2014 click Configure to generate compile_commands.json"),
                        6000);
                }
            }
        }

        // Start language server (with compile_commands.json dir if available)
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->startServer(root, clangdArgs);
    });

    // Load custom workspace theme (.exorcist/theme.json)
    {
        const QString themePath = root + QStringLiteral("/.exorcist/theme.json");
        if (QFileInfo::exists(themePath)) {
            QString err;
            if (m_workbenchServices->themeManager()->loadCustomTheme(themePath, &err))
                m_workbenchServices->themeManager()->setTheme(ThemeManager::Custom);
            else
                statusBar()->showMessage(tr("Theme error: %1").arg(err), 5000);
        }
    }

    // Update agent tools with new workspace root
    if (m_agentPlatform)
        m_agentPlatform->setWorkspaceRoot(root);

    // Load custom AI instructions from workspace (.github/copilot-instructions.md etc.)
    const QString instructions = PromptFileLoader::load(root);
    m_agentPlatform->contextBuilder()->setCustomInstructions(instructions);

    // Wire workspace root to chat panel for prompt files
    m_chatPanel->setWorkspaceRoot(root);

    // Scan for .prompt.md files
    if (m_aiServices && m_aiServices->promptFileManager())
        m_aiServices->promptFileManager()->scanWorkspace(root);

    // Set workspace root for commit message generator
    if (m_aiServices && m_aiServices->commitMsgGen())
        m_aiServices->commitMsgGen()->setWorkspaceRoot(root);

    // Start workspace indexer — delegated to search plugin (plugins/search/)
    // SearchPlugin owns WorkspaceIndexer and wires status bar updates internally.
    if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
        srch->indexWorkspace(root);

    // Restore previously open editor tabs for this workspace
    {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        const QString wsKey = QStringLiteral("workspace/")
                              + QString::fromUtf8(QCryptographicHash::hash(
                                    root.toUtf8(), QCryptographicHash::Md5).toHex());
        s.beginGroup(wsKey);

        const QString tabsJson = s.value(QStringLiteral("openTabs")).toString();
        const int savedActive  = s.value(QStringLiteral("activeTab"), -1).toInt();
        const QString bpsJson  = s.value(QStringLiteral("breakpoints")).toString();
        s.endGroup();

        if (!tabsJson.isEmpty()) {
            const QJsonArray tabs = QJsonDocument::fromJson(tabsJson.toUtf8()).array();
            if (!tabs.isEmpty()) {
                // Close the blank tab that was created above
                if (m_editorMgr->tabs()->count() == 1) {
                    QWidget *first = m_editorMgr->tabs()->widget(0);
                    if (first && first->property("filePath").toString().isEmpty())
                        m_editorMgr->closeTab(0);
                }

                for (const QJsonValue &v : tabs) {
                    const QJsonObject entry = v.toObject();
                    const QString fp = entry[QLatin1String("path")].toString();
                    if (fp.isEmpty() || !QFileInfo::exists(fp))
                        continue;

                    m_editorMgr->openFile(fp);

                    // Restore cursor and scroll position
                    const int line   = entry[QLatin1String("line")].toInt(-1);
                    const int col    = entry[QLatin1String("column")].toInt(0);
                    const int scroll = entry[QLatin1String("scroll")].toInt(-1);
                    if (line >= 0) {
                        if (auto *ev = m_editorMgr->currentEditor()) {
                            QTextCursor tc = ev->textCursor();
                            tc.movePosition(QTextCursor::Start);
                            tc.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);
                            tc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col);
                            ev->setTextCursor(tc);
                            if (scroll >= 0)
                                ev->verticalScrollBar()->setValue(scroll);
                        }
                    }
                }

                // Restore the active tab
                if (savedActive >= 0 && savedActive < m_editorMgr->tabs()->count())
                    m_editorMgr->tabs()->setCurrentIndex(savedActive);
            }
        }

        // Restore breakpoints: set gutter markers and queue for next debug session
        if (!bpsJson.isEmpty()) {
            const QJsonArray bps = QJsonDocument::fromJson(bpsJson.toUtf8()).array();
            auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
            for (const QJsonValue &v : bps) {
                const QJsonObject bp = v.toObject();
                const QString fp   = bp[QLatin1String("file")].toString();
                const int line     = bp[QLatin1String("line")].toInt(0);
                if (fp.isEmpty() || line <= 0) continue;

                // Restore gutter marker on open editor (if the file is open)
                for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                    auto *ev = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
                    if (ev && ev->property("filePath").toString() == fp) {
                        QSet<int> lines = ev->breakpointLines();
                        lines.insert(line);
                        ev->setBreakpointLines(lines);
                        break;
                    }
                }

                // Queue breakpoint for the next debug session
                if (adapter) {
                    DebugBreakpoint dbp;
                    dbp.filePath = fp;
                    dbp.line     = line;
                    adapter->addBreakpoint(dbp);
                }
            }
        }
    }
}

// Write CMakePresets.json for the selected kit so CMake uses the right
// compiler and generator without manual reconfiguration.  When no kit is
// selected the preset still defaults to Ninja so CMake never falls back
// to the Visual Studio / MSVC generator on Windows.
static void writeCMakePresetsJson(const QString &projectDir, const Kit &kit)
{
    const QString generator = kit.generator.isEmpty()
                                ? QStringLiteral("Ninja") : kit.generator;

    // Base cache variables: compiler paths if known
    QJsonObject baseCacheVars;
    if (!kit.cCompilerPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_C_COMPILER")]   = kit.cCompilerPath;
    if (!kit.cxxCompilerPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_CXX_COMPILER")] = kit.cxxCompilerPath;
    if (!kit.debuggerPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_DEBUGGER")]     = kit.debuggerPath;
    if (!kit.generatorPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_MAKE_PROGRAM")] = kit.generatorPath;

    // Three presets: debug, release, relwithdebinfo
    struct PresetInfo { QString suffix; QString display; QString buildType; };
    const PresetInfo infos[] = {
        { QStringLiteral("debug"),          QStringLiteral("Debug"),                QStringLiteral("Debug") },
        { QStringLiteral("release"),        QStringLiteral("Release"),              QStringLiteral("Release") },
        { QStringLiteral("relwithdebinfo"), QStringLiteral("Release with Debug Info"), QStringLiteral("RelWithDebInfo") },
    };

    QJsonArray configurePresets;
    QJsonArray buildPresets;

    for (const auto &p : infos) {
        QJsonObject vars = baseCacheVars;
        vars[QStringLiteral("CMAKE_BUILD_TYPE")] = p.buildType;

        QJsonObject cfg;
        cfg[QStringLiteral("name")]           = p.suffix;
        cfg[QStringLiteral("displayName")]    = p.display;
        cfg[QStringLiteral("generator")]      = generator;
        cfg[QStringLiteral("binaryDir")]      = QStringLiteral("${sourceDir}/build-") + p.suffix;
        cfg[QStringLiteral("cacheVariables")] = vars;
        configurePresets.append(cfg);

        QJsonObject bld;
        bld[QStringLiteral("name")]            = p.suffix;
        bld[QStringLiteral("configurePreset")] = p.suffix;
        buildPresets.append(bld);
    }

    QJsonObject root;
    root[QStringLiteral("version")]          = 3;
    root[QStringLiteral("configurePresets")] = configurePresets;
    root[QStringLiteral("buildPresets")]     = buildPresets;

    const QString presetsPath = projectDir + QStringLiteral("/CMakePresets.json");
    QFile f(presetsPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void MainWindow::newSolution()
{
    auto *registry = m_services->service<ProjectTemplateRegistry>(
        QStringLiteral("projectTemplateRegistry"));
    if (!registry)
        return;

    auto *kitMgr = m_services->service<IKitManager>(QStringLiteral("kitManager"));
    NewProjectWizard wizard(registry, m_editorMgr->currentFolder(), kitMgr, this);
    if (wizard.exec() != QDialog::Accepted)
        return;

    const QString projectDir = wizard.createdProjectPath();
    if (projectDir.isEmpty())
        return;

    // Generate CMakePresets.json in the project dir so CMake uses the
    // right compiler + generator without manual configuration.  Even when
    // no kit is selected the preset defaults to the Ninja generator so
    // CMake never falls back to the Visual Studio / MSVC generator.
    writeCMakePresetsJson(projectDir, wizard.selectedKit());

    // Single-project creation: the .exsln lives alongside the project files
    // in the same directory.  No VS-style nesting for the common case (one
    // project = one solution).  Multi-project solutions are built later via
    // "Add Project to Solution" which places each project in its own subdir.

    const QDir projDir(projectDir);
    const QString projectName = projDir.dirName();
    const QString solutionRoot = projDir.absolutePath();

    // Create .exsln solution file alongside project files
    const QString slnPath = solutionRoot + QStringLiteral("/") + projectName + QStringLiteral(".exsln");
    auto *pm = m_editorMgr->projectManager();
    pm->createSolution(projectName, slnPath);
    pm->addProject(projectName, solutionRoot,
                   wizard.selectedLanguage(), wizard.selectedTemplateId());
    pm->saveSolution();

    // Open the project root (which is also the solution root)
    openFolder(solutionRoot);
}

void MainWindow::openSolutionFile()
{
    const QString slnPath = QFileDialog::getOpenFileName(this, tr("Open Solution"),
                                                        m_editorMgr->currentFolder(), tr("Exorcist Solution (*.exsln)"));
    if (slnPath.isEmpty()) {
        return;
    }

    if (m_editorMgr->projectManager()->openSolution(slnPath)) {
        const QString root = m_editorMgr->projectManager()->activeSolutionDir();
        m_gitService->setWorkingDirectory(root);
        if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
            srch->setRootPath(root);
        setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
        statusBar()->showMessage(tr("Solution: %1").arg(QFileInfo(slnPath).fileName()), 4000);

        m_editorMgr->setCurrentFolder(root);
        if (auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService"))))
            ts->setWorkingDirectory(root);
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->startServer(root);
    }
}

void MainWindow::addProjectToSolution()
{
    auto *pm = m_editorMgr->projectManager();
    if (pm->solution().name.isEmpty()) {
        QMessageBox::information(this, tr("No Solution"),
                                 tr("Open or create a solution first."));
        return;
    }

    // Offer two choices: create new project from template, or add existing folder
    QStringList options;
    options << tr("New Project (from template)...")
            << tr("Existing Folder...");

    bool ok = false;
    const QString choice = QInputDialog::getItem(
        this, tr("Add Project to Solution"), tr("Choose:"), options, 0, false, &ok);
    if (!ok)
        return;

    if (choice == options.at(0)) {
        // Create new project via wizard, place inside solution root
        auto *registry = m_services->service<ProjectTemplateRegistry>(
            QStringLiteral("projectTemplateRegistry"));
        if (!registry)
            return;

        auto *kitMgr2 = m_services->service<IKitManager>(QStringLiteral("kitManager"));
        NewProjectWizard wizard(registry, pm->activeSolutionDir(), kitMgr2, this);
        if (wizard.exec() != QDialog::Accepted)
            return;

        const QString projectDir = wizard.createdProjectPath();
        if (projectDir.isEmpty())
            return;

        writeCMakePresetsJson(projectDir, wizard.selectedKit());

        const QString projectName = QDir(projectDir).dirName();
        pm->addProject(projectName, projectDir,
                       wizard.selectedLanguage(), wizard.selectedTemplateId());
        pm->saveSolution();
        // Refresh tree
        emit pm->solutionChanged();
    } else {
        // Add existing folder
        const QString folder = QFileDialog::getExistingDirectory(
            this, tr("Add Folder to Solution"), pm->activeSolutionDir());
        if (folder.isEmpty())
            return;

        const QString projectName = QDir(folder).dirName();
        pm->addProject(projectName, folder);
        if (!pm->solution().filePath.isEmpty())
            pm->saveSolution();
    }
}

void MainWindow::saveCurrentTab()
{
    EditorView *editor = currentEditor();
    if (!editor) return;

    const QString langId = editor->property("languageId").toString();
    auto *lpm = findChild<LanguageProfileManager *>(
        QStringLiteral("languageProfileManager"));

    // Trim trailing whitespace per language profile
    if (lpm && lpm->trimTrailingWhitespace(langId)) {
        QTextCursor cursor(editor->document());
        cursor.beginEditBlock();
        QTextBlock block = editor->document()->begin();
        while (block.isValid()) {
            const QString text = block.text();
            int trailing = text.length();
            while (trailing > 0 && text.at(trailing - 1).isSpace())
                --trailing;
            if (trailing < text.length()) {
                cursor.setPosition(block.position() + trailing);
                cursor.setPosition(block.position() + text.length(),
                                   QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            block = block.next();
        }
        cursor.endEditBlock();
    }

    // Format on save: trigger LSP formatting if language profile requests it,
    // or always if no profile exists (legacy behavior)
    const bool shouldFormat = lpm ? lpm->formatOnSave(langId) : true;
    if (shouldFormat) {
        auto *bridge = editor->findChild<LspEditorBridge *>();
        if (bridge)
            bridge->formatDocument();
    }

    QString path = editor->property("filePath").toString();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save File"));
        if (path.isEmpty()) return;
        editor->setProperty("filePath", path);
    }

    QString error;
    if (!m_fileSystem->writeTextFile(path, editor->toPlainText(), &error)) {
        statusBar()->showMessage(tr("Save failed: %1").arg(error), 5000);
        return;
    }

    const QString title = QFileInfo(path).fileName();
    const int idx = m_editorMgr->tabs()->currentIndex();
    m_editorMgr->tabs()->setTabText(idx, title);
    m_editorMgr->tabs()->setTabToolTip(idx, QDir::toNativeSeparators(path));
    statusBar()->showMessage(tr("Saved %1").arg(title), 3000);
    if (m_pluginManager)
        m_pluginManager->fireLuaEvent(QStringLiteral("editor.save"), {path});
}

// ── Command palette ───────────────────────────────────────────────────────────

void MainWindow::showFilePalette()
{
    QStringList files;
    QDirIterator it(m_editorMgr->currentFolder(), QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isFile())
            files << it.filePath();
    }

    CommandPalette palette(CommandPalette::FileMode, this);
    palette.setFiles(files);

    const QRect geo = geometry();
    palette.move(geo.center().x() - palette.minimumWidth() / 2,
                 geo.top() + 80);

    if (palette.exec() == QDialog::Accepted) {
        const QString selected = palette.selectedFile();
        if (!selected.isEmpty()) openFile(selected);
    }
}

void MainWindow::showCommandPalette()
{
    const QStringList builtInCommands = {
        tr("New Tab\tCtrl+N"),
        tr("Open File...\tCtrl+O"),
        tr("Open Folder...\tCtrl+Shift+O"),
        tr("Save\tCtrl+S"),
        tr("Go to File...\tCtrl+P"),
        tr("Find / Replace\tCtrl+F"),
        tr("Toggle Project Panel"),
        tr("Toggle Search Panel"),
        tr("Toggle Terminal Panel"),
        tr("Toggle AI Panel"),
        tr("Go to Symbol...\tCtrl+T"),
        tr("Quit\tCtrl+Q"),
    };

    // Merge built-in commands with plugin-contributed commands
    QStringList commands = builtInCommands;
    QStringList pluginEntries;
    if (m_contributions)
        pluginEntries = m_contributions->commandPaletteEntries();
    commands.append(pluginEntries);

    CommandPalette palette(CommandPalette::CommandMode, this);
    palette.setCommands(commands);

    const QRect geo = geometry();
    palette.move(geo.center().x() - palette.minimumWidth() / 2,
                 geo.top() + 80);

    if (palette.exec() != QDialog::Accepted) return;

    const int idx = palette.selectedCommandIndex();
    const int builtInCount = builtInCommands.size();

    if (idx < builtInCount) {
        // Built-in command
        switch (idx) {
        case 0:  openNewTab();           break;
        case 1: {
            const QString p = QFileDialog::getOpenFileName(this, tr("Open File"));
            if (!p.isEmpty()) openFile(p);
            break;
        }
        case 2:  openFolder();           break;
        case 3:  saveCurrentTab();       break;
        case 4:  showFilePalette();      break;
        case 5:
            if (auto *e = currentEditor()) e->showFindBar();
            break;
        case 6:  m_dockManager->toggleDock(dock(QStringLiteral("ProjectDock")));  break;
        case 7:  m_dockManager->toggleDock(dock(QStringLiteral("SearchDock")));    break;
        case 8:  m_dockManager->toggleDock(dock(QStringLiteral("TerminalDock"))); break;
        case 9:  m_dockManager->toggleDock(dock(QStringLiteral("AIDock")));            break;
        case 10: showSymbolPalette();    break;
        case 11: close();                break;
        default: break;
        }
    } else if (m_contributions) {
        // Plugin-contributed command
        m_contributions->executeCommandByIndex(idx - builtInCount);
    }
}

void MainWindow::showSymbolPalette()
{
    auto *sidx = dynamic_cast<SymbolIndex *>(m_services->service(QStringLiteral("symbolIndex")));
    if (!sidx) return;

    CommandPalette palette(CommandPalette::CommandMode, this);

    const auto allSyms = sidx->search(QString(), 500);
    QStringList items;
    for (const auto &sym : allSyms) {
        const QString kindStr = sym.kind == WorkspaceSymbol::Class ? QStringLiteral("class")
            : sym.kind == WorkspaceSymbol::Struct ? QStringLiteral("struct")
            : sym.kind == WorkspaceSymbol::Enum ? QStringLiteral("enum")
            : sym.kind == WorkspaceSymbol::Namespace ? QStringLiteral("namespace")
            : QStringLiteral("func");
        items << QStringLiteral("%1  [%2]  %3:%4")
                 .arg(sym.name, kindStr, QFileInfo(sym.filePath).fileName())
                 .arg(sym.line);
    }

    palette.setCommands(items);
    const QRect geo = geometry();
    palette.move(geo.center().x() - palette.minimumWidth() / 2,
                 geo.top() + 80);

    if (palette.exec() != QDialog::Accepted) return;
    const int idx = palette.selectedCommandIndex();
    if (idx < 0 || idx >= allSyms.size()) return;

    const auto &sym = allSyms[idx];
    openFile(sym.filePath);

    if (auto *editor = currentEditor()) {
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, sym.line - 1);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    }
}

void MainWindow::loadSettings()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));

    const QByteArray geometry = s.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    // Welcome page handles project selection — no auto-open.
}

void MainWindow::saveSettings()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.setValue(QStringLiteral("geometry"),    saveGeometry());
    s.setValue(QStringLiteral("lastFolder"),  m_editorMgr->projectManager()->activeSolutionDir());

    // Save DockManager layout as JSON
    if (m_dockManager) {
        const QJsonObject dockState = m_dockManager->saveState();
        s.setValue(QStringLiteral("dockState"),
                   QString::fromUtf8(QJsonDocument(dockState).toJson(QJsonDocument::Compact)));
    }

    // Save open editor tabs per workspace
    const QString wsRoot = m_editorMgr->projectManager()->activeSolutionDir();
    if (!wsRoot.isEmpty() && m_editorMgr->tabs()) {
        const QString wsKey = QStringLiteral("workspace/")
                              + QString::fromUtf8(QCryptographicHash::hash(
                                    wsRoot.toUtf8(), QCryptographicHash::Md5).toHex());
        s.beginGroup(wsKey);

        QJsonArray tabs;
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            QWidget *w = m_editorMgr->tabs()->widget(i);
            if (!w) continue;
            const QString fp = w->property("filePath").toString();
            if (fp.isEmpty()) continue;

            QJsonObject entry;
            entry[QLatin1String("path")] = fp;
            if (auto *ev = qobject_cast<EditorView *>(w)) {
                const QTextCursor tc = ev->textCursor();
                entry[QLatin1String("line")]   = tc.blockNumber();
                entry[QLatin1String("column")] = tc.columnNumber();
                entry[QLatin1String("scroll")] = ev->verticalScrollBar()->value();
            }
            tabs.append(entry);
        }

        s.setValue(QStringLiteral("openTabs"),
                   QString::fromUtf8(QJsonDocument(tabs).toJson(QJsonDocument::Compact)));
        s.setValue(QStringLiteral("activeTab"), m_editorMgr->tabs()->currentIndex());

        // Save breakpoints (gutter markers) for this workspace
        QJsonArray bps;
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *ev = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
            if (!ev) continue;
            const QString fp = ev->property("filePath").toString();
            if (fp.isEmpty()) continue;
            for (int line : ev->breakpointLines()) {
                QJsonObject bp;
                bp[QLatin1String("file")] = fp;
                bp[QLatin1String("line")] = line;
                bps.append(bp);
            }
        }
        s.setValue(QStringLiteral("breakpoints"),
                   QString::fromUtf8(QJsonDocument(bps).toJson(QJsonDocument::Compact)));

        s.endGroup();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_agentPlatform && m_agentPlatform->brainService())
        m_agentPlatform->brainService()->save();

    // Shut down plugins BEFORE window destruction — kills child processes
    // (clangd, GDB, build tools, etc.) so the process can exit cleanly.
    if (m_pluginManager)
        m_pluginManager->shutdownAll();

    QMainWindow::closeEvent(event);

    // Ensure the application quits even if child widgets/processes linger.
    QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    // Keep custom dock sidebars (which are children of MainWindow, not part of
    // QMainWindow's central layout) sized to match the new window geometry.
    if (m_dockManager)
        m_dockManager->repositionSideBars();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Middle-click on tab bar → close that tab
    if (watched == m_editorMgr->tabs()->tabBar() && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton) {
            int idx = m_editorMgr->tabs()->tabBar()->tabAt(me->pos());
            if (idx >= 0) {
                QWidget *widget = m_editorMgr->tabs()->widget(idx);
                m_editorMgr->tabs()->removeTab(idx);
                if (widget) widget->deleteLater();
                return true;
            }
        }
    }

    // Right-click context menu on tab bar
    if (watched == m_editorMgr->tabs()->tabBar() && event->type() == QEvent::ContextMenu) {
        auto *ce = static_cast<QContextMenuEvent *>(event);
        int idx = m_editorMgr->tabs()->tabBar()->tabAt(ce->pos());
        if (idx >= 0)
            showTabContextMenu(idx, ce->globalPos());
        return true;
    }

    return QMainWindow::eventFilter(watched, event);
}

exdock::ExDockWidget *MainWindow::dock(const QString &id) const
{
    return m_dockManager->dockWidget(id);
}

void MainWindow::showTabContextMenu(int tabIndex, const QPoint &globalPos)
{
    QMenu menu(this);

    const QString filePath =
        m_editorMgr->tabs()->widget(tabIndex)->property("filePath").toString();
    const bool hasFile = !filePath.isEmpty();

    // Save
    QAction *saveAct = menu.addAction(tr("Save"), this, [this, tabIndex]() {
        m_editorMgr->tabs()->setCurrentIndex(tabIndex);
        saveCurrentTab();
    });
    saveAct->setShortcut(QKeySequence::Save);
    saveAct->setEnabled(hasFile || m_editorMgr->tabs()->widget(tabIndex) != nullptr);

    menu.addSeparator();

    // Close
    menu.addAction(tr("Close"), this, [this, tabIndex]() {
        m_editorMgr->closeTab(tabIndex);
    });

    // Close All Tabs
    menu.addAction(tr("Close All Tabs"), this, [this]() {
        m_editorMgr->closeAllTabs();
    });

    // Close Other Tabs
    menu.addAction(tr("Close Other Tabs"), this, [this, tabIndex]() {
        m_editorMgr->closeOtherTabs(tabIndex);
    });

    menu.addSeparator();

    // Copy Full Path
    QAction *copyPathAct = menu.addAction(tr("Copy Full Path"), this, [filePath]() {
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(filePath));
    });
    copyPathAct->setEnabled(hasFile);

    // Open Containing Folder
    QAction *openFolderAct = menu.addAction(tr("Open Containing Folder"), this, [filePath]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(filePath).absolutePath()));
    });
    openFolderAct->setEnabled(hasFile);

    menu.exec(globalPos);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

EditorView *MainWindow::currentEditor() const
{
    return m_editorMgr->currentEditor();
}

// ── LSP ───────────────────────────────────────────────────────────────────────

void MainWindow::createLspBridge(EditorView *editor, const QString &path)
{
    auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
    if (!lspClient) {
        qWarning() << "[LSP-DIAG] createLspBridge: lspClient is null, skipping bridge for" << path;
        return;
    }
    qWarning() << "[LSP-DIAG] createLspBridge: creating bridge for" << path 
               << "initialized=" << lspClient->isInitialized();

    // Bridge is parented to the editor — auto-deleted when the tab is closed.
    auto *bridge = new LspEditorBridge(editor, lspClient, path, editor);

    connect(bridge, &LspEditorBridge::navigateToLocation,
            this,   &MainWindow::navigateToLocation);

    // Open #include files: resolve relative to current file, then src/, then project root
    connect(editor, &EditorView::openIncludeRequested,
            this, [this, path](const QString &includePath) {
        const QFileInfo fi(path);
        // Try relative to current file's directory
        const QString relPath = fi.absolutePath() + "/" + includePath;
        if (QFileInfo::exists(relPath)) {
            openFile(QFileInfo(relPath).absoluteFilePath());
            return;
        }
        // Try relative to project root and common subdirectories
        if (!m_editorMgr->currentFolder().isEmpty()) {
            const QStringList searchDirs = {
                m_editorMgr->currentFolder(),
                m_editorMgr->currentFolder() + "/src",
                m_editorMgr->currentFolder() + "/include",
            };
            for (const QString &dir : searchDirs) {
                const QString candidate = dir + "/" + includePath;
                if (QFileInfo::exists(candidate)) {
                    openFile(QFileInfo(candidate).absoluteFilePath());
                    return;
                }
            }
        }
        // Try include paths from compile_commands.json (Qt, system headers, etc.)
        for (const QString &dir : m_editorMgr->includePaths()) {
            const QString candidate = dir + "/" + includePath;
            if (QFileInfo::exists(candidate)) {
                openFile(QFileInfo(candidate).absoluteFilePath());
                return;
            }
        }
        statusBar()->showMessage(tr("Could not find: %1").arg(includePath), 4000);
    });

    connect(bridge, &LspEditorBridge::referencesFound,
            this, [this](const QString &symbol, const QJsonArray &locations) {
        m_referencesPanel->showReferences(symbol, locations);
        m_dockManager->showDock(dock(QStringLiteral("ReferencesDock")), exdock::SideBarArea::Bottom);
    });

    connect(bridge, &LspEditorBridge::workspaceEditReady,
            this, &MainWindow::applyWorkspaceEdit);

    connect(bridge, &LspEditorBridge::symbolsUpdated,
            this, [this, editor](const QString &/*uri*/, const QJsonArray &symbols) {
        if (m_editorMgr->tabs()->currentWidget() == editor) {
            m_symbolPanel->setSymbols(symbols);
            m_editorMgr->breadcrumb()->setSymbols(symbols);
        }
    });
}

void MainWindow::showInlineChat(EditorView *editor,
                                const QString &selectedText,
                                const QString &filePath,
                                const QString &languageId)
{
    if (!m_agentOrchestrator) return;

    // Create once, reuse (parented to this so it's deleted with the window)
    if (!m_inlineChat) {
        m_inlineChat = new InlineChatWidget(m_agentOrchestrator, this);
        m_inlineChat->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);

        connect(m_inlineChat, &InlineChatWidget::dismissed,
                this, [this]() { m_inlineChat->hide(); });

        connect(m_inlineChat, &InlineChatWidget::applyRequested,
                this, [this](const QString &code) {
            if (auto *ed = currentEditor()) {
                QTextCursor c = ed->textCursor();
                if (c.hasSelection())
                    c.insertText(code);
                else {
                    c.movePosition(QTextCursor::StartOfLine);
                    c.insertText(code + '\n');
                }
            }
        });

        connect(m_inlineChat, &InlineChatWidget::applyMultiEditsRequested,
                this, [this](const QList<EditChunk> &chunks) {
            if (auto *ed = currentEditor()) {
                QTextCursor c = ed->textCursor();
                c.beginEditBlock();
                // Apply chunks bottom-up (reversed) to avoid line shifts
                for (int i = chunks.size() - 1; i >= 0; --i) {
                    const auto &ch = chunks[i];
                    const QTextBlock startBlock = ed->document()->findBlockByLineNumber(ch.startLine - 1);
                    const QTextBlock endBlock   = ed->document()->findBlockByLineNumber(ch.endLine - 1);
                    if (!startBlock.isValid()) continue;

                    c.setPosition(startBlock.position());
                    if (endBlock.isValid())
                        c.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
                    else
                        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                    c.insertText(ch.newCode);
                }
                c.endEditBlock();
            }
        });
    }

    m_inlineChat->setContext(selectedText, filePath, languageId);
    m_inlineChat->setFileContent(editor->toPlainText());

    // Position widget in bottom-right corner of the editor, floating above it
    const QPoint editorGlobal = editor->mapToGlobal(QPoint(0, 0));
    const int x = editorGlobal.x() + editor->width()  - m_inlineChat->sizeHint().width()  - 16;
    const int y = editorGlobal.y() + editor->height() - m_inlineChat->sizeHint().height() - 40;
    m_inlineChat->move(x, y);
    m_inlineChat->show();
    m_inlineChat->raise();
    m_inlineChat->focusInput();
}

void MainWindow::navigateToLocation(const QString &path, int line, int character)
{
    openFile(path);   // opens or switches to the existing tab

    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        auto *editor = m_editorMgr->editorAt(i);
        if (!editor || editor->property("filePath").toString() != path)
            continue;

        m_editorMgr->tabs()->setCurrentIndex(i);

        const QTextBlock block = editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cur(block);
            cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                             qMin(character, block.length() - 1));
            editor->setTextCursor(cur);
            editor->centerCursor();
        }
        editor->setFocus();
        break;
    }
}

void MainWindow::onLspInitialized()
{
    qWarning() << "[LSP-DIAG] onLspInitialized() called, tabs=" << m_editorMgr->tabs()->count();
    // Open bridges for any tabs that were already open before LSP was ready.
    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        auto *editor = m_editorMgr->editorAt(i);
        if (!editor) continue;
        const QString path = editor->property("filePath").toString();
        if (path.isEmpty()) continue;
        // Avoid double-bridging (bridge already exists as a child of editor)
        if (editor->findChild<LspEditorBridge *>()) continue;
        createLspBridge(editor, path);
    }
}

void MainWindow::onLspReferencesReady(const QJsonArray &locations)
{
    if (m_referencesPanel)
        m_referencesPanel->showReferences(tr("Symbol"), locations);
    if (auto *d = dock(QStringLiteral("ReferencesDock")))
        m_dockManager->showDock(d, exdock::SideBarArea::Bottom);
}

void MainWindow::onLspStatusMessage(const QString &msg, int timeoutMs)
{
    if (statusBar())
        statusBar()->showMessage(msg, timeoutMs);
}

void MainWindow::applyWorkspaceEdit(const QJsonObject &workspaceEdit)
{
    auto applyEdits = [this](const QString &filePath, const QJsonArray &edits) {
        if (filePath.isEmpty() || edits.isEmpty()) return;
        openFile(filePath);
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *editor = m_editorMgr->editorAt(i);
            if (!editor || editor->property("filePath").toString() != filePath) continue;
            editor->applyTextEdits(edits);
            break;
        }
    };

    if (workspaceEdit.contains("changes")) {
        const QJsonObject changes = workspaceEdit["changes"].toObject();
        for (auto it = changes.begin(); it != changes.end(); ++it)
            applyEdits(QUrl(it.key()).toLocalFile(), it.value().toArray());
    } else if (workspaceEdit.contains("documentChanges")) {
        for (const QJsonValue &v : workspaceEdit["documentChanges"].toArray()) {
            const QJsonObject dc = v.toObject();
            const QString uri = dc["textDocument"].toObject()["uri"].toString();
            applyEdits(QUrl(uri).toLocalFile(), dc["edits"].toArray());
        }
    }
}

// ── Push build diagnostics to open editors ────────────────────────────────────

void MainWindow::pushBuildDiagnostics()
{
    auto *outPanel = qobject_cast<OutputPanel *>(m_services->service(QStringLiteral("outputPanel")));
    if (!outPanel) return;

    const auto problems = outPanel->problems();

    // Group problems by file path
    QHash<QString, QJsonArray> diagsByFile;
    for (const OutputPanel::ProblemMatch &p : problems) {
        QJsonObject range;
        QJsonObject start;
        start[QLatin1String("line")]      = qMax(0, p.line - 1); // LSP uses 0-based lines
        start[QLatin1String("character")] = qMax(0, p.column - 1);
        QJsonObject end;
        end[QLatin1String("line")]        = qMax(0, p.line - 1);
        end[QLatin1String("character")]   = 1000; // underline to end of line
        range[QLatin1String("start")] = start;
        range[QLatin1String("end")]   = end;

        int severity = 1; // Error
        if (p.severity == OutputPanel::ProblemMatch::Warning) severity = 2;
        else if (p.severity == OutputPanel::ProblemMatch::Info) severity = 3;

        QJsonObject diag;
        diag[QLatin1String("range")]    = range;
        diag[QLatin1String("severity")] = severity;
        diag[QLatin1String("source")]   = QStringLiteral("build");
        diag[QLatin1String("message")]  = p.message;

        diagsByFile[p.file].append(diag);
    }

    // Push to each open editor tab that has build problems
    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        auto *editor = m_editorMgr->editorAt(i);
        if (!editor) continue;
        const QString filePath = editor->property("filePath").toString();
        if (filePath.isEmpty()) continue;

        auto it = diagsByFile.constFind(filePath);
        if (it != diagsByFile.constEnd())
            editor->setDiagnostics(it.value());
    }
}

// ── File tree context menu ────────────────────────────────────────────────────

void MainWindow::onTreeContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_editorMgr->fileTree()->indexAt(pos);
    const QString path = index.isValid()
        ? m_editorMgr->treeModel()->data(index, SolutionTreeModel::FilePathRole).toString()
        : QString();
    const bool isDir = index.isValid() && m_editorMgr->treeModel()->isDir(index);
    const bool isFile = index.isValid() && !isDir && !path.isEmpty();
    const QString dirPath = isFile ? QFileInfo(path).absolutePath()
                          : (isDir && !path.isEmpty() ? path : m_editorMgr->currentFolder());

    QMenu menu(this);

    // ── New File / New Folder ─────────────────────────────────────────────
    QAction *newFileAction = menu.addAction(tr("New File..."));
    connect(newFileAction, &QAction::triggered, this, [this, dirPath]() {
        if (dirPath.isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("New File"), tr("File name:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const QString filePath = dirPath + "/" + name.trimmed();
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.close();
            m_editorMgr->treeModel()->rebuildFromSolution();
            openFile(filePath);
        }
    });

    QAction *newFolderAction = menu.addAction(tr("New Folder..."));
    connect(newFolderAction, &QAction::triggered, this, [this, dirPath]() {
        if (dirPath.isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("New Folder"), tr("Folder name:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        QDir(dirPath).mkdir(name.trimmed());
        m_editorMgr->treeModel()->rebuildFromSolution();
    });

    menu.addSeparator();

    // ── Rename ────────────────────────────────────────────────────────────
    QAction *renameAction = menu.addAction(tr("Rename..."));
    renameAction->setEnabled(!path.isEmpty());
    connect(renameAction, &QAction::triggered, this, [this, path, isFile]() {
        const QFileInfo fi(path);
        bool ok = false;
        const QString newName = QInputDialog::getText(
            this, tr("Rename"), tr("New name:"),
            QLineEdit::Normal, fi.fileName(), &ok);
        if (!ok || newName.trimmed().isEmpty() || newName == fi.fileName())
            return;
        const QString newPath = fi.absolutePath() + "/" + newName.trimmed();
        if (QFile::rename(path, newPath)) {
            m_editorMgr->treeModel()->rebuildFromSolution();
            // Update any open tab pointing to the old path
            if (isFile) {
                for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                    if (m_editorMgr->tabs()->widget(i)->property("filePath").toString() == path) {
                        m_editorMgr->tabs()->widget(i)->setProperty("filePath", newPath);
                        m_editorMgr->tabs()->setTabText(i, QFileInfo(newPath).fileName());
                        break;
                    }
                }
            }
        }
    });

    // ── Delete ────────────────────────────────────────────────────────────
    QAction *deleteAction = menu.addAction(tr("Delete"));
    deleteAction->setEnabled(!path.isEmpty());
    connect(deleteAction, &QAction::triggered, this, [this, path, isDir]() {
        const QString msg = isDir
            ? tr("Delete folder \"%1\" and all its contents?").arg(QFileInfo(path).fileName())
            : tr("Delete \"%1\"?").arg(QFileInfo(path).fileName());
        if (QMessageBox::question(this, tr("Delete"), msg) != QMessageBox::Yes)
            return;
        bool ok = false;
        if (isDir) {
            ok = QDir(path).removeRecursively();
        } else {
            ok = QFile::remove(path);
        }
        if (ok) m_editorMgr->treeModel()->rebuildFromSolution();
    });

    menu.addSeparator();

    // ── Copy actions ──────────────────────────────────────────────────────
    QAction *copyNameAction = menu.addAction(tr("Copy Name"));
    copyNameAction->setEnabled(!path.isEmpty());
    connect(copyNameAction, &QAction::triggered, this, [path]() {
        QGuiApplication::clipboard()->setText(QFileInfo(path).fileName());
    });

    QAction *copyPathAction = menu.addAction(tr("Copy Path"));
    copyPathAction->setEnabled(!path.isEmpty());
    connect(copyPathAction, &QAction::triggered, this, [path]() {
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(path));
    });

    QAction *copyRelativeAction = menu.addAction(tr("Copy Relative Path"));
    copyRelativeAction->setEnabled(!path.isEmpty() && !m_editorMgr->currentFolder().isEmpty());
    connect(copyRelativeAction, &QAction::triggered, this, [this, path]() {
        QDir root(m_editorMgr->currentFolder());
        QGuiApplication::clipboard()->setText(root.relativeFilePath(path));
    });

    menu.addSeparator();

    // ── Reveal in file manager ────────────────────────────────────────────
    QAction *revealAction = menu.addAction(tr("Reveal in File Manager"));
    revealAction->setEnabled(!path.isEmpty());
    connect(revealAction, &QAction::triggered, this, [path, isDir]() {
        const QString target = isDir ? path : QFileInfo(path).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    });

    // ── Open in terminal ──────────────────────────────────────────────────
    QAction *terminalAction = menu.addAction(tr("Open in Terminal"));
    terminalAction->setEnabled(!dirPath.isEmpty());
    connect(terminalAction, &QAction::triggered, this, [this, dirPath]() {
        m_dockManager->showDock(dock(QStringLiteral("TerminalDock")), exdock::SideBarArea::Bottom);
        if (auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService"))))
            ts->setWorkingDirectory(dirPath);
    });

    menu.addSeparator();

    // ── Refresh ───────────────────────────────────────────────────────────
    QAction *refreshAction = menu.addAction(tr("Refresh"));
    connect(refreshAction, &QAction::triggered, this, [this, index]() {
        if (index.isValid() && m_editorMgr->treeModel()->isDir(index))
            m_editorMgr->treeModel()->refreshDirectory(index);
        else
            m_editorMgr->treeModel()->rebuildFromSolution();
    });

    menu.exec(m_editorMgr->fileTree()->viewport()->mapToGlobal(pos));
}

void MainWindow::loadPlugins()
{
    m_pluginManager = std::make_unique<PluginManager>();

    // Kill child processes (clangd, etc.) when the event loop stops.
    // aboutToQuit fires after all windows are destroyed, so LspEditorBridges
    // are already gone — no dangling-pointer risk during plugin shutdown.
    PluginManager *pm = m_pluginManager.get();
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
            pm, [pm]() { pm->shutdownAll(); });

    const QString pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
    int loaded = m_pluginManager->loadPluginsFrom(pluginDir);

    // Wire plugin gallery panel (lazy — created on demand)
    {
        auto *pg = m_dockBootstrap->pluginGallery();
        pg->setPluginManager(m_pluginManager.get());

        // Marketplace service — owned by gallery panel (no MainWindow member)
        auto *marketplace = new PluginMarketplaceService(pg);
        marketplace->setPluginManager(m_pluginManager.get());
        marketplace->setPluginsDirectory(pluginDir);
        m_services->registerService(QStringLiteral("pluginMarketplace"), marketplace);

        // Load bundled registry if present
        const QString registryPath = QCoreApplication::applicationDirPath()
                                     + QLatin1String("/plugin_registry.json");
        if (QFile::exists(registryPath)) {
            marketplace->loadRegistryFromFile(registryPath);
            m_dockBootstrap->pluginGallery()->loadRegistryFromFile(registryPath);
        }

        // Wire install button → download & install
        connect(pg, &PluginGalleryPanel::installRequested,
                marketplace, [marketplace, this](const QString &pluginId, const QString &downloadUrl) {
            for (const auto &entry : marketplace->entries()) {
                if (entry.id == pluginId) {
                    marketplace->downloadAndInstall(entry);
                    return;
                }
            }
            // Fallback: construct minimal entry from signal data
            MarketplaceEntry e;
            e.id = pluginId;
            e.downloadUrl = downloadUrl;
            marketplace->downloadAndInstall(e);
        });

        // Refresh gallery when a plugin is installed
        connect(marketplace, &PluginMarketplaceService::pluginInstalled,
                pg, &PluginGalleryPanel::refreshInstalled);
    }

    // Create SDK host services for the new plugin interface
    m_hostServices = new HostServices(this, this);
    m_hostServices->initSubsystemServices(m_fileSystem.get(), m_gitService);
    m_hostServices->initUIManagers();
    m_hostServices->setServiceRegistry(m_services.get());

    {
        auto *sharedServices = new SharedServicesBootstrap(this);
        sharedServices->initialize(m_services.get(), m_hostServices, m_pluginManager.get());

        auto *templateServices = new TemplateServicesBootstrap(this);
        templateServices->initialize(m_services.get());

        auto *wss = m_services->service<WorkspaceSettings>(QStringLiteral("workspaceSettings"));
        auto *profileMgr = qobject_cast<ProfileManager *>(
            m_services->service(QStringLiteral("profileManager")));

        auto *workbenchState = new WorkbenchStateBootstrap(this);
        WorkbenchStateBootstrap::Callbacks callbacks;
        callbacks.applyWorkspaceSettings = [this](WorkspaceSettings *settings) {
            if (!settings)
                return;

            const QFont font(settings->fontFamily(), settings->fontSize());
            const int tabSize = settings->tabSize();
            const bool wrap = settings->wordWrap();
            const bool minimap = settings->showMinimap();

            for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                if (auto *e = m_editorMgr->editorAt(i)) {
                    e->setFont(font);
                    e->setTabStopDistance(
                        QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * tabSize);
                    e->setWordWrapMode(wrap ? QTextOption::WordWrap
                                            : QTextOption::NoWrap);
                    e->setMinimapVisible(minimap);
                }
            }
        };
        callbacks.setDockVisible = [this](const QString &dockId, bool visible) {
            setShellDockVisible(dockId, visible);
        };
        workbenchState->initialize(wss, profileMgr, callbacks);
    }

    // LSP diagnostics → SDK DiagnosticsService: deferred to post-plugin wiring

    {
        auto *workbenchCommands = new WorkbenchCommandBootstrap(this);
        WorkbenchCommandBootstrap::Callbacks callbacks;
        callbacks.newTab = [this]() { openNewTab(); };
        callbacks.openFile = [this]() {
            const QString p = QFileDialog::getOpenFileName(this, tr("Open File"));
            if (!p.isEmpty())
                openFile(p);
        };
        callbacks.openFolder = [this]() { openFolder(); };
        callbacks.save = [this]() { saveCurrentTab(); };
        callbacks.goToFile = [this]() { showFilePalette(); };
        callbacks.findReplace = [this]() {
            if (auto *e = currentEditor())
                e->showFindBar();
        };
        callbacks.toggleProject = [this]() {
            m_dockManager->toggleDock(dock(QStringLiteral("ProjectDock")));
        };
        callbacks.toggleSearch = [this]() {
            m_dockManager->toggleDock(dock(QStringLiteral("SearchDock")));
        };
        callbacks.toggleTerminal = [this]() {
            m_dockManager->toggleDock(dock(QStringLiteral("TerminalDock")));
        };
        callbacks.toggleAi = [this]() {
            m_dockManager->toggleDock(dock(QStringLiteral("AIDock")));
        };
        callbacks.goToSymbol = [this]() { showSymbolPalette(); };
        callbacks.quit = [this]() { close(); };
        callbacks.commandPalette = [this]() { showCommandPalette(); };
        workbenchCommands->initialize(m_hostServices->commandService(), callbacks);
    }

    // Create contribution registry for wiring plugin manifests into the IDE
    m_contributions = new ContributionRegistry(this, m_hostServices->commandService(), this);

    // Give PluginManager access to ContributionRegistry for suspend/resume
    m_pluginManager->setContributionRegistry(m_contributions);

    // ── Global shortcut dispatcher ───────────────────────────────────────
    //
    // Plugins attach hot-keys to QActions inside docks/menus, but those
    // shortcuts often don't fire when focus is on a non-window child widget
    // (chat panel, sidebars, dock contents) because the focused widget
    // consumes the key event before Qt's shortcut machinery walks up.
    //
    // The dispatcher installs a QApplication-level event filter, maps key
    // sequences -> command ids, and routes through ICommandService —
    // independent of focus. Plugins keep their existing QAction shortcuts;
    // when the dispatcher consumes a matching event, the QAction simply
    // doesn't fire (no double-dispatch).
    {
        auto *globalDispatch = new GlobalShortcutDispatcher(this);
        globalDispatch->setCommandService(m_hostServices->commandService());

        // Build / Run / Debug
        globalDispatch->registerShortcut(QKeySequence(Qt::Key_F5),
                                         QStringLiteral("build.debug"));
        globalDispatch->registerShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5),
                                         QStringLiteral("build.stop"));

        // Debug stepping (works regardless of which dock has focus)
        globalDispatch->registerShortcut(QKeySequence(Qt::Key_F10),
                                         QStringLiteral("debug.stepOver"));
        globalDispatch->registerShortcut(QKeySequence(Qt::Key_F11),
                                         QStringLiteral("debug.stepInto"));
        globalDispatch->registerShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11),
                                         QStringLiteral("debug.stepOut"));

        // Plugin-supplied formatters / refactors
        globalDispatch->registerShortcut(
            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F),
            QStringLiteral("cpp.formatDocument"));

        // Note: Ctrl+T (go-to-symbol) and Ctrl+G (goto-line) are wired
        // through MainWindow Edit-menu lambdas without command ids, so we
        // intentionally don't bind them here to avoid silent no-ops. If
        // those get promoted to plugin commands later, register them here.
    }

    // Set active language profiles before plugin initialization.
    // Language-specific plugins only load when their profile is enabled.
    if (auto *lpm = findChild<LanguageProfileManager *>(
            QStringLiteral("languageProfileManager"))) {
        m_pluginManager->setActiveLanguageProfiles(lpm->activeLanguageIds());
    }

    // Initialize plugins: SDK v1 first, then legacy
    m_pluginManager->initializeAll(static_cast<IHostServices *>(m_hostServices));
    m_pluginManager->initializeAll(static_cast<QObject *>(m_services.get()));

    // Load Lua script plugins from plugins/lua/ directory
    const QString luaPluginDir = QCoreApplication::applicationDirPath() + "/plugins/lua";
    m_pluginManager->loadLuaPluginsFrom(luaPluginDir, m_hostServices);

    // Process plugin manifests → wire contributions into the IDE.
    // Skip deferred plugins: they haven't been initialized yet (createView()
    // members are null), and their manifests will be registered by
    // activateByWorkspace() / activateByEvent() once they're actually activated.
    for (const auto &lp : m_pluginManager->loadedPlugins()) {
        if (lp.manifest.hasContributions()
                && !m_pluginManager->isPluginDeferred(lp.instance)) {
            m_contributions->registerManifest(
                lp.manifest.id.isEmpty() ? lp.instance->info().id : lp.manifest.id,
                lp.manifest, lp.instance);
        }
    }

    // Wire plugin-registered languages into the highlighter factory
    HighlighterFactory::setLanguageLookup(
        [reg = m_contributions](const QString &ext) -> QString {
            const auto *lc = reg->languageForExtension(ext);
            return lc ? lc->id : QString();
        });

    // ── Post-plugin wiring (delegated to PostPluginBootstrap) ──
    // NOTE: bare `new` required — Qt parent (this) manages lifetime;
    //       connections + QTimer::singleShot must outlive this scope.
    {
        auto *postPlugin = new PostPluginBootstrap(this);  // NOLINT(cppcoreguidelines-owning-memory)
        PostPluginBootstrap::Deps deps;
        deps.services          = m_services.get();
        deps.hostServices      = m_hostServices;
        deps.agentPlatform     = m_agentPlatform;
        deps.agentOrchestrator = m_agentOrchestrator;
        deps.inlineEngine      = m_inlineEngine;
        deps.nesEngine         = m_nesEngine;
        deps.statusBarMgr      = m_statusBarMgr;
        deps.editorMgr         = m_editorMgr;
        deps.pluginManager     = m_pluginManager.get();
        postPlugin->wire(deps);

        // PostPluginBootstrap emits navigateToSource → MainWindow handles it
        connect(postPlugin, &PostPluginBootstrap::navigateToSource,
                this, &MainWindow::navigateToLocation);

        // LSP workspace edits must still go through MainWindow (owns editor tabs)
        if (auto *lspSvc = m_services->service<ILspService>(
                QStringLiteral("lspService"))) {
            connect(lspSvc, &ILspService::workspaceEditRequested,
                    this, &MainWindow::applyWorkspaceEdit);
        }

        // LSP initialized → create bridges for open tabs
        if (auto *lspClient = m_services->service<LspClient>(
                QStringLiteral("lspClient"))) {
            connect(lspClient, &LspClient::initialized,
                    this, &MainWindow::onLspInitialized);
        }

        // References panel → show dock + populate
        if (auto *lspSvc = m_services->service<ILspService>(
                QStringLiteral("lspService"))) {
            connect(lspSvc, &ILspService::referencesReady,
                    this, [this](const QJsonArray &locations) {
                if (m_referencesPanel)
                    m_referencesPanel->showReferences(tr("Symbol"), locations);
                if (dock(QStringLiteral("ReferencesDock")))
                    m_dockManager->showDock(dock(QStringLiteral("ReferencesDock")),
                                            exdock::SideBarArea::Bottom);
            });
        }

        // LSP status messages → main status bar
        if (auto *lspSvc = m_services->service<ILspService>(
                QStringLiteral("lspService"))) {
            connect(lspSvc, &ILspService::statusMessage,
                    this, [this](const QString &msg, int timeout) {
                statusBar()->showMessage(msg, timeout);
            });
        }
    }
    // Populate tool toggles in settings panel
    if (m_settingsPanel) {
        QStringList toolNames = m_agentPlatform->toolRegistry()->toolNames();
        toolNames.sort();
        m_settingsPanel->setToolNames(toolNames);
    }

    if (loaded > 0) {
        statusBar()->showMessage(tr("Loaded %1 plugins").arg(loaded), 4000);
    }

    const bool editing = !m_centralStack || m_centralStack->currentIndex() != 0;
    for (auto *tb : findChildren<QToolBar *>())
        tb->setVisible(editing);
    updateMenuBarVisibility(editing);
    QTimer::singleShot(0, this, [this]() {
        const bool editingNow = !m_centralStack || m_centralStack->currentIndex() != 0;
        for (auto *tb : findChildren<QToolBar *>())
            tb->setVisible(editingNow);
        updateMenuBarVisibility(editingNow);
    });
}
