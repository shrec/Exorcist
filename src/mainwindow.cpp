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
#include "sdk/kit.h"
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
#include "component/outputpanel.h"
#include "problems/problemspanel.h"
#include "settings/workspacesettings.h"
#include "settings/scopedsettings.h"
#include "settings/languageprofile.h"
// DiffExplorerPanel + MergeEditor are owned by plugins/git/ (Rule 3).
// MainWindow no longer needs to include their headers — the panels are
// constructed and registered as docks by GitPlugin::initializePlugin().
#include "component/runlaunchpanel.h"
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
// WelcomeWidget moved to plugins/start-page/ (rule L3).  MainWindow no
// longer references its concrete type — it queries the "welcomeWidget"
// service to obtain the QWidget* during deferredInit.
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

    // ── Wire the start-page plugin's WelcomeWidget into the central stack ──
    // The plugin (plugins/start-page/) constructed the widget during its
    // initializePlugin() and registered it under "welcomeWidget".  Swap
    // it into m_centralStack page 0, replacing the empty placeholder.
    if (auto *welcome = m_services->service<QWidget>(QStringLiteral("welcomeWidget"))) {
        if (m_centralStack && m_centralStack->count() > 0) {
            QWidget *placeholder = m_centralStack->widget(0);
            if (placeholder && placeholder->objectName() == QStringLiteral("welcomePlaceholder")) {
                m_centralStack->removeWidget(placeholder);
                placeholder->deleteLater();
            }
            m_centralStack->insertWidget(0, welcome);
            // Keep current index — if we were already on the welcome page,
            // the new widget is now visible at the same slot.
            if (!m_centralStack->currentWidget())
                m_centralStack->setCurrentIndex(0);
        }
        m_welcome = welcome;
        // Refresh the recent list now that plugins have loaded.
        QMetaObject::invokeMethod(welcome, "refreshRecent");
    }

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

        // Breakpoint gutter → debug adapter is wired by the debug plugin
        // (rule L1, L5).  MainWindow no longer owns this signal route.

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
