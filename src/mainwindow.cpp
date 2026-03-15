#include "mainwindow.h"

#include "editor/codefoldingengine.h"
#include "startupprofiler.h"

#include <QAction>
#include <QDialog>
#include <QDirIterator>
#include <QDir>
#include <QFile>
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
#include <QCoreApplication>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeView>
#include <QUuid>
#include <QInputDialog>
#include <QClipboard>
#include <QDesktopServices>
#include <QMessageBox>
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
#include "thememanager.h"
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
#include "ui/keymapmanager.h"
#include "ui/keymapdialog.h"
#include "ui/dock/DockManager.h"
#include "ui/dock/ExDockWidget.h"
#include "pluginmanager.h"
#include "serviceregistry.h"
#include "core/qtfilesystem.h"
#include "core/qtprocess.h"
#include "search/searchpanel.h"
#include "search/searchservice.h"
#include "search/workspaceindexer.h"
#include "search/symbolindex.h"
#include "lsp/lspclient.h"
#include "lsp/lspeditorbridge.h"
#include "lsp/referencespanel.h"
#include "sdk/ilspservice.h"
#include "sdk/ibuildsystem.h"
#include "sdk/ilaunchservice.h"
#include "bootstrap/bridgebootstrap.h"
#include "process/bridgeclient.h"
#include "bootstrap/statusbarmanager.h"
#include "bootstrap/aiservicesbootstrap.h"
#include "editor/symboloutlinepanel.h"
#include "terminal/terminalpanel.h"
#include "project/projectmanager.h"
#include "project/solutiontreemodel.h"
#include "git/gitservice.h"
#include "git/gitpanel.h"
#include "build/outputpanel.h"
#include "problems/problemspanel.h"
#include "settings/workspacesettings.h"
#include "settings/languageprofile.h"
#include "git/diffexplorerpanel.h"
#include "git/mergeeditor.h"
#include "build/runlaunchpanel.h"
#include "editor/filewatchservice.h"
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
#include "search/workspacechunkindex.h"
#include "agent/modelconfigwidget.h"
#include "agent/contextinspector.h"
#include "agent/toolcalltrace.h"
#include "agent/trajectoryreplaywidget.h"
#include "agent/promptarchiveexporter.h"
#include "agent/memoryfileeditor.h"
#include "agent/backgroundcompactor.h"
#include "agent/errorstatewidget.h"
#include "agent/setuptestswizard.h"
#include "agent/renameintegration.h"
#include "agent/accessibilityhelper.h"
#include "agent/tooltogglemanager.h"
#include "agent/contextscopeconfig.h"
#include "search/embeddingindex.h"
#include "search/gitignorefilter.h"
#include "search/regexsearchengine.h"
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
      m_searchPanel(nullptr),
      m_gitService(nullptr),
      m_gitPanel(nullptr),
      m_searchService(nullptr),
      m_agentOrchestrator(nullptr),
      m_chatPanel(nullptr),
    m_promptResolver(nullptr),
      m_referencesPanel(nullptr),
      m_symbolPanel(nullptr),
      m_terminal(nullptr),
      m_inlineEngine(nullptr)
{
    setWindowTitle(tr("Exorcist"));

    // EditorManager owns tab/project/tree state
    m_editorMgr = new EditorManager(this);

    // Services that dock widgets depend on must exist before setupUi().
    m_searchService     = new SearchService(this);
    m_agentOrchestrator = new AgentOrchestrator(this);
    m_editorMgr->setProjectManager(new ProjectManager(this));
    m_gitService        = new GitService(this);

    // ServiceRegistry must exist before setupUi (createDockWidgets uses it).
    m_services = std::make_unique<ServiceRegistry>();

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

    // ── Keymap Manager ────────────────────────────────────────────────────
    m_keymapManager = new KeymapManager(this);
    // Auto-register all actions that have shortcuts
    for (QAction *action : actions()) {
        if (!action->shortcut().isEmpty()) {
            QString id = action->text().remove(QLatin1Char('&')).trimmed();
            if (!id.isEmpty())
                m_keymapManager->registerAction(id, action, action->shortcut());
        }
    }
    // Also register menu actions
    for (QAction *menuAction : menuBar()->actions()) {
        if (auto *menu = menuAction->menu()) {
            for (QAction *action : menu->actions()) {
                if (!action->shortcut().isEmpty()) {
                    QString id = action->text().remove(QLatin1Char('&')).trimmed();
                    if (!id.isEmpty() && !m_keymapManager->actionIds().contains(id))
                        m_keymapManager->registerAction(id, action, action->shortcut());
                }
            }
        }
    }
    m_keymapManager->load();  // Apply saved overrides

    m_services->registerService("mainwindow", this);
    m_services->registerService("agentOrchestrator", m_agentOrchestrator);
    if (m_aiServices && m_aiServices->keyStorage())
        m_services->registerService(QStringLiteral("secureKeyStorage"), m_aiServices->keyStorage());

    // ── Build System — registered by build plugin (plugins/build/) ──────
    // Debug adapter — registered by debug plugin (plugins/debug/)

    // ── Workspace settings (global → workspace hierarchy) ───────────────
    {
        auto *wss = new WorkspaceSettings(this);
        m_services->registerService(QStringLiteral("workspaceSettings"), wss);

        // When workspace settings change, apply to all open editor tabs
        connect(wss, &WorkspaceSettings::settingsChanged, this, [this, wss]() {
            const QFont font(wss->fontFamily(), wss->fontSize());
            const int tabSize = wss->tabSize();
            const bool wrap = wss->wordWrap();
            const bool minimap = wss->showMinimap();

            for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i))) {
                    e->setFont(font);
                    e->setTabStopDistance(
                        QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * tabSize);
                    e->setWordWrapMode(wrap ? QTextOption::WordWrap
                                           : QTextOption::NoWrap);
                    e->setMinimapVisible(minimap);
                }
            }
        });
    }

    m_fileSystem = std::make_unique<QtFileSystem>();

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

    AgentPlatformBootstrap::Callbacks agentCallbacks;

    // ── Basic context getters (already working) ───────────────────────────
    agentCallbacks.openFilesGetter = [this]() {
        QStringList paths;
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *ed = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
            const QString fp = ed ? ed->property("filePath").toString() : QString();
            if (!fp.isEmpty())
                paths.append(fp);
        }
        return paths;
    };

    agentCallbacks.gitStatusGetter = [this]() -> QString {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        QString result = QStringLiteral("Branch: %1\n").arg(m_gitService->currentBranch());
        const auto statuses = m_gitService->fileStatuses();
        for (auto it = statuses.begin(); it != statuses.end(); ++it)
            result += QStringLiteral("  %1 %2\n").arg(it.value(), it.key());
        return result;
    };

    agentCallbacks.terminalOutputGetter = [this]() -> QString {
        if (!m_terminal) return {};
        return m_terminal->recentOutput(80);
    };

    agentCallbacks.diagnosticsGetter = [this]() -> QList<AgentDiagnostic> {
        auto *ed = currentEditor();
        if (!ed) return {};
        QList<AgentDiagnostic> result;
        const QString path = ed->property("filePath").toString();
        for (const DiagnosticMark &d : ed->diagnosticMarks()) {
            AgentDiagnostic ag;
            ag.filePath = path;
            ag.line     = d.line;
            ag.column   = d.startChar;
            ag.message  = d.message;
            ag.severity = (d.severity == DiagSeverity::Error)
                ? AgentDiagnostic::Severity::Error
                : (d.severity == DiagSeverity::Warning)
                    ? AgentDiagnostic::Severity::Warning
                    : AgentDiagnostic::Severity::Info;
            result.append(ag);
        }
        return result;
    };

    agentCallbacks.changedFilesGetter = [this]() -> QHash<QString, QString> {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        return m_gitService->fileStatuses();
    };

    agentCallbacks.gitDiffGetter = [this](const QString &filePath) -> QString {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        return m_gitService->diff(filePath);
    };

    // ── Debug adapter callbacks ───────────────────────────────────────────
    agentCallbacks.debugBreakpointSetter =
        [this](const QString &filePath, int line, const QString &condition) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter) return {};
        DebugBreakpoint bp;
        bp.filePath  = filePath;
        bp.line      = line;
        bp.condition = condition;
        bp.enabled   = true;
        adapter->addBreakpoint(bp);
        return QStringLiteral("Breakpoint set at %1:%2").arg(filePath).arg(line);
    };

    agentCallbacks.debugBreakpointRemover =
        [this](const QString &filePath, int line) -> bool {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter) return false;
        // Find breakpoint ID by file:line and remove it
        Q_UNUSED(filePath)
        adapter->removeBreakpoint(line);
        return true;
    };

    agentCallbacks.debugStackGetter = [this](int threadId) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return {};
        QString result;
        QEventLoop loop;
        auto conn = connect(adapter, &IDebugAdapter::stackTraceReceived,
            &loop, [&](int tid, const QList<DebugFrame> &frames) {
                if (tid != threadId) return;
                for (const auto &f : frames)
                    result += QStringLiteral("#%1 %2 at %3:%4\n")
                        .arg(f.id).arg(f.name, f.filePath).arg(f.line);
                loop.quit();
            });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        adapter->requestStackTrace(threadId);
        loop.exec();
        disconnect(conn);
        return result;
    };

    agentCallbacks.debugVariablesGetter = [this](int variablesRef) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return {};
        QString result;
        QEventLoop loop;
        auto conn = connect(adapter, &IDebugAdapter::variablesReceived,
            &loop, [&](int ref, const QList<DebugVariable> &vars) {
                if (ref != variablesRef) return;
                for (const auto &v : vars)
                    result += QStringLiteral("%1 %2 = %3\n")
                        .arg(v.type, v.name, v.value);
                loop.quit();
            });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        adapter->requestVariables(variablesRef);
        loop.exec();
        disconnect(conn);
        return result;
    };

    agentCallbacks.debugEvaluator = [this](const QString &expression, int frameId) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return {};
        QString result;
        QEventLoop loop;
        auto conn = connect(adapter, &IDebugAdapter::evaluateResult,
            &loop, [&](const QString &expr, const QString &val) {
                if (expr == expression) {
                    result = val;
                    loop.quit();
                }
            });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        adapter->evaluate(expression, frameId);
        loop.exec();
        disconnect(conn);
        return result;
    };

    agentCallbacks.debugStepper = [this](const QString &action) -> bool {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return false;
        if (action == QLatin1String("over"))     adapter->stepOver();
        else if (action == QLatin1String("into")) adapter->stepInto();
        else if (action == QLatin1String("out"))  adapter->stepOut();
        else if (action == QLatin1String("continue")) adapter->continueExecution();
        else return false;
        return true;
    };

    // ── Screenshot ────────────────────────────────────────────────────────
    agentCallbacks.widgetGrabber = [this](const QString &target) -> QPixmap {
        if (target == QLatin1String("window"))
            return grab();
        if (target == QLatin1String("editor")) {
            auto *ed = currentEditor();
            return ed ? ed->grab() : QPixmap();
        }
        if (target == QLatin1String("terminal"))
            return m_terminal ? m_terminal->grab() : QPixmap();
        if (target == QLatin1String("debug")) {
            auto *d = dock(QStringLiteral("DebugDock"));
            return d ? d->grab() : QPixmap();
        }
        if (target == QLatin1String("agent") || target == QLatin1String("chat"))
            return m_chatPanel ? m_chatPanel->grab() : QPixmap();
        if (target == QLatin1String("search"))
            return m_searchPanel ? m_searchPanel->grab() : QPixmap();
        if (target == QLatin1String("git"))
            return m_gitPanel ? m_gitPanel->grab() : QPixmap();
        return grab();
    };

    // ── Introspection ─────────────────────────────────────────────────────
    agentCallbacks.introspectionHandler = [this](const QString &query) -> QString {
        if (query == QLatin1String("services") && m_services)
            return m_services->keys().join(QLatin1Char('\n'));
        if (query == QLatin1String("plugins") && m_pluginManager) {
            QStringList names;
            for (const auto &lp : m_pluginManager->loadedPlugins())
                names << lp.manifest.name;
            return names.join(QLatin1Char('\n'));
        }
        if (query == QLatin1String("tools") && m_agentPlatform && m_agentPlatform->toolRegistry())
            return m_agentPlatform->toolRegistry()->availableToolNames().join(QLatin1Char('\n'));
        if (query == QLatin1String("config"))
            return QStringLiteral("Workspace: %1\nFolder: %2")
                .arg(QCoreApplication::applicationDirPath(), m_editorMgr->currentFolder());
        return QStringLiteral("Unknown query: %1. Try: services, plugins, tools, config")
            .arg(query);
    };

    // (LuaJIT executor is wired below, before initialize() call)

    // ── Code graph / intelligence ─────────────────────────────────────────
    agentCallbacks.symbolSearchFn = [this](const QString &query, int maxResults) -> QString {
        if (!m_symbolIndex) return {};
        const auto syms = m_symbolIndex->search(query, maxResults);
        QString result;
        for (const auto &s : syms) {
            static const char *kindNames[] =
                {"class", "function", "struct", "enum", "namespace", "variable", "method"};
            result += QStringLiteral("%1 [%2] %3:%4\n")
                .arg(s.name,
                     QLatin1String(kindNames[static_cast<int>(s.kind)]),
                     s.filePath)
                .arg(s.line);
        }
        return result;
    };

    agentCallbacks.symbolsInFileFn = [this](const QString &filePath) -> QString {
        if (!m_symbolIndex) return {};
        const auto syms = m_symbolIndex->symbolsInFile(filePath);
        QString result;
        for (const auto &s : syms) {
            static const char *kindNames[] =
                {"class", "function", "struct", "enum", "namespace", "variable", "method"};
            result += QStringLiteral("L%1 %2 [%3]\n")
                .arg(s.line)
                .arg(s.name,
                     QLatin1String(kindNames[static_cast<int>(s.kind)]));
        }
        return result;
    };

    agentCallbacks.findReferencesFn =
        [this](const QString &filePath, int line, int column) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) return {};
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::referencesResult,
            &loop, [&](const QString &resUri, const QJsonArray &locations) {
                Q_UNUSED(resUri)
                for (const auto &loc : locations) {
                    const QJsonObject obj = loc.toObject();
                    const QJsonObject range = obj[QLatin1String("range")].toObject();
                    const QJsonObject start = range[QLatin1String("start")].toObject();
                    const QString locUri = obj[QLatin1String("uri")].toString();
                    // Convert file URI back to path
                    QString path = QUrl(locUri).toLocalFile();
                    result += QStringLiteral("%1:%2:%3\n")
                        .arg(path)
                        .arg(start[QLatin1String("line")].toInt() + 1)
                        .arg(start[QLatin1String("character")].toInt() + 1);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestReferences(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        return result;
    };

    agentCallbacks.findDefinitionFn =
        [this](const QString &filePath, int line, int column) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) return {};
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::definitionResult,
            &loop, [&](const QString &resUri, const QJsonArray &locations) {
                Q_UNUSED(resUri)
                for (const auto &loc : locations) {
                    const QJsonObject obj = loc.toObject();
                    const QJsonObject range = obj[QLatin1String("range")].toObject();
                    const QJsonObject start = range[QLatin1String("start")].toObject();
                    const QString locUri = obj[QLatin1String("uri")].toString();
                    QString path = QUrl(locUri).toLocalFile();
                    result += QStringLiteral("%1:%2:%3\n")
                        .arg(path)
                        .arg(start[QLatin1String("line")].toInt() + 1)
                        .arg(start[QLatin1String("character")].toInt() + 1);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestDefinition(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        return result;
    };

    agentCallbacks.chunkSearchFn = [this](const QString &query, int maxResults) -> QString {
        if (!m_aiServices || !m_aiServices->chunkIndex()) return {};
        const auto chunks = m_aiServices->chunkIndex()->search(query, maxResults);
        QString result;
        for (const auto &c : chunks) {
            result += QStringLiteral("── %1:%2-%3")
                .arg(c.filePath).arg(c.startLine + 1).arg(c.endLine + 1);
            if (!c.symbol.isEmpty())
                result += QStringLiteral(" (%1)").arg(c.symbol);
            result += QLatin1Char('\n');
            result += c.content;
            result += QLatin1Char('\n');
        }
        return result;
    };

    // ── Build & test (via ServiceRegistry) ──────────────────────────────
    agentCallbacks.buildProjectFn =
        [this](const QString &target, std::atomic<bool> &cancelled) -> BuildProjectTool::BuildResult {
        auto *buildSvc = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (!buildSvc)
            return {false, QStringLiteral("No build system configured."), -1};
        QString output;
        bool success = false;
        int exitCode = -1;
        QEventLoop loop;
        auto connOut = connect(buildSvc, &IBuildSystem::buildOutput,
            &loop, [&](const QString &line, bool isError) {
                Q_UNUSED(isError)
                output += line + QLatin1Char('\n');
            });
        auto connDone = connect(buildSvc, &IBuildSystem::buildFinished,
            &loop, [&](bool ok, int code) {
                success  = ok;
                exitCode = code;
                loop.quit();
            });
        // Safety timeout
        QTimer::singleShot(120000, &loop, &QEventLoop::quit);
        // Cancel check — poll every 200ms so tool responds promptly
        QTimer cancelCheck;
        QObject::connect(&cancelCheck, &QTimer::timeout, &loop, [&]() {
            if (cancelled.load()) loop.quit();
        });
        cancelCheck.start(200);
        buildSvc->build(target);
        loop.exec();
        disconnect(connOut);
        disconnect(connDone);
        if (cancelled.load())
            return {false, QStringLiteral("Build cancelled."), -1};
        return {success, output, exitCode};
    };

    agentCallbacks.runTestsFn =
        [this](const QString &scope, const QString &filter,
               std::atomic<bool> &cancelled) -> RunTestsTool::TestResult {
        Q_UNUSED(scope)
        auto *buildSvc = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (!buildSvc)
            return {false, 0, 0, 0, QStringLiteral("No build system.")};
        const QString buildDir = buildSvc->buildDirectory();
        if (buildDir.isEmpty())
            return {false, 0, 0, 0, QStringLiteral("No build directory.")};
        QStringList args = {QStringLiteral("--test-dir"), buildDir,
                            QStringLiteral("--output-on-failure")};
        if (!filter.isEmpty())
            args << QStringLiteral("-R") << filter;
        QProcess proc;
        proc.setWorkingDirectory(buildDir);
        proc.start(QStringLiteral("ctest"), args);
        // Poll with cancel check instead of blocking waitForFinished
        constexpr int kTestTimeout = 120000;
        QElapsedTimer elapsed;
        elapsed.start();
        while (!proc.waitForFinished(200)) {
            if (elapsed.elapsed() > kTestTimeout || cancelled.load()) {
                proc.kill();
                proc.waitForFinished(2000);
                break;
            }
        }
        const QString output = QString::fromUtf8(proc.readAllStandardOutput())
                             + QString::fromUtf8(proc.readAllStandardError());
        const int exitCode = proc.exitCode();
        if (cancelled.load()) {
            m_lastTestResult = {false, 0, 0, 0, QStringLiteral("Tests cancelled.")};
            return m_lastTestResult;
        }
        int passed = 0, failed = 0, total = 0;
        QRegularExpression re(QStringLiteral("(\\d+)% tests passed, (\\d+) tests failed out of (\\d+)"));
        auto match = re.match(output);
        if (match.hasMatch()) {
            failed = match.captured(2).toInt();
            total  = match.captured(3).toInt();
            passed = total - failed;
        }
        m_lastTestResult = {exitCode == 0, passed, failed, total, output};
        return m_lastTestResult;
    };

    agentCallbacks.buildTargetsGetter = [this]() -> QStringList {
        auto *buildSvc = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (!buildSvc) return {};
        return buildSvc->targets();
    };

    // ── Code formatting ───────────────────────────────────────────────────
    agentCallbacks.codeFormatter =
        [this](const QString &filePath, const QString &content,
               const QString &language, int rangeStart, int rangeEnd)
            -> FormatCodeTool::FormatResult {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, {}, {}, QStringLiteral("LSP not available."), {}};
        const QString uri = LspClient::pathToUri(filePath);
        FormatCodeTool::FormatResult result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::formattingResult,
            &loop, [&](const QString &resUri, const QJsonArray &textEdits) {
                Q_UNUSED(resUri)
                if (textEdits.isEmpty()) {
                    result = {true, content, {}, {}, QStringLiteral("LSP")};
                } else {
                    result.ok = true;
                    result.formatterUsed = QStringLiteral("LSP (clangd)");
                    result.formatted = content; // simplified — edits applied by caller
                    QStringList changes;
                    for (const auto &e : textEdits)
                        changes << e.toObject()[QLatin1String("newText")].toString();
                    result.diff = changes.join(QLatin1Char('\n'));
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        Q_UNUSED(language)
        if (rangeStart > 0 && rangeEnd > 0)
            lspClient->requestRangeFormatting(uri, rangeStart - 1, 0, rangeEnd - 1, 0);
        else
            lspClient->requestFormatting(uri);
        loop.exec();
        disconnect(conn);
        return result;
    };

    // ── Refactoring (LSP) ─────────────────────────────────────────────────
    agentCallbacks.refactorer =
        [this](const QString &operation, const QString &filePath,
               int line, int column, const QString &newName,
               int rangeStartLine, int rangeEndLine) -> RefactorTool::RefactorResult {
        Q_UNUSED(rangeStartLine) Q_UNUSED(rangeEndLine)
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, 0, 0, {}, {}, QStringLiteral("LSP not available.")};
        if (operation != QLatin1String("rename"))
            return {false, 0, 0, {}, {},
                    QStringLiteral("Only 'rename' is currently supported via LSP.")};
        const QString uri = LspClient::pathToUri(filePath);
        RefactorTool::RefactorResult result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::renameResult,
            &loop, [&](const QString &, const QJsonObject &workspaceEdit) {
                applyWorkspaceEdit(workspaceEdit);
                const auto changes = workspaceEdit[QLatin1String("changes")].toObject();
                result.ok = true;
                result.filesChanged = changes.keys().size();
                int edits = 0;
                for (const auto &key : changes.keys())
                    edits += changes[key].toArray().size();
                result.editsApplied = edits;
                result.summary = QStringLiteral("Renamed to '%1': %2 edits across %3 files.")
                    .arg(newName).arg(edits).arg(result.filesChanged);
                loop.quit();
            });
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        lspClient->requestRename(uri, line - 1, column - 1, newName);
        loop.exec();
        disconnect(conn);
        return result;
    };

    // ── Git operations ────────────────────────────────────────────────────
    agentCallbacks.gitExecutor =
        [this](const QString &operation, const QJsonObject &args) -> GitOpsTool::GitResult {
        if (!m_gitService || !m_gitService->isGitRepo())
            return {false, {}, QStringLiteral("No Git repository.")};
        if (operation == QLatin1String("add")) {
            const QJsonArray files = args[QLatin1String("files")].toArray();
            for (const auto &f : files)
                m_gitService->stageFile(f.toString());
            return {true, QStringLiteral("Staged %1 file(s).").arg(files.size()), {}};
        }
        if (operation == QLatin1String("commit")) {
            const QString msg = args[QLatin1String("message")].toString();
            if (msg.isEmpty()) return {false, {}, QStringLiteral("Commit message required.")};
            bool ok = m_gitService->commit(msg);
            return {ok, ok ? QStringLiteral("Committed.") : QStringLiteral("Commit failed."),
                    ok ? QString() : QStringLiteral("Commit failed.")};
        }
        if (operation == QLatin1String("branch")) {
            if (args[QLatin1String("list")].toBool())
                return {true, m_gitService->localBranches().join(QLatin1Char('\n')), {}};
            const QString name = args[QLatin1String("name")].toString();
            if (!name.isEmpty()) {
                bool ok = m_gitService->createBranch(name);
                return {ok, ok ? QStringLiteral("Branch '%1' created.").arg(name)
                              : QStringLiteral("Failed to create branch."),
                        ok ? QString() : QStringLiteral("Failed.")};
            }
            return {true, m_gitService->currentBranch(), {}};
        }
        if (operation == QLatin1String("checkout")) {
            const QString target = args[QLatin1String("target")].toString();
            bool ok = m_gitService->checkoutBranch(target);
            return {ok, ok ? QStringLiteral("Checked out '%1'.").arg(target) : QStringLiteral("Checkout failed."),
                    ok ? QString() : QStringLiteral("Failed.")};
        }
        if (operation == QLatin1String("blame")) {
            const QString fp = args[QLatin1String("filePath")].toString();
            const auto entries = m_gitService->blame(fp);
            QString result;
            for (const auto &e : entries)
                result += QStringLiteral("%1 %2 %3\n")
                    .arg(e.commitHash.left(8), e.author, e.summary);
            return {true, result, {}};
        }
        if (operation == QLatin1String("reset")) {
            const QJsonArray files = args[QLatin1String("files")].toArray();
            for (const auto &f : files)
                m_gitService->unstageFile(f.toString());
            return {true, QStringLiteral("Unstaged %1 file(s).").arg(files.size()), {}};
        }
        if (operation == QLatin1String("log")) {
            int count = args[QLatin1String("count")].toInt(10);
            if (count < 1) count = 1;
            if (count > 50) count = 50;
            // Use non-blocking QEventLoop approach instead of GitService::log()
            // which calls waitForFinished() and blocks the UI thread
            QProcess proc;
            proc.setWorkingDirectory(m_gitService->workingDirectory());
            QStringList gitArgs = {QStringLiteral("log"),
                                   QStringLiteral("--format=%h|%an|%as|%s"),
                                   QStringLiteral("-n"), QString::number(count)};
            const QString fp = args[QLatin1String("filePath")].toString();
            if (!fp.isEmpty()) {
                gitArgs << QStringLiteral("--") << fp;
            }
            const QString author = args[QLatin1String("author")].toString();
            if (!author.isEmpty())
                gitArgs << QStringLiteral("--author=%1").arg(author);
            const QString since = args[QLatin1String("since")].toString();
            if (!since.isEmpty())
                gitArgs << QStringLiteral("--since=%1").arg(since);
            const QString grepPat = args[QLatin1String("grep")].toString();
            if (!grepPat.isEmpty())
                gitArgs << QStringLiteral("--grep=%1").arg(grepPat);
            proc.start(QStringLiteral("git"), gitArgs);
            QEventLoop loop;
            QObject::connect(&proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                             &loop, &QEventLoop::quit);
            QTimer::singleShot(15000, &loop, &QEventLoop::quit);
            if (proc.state() != QProcess::NotRunning)
                loop.exec();
            if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
                return {false, {}, QStringLiteral("git log failed")};
            const QString out = QString::fromUtf8(proc.readAllStandardOutput());
            if (out.trimmed().isEmpty())
                return {true, QStringLiteral("No commits found."), {}};
            return {true, out.trimmed(), {}};
        }
        // stash, tag, cherry_pick — run git directly (non-blocking via QEventLoop)
        auto runGitDirect = [&](const QStringList &gitArgs) -> GitOpsTool::GitResult {
            QProcess proc;
            proc.setWorkingDirectory(m_gitService->workingDirectory());
            proc.start(QStringLiteral("git"), gitArgs);
            // Use QEventLoop so the UI stays responsive while git runs
            QEventLoop loop;
            QObject::connect(&proc, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                             &loop, &QEventLoop::quit);
            QTimer::singleShot(15000, &loop, &QEventLoop::quit);
            if (proc.state() != QProcess::NotRunning)
                loop.exec();
            const QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
                return {false, {}, err.isEmpty() ? QStringLiteral("git failed") : err};
            return {true, out.isEmpty() ? QStringLiteral("OK") : out, {}};
        };
        if (operation == QLatin1String("stash")) {
            const QString action = args[QLatin1String("action")].toString(QStringLiteral("push"));
            if (action == QLatin1String("push"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("push")});
            if (action == QLatin1String("pop"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("pop")});
            if (action == QLatin1String("list"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("list")});
            if (action == QLatin1String("drop"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("drop")});
            return {false, {}, QStringLiteral("Unknown stash action: %1").arg(action)};
        }
        if (operation == QLatin1String("tag")) {
            if (args[QLatin1String("list")].toBool())
                return runGitDirect({QStringLiteral("tag"), QStringLiteral("--list")});
            const QString name = args[QLatin1String("name")].toString();
            if (name.isEmpty())
                return {false, {}, QStringLiteral("Tag name required.")};
            const QString msg = args[QLatin1String("message")].toString();
            if (!msg.isEmpty())
                return runGitDirect({QStringLiteral("tag"), QStringLiteral("-a"),
                                     name, QStringLiteral("-m"), msg});
            return runGitDirect({QStringLiteral("tag"), name});
        }
        if (operation == QLatin1String("cherry_pick")) {
            const QString hash = args[QLatin1String("commitHash")].toString();
            if (hash.isEmpty())
                return {false, {}, QStringLiteral("commitHash required.")};
            return runGitDirect({QStringLiteral("cherry-pick"), hash});
        }
        return {false, {}, QStringLiteral("Unknown operation: %1").arg(operation)};
    };

    // ── Ask user ──────────────────────────────────────────────────────────
    agentCallbacks.userPrompter =
        [this](const QString &question, const QStringList &options,
               bool allowFreeText) -> AskUserTool::UserResponse {
        if (options.isEmpty() || allowFreeText) {
            bool ok = false;
            const QString answer = QInputDialog::getText(
                this, tr("Agent Question"), question,
                QLineEdit::Normal, QString(), &ok);
            return {ok, answer, -1};
        }
        // Multiple choice via QMessageBox
        QStringList labels = options;
        if (labels.size() <= 3) {
            auto box = std::make_unique<QMessageBox>(this);
            box->setWindowTitle(tr("Agent Question"));
            box->setText(question);
            QList<QPushButton *> buttons;
            for (const QString &opt : labels)
                buttons << box->addButton(opt, QMessageBox::ActionRole);
            box->addButton(QMessageBox::Cancel);
            box->exec();
            int idx = -1;
            for (int i = 0; i < buttons.size(); ++i) {
                if (box->clickedButton() == buttons[i]) { idx = i; break; }
            }
            bool answered = idx >= 0;
            return {answered, answered ? labels[idx] : QString(), idx};
        }
        // Many options → use QInputDialog combo
        bool ok = false;
        const QString chosen = QInputDialog::getItem(
            this, tr("Agent Question"), question, labels, 0, false, &ok);
        int idx = ok ? labels.indexOf(chosen) : -1;
        return {ok, chosen, idx};
    };

    // ── Editor context ────────────────────────────────────────────────────
    agentCallbacks.editorStateGetter = [this]() -> EditorContextTool::EditorState {
        EditorContextTool::EditorState state;
        auto *ed = currentEditor();
        if (!ed) return state;
        state.activeFilePath = ed->property("filePath").toString();
        const QTextCursor cur = ed->textCursor();
        state.cursorLine   = cur.blockNumber() + 1;
        state.cursorColumn = cur.positionInBlock() + 1;
        state.selectedText = cur.selectedText();
        if (cur.hasSelection()) {
            state.selectionStartLine = cur.document()->findBlock(cur.selectionStart()).blockNumber() + 1;
            state.selectionEndLine   = cur.document()->findBlock(cur.selectionEnd()).blockNumber() + 1;
        }
        state.totalLines = ed->document()->blockCount();
        state.language   = ed->languageId();
        state.isModified = ed->document()->isModified();
        // Viewport
        const QTextCursor topCur = ed->cursorForPosition(QPoint(0, 0));
        state.visibleStartLine = topCur.blockNumber() + 1;
        const QTextCursor botCur = ed->cursorForPosition(
            QPoint(0, ed->viewport()->height() - 1));
        state.visibleEndLine = botCur.blockNumber() + 1;
        // Open files
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *tab = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
            const QString fp = tab ? tab->property("filePath").toString() : QString();
            if (!fp.isEmpty())
                state.openFiles.append(fp);
        }
        return state;
    };

    // ── Change impact analysis ────────────────────────────────────────────
    agentCallbacks.changeImpactAnalyzer =
        [this](const QString &filePath, int line, int column,
               const QString &changeType) -> ChangeImpactTool::ImpactResult {
        ChangeImpactTool::ImpactResult result;
        result.ok = false;
        // Use LSP references to estimate impact
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) {
            result.error = QStringLiteral("LSP not available.");
            return result;
        }
        // Find references synchronously
        QStringList refFiles;
        QEventLoop loop;
        const QString uri = LspClient::pathToUri(filePath);
        auto conn = connect(lspClient, &LspClient::referencesResult,
            &loop, [&](const QString &, const QJsonArray &locations) {
                for (const auto &loc : locations) {
                    QString path = QUrl(loc.toObject()[QLatin1String("uri")].toString()).toLocalFile();
                    if (!refFiles.contains(path))
                        refFiles.append(path);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestReferences(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        result.ok = true;
        result.directReferences = refFiles.size();
        result.affectedFiles    = refFiles;
        // Risk scoring
        Q_UNUSED(changeType)
        result.riskScore = qBound(1, refFiles.size(), 10);
        result.summary = QStringLiteral("%1 files reference this symbol.")
            .arg(refFiles.size());
        return result;
    };

    // ── Navigation ────────────────────────────────────────────────────────
    agentCallbacks.fileOpener =
        [this](const QString &filePath, int line, int column) -> bool {
        navigateToLocation(filePath, line - 1, column > 0 ? column - 1 : 0);
        return true;
    };

    agentCallbacks.headerSourceSwitcher = [this](const QString &filePath) -> QString {
        const QFileInfo fi(filePath);
        const QString base = fi.completeBaseName();
        const QString dir  = fi.absolutePath();
        static const QStringList headerExts = {
            QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hxx"), QStringLiteral("hh")};
        static const QStringList sourceExts = {
            QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("cxx"), QStringLiteral("c")};
        const QString ext = fi.suffix().toLower();
        const QStringList &searchExts = headerExts.contains(ext) ? sourceExts : headerExts;
        for (const QString &e : searchExts) {
            const QString candidate = dir + QLatin1Char('/') + base + QLatin1Char('.') + e;
            if (QFile::exists(candidate)) return candidate;
        }
        return {};
    };

    // ── LSP rename & usages ───────────────────────────────────────────────
    agentCallbacks.symbolRenamer =
        [this](const QString &filePath, int line, int column,
               const QString &newName) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return QStringLiteral("Error: LSP not available.");
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::renameResult,
            &loop, [&](const QString &, const QJsonObject &workspaceEdit) {
                applyWorkspaceEdit(workspaceEdit);
                const auto changes = workspaceEdit[QLatin1String("changes")].toObject();
                int edits = 0;
                for (const auto &key : changes.keys())
                    edits += changes[key].toArray().size();
                result = QStringLiteral("%1 edits across %2 files.")
                    .arg(edits).arg(changes.keys().size());
                loop.quit();
            });
        auto errConn = connect(lspClient, &LspClient::serverError,
            &loop, [&](const QString &msg) {
                result = QStringLiteral("Error: %1").arg(msg);
                loop.quit();
            });
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        lspClient->requestRename(uri, line - 1, column - 1, newName);
        loop.exec();
        disconnect(conn);
        disconnect(errConn);
        return result;
    };

    agentCallbacks.usageFinder =
        [this](const QString &filePath, int line, int column) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) return {};
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::referencesResult,
            &loop, [&](const QString &, const QJsonArray &locations) {
                for (const auto &loc : locations) {
                    const QJsonObject obj = loc.toObject();
                    const QJsonObject range = obj[QLatin1String("range")].toObject();
                    const QJsonObject start = range[QLatin1String("start")].toObject();
                    QString path = QUrl(obj[QLatin1String("uri")].toString()).toLocalFile();
                    result += QStringLiteral("%1:%2:%3\n")
                        .arg(path)
                        .arg(start[QLatin1String("line")].toInt() + 1)
                        .arg(start[QLatin1String("character")].toInt() + 1);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestReferences(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        return result;
    };

    // ── LuaJIT executor ──────────────────────────────────────────────────
    agentCallbacks.luaExecutor = [this](const QString &script) -> LuaExecuteTool::LuaResult {
        auto *engine = m_pluginManager ? m_pluginManager->luaEngine() : nullptr;
        if (!engine)
            return {false, {}, {}, QStringLiteral("LuaJIT engine not available."), 0};

        auto adhoc = engine->executeAdhoc(script);
        return {adhoc.ok, adhoc.output, adhoc.returnValue, adhoc.error,
                adhoc.memoryUsedBytes};
    };

    // ── Diff viewer ──────────────────────────────────────────────────────
    agentCallbacks.diffViewer =
        [this](const QString &left, const QString &right,
               const QString &leftTitle, const QString &rightTitle) -> bool {
        // Display diff in a new dock widget with side-by-side view
        auto diffWidget = std::make_unique<QWidget>();
        auto *layout = new QHBoxLayout(diffWidget.get());

        auto leftEdit = std::make_unique<QPlainTextEdit>();
        leftEdit->setReadOnly(true);
        leftEdit->setPlainText(left);
        leftEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto rightEdit = std::make_unique<QPlainTextEdit>();
        rightEdit->setReadOnly(true);
        rightEdit->setPlainText(right);
        rightEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto *leftGroup = new QGroupBox(leftTitle);
        auto *leftLayout = new QVBoxLayout(leftGroup);
        leftLayout->addWidget(leftEdit.release());

        auto *rightGroup = new QGroupBox(rightTitle);
        auto *rightLayout = new QVBoxLayout(rightGroup);
        rightLayout->addWidget(rightEdit.release());

        layout->addWidget(leftGroup);
        layout->addWidget(rightGroup);

        auto *dock = new QDockWidget(
            tr("Diff: %1 vs %2").arg(leftTitle, rightTitle), this);
        dock->setWidget(diffWidget.release());
        dock->setAttribute(Qt::WA_DeleteOnClose);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->show();
        return true;
    };

    // ── Static analysis callback ─────────────────────────────────────────
    agentCallbacks.staticAnalyzer =
        [this](const QString &filePath, const QStringList &checkers,
               bool fixMode) -> StaticAnalysisTool::AnalysisResult {
        // Dispatch to the appropriate analyzer based on file extension
        StaticAnalysisTool::AnalysisResult result;
        result.ok = false;

        if (filePath.isEmpty()) {
            result.error = QStringLiteral("No file path provided.");
            return result;
        }

        const QFileInfo fi(filePath);
        const QString suffix = fi.suffix().toLower();

        // Build the command based on language
        QString program;
        QStringList args;

        if (suffix == QLatin1String("cpp") || suffix == QLatin1String("cxx") ||
            suffix == QLatin1String("cc")  || suffix == QLatin1String("c")   ||
            suffix == QLatin1String("h")   || suffix == QLatin1String("hpp")) {
            // clang-tidy
            program = QStringLiteral("clang-tidy");
            args << filePath;
            if (!checkers.isEmpty()) {
                args << QStringLiteral("-checks=%1").arg(checkers.join(QLatin1Char(',')));
            }
            if (fixMode)
                args << QStringLiteral("--fix");
            // Add compile_commands.json path if available
            const QString compileDb = m_editorMgr->currentFolder() + QStringLiteral("/build-llvm/compile_commands.json");
            if (QFileInfo::exists(compileDb))
                args << QStringLiteral("-p") << m_editorMgr->currentFolder() + QStringLiteral("/build-llvm");
            result.analyzerUsed = QStringLiteral("clang-tidy");

        } else if (suffix == QLatin1String("py")) {
            program = QStringLiteral("pylint");
            args << QStringLiteral("--output-format=text") << filePath;
            result.analyzerUsed = QStringLiteral("pylint");

        } else if (suffix == QLatin1String("js") || suffix == QLatin1String("ts") ||
                   suffix == QLatin1String("jsx") || suffix == QLatin1String("tsx")) {
            program = QStringLiteral("eslint");
            args << QStringLiteral("--format=compact") << filePath;
            if (fixMode)
                args << QStringLiteral("--fix");
            result.analyzerUsed = QStringLiteral("eslint");

        } else if (suffix == QLatin1String("rs")) {
            program = QStringLiteral("cargo");
            args << QStringLiteral("clippy") << QStringLiteral("--message-format=short");
            result.analyzerUsed = QStringLiteral("clippy");

        } else if (suffix == QLatin1String("go")) {
            program = QStringLiteral("staticcheck");
            args << filePath;
            result.analyzerUsed = QStringLiteral("staticcheck");

        } else {
            result.error = QStringLiteral("No analyzer available for .%1 files.").arg(suffix);
            return result;
        }

        // Run the analyzer process
        QProcess proc;
        proc.setWorkingDirectory(m_editorMgr->currentFolder());
        proc.start(program, args);
        if (!proc.waitForFinished(55000)) {
            result.error = QStringLiteral("Analyzer timed out or failed to start: %1").arg(program);
            return result;
        }

        result.ok = true;
        const QString output = QString::fromUtf8(proc.readAllStandardOutput())
                             + QString::fromUtf8(proc.readAllStandardError());

        // Parse output into findings (simple line-based parsing)
        const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        static const QRegularExpression findingRe(
            QStringLiteral("^(.+?):(\\d+):(\\d+):\\s*(warning|error|note|info):\\s*(.+)$"));

        for (const QString &line : lines) {
            const QRegularExpressionMatch m = findingRe.match(line);
            if (m.hasMatch()) {
                StaticAnalysisTool::Finding f;
                f.filePath = m.captured(1);
                f.line     = m.captured(2).toInt();
                f.column   = m.captured(3).toInt();
                f.severity = m.captured(4);
                f.message  = m.captured(5);

                // Extract rule ID if present (e.g. [bugprone-...] or (C1234))
                static const QRegularExpression ruleRe(
                    QStringLiteral("\\[([a-zA-Z0-9._-]+)\\]$"));
                const QRegularExpressionMatch ruleMatch = ruleRe.match(f.message);
                if (ruleMatch.hasMatch()) {
                    f.ruleId = ruleMatch.captured(1);
                    f.message = f.message.left(ruleMatch.capturedStart()).trimmed();
                }

                if (f.severity == QLatin1String("error"))
                    result.errorCount++;
                else if (f.severity == QLatin1String("warning"))
                    result.warningCount++;

                result.findings.append(f);
            }
        }

        return result;
    };

    // ── Terminal selection callback ──────────────────────────────────────
    agentCallbacks.terminalSelectionGetter = [this]() -> QString {
        if (!m_terminal) return {};
        return m_terminal->selectedText();
    };

    // ── Test failure cache callback ─────────────────────────────────────
    agentCallbacks.testFailureGetter = [this]() -> RunTestsTool::TestResult {
        return m_lastTestResult;
    };

    // ── IDE command execution callbacks ─────────────────────────────────
    agentCallbacks.commandExecutor = [this](const QString &id) -> bool {
        auto *cmdSvc = m_hostServices->commandService();
        return cmdSvc ? cmdSvc->executeCommand(id) : false;
    };
    agentCallbacks.commandListGetter = [this]() -> QStringList {
        auto *cmdSvc = m_hostServices->commandService();
        return cmdSvc ? cmdSvc->commandIds() : QStringList{};
    };

    // ── Tree-sitter AST access callbacks ────────────────────────────────
    auto tsHelper = std::make_shared<TreeSitterAgentHelper>();
    agentCallbacks.tsFileParser = [tsHelper](const QString &fp, int md) {
        return tsHelper->parseFile(fp, md);
    };
    agentCallbacks.tsQueryRunner = [tsHelper](const QString &fp, const QString &qp) {
        return tsHelper->runQuery(fp, qp);
    };
    agentCallbacks.tsSymbolExtractor = [tsHelper](const QString &fp) {
        return tsHelper->extractSymbols(fp);
    };
    agentCallbacks.tsNodeAtPosition = [tsHelper](const QString &fp, int l, int c) {
        return tsHelper->nodeAtPosition(fp, l, c);
    };

    // ── Symbol documentation (LSP hover) ────────────────────────────────
    agentCallbacks.symbolDocGetter =
        [this](const QString &filePath, int line, int column) -> SymbolDocTool::DocResult {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, {}, {}, {}, QStringLiteral("LSP not available.")};
        const QString uri = LspClient::pathToUri(filePath);
        SymbolDocTool::DocResult result;
        result.ok = false;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::hoverResult,
            &loop, [&](const QString &, int, int, const QString &markdown) {
                if (markdown.isEmpty()) {
                    result.error = QStringLiteral("No documentation found at this location.");
                } else {
                    result.ok = true;
                    result.documentation = markdown;
                    // Try to extract signature (first code block or first line)
                    const int codeStart = markdown.indexOf(QLatin1String("```"));
                    if (codeStart >= 0) {
                        const int lineEnd = markdown.indexOf(QLatin1Char('\n'), codeStart + 3);
                        const int codeEnd = markdown.indexOf(QLatin1String("```"), lineEnd);
                        if (lineEnd >= 0 && codeEnd > lineEnd)
                            result.signature = markdown.mid(lineEnd + 1, codeEnd - lineEnd - 1).trimmed();
                    }
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestHover(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        if (!result.ok && result.error.isEmpty())
            result.error = QStringLiteral("Hover request timed out.");
        return result;
    };

    // ── Code completion (LSP) ───────────────────────────────────────────
    agentCallbacks.completionGetter =
        [this](const QString &filePath, int line, int column,
               const QString &prefix) -> CodeCompletionTool::CompletionResult {
        Q_UNUSED(prefix)
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, {}, false, QStringLiteral("LSP not available.")};
        const QString uri = LspClient::pathToUri(filePath);
        CodeCompletionTool::CompletionResult result;
        result.ok = false;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::completionResult,
            &loop, [&](const QString &, int, int,
                       const QJsonArray &items, bool isIncomplete) {
                result.ok = true;
                result.isIncomplete = isIncomplete;
                static const char *kindNames[] = {
                    "", "text", "method", "function", "constructor", "field",
                    "variable", "class", "interface", "module", "property",
                    "unit", "value", "enum", "keyword", "snippet", "color",
                    "file", "reference", "folder", "enum_member", "constant",
                    "struct", "event", "operator", "type_parameter"
                };
                for (const auto &item : items) {
                    const QJsonObject obj = item.toObject();
                    CodeCompletionTool::CompletionItem ci;
                    ci.label = obj[QLatin1String("label")].toString();
                    const int kind = obj[QLatin1String("kind")].toInt();
                    ci.kind = (kind >= 0 && kind <= 25)
                        ? QString::fromLatin1(kindNames[kind])
                        : QString::number(kind);
                    ci.detail = obj[QLatin1String("detail")].toString();
                    ci.documentation = obj[QLatin1String("documentation")].toString();
                    if (ci.documentation.isEmpty()) {
                        const QJsonObject docObj = obj[QLatin1String("documentation")].toObject();
                        ci.documentation = docObj[QLatin1String("value")].toString();
                    }
                    ci.insertText = obj[QLatin1String("insertText")].toString();
                    if (ci.insertText.isEmpty())
                        ci.insertText = ci.label;
                    result.items.append(ci);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestCompletion(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        if (!result.ok && result.error.isEmpty())
            result.error = QStringLiteral("Completion request timed out.");
        return result;
    };

    // ── Diagram generation (Mermaid CLI) ────────────────────────────────
    agentCallbacks.diagramRenderer =
        [this](const QString &markup, const QString &format,
               const QString &outputPath) -> GenerateDiagramTool::DiagramResult {
        Q_UNUSED(this)
        GenerateDiagramTool::DiagramResult result;
        result.ok = false;

        // Determine output file path
        QString outFile = outputPath;
        const QString diagramCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(diagramCache);
        if (outFile.isEmpty()) {
            outFile = diagramCache + QStringLiteral("/exorcist_diagram.svg");
        }

        // Write markup to a temp input file
        const QString inputFile = diagramCache + QStringLiteral("/exorcist_diagram_input.mmd");
        {
            QFile f(inputFile);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                result.error = QStringLiteral("Failed to write temp input file.");
                return result;
            }
            f.write(markup.toUtf8());
        }

        if (format == QLatin1String("mermaid") || format.isEmpty()) {
            // Use mmdc (Mermaid CLI)
            QProcess proc;
            QStringList args = {
                QStringLiteral("-i"), inputFile,
                QStringLiteral("-o"), outFile
            };
            // Force SVG output unless outputPath ends with .png
            if (outFile.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
                args << QStringLiteral("-e") << QStringLiteral("png");

            proc.start(QStringLiteral("mmdc"), args);
            if (!proc.waitForFinished(30000)) {
                result.error = QStringLiteral("mmdc (Mermaid CLI) timed out or not found. "
                    "Install with: npm install -g @mermaid-js/mermaid-cli");
                return result;
            }
            if (proc.exitCode() != 0) {
                result.error = QStringLiteral("mmdc failed: %1")
                    .arg(QString::fromUtf8(proc.readAllStandardError()));
                return result;
            }
        } else if (format == QLatin1String("plantuml")) {
            // Use plantuml.jar or plantuml command
            QProcess proc;
            QStringList args = {
                QStringLiteral("-tsvg"),
                QStringLiteral("-o"), QFileInfo(outFile).absolutePath(),
                inputFile
            };
            proc.start(QStringLiteral("plantuml"), args);
            if (!proc.waitForFinished(30000)) {
                result.error = QStringLiteral("PlantUML timed out or not found.");
                return result;
            }
            if (proc.exitCode() != 0) {
                result.error = QStringLiteral("PlantUML failed: %1")
                    .arg(QString::fromUtf8(proc.readAllStandardError()));
                return result;
            }
            // PlantUML outputs next to input file with .svg extension
            const QString plantOut = diagramCache + QStringLiteral("/exorcist_diagram_input.svg");
            if (plantOut != outFile)
                QFile::rename(plantOut, outFile);
        } else {
            result.error = QStringLiteral("Unknown format: %1. Use 'mermaid' or 'plantuml'.").arg(format);
            return result;
        }

        // Read SVG content if output is SVG
        if (outFile.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
            QFile f(outFile);
            if (f.open(QIODevice::ReadOnly))
                result.svgContent = QString::fromUtf8(f.readAll());
        }
        result.pngPath = outFile;
        result.ok = true;
        return result;
    };

    // ── Performance profiling ───────────────────────────────────────────
    agentCallbacks.profiler =
        [this](const QString &target, int duration,
               const QString &type) -> PerformanceProfileTool::ProfileResult {
        Q_UNUSED(this)
        PerformanceProfileTool::ProfileResult result;
        result.ok = false;

        if (target.isEmpty()) {
            result.error = QStringLiteral("No profiling target specified.");
            return result;
        }

#ifdef Q_OS_WIN
        // Windows: use xperf / WPR or fallback to sampling via tasklist
        Q_UNUSED(duration) Q_UNUSED(type)
        QProcess proc;
        // Use "perf" (WSL) or dotnet-trace, or fallback to basic process stats
        proc.start(QStringLiteral("powershell"), {
            QStringLiteral("-Command"),
            QStringLiteral("Get-Process -Name '%1' -ErrorAction SilentlyContinue | "
                "Select-Object Name, Id, CPU, WorkingSet64, "
                "VirtualMemorySize64, HandleCount, Threads | "
                "Format-List | Out-String").arg(target)
        });
        proc.waitForFinished(15000);
        const QString output = QString::fromUtf8(proc.readAllStandardOutput());
        if (output.trimmed().isEmpty()) {
            result.error = QStringLiteral("Process '%1' not found or no data available.").arg(target);
            return result;
        }
        result.ok = true;
        result.report = output;
        result.totalMs = 0;
        result.totalSamples = 0;
#else
        if (type == QLatin1String("cpu") || type.isEmpty()) {
            QProcess proc;
            proc.start(QStringLiteral("perf"), {
                QStringLiteral("record"), QStringLiteral("-g"),
                QStringLiteral("-p"), target,
                QStringLiteral("--"), QStringLiteral("sleep"),
                QString::number(duration)
            });
            if (!proc.waitForFinished((duration + 5) * 1000)) {
                result.error = QStringLiteral("perf timed out.");
                return result;
            }
            // Get report
            QProcess report;
            report.start(QStringLiteral("perf"), {
                QStringLiteral("report"), QStringLiteral("--stdio"),
                QStringLiteral("--no-children")
            });
            report.waitForFinished(15000);
            result.ok = true;
            result.report = QString::fromUtf8(report.readAllStandardOutput());
            result.totalMs = duration * 1000.0;
        } else if (type == QLatin1String("memory")) {
            QProcess proc;
            proc.start(QStringLiteral("valgrind"), {
                QStringLiteral("--tool=massif"),
                QStringLiteral("--pages-as-heap=yes"),
                target
            });
            proc.waitForFinished(qMax(duration, 10) * 1000);
            result.ok = true;
            result.report = QString::fromUtf8(proc.readAllStandardOutput())
                          + QString::fromUtf8(proc.readAllStandardError());
        } else {
            result.error = QStringLiteral("Unsupported profile type: %1").arg(type);
            return result;
        }
#endif
        return result;
    };

    m_agentPlatform->initialize(agentCallbacks);
    m_agentPlatform->registerCoreTools(m_editorMgr->currentFolder());

    // ── Wire LSP diagnostics push to agent (deferred to post-plugin wiring) ──

    // Deferred from createDockWidgets — m_agentPlatform was null there.
    if (m_memoryBrowser)
        m_memoryBrowser->setBrainService(m_agentPlatform->brainService());

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
        m_requestLogPanel->logRequest(QStringLiteral("turn"), QStringLiteral("-"),
                                       1, 0);
    });
    connect(ac, &AgentController::turnFinished,
            this, [this](const AgentTurn &turn) {
        const QString text = turn.steps.isEmpty() ? QString() : turn.steps.last().finalText;
        m_requestLogPanel->logResponse(QStringLiteral("turn"), text.left(500));
    });
    connect(ac, &AgentController::turnError,
            this, [this](const QString &err) {
        m_requestLogPanel->logError(QStringLiteral("turn"), err);
        NotificationToast::show(this, tr("AI Error: %1").arg(err.left(100)),
                                NotificationToast::Error, 5000);
    });
    connect(ac, &AgentController::toolCallStarted,
            this, [this](const QString &name, const QJsonObject &args) {
        m_requestLogPanel->logToolCall(name, args, true, QStringLiteral("started"));
    });
    connect(ac, &AgentController::toolCallFinished,
            this, [this](const QString &name, const ToolExecResult &result) {
        m_requestLogPanel->logToolCall(name, {}, result.ok,
            result.ok ? result.textContent.left(200) : result.error);
    });

    // Wire trajectory panel
    connect(ac, &AgentController::turnStarted,
            this, [this](const QString &) {
        m_trajectoryPanel->clear();
    });
    connect(ac, &AgentController::turnFinished,
            this, [this](const AgentTurn &turn) {
        m_trajectoryPanel->setTurn(turn);
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
        m_trajectoryPanel->appendStep(step);
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
                auto *ed = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
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
            if (!m_terminal) return tr("No terminal output.");
            const QString out = m_terminal->recentOutput(120);
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
            if (!m_terminal) return;
            m_dockManager->showDock(dock(QStringLiteral("TerminalDock")), exdock::SideBarArea::Bottom);
            m_terminal->sendInput(code);
            m_terminal->sendInput(QStringLiteral("\r\n"));
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
        updateDiffRanges(currentEditor());
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

    // Restore dock layout from DockManager JSON state.
    {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        const QString dockJson = s.value(QStringLiteral("dockState")).toString();
        if (!dockJson.isEmpty()) {
            const QJsonObject obj = QJsonDocument::fromJson(dockJson.toUtf8()).object();
            if (!obj.isEmpty())
                m_dockManager->restoreState(obj);
        }
    }

    // Re-open last folder (deferred to avoid blocking first paint).
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    const QString lastFolder = s.value(QStringLiteral("lastFolder")).toString();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists())
        openFolder(lastFolder);
    StartupProfiler::instance().mark(QStringLiteral("deferred init done"));
}

// ── UI setup ────────────────────────────────────────────────────────────────

void MainWindow::setupUi()
{
    m_editorMgr->setTabs(new QTabWidget(this));
    m_editorMgr->tabs()->setDocumentMode(true);
    m_editorMgr->tabs()->setTabsClosable(true);
    m_editorMgr->tabs()->setMovable(true);

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

    connect(m_editorMgr->tabs(), &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *widget = m_editorMgr->tabs()->widget(index);
        const QString closedPath = widget ? widget->property("filePath").toString() : QString();
        m_editorMgr->tabs()->removeTab(index);
        if (widget) {
            widget->deleteLater();
        }
        if (!closedPath.isEmpty() && m_pluginManager)
            m_pluginManager->fireLuaEvent(QStringLiteral("editor.close"), {closedPath});
    });
    connect(m_editorMgr->tabs(), &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    createDockWidgets();
    openNewTab();
}

void MainWindow::setupMenus()
{
    // ── File ──────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *newTabAction = fileMenu->addAction(tr("New &Tab"));
    newTabAction->setShortcut(QKeySequence::New);

    QAction *openAction = fileMenu->addAction(tr("&Open File..."));
    openAction->setShortcut(QKeySequence::Open);

    QAction *openFolderAction = fileMenu->addAction(tr("Open &Folder..."));
    openFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);

    QAction *saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();

    auto *recentMgr = new RecentFilesManager(this);
    recentMgr->setObjectName(QStringLiteral("recentFilesManager"));

    auto *langProfileMgr = new LanguageProfileManager(this);
    langProfileMgr->setObjectName(QStringLiteral("languageProfileManager"));
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
    connect(closeSolutionAction, &QAction::triggered, this, [this]() {
        m_editorMgr->projectManager()->closeSolution();
        m_editorMgr->setCurrentFolder(QString());
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->stopServer();
        m_gitService->setWorkingDirectory({});
        m_searchPanel->setRootPath({});
        m_terminal->setWorkingDirectory({});
        setWindowTitle(tr("Exorcist"));
        statusBar()->showMessage(tr("Solution closed"), 3000);
    });
    connect(closeFolderAction, &QAction::triggered, this, [this]() {
        m_editorMgr->projectManager()->closeSolution();
        m_editorMgr->setCurrentFolder(QString());
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->stopServer();
        m_gitService->setWorkingDirectory({});
        m_searchPanel->setRootPath({});
        m_terminal->setWorkingDirectory({});
        if (m_fileWatcher) m_fileWatcher->unwatchAll();
        setWindowTitle(tr("Exorcist"));
        statusBar()->showMessage(tr("Folder closed"), 3000);
    });
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // ── Edit ──────────────────────────────────────────────────────────────
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    QAction *undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->undo();
    });

    QAction *redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->redo();
    });

    editMenu->addSeparator();

    QAction *cutAction = editMenu->addAction(tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->cut();
    });

    QAction *copyAction = editMenu->addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->copy();
    });

    QAction *pasteAction = editMenu->addAction(tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->paste();
    });

    editMenu->addSeparator();

    QAction *selectAllAction = editMenu->addAction(tr("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->selectAll();
    });

    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction(tr("&Find..."));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->showFindBar();
    });

    QAction *findReplAction = editMenu->addAction(tr("Find && &Replace..."));
    findReplAction->setShortcut(QKeySequence::Replace);
    connect(findReplAction, &QAction::triggered, this, [this]() {
        if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget()))
            e->showFindBar();
    });

    editMenu->addSeparator();

    QAction *prefsAction = editMenu->addAction(tr("&Preferences..."));
    prefsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(prefsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(m_themeManager, this);

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
                    if (auto *e = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i))) {
                        e->setFont(font);
                        e->setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * tabSize);
                        e->setWordWrapMode(wrap ? QTextOption::WordWrap : QTextOption::NoWrap);
                        e->setMinimapVisible(minimap);
                    }
                }
            }

            // Re-index workspace if indexer settings changed
            if (m_workspaceIndexer && !m_workspaceIndexer->rootPath().isEmpty())
                m_workspaceIndexer->indexWorkspace(m_workspaceIndexer->rootPath());

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
        KeymapDialog dlg(m_keymapManager, this);
        dlg.exec();
    });

    // ── View ──────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QAction *cmdPaletteAction = viewMenu->addAction(tr("&Command Palette"));
    cmdPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

    QAction *filePaletteAction = viewMenu->addAction(tr("&Go to File..."));
    filePaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
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
    QAction *toggleDebugAction     = viewMenu->addAction(tr("&Debug panel"));
    viewMenu->addSeparator();
    QAction *toggleBlameAction     = viewMenu->addAction(tr("Toggle Git &Blame"));
    toggleBlameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
    toggleBlameAction->setCheckable(true);
    toggleBlameAction->setChecked(false);

    QAction *themeToggleAction = viewMenu->addAction(tr("Toggle &Dark/Light Theme"));
    themeToggleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F10));
    connect(themeToggleAction, &QAction::triggered, this, [this]() {
        m_themeManager->toggle();
    });

    QAction *minimapAction = viewMenu->addAction(tr("Toggle &Minimap"));
    minimapAction->setCheckable(true);
    minimapAction->setChecked(true);
    connect(minimapAction, &QAction::toggled, this, [this](bool on) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            if (auto *ev = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i)))
                ev->setMinimapVisible(on);
        }
    });

    QAction *indentGuidesAction = viewMenu->addAction(tr("Toggle Indent &Guides"));
    indentGuidesAction->setCheckable(true);
    indentGuidesAction->setChecked(true);
    connect(indentGuidesAction, &QAction::toggled, this, [this](bool on) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            if (auto *ev = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i)))
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
        m_diffViewer->showDiff(filePath, headText, currentText);
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
    toggleDebugAction->setCheckable(true);
    toggleDebugAction->setChecked(false);

    // ── Run ─────────────────────────────────────────────────────────────
    QMenu *runMenu = menuBar()->addMenu(tr("&Run"));

    auto *buildAct = runMenu->addAction(tr("&Build"));
    buildAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    connect(buildAct, &QAction::triggered, this, [this]() {
        auto *buildSys = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (buildSys && buildSys->hasProject()) {
            buildSys->build();
            m_dockManager->showDock(dock(QStringLiteral("OutputDock")), exdock::SideBarArea::Bottom);
        }
    });

    runMenu->addSeparator();

    auto *runAct = runMenu->addAction(tr("Start &Debugging"));
    runAct->setShortcut(QKeySequence(Qt::Key_F5));
    connect(runAct, &QAction::triggered, this, [this]() {
        // If debugger is running and paused, continue
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (adapter && adapter->isRunning()) {
            adapter->continueExecution();
            return;
        }
        // Use ILaunchService from build plugin
        auto *launchSvc = m_services->service<ILaunchService>(QStringLiteral("launchService"));
        auto *buildSys = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (launchSvc && buildSys && buildSys->hasProject()) {
            const QStringList targets = buildSys->targets();
            if (!targets.isEmpty()) {
                LaunchConfig cfg;
                cfg.executable = targets.first();
                cfg.workingDir = m_editorMgr->currentFolder();
                launchSvc->startDebugging(cfg);
                m_dockManager->showDock(dock(QStringLiteral("DebugDock")), exdock::SideBarArea::Bottom);
                return;
            }
        }
    });

    auto *runNodebugAct = runMenu->addAction(tr("Run &Without Debugging"));
    runNodebugAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F5));
    connect(runNodebugAct, &QAction::triggered, this, [this]() {
        auto *launchSvc = m_services->service<ILaunchService>(QStringLiteral("launchService"));
        auto *buildSys = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (launchSvc && buildSys && buildSys->hasProject()) {
            const QStringList targets = buildSys->targets();
            if (!targets.isEmpty()) {
                LaunchConfig cfg;
                cfg.executable = targets.first();
                cfg.workingDir = m_editorMgr->currentFolder();
                launchSvc->startWithoutDebugging(cfg);
                m_dockManager->showDock(dock(QStringLiteral("RunDock")), exdock::SideBarArea::Bottom);
                return;
            }
        }
    });

    runMenu->addSeparator();

    auto *stopRunAct = runMenu->addAction(tr("&Stop"));
    stopRunAct->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
    connect(stopRunAct, &QAction::triggered, this, [this]() {
        auto *launchSvc = m_services->service<ILaunchService>(QStringLiteral("launchService"));
        if (launchSvc) launchSvc->stopSession();
    });

    // ── Help ──────────────────────────────────────────────────────────────
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    QAction *aboutAction = helpMenu->addAction(tr("&About Exorcist..."));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });

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
    auto dockToggle = [this](exdock::ExDockWidget *dock) {
        return [this, dock](bool on) {
            if (on)
                m_dockManager->showDock(dock, m_dockManager->inferSide(dock));
            else
                m_dockManager->closeDock(dock);
        };
    };
    connect(toggleProjectAction,  &QAction::toggled, this, dockToggle(dock(QStringLiteral("ProjectDock"))));
    connect(toggleSearchAction,   &QAction::toggled, this, [this](bool on) {
        if (on) {
            m_dockManager->showDock(dock(QStringLiteral("SearchDock")), m_dockManager->inferSide(dock(QStringLiteral("SearchDock"))));
            m_searchPanel->activateSearch();
        } else {
            m_dockManager->closeDock(dock(QStringLiteral("SearchDock")));
        }
    });
    connect(toggleGitAction,      &QAction::toggled, this, dockToggle(dock(QStringLiteral("GitDock"))));
    connect(toggleTerminalAction, &QAction::toggled, this, dockToggle(dock(QStringLiteral("TerminalDock"))));
    connect(toggleAiAction,       &QAction::toggled, this, dockToggle(dock(QStringLiteral("AIDock"))));
    connect(toggleOutlineAction,  &QAction::toggled, this, dockToggle(dock(QStringLiteral("OutlineDock"))));
    connect(toggleRefsAction,     &QAction::toggled, this, dockToggle(dock(QStringLiteral("ReferencesDock"))));
    connect(toggleLogAction,      &QAction::toggled, this, dockToggle(dock(QStringLiteral("RequestLogDock"))));
    connect(toggleTrajectoryAction, &QAction::toggled, this, dockToggle(dock(QStringLiteral("TrajectoryDock"))));
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
        if (on) m_memoryBrowser->refresh();
    });
    connect(toggleMcpAction,    &QAction::toggled, this, dockToggle(dock(QStringLiteral("McpDock"))));
    connect(togglePluginAction, &QAction::toggled, this, dockToggle(dock(QStringLiteral("PluginDock"))));
    connect(toggleThemeAction,  &QAction::toggled, this, dockToggle(dock(QStringLiteral("ThemeDock"))));
    connect(toggleOutputAction, &QAction::toggled, this, dockToggle(dock(QStringLiteral("OutputDock"))));
    connect(toggleDebugAction,  &QAction::toggled, this, dockToggle(dock(QStringLiteral("DebugDock"))));

    // Sync View-menu checkbox with dock state changes.
    // Use QSignalBlocker to prevent setChecked from firing toggled,
    // which would re-enter showDock/closeDock via dockToggle.
    auto syncAction = [](QAction *action, exdock::ExDockWidget *dock) {
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
    syncAction(toggleDebugAction,      dock(QStringLiteral("DebugDock")));
}

void MainWindow::setupToolBar()
{
    auto *bar = addToolBar(tr("Main"));
    bar->setMovable(false);

    bar->addAction(tr("New"),  this, &MainWindow::openNewTab);
    bar->addAction(tr("Open"), this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    bar->addAction(tr("Save"), this, &MainWindow::saveCurrentTab);
    bar->addSeparator();
    bar->addAction(tr("Folder"), this, [this]() { openFolder(); });

    // Build toolbar is contributed by the build plugin — added in loadPlugins().
}

void MainWindow::createDockWidgets()
{
    using namespace exdock;

    // ── Create DockManager (takes over centralWidget) ─────────────────────
    m_dockManager = new DockManager(this, this);


    // (ThemeManager connection deferred — m_themeManager is created later.)

    // Build the central editor container (breadcrumb + tab widget) and set
    // it as the center content BEFORE any dock panels are added, so that
    // Bottom/Top areas can find the editor position in the center splitter.
    auto *centralContainer = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_editorMgr->breadcrumb());
    centralLayout->addWidget(m_editorMgr->tabs());
    m_dockManager->setCentralContent(centralContainer);


    // ── Project tree ──────────────────────────────────────────────────────
    auto *dkProjectDock = new ExDockWidget(tr("Project"), this);
    dkProjectDock->setDockId(QStringLiteral("ProjectDock"));

    m_editorMgr->setTreeModel(new SolutionTreeModel(m_editorMgr->projectManager(), m_gitService, this));

    m_editorMgr->setFileTree(new QTreeView);
    m_editorMgr->fileTree()->setModel(m_editorMgr->treeModel());
    m_editorMgr->fileTree()->setHeaderHidden(true);
    m_editorMgr->fileTree()->setIndentation(14);
    m_editorMgr->fileTree()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_editorMgr->fileTree(), &QTreeView::doubleClicked, this, &MainWindow::openFileFromIndex);
    connect(m_editorMgr->fileTree(), &QTreeView::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);

    dkProjectDock->setContentWidget(m_editorMgr->fileTree());
    m_dockManager->addDockWidget(dkProjectDock, SideBarArea::Left, /*startPinned=*/true);

    // ── Search ────────────────────────────────────────────────────────────
    m_searchPanel = new SearchPanel(m_searchService, this);
    auto *dkSearchDock = new ExDockWidget(tr("Search"), this);
    dkSearchDock->setDockId(QStringLiteral("SearchDock"));
    dkSearchDock->setContentWidget(m_searchPanel);
    m_dockManager->addDockWidget(dkSearchDock, SideBarArea::Left, /*startPinned=*/false);

    // Search result → navigate to file:line
    connect(m_searchPanel, &SearchPanel::resultActivated,
            this, [this](const QString &filePath, int line, int col) {
        navigateToLocation(filePath, line - 1, col);  // panel uses 1-based
    });

    m_gitPanel = new GitPanel(m_gitService, this);
    auto *dkGitDock = new ExDockWidget(tr("Git"), this);
    dkGitDock->setDockId(QStringLiteral("GitDock"));
    dkGitDock->setContentWidget(m_gitPanel);
    m_dockManager->addDockWidget(dkGitDock, SideBarArea::Left, /*startPinned=*/false);

    // Generate commit message with AI
    connect(m_gitPanel, &GitPanel::generateCommitMessageRequested,
            this, [this]() {
        if (!m_gitService || !m_gitService->isGitRepo()) return;
        // Async diff — Manifesto #2: never block UI thread
        statusBar()->showMessage(tr("Reading changes..."), 0);
        m_gitService->diffAsync({});
    });
    connect(m_gitService, &GitService::diffReady, this,
            [this](const QString & /*filePath*/, const QString &diff) {
        if (diff.trimmed().isEmpty()) {
            statusBar()->showMessage(tr("No changes to generate commit message for"), 3000);
            return;
        }
        const QString prompt = tr("Generate a concise, conventional commit message "
                                  "for these changes:\n\n```diff\n%1\n```").arg(diff.left(8000));
        m_chatPanel->focusInput();
        // Use the orchestrator to generate and insert into the commit input
        if (m_agentOrchestrator && m_agentOrchestrator->activeProvider()) {
            AgentRequest req;
            req.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            req.intent = AgentIntent::SuggestCommitMessage;
            req.userPrompt = prompt;
            req.workspaceRoot = m_editorMgr->currentFolder();
            statusBar()->showMessage(tr("Generating commit message..."), 0);
            m_agentOrchestrator->sendRequest(req);
            // Connect one-shot to receive the response
            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = connect(m_agentOrchestrator, &AgentOrchestrator::responseFinished,
                            this, [this, conn](const QString &, const AgentResponse &resp) {
                disconnect(*conn);
                QString msg = resp.text.trimmed();
                // Strip markdown code block wrapping if present
                if (msg.startsWith(QLatin1String("```")))
                    msg = msg.mid(msg.indexOf(QLatin1Char('\n')) + 1);
                if (msg.endsWith(QLatin1String("```")))
                    msg.chop(3);
                msg = msg.trimmed();
                m_gitPanel->commitMessageInput()->setText(msg);
                statusBar()->showMessage(tr("Commit message generated"), 3000);
            });
        }
    });

    // Resolve merge conflicts with AI
    connect(m_gitPanel, &GitPanel::resolveConflictsRequested,
            this, [this]() {
        if (!m_gitService || !m_gitService->isGitRepo()) return;
        const QStringList conflicts = m_gitService->conflictFiles();
        if (conflicts.isEmpty()) {
            statusBar()->showMessage(tr("No merge conflicts found"), 3000);
            return;
        }
        // Build a prompt with all conflict file contents
        QString prompt = tr("Resolve the following merge conflicts. "
                            "For each file, output the fully resolved content.\n\n");
        for (const QString &file : conflicts) {
            const QString content = m_gitService->conflictContent(file);
            const QString relPath = QDir(m_editorMgr->currentFolder()).relativeFilePath(file);
            prompt += QStringLiteral("### %1\n```\n%2\n```\n\n")
                          .arg(relPath, content.left(4000));
        }
        m_chatPanel->setInputText(QStringLiteral("/resolve ") + prompt.left(12000));
        m_chatPanel->focusInput();
        m_dockManager->showDock(dock(QStringLiteral("AIDock")), exdock::SideBarArea::Right);
    });

    // ── Terminal ──────────────────────────────────────────────────────────
    m_terminal     = new TerminalPanel(this);
    auto *dkTerminalDock = new ExDockWidget(tr("Terminal"), this);
    dkTerminalDock->setDockId(QStringLiteral("TerminalDock"));
    dkTerminalDock->setContentWidget(m_terminal);
    m_dockManager->addDockWidget(dkTerminalDock, SideBarArea::Bottom, /*startPinned=*/true);


    // ── AI ────────────────────────────────────────────────────────────────
    auto *dkAIDock = new ExDockWidget(tr("AI"), this);
    dkAIDock->setDockId(QStringLiteral("AIDock"));
    m_chatPanel = new ChatPanelWidget(m_agentOrchestrator, nullptr);
    // NOTE: AgentController is wired after it is created in the constructor.
    dkAIDock->setContentWidget(m_chatPanel);
    m_dockManager->addDockWidget(dkAIDock, SideBarArea::Right, /*startPinned=*/false);
    // Manifesto #9: start network monitor only when user opens AI panel
    connect(dkAIDock, &ExDockWidget::stateChanged, this, [this](exdock::DockState state) {
        if (state != exdock::DockState::Closed && m_aiServices && m_aiServices->networkMonitor())
            m_aiServices->networkMonitor()->start();
    });


    // ── Agent Dashboard (operational view) ────────────────────────────────
    {
        auto *dashPanel = new AgentDashboardPanel(nullptr);
        auto *dkDashDock = new ExDockWidget(tr("Agent Dashboard"), this);
        dkDashDock->setDockId(QStringLiteral("AgentDashboardDock"));
        dkDashDock->setContentWidget(dashPanel);
        m_dockManager->addDockWidget(dkDashDock, SideBarArea::Right, /*startPinned=*/false);
        // Register panel in ServiceRegistry so bootstrap can find it later
        if (m_services)
            m_services->registerService(QStringLiteral("agentDashboardPanel"), dashPanel);
    }


    // ── Symbol outline ────────────────────────────────────────────────────
    m_symbolPanel = new SymbolOutlinePanel(this);
    auto *dkOutlineDock = new ExDockWidget(tr("Outline"), this);
    dkOutlineDock->setDockId(QStringLiteral("OutlineDock"));
    dkOutlineDock->setContentWidget(m_symbolPanel);
    m_dockManager->addDockWidget(dkOutlineDock, SideBarArea::Left, /*startPinned=*/false);

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

    // ── References ────────────────────────────────────────────────────────
    m_referencesPanel = new ReferencesPanel(this);
    auto *dkReferencesDock = new ExDockWidget(tr("References"), this);
    dkReferencesDock->setDockId(QStringLiteral("ReferencesDock"));
    dkReferencesDock->setContentWidget(m_referencesPanel);
    m_dockManager->addDockWidget(dkReferencesDock, SideBarArea::Bottom, /*startPinned=*/false);

    connect(m_referencesPanel, &ReferencesPanel::referenceActivated,
            this, &MainWindow::navigateToLocation);

    // ── Request Log (Debug) ───────────────────────────────────────────────
    m_requestLogPanel = new RequestLogPanel(this);
    auto *dkRequestLogDock = new ExDockWidget(tr("Request Log"), this);
    dkRequestLogDock->setDockId(QStringLiteral("RequestLogDock"));
    dkRequestLogDock->setContentWidget(m_requestLogPanel);
    m_dockManager->addDockWidget(dkRequestLogDock, SideBarArea::Bottom, /*startPinned=*/false);

    // ── Trajectory Viewer (Debug) ─────────────────────────────────────────
    m_trajectoryPanel = new TrajectoryPanel(this);
    auto *dkTrajectoryDock = new ExDockWidget(tr("Trajectory"), this);
    dkTrajectoryDock->setDockId(QStringLiteral("TrajectoryDock"));
    dkTrajectoryDock->setContentWidget(m_trajectoryPanel);
    m_dockManager->addDockWidget(dkTrajectoryDock, SideBarArea::Bottom, /*startPinned=*/false);

    // ── Settings (dialog, not dock) ──────────────────────────────────────────
    m_settingsPanel = new SettingsPanel(this);
    m_settingsPanel->hide();

    // ── Memory Browser ────────────────────────────────────────────────────
    const QString memPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/memories");
    m_symbolIndex  = new SymbolIndex(this);
    m_memoryBrowser = new MemoryBrowserPanel(memPath, this);
    // setBrainService deferred — m_agentPlatform is not yet created here
    auto *dkMemoryDock = new ExDockWidget(tr("Memory"), this);
    dkMemoryDock->setDockId(QStringLiteral("MemoryDock"));
    dkMemoryDock->setContentWidget(m_memoryBrowser);
    m_dockManager->addDockWidget(dkMemoryDock, SideBarArea::Right, /*startPinned=*/false);

    // ── Debug Panel — contributed by debug plugin (plugins/debug/) ───────
    // DebugDock view, IDebugAdapter ("debugAdapter"), and IDebugService
    // ("debugService") are registered by the plugin. Post-plugin wiring
    // connects IDebugService signals in loadPlugins().

    // ── Theme Manager ─────────────────────────────────────────────────────
    m_themeManager = new ThemeManager(this);
    // Wire theme → dock stylesheet (deferred from createDockWidgets)
    connect(m_themeManager, &ThemeManager::themeChanged,
            m_dockManager, &DockManager::applyDockStyleSheet);

    // ── Theme Gallery panel ───────────────────────────────────────────────
    m_themeGallery = new ThemeGalleryPanel(this);
    m_themeGallery->setThemeManager(m_themeManager);
    auto *dkThemeDock = new ExDockWidget(tr("Themes"), this);
    dkThemeDock->setDockId(QStringLiteral("ThemeDock"));
    dkThemeDock->setContentWidget(m_themeGallery);
    m_dockManager->addDockWidget(dkThemeDock, SideBarArea::Left, /*startPinned=*/false);

    // ── Diff Viewer panel ─────────────────────────────────────────────────
    m_diffViewer = new DiffViewerPanel(this);
    auto *dkDiffDock = new ExDockWidget(tr("Diff Viewer"), this);
    dkDiffDock->setDockId(QStringLiteral("DiffDock"));
    dkDiffDock->setContentWidget(m_diffViewer);
    m_dockManager->addDockWidget(dkDiffDock, SideBarArea::Bottom, /*startPinned=*/false);

    // ── Proposed Edit panel ───────────────────────────────────────────────
    m_proposedEditPanel = new ProposedEditPanel(this);
    auto *dkProposedEditDock = new ExDockWidget(tr("Proposed Edits"), this);
    dkProposedEditDock->setDockId(QStringLiteral("ProposedEditDock"));
    dkProposedEditDock->setContentWidget(m_proposedEditPanel);
    m_dockManager->addDockWidget(dkProposedEditDock, SideBarArea::Bottom, /*startPinned=*/false);

    connect(m_proposedEditPanel, &ProposedEditPanel::editAccepted,
            this, [this](const QString &filePath, const QString &newText) {
        // Write accepted edit to file and reload in editor
        QFile f(filePath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            f.write(newText.toUtf8());
            f.close();
        }
        // Update the open editor tab if present
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *editor = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
            if (editor && editor->property("filePath").toString() == filePath) {
                editor->setPlainText(newText);
                break;
            }
        }
    });

    // ── MCP client + panel ────────────────────────────────────────────────
    m_mcpClient = new McpClient(this);
    if (m_services) {
        if (auto *bc = m_services->service<BridgeClient>(QStringLiteral("bridgeClient")))
            m_mcpClient->setBridgeClient(bc);
    }
    m_mcpPanel  = new McpPanel(m_mcpClient, this);
    auto *dkMcpDock = new ExDockWidget(tr("MCP Servers"), this);
    dkMcpDock->setDockId(QStringLiteral("McpDock"));
    dkMcpDock->setContentWidget(m_mcpPanel);
    m_dockManager->addDockWidget(dkMcpDock, SideBarArea::Right, /*startPinned=*/false);

    // ── Plugin Gallery panel ──────────────────────────────────────────────
    m_pluginGallery = new PluginGalleryPanel(this);
    auto *dkPluginDock = new ExDockWidget(tr("Extensions"), this);
    dkPluginDock->setDockId(QStringLiteral("PluginDock"));
    dkPluginDock->setContentWidget(m_pluginGallery);
    m_dockManager->addDockWidget(dkPluginDock, SideBarArea::Left, /*startPinned=*/false);

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

        auto *dkProblemsDock = new ExDockWidget(tr("Problems"), this);
        dkProblemsDock->setDockId(QStringLiteral("ProblemsDock"));
        dkProblemsDock->setContentWidget(problemsPanel);
        m_dockManager->addDockWidget(dkProblemsDock, SideBarArea::Bottom, /*startPinned=*/false);

        connect(problemsPanel, &ProblemsPanel::navigateToFile,
                this, &MainWindow::navigateToLocation);
    }

    // ── Diff Explorer panel ────────────────────────────────────────────────
    {
        auto *diffExplorer = new DiffExplorerPanel(m_gitService, this);

        auto *dkDiffDock = new ExDockWidget(tr("Diff Explorer"), this);
        dkDiffDock->setDockId(QStringLiteral("DiffExplorerDock"));
        dkDiffDock->setContentWidget(diffExplorer);
        m_dockManager->addDockWidget(dkDiffDock, SideBarArea::Bottom, /*startPinned=*/false);
    }

    // ── Merge Editor panel ─────────────────────────────────────────────────
    {
        auto *mergeEditor = new MergeEditor(m_gitService, this);

        auto *dkMergeDock = new ExDockWidget(tr("Merge Editor"), this);
        dkMergeDock->setDockId(QStringLiteral("MergeEditorDock"));
        dkMergeDock->setContentWidget(mergeEditor);
        m_dockManager->addDockWidget(dkMergeDock, SideBarArea::Bottom, /*startPinned=*/false);
    }

    // ── File Watch Service ────────────────────────────────────────────────
    m_fileWatcher = new FileWatchService(this);
    connect(m_fileWatcher, &FileWatchService::fileChangedExternally,
            this, [this](const QString &path) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *editor = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
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

    // When MCP discovers new tools, register them in the ToolRegistry
    connect(m_mcpPanel, &McpPanel::toolsChanged, this, [this]() {
        const QList<McpToolInfo> tools = m_mcpClient->allTools();
        for (const McpToolInfo &t : tools) {
            const QString regName = QStringLiteral("mcp_%1_%2").arg(t.serverName, t.name);
            if (!m_agentPlatform->toolRegistry()->hasTool(regName))
                m_agentPlatform->toolRegistry()->registerTool(std::make_unique<McpToolAdapter>(m_mcpClient, t));
        }
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
    if (!m_fileSystem->exists(path)) {
        statusBar()->showMessage(tr("File not found: %1").arg(path), 4000);
        return;
    }

    // Track in recent files
    if (auto *rfm = findChild<RecentFilesManager *>(
            QStringLiteral("recentFilesManager"))) {
        rfm->addFile(path);
    }

    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        if (m_editorMgr->tabs()->widget(i)->property("filePath").toString() == path) {
            m_editorMgr->tabs()->setCurrentIndex(i);
            return;
        }
    }

    auto *editor = new EditorView();
    constexpr qint64 kLargeFileThreshold = 10 * 1024 * 1024;
    LargeFileLoader::applyToEditor(editor, path, kLargeFileThreshold);
    editor->setProperty("filePath", path);
    HighlighterFactory::create(path, editor->document());

    // Apply saved editor settings
    {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        s.beginGroup(QStringLiteral("editor"));
        const QFont font(s.value(QStringLiteral("fontFamily"), QStringLiteral("Consolas")).toString(),
                         s.value(QStringLiteral("fontSize"), 11).toInt());
        const int tabSize = s.value(QStringLiteral("tabSize"), 4).toInt();
        const bool wrap = s.value(QStringLiteral("wordWrap"), false).toBool();
        const bool minimap = s.value(QStringLiteral("showMinimap"), false).toBool();
        s.endGroup();

        editor->setFont(font);
        editor->setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * tabSize);
        editor->setWordWrapMode(wrap ? QTextOption::WordWrap : QTextOption::NoWrap);
        editor->setMinimapVisible(minimap);
    }

    // Set language ID on the editor for inline chat / inline completion
    const QString langId = LspClient::languageIdForPath(path);
    editor->setLanguageId(langId);

    // Apply language profile overrides (tab size, indent style)
    if (auto *lpm = findChild<LanguageProfileManager *>(
            QStringLiteral("languageProfileManager"))) {
        QSettings gs(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        gs.beginGroup(QStringLiteral("editor"));
        const int globalTab = gs.value(QStringLiteral("tabSize"), 4).toInt();
        gs.endGroup();

        const int langTab = lpm->tabSize(langId, globalTab);
        if (langTab != globalTab) {
            const QFont f = editor->font();
            editor->setTabStopDistance(
                QFontMetricsF(f).horizontalAdvance(QLatin1Char(' ')) * langTab);
        }

        // Activate language-specific plugins when a file of this language opens
        if (m_pluginManager && lpm->isActive(langId))
            m_pluginManager->activateByLanguageProfile(langId);
    }

    // Ctrl+I inline chat
    connect(editor, &EditorView::inlineChatRequested,
            this, [this, editor, path](const QString &sel, const QString &lang) {
        showInlineChat(editor, sel, path, lang);
    });

    // AI context menu: wire all five actions to chat panel
    auto wireAiAction = [this](const QString &sel, const QString &fp,
                               const QString &lang, const QString &cmd) {
        m_chatPanel->setEditorContext(fp, sel, lang);
        m_chatPanel->focusInput();
        // Build the slash command text and insert it
        const QString prompt = sel.isEmpty()
            ? cmd
            : QStringLiteral("%1 %2").arg(cmd, sel.left(200));
        m_chatPanel->setInputText(prompt);
    };
    connect(editor, &EditorView::aiExplainRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/explain"));
    });
    connect(editor, &EditorView::aiReviewRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/review"));
    });
    connect(editor, &EditorView::aiFixRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/fix"));
    });
    connect(editor, &EditorView::aiTestRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/test"));
    });
    connect(editor, &EditorView::aiDocRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/doc"));
    });

    // Alt+\ manual completion trigger
    connect(editor, &EditorView::manualCompletionRequested,
            this, [this]() {
        if (m_inlineEngine)
            m_inlineEngine->triggerCompletion();
    });

    // Review annotation navigation (F8 next, Shift+F8 prev)
    auto *nextRevAction = new QAction(editor);
    nextRevAction->setShortcut(QKeySequence(Qt::Key_F8));
    nextRevAction->setShortcutContext(Qt::WidgetShortcut);
    connect(nextRevAction, &QAction::triggered, editor, &EditorView::nextReviewAnnotation);
    editor->addAction(nextRevAction);

    auto *prevRevAction = new QAction(editor);
    prevRevAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F8));
    prevRevAction->setShortcutContext(Qt::WidgetShortcut);
    connect(prevRevAction, &QAction::triggered, editor, &EditorView::prevReviewAnnotation);
    editor->addAction(prevRevAction);

    // Wire breakpoint gutter → debug service (plugin-provided)
    connect(editor, &EditorView::breakpointToggled,
            this, [this](const QString &fp, int ln, bool added) {
        auto *debugSvc = m_services->service<IDebugService>(QStringLiteral("debugService"));
        if (added) {
            if (debugSvc) debugSvc->addBreakpointEntry(fp, ln);
            auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
            if (adapter) {
                DebugBreakpoint bp;
                bp.filePath = fp;
                bp.line = ln;
                adapter->addBreakpoint(bp);
            }
        } else {
            if (debugSvc) debugSvc->removeBreakpointEntry(fp, ln);
            // Adapter breakpoint removal requires the adapter-assigned ID,
            // which is tracked inside GdbMiAdapter. For now, re-setting
            // breakpoints on next launch is the simplest approach.
        }
    });

    createLspBridge(editor, path);

    const QString title = QFileInfo(path).fileName();
    int index = m_editorMgr->tabs()->addTab(editor, title);
    m_editorMgr->tabs()->setTabToolTip(index, QDir::toNativeSeparators(path));
    m_editorMgr->tabs()->setCurrentIndex(index);

    if (editor->isLargeFilePreview()) {
        statusBar()->showMessage(tr("Large file preview (read-only)"), 5000);
        connect(editor, &EditorView::requestMoreData, this, [editor]() {
            LargeFileLoader::appendNextChunk(editor, 2 * 1024 * 1024);
        });
    }

    // Watch for external changes
    if (m_fileWatcher && !path.isEmpty())
        m_fileWatcher->watchFile(path);

    if (m_pluginManager)
        m_pluginManager->fireLuaEvent(QStringLiteral("editor.open"), {path});
}

void MainWindow::openFolder(const QString &path)
{
    QString folder = path;
    if (folder.isEmpty()) {
        folder = QFileDialog::getExistingDirectory(this, tr("Open Folder"));
    }
    if (folder.isEmpty()) return;

    const bool hasSolution = m_editorMgr->projectManager()->openFolder(folder);
    m_gitService->setWorkingDirectory(folder);
    const QString root = m_editorMgr->projectManager()->activeSolutionDir();
    m_searchPanel->setRootPath(root);
    setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
    statusBar()->showMessage(tr("Opened: %1").arg(root), 4000);
    if (hasSolution) {
        statusBar()->showMessage(tr("Solution: %1")
            .arg(QFileInfo(m_editorMgr->projectManager()->solution().filePath).fileName()), 4000);
    }

    // Notify build plugin of workspace change (via OutputPanel/RunPanel services)
    if (auto *outPanel = qobject_cast<OutputPanel *>(m_services->service(QStringLiteral("outputPanel")))) {
        outPanel->setWorkingDirectory(root);
        const QString tasksPath = root + QStringLiteral("/.exorcist/tasks.json");
        if (QFileInfo::exists(tasksPath))
            outPanel->loadTasksFromJson(tasksPath);
    }

    if (auto *rnPanel = qobject_cast<RunLaunchPanel *>(m_services->service(QStringLiteral("runPanel")))) {
        rnPanel->setWorkingDirectory(root);
        const QString launchPath = root + QStringLiteral("/.exorcist/launch.json");
        if (QFileInfo::exists(launchPath))
            rnPanel->loadLaunchJson(launchPath);
    }
    m_editorMgr->setCurrentFolder(root);
    m_terminal->setWorkingDirectory(root);

    // Apply workspace-level settings (.exorcist/settings.json)
    if (auto *wss = m_services->service<WorkspaceSettings>(QStringLiteral("workspaceSettings")))
        wss->setWorkspaceRoot(root);

    // Activate plugins with workspaceContains activation events
    if (m_pluginManager)
        m_pluginManager->activateByWorkspace(root);

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
                statusBar()->showMessage(
                    tr("CMake project detected \u2014 click Configure to generate compile_commands.json"),
                    6000);
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
            if (m_themeManager->loadCustomTheme(themePath, &err))
                m_themeManager->setTheme(ThemeManager::Custom);
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

    // Start workspace indexer
    if (!m_workspaceIndexer) {
        m_workspaceIndexer = new WorkspaceIndexer(this);
        connect(m_workspaceIndexer, &WorkspaceIndexer::indexingStarted, this, [this] {
            m_statusBarMgr->indexLabel()->setText(tr("\u2699 Indexing..."));
        });
        connect(m_workspaceIndexer, &WorkspaceIndexer::indexingProgress,
                this, [this](int done, int total) {
            m_statusBarMgr->indexLabel()->setText(tr("\u2699 Indexing %1/%2").arg(done).arg(total));
        });
        connect(m_workspaceIndexer, &WorkspaceIndexer::indexingFinished,
                this, [this](int files, int chunks) {
            m_statusBarMgr->indexLabel()->setText(tr("\u2713 %1 files (%2 chunks)")
                                 .arg(files).arg(chunks));
            // Rebuild symbol index from indexed chunks (already in memory)
            if (m_symbolIndex) {
                m_symbolIndex->clear();
                const auto allChunks = m_workspaceIndexer->search(QString(), 50000);
                for (const auto &chunk : allChunks) {
                    if (!chunk.symbolName.isEmpty())
                        m_symbolIndex->indexFile(chunk.filePath, chunk.content);
                }
            }
        });
    }
    m_workspaceIndexer->indexWorkspace(root);
}

void MainWindow::newSolution()
{
    const QString name = QInputDialog::getText(this, tr("New Solution"), tr("Solution name"));
    if (name.trimmed().isEmpty()) {
        return;
    }

    QString slnPath = QFileDialog::getSaveFileName(this, tr("Save Solution"),
                                                   m_editorMgr->currentFolder(), tr("Exorcist Solution (*.exsln)"));
    if (slnPath.isEmpty()) {
        return;
    }
    if (!slnPath.endsWith(".exsln")) {
        slnPath += ".exsln";
    }

    if (m_editorMgr->projectManager()->createSolution(name, slnPath)) {
        const QString root = m_editorMgr->projectManager()->activeSolutionDir();
        m_gitService->setWorkingDirectory(root);
        m_searchPanel->setRootPath(root);
        setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
        m_editorMgr->setCurrentFolder(root);
        m_terminal->setWorkingDirectory(root);
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->startServer(root);
    }
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
        m_searchPanel->setRootPath(root);
        setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
        statusBar()->showMessage(tr("Solution: %1").arg(QFileInfo(slnPath).fileName()), 4000);

        m_editorMgr->setCurrentFolder(root);
        m_terminal->setWorkingDirectory(root);
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->startServer(root);
    }
}

void MainWindow::addProjectToSolution()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Add Folder to Solution"));
    if (folder.isEmpty()) {
        return;
    }
    const QString name = QInputDialog::getText(this, tr("Project Name"), tr("Project name"));
    const QString projectName = name.trimmed().isEmpty() ? QFileInfo(folder).baseName() : name.trimmed();

    if (m_editorMgr->projectManager()->addProject(projectName, folder)) {
        if (!m_editorMgr->projectManager()->solution().filePath.isEmpty()) {
            m_editorMgr->projectManager()->saveSolution();
        }
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
    if (!m_symbolIndex) return;

    CommandPalette palette(CommandPalette::CommandMode, this);

    const auto allSyms = m_symbolIndex->search(QString(), 500);
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

    const QString lastFolder = s.value(QStringLiteral("lastFolder")).toString();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists())
        openFolder(lastFolder);
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
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_agentPlatform && m_agentPlatform->brainService())
        m_agentPlatform->brainService()->save();
    QMainWindow::closeEvent(event);
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
        QWidget *w = m_editorMgr->tabs()->widget(tabIndex);
        m_editorMgr->tabs()->removeTab(tabIndex);
        if (w) w->deleteLater();
    });

    // Close All Tabs
    menu.addAction(tr("Close All Tabs"), this, [this]() {
        while (m_editorMgr->tabs()->count() > 0) {
            QWidget *w = m_editorMgr->tabs()->widget(0);
            m_editorMgr->tabs()->removeTab(0);
            if (w) w->deleteLater();
        }
    });

    // Close Other Tabs
    menu.addAction(tr("Close Other Tabs"), this, [this, tabIndex]() {
        QWidget *keep = m_editorMgr->tabs()->widget(tabIndex);
        for (int i = m_editorMgr->tabs()->count() - 1; i >= 0; --i) {
            if (m_editorMgr->tabs()->widget(i) != keep) {
                QWidget *w = m_editorMgr->tabs()->widget(i);
                m_editorMgr->tabs()->removeTab(i);
                if (w) w->deleteLater();
            }
        }
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
    return qobject_cast<EditorView *>(m_editorMgr->tabs()->currentWidget());
}

// ── LSP ───────────────────────────────────────────────────────────────────────

void MainWindow::createLspBridge(EditorView *editor, const QString &path)
{
    auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
    if (!lspClient) return;

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
        auto *editor = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
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
    // Open bridges for any tabs that were already open before LSP was ready.
    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        auto *editor = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
        if (!editor) continue;
        const QString path = editor->property("filePath").toString();
        if (path.isEmpty()) continue;
        // Avoid double-bridging (bridge already exists as a child of editor)
        if (editor->findChild<LspEditorBridge *>()) continue;
        createLspBridge(editor, path);
    }
}

void MainWindow::applyWorkspaceEdit(const QJsonObject &workspaceEdit)
{
    auto applyEdits = [this](const QString &filePath, const QJsonArray &edits) {
        if (filePath.isEmpty() || edits.isEmpty()) return;
        openFile(filePath);
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *editor = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
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
        auto *editor = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
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
        m_terminal->setWorkingDirectory(dirPath);
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
    const QString pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
    int loaded = m_pluginManager->loadPluginsFrom(pluginDir);

    // Wire plugin gallery panel
    if (m_pluginGallery) {
        m_pluginGallery->setPluginManager(m_pluginManager.get());

        // Marketplace service — owned by gallery panel (no MainWindow member)
        auto *marketplace = new PluginMarketplaceService(m_pluginGallery);
        marketplace->setPluginManager(m_pluginManager.get());
        marketplace->setPluginsDirectory(pluginDir);
        m_services->registerService(QStringLiteral("pluginMarketplace"), marketplace);

        // Load bundled registry if present
        const QString registryPath = QCoreApplication::applicationDirPath()
                                     + QLatin1String("/plugin_registry.json");
        if (QFile::exists(registryPath)) {
            marketplace->loadRegistryFromFile(registryPath);
            m_pluginGallery->loadRegistryFromFile(registryPath);
        }

        // Wire install button → download & install
        connect(m_pluginGallery, &PluginGalleryPanel::installRequested,
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
                m_pluginGallery, &PluginGalleryPanel::refreshInstalled);
    }

    // Create SDK host services for the new plugin interface
    m_hostServices = new HostServices(this, this);
    m_hostServices->initSubsystemServices(m_fileSystem.get(), m_gitService, m_terminal);
    m_hostServices->initUIManagers();
    m_hostServices->setServiceRegistry(m_services.get());

    // LSP diagnostics → SDK DiagnosticsService: deferred to post-plugin wiring

    // Register core IDE commands so plugins can invoke them
    auto *cmdSvc = m_hostServices->commandService();
    cmdSvc->registerCommand(QStringLiteral("workbench.action.newTab"), tr("New Tab"),
                            [this]() { openNewTab(); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.openFile"), tr("Open File..."),
                            [this]() {
        const QString p = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!p.isEmpty()) openFile(p);
    });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.openFolder"), tr("Open Folder..."),
                            [this]() { openFolder(); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.save"), tr("Save"),
                            [this]() { saveCurrentTab(); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.goToFile"), tr("Go to File..."),
                            [this]() { showFilePalette(); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.findReplace"), tr("Find / Replace"),
                            [this]() { if (auto *e = currentEditor()) e->showFindBar(); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.toggleProject"), tr("Toggle Project Panel"),
                            [this]() { m_dockManager->toggleDock(dock(QStringLiteral("ProjectDock"))); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.toggleSearch"), tr("Toggle Search Panel"),
                            [this]() { m_dockManager->toggleDock(dock(QStringLiteral("SearchDock"))); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.toggleTerminal"), tr("Toggle Terminal Panel"),
                            [this]() { m_dockManager->toggleDock(dock(QStringLiteral("TerminalDock"))); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.toggleAI"), tr("Toggle AI Panel"),
                            [this]() { m_dockManager->toggleDock(dock(QStringLiteral("AIDock"))); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.goToSymbol"), tr("Go to Symbol..."),
                            [this]() { showSymbolPalette(); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.quit"), tr("Quit"),
                            [this]() { close(); });
    cmdSvc->registerCommand(QStringLiteral("workbench.action.commandPalette"), tr("Command Palette"),
                            [this]() { showCommandPalette(); });

    // Create contribution registry for wiring plugin manifests into the IDE
    m_contributions = new ContributionRegistry(this, m_hostServices->commandService(), this);

    // Give PluginManager access to ContributionRegistry for suspend/resume
    m_pluginManager->setContributionRegistry(m_contributions);

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

    // Process plugin manifests → wire contributions into the IDE
    for (const auto &lp : m_pluginManager->loadedPlugins()) {
        if (lp.manifest.hasContributions()) {
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

    // ── Post-plugin wiring: build toolbar, ProblemsPanel → OutputPanel ──
    if (auto *toolbar = qobject_cast<QToolBar *>(m_services->service(QStringLiteral("buildToolbar")))) {
        addToolBar(toolbar);
    }
    if (auto *outPanel = qobject_cast<OutputPanel *>(m_services->service(QStringLiteral("outputPanel")))) {
        if (auto *pp = qobject_cast<ProblemsPanel *>(m_services->service(QStringLiteral("problemsPanel")))) {
            pp->setOutputPanel(outPanel);
        }
    }

    // ── Post-plugin wiring: debug service signals → editor integration ──
    if (auto *debugSvc = m_services->service<IDebugService>(QStringLiteral("debugService"))) {
        // Navigate to source on stack frame double-click
        connect(debugSvc, &IDebugService::navigateToSource,
                this, [this](const QString &filePath, int line) {
            navigateToLocation(filePath, line - 1, 0);
        });

        // Highlight current stopped line in editor
        connect(debugSvc, &IDebugService::debugStopped,
                this, [this](const QList<DebugFrame> &frames) {
            for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                auto *ed = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
                if (ed) ed->setCurrentDebugLine(0);
            }
            if (!frames.isEmpty()) {
                const auto &top = frames.first();
                for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                    auto *ed = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
                    if (ed && ed->property("filePath").toString() == top.filePath) {
                        ed->setCurrentDebugLine(top.line);
                        m_editorMgr->tabs()->setCurrentIndex(i);
                        break;
                    }
                }
            }
        });

        // Clear debug line on session end
        connect(debugSvc, &IDebugService::debugTerminated,
                this, [this]() {
            for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                auto *ed = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
                if (ed) ed->setCurrentDebugLine(0);
            }
        });
    }

    // ── Post-plugin wiring: LSP service signals → editor integration ──
    if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService"))) {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));

        // Server ready → initialize LSP client with workspace root
        connect(lspSvc, &ILspService::serverReady, this, [this, lspClient]() {
            if (lspClient) lspClient->initialize(m_editorMgr->currentFolder());
        });

        // LSP initialized → create bridges for already-open tabs
        if (lspClient) {
            connect(lspClient, &LspClient::initialized,
                    this, &MainWindow::onLspInitialized);
        }

        // Go-to-Definition (F12)
        connect(lspSvc, &ILspService::navigateToLocation,
                this, &MainWindow::navigateToLocation);

        // Find References → References panel
        connect(lspSvc, &ILspService::referencesReady,
                this, [this](const QJsonArray &locations) {
            if (m_referencesPanel)
                m_referencesPanel->showReferences(tr("Symbol"), locations);
            if (dock(QStringLiteral("ReferencesDock")))
                m_dockManager->showDock(dock(QStringLiteral("ReferencesDock")), exdock::SideBarArea::Bottom);
        });

        // Rename Symbol → apply workspace edit
        connect(lspSvc, &ILspService::workspaceEditRequested,
                this, &MainWindow::applyWorkspaceEdit);

        // LSP status messages (e.g. "No definition found")
        connect(lspSvc, &ILspService::statusMessage,
                this, [this](const QString &msg, int timeout) {
            statusBar()->showMessage(msg, timeout);
        });

        // Wire LSP diagnostics push to agent
        if (lspClient && m_agentPlatform) {
            if (auto *notifier = m_agentPlatform->diagnosticsNotifier()) {
                connect(lspClient, &LspClient::diagnosticsPublished,
                        notifier, &DiagnosticsNotifier::onDiagnosticsPublished);
            }
        }

        // Wire LSP diagnostics into the SDK DiagnosticsService
        if (lspClient && m_hostServices) {
            connect(lspClient, &LspClient::diagnosticsPublished,
                    m_hostServices->diagnosticsService(),
                    &DiagnosticsServiceImpl::onDiagnosticsPublished);
        }
    }

    if (m_agentPlatform)
        m_agentPlatform->registerPluginProviders(m_pluginManager.get());

    // Wire inline completion engine to the first provider that supports it
    for (IAgentProvider *p : m_agentOrchestrator->providers()) {
        if (p->capabilities() & AgentCapability::InlineCompletion) {
            m_inlineEngine->setProvider(p);
            m_nesEngine->setProvider(p);
            break;
        }
    }

    // Update inline engine provider when active provider changes
    connect(m_agentOrchestrator, &AgentOrchestrator::activeProviderChanged,
            this, [this](const QString &) {
        IAgentProvider *active = m_agentOrchestrator->activeProvider();
        if (active && (active->capabilities() & AgentCapability::InlineCompletion)) {
            m_inlineEngine->setProvider(active);
            m_nesEngine->setProvider(active);
        }
    });

    // Update status bar on AI errors
    connect(m_agentOrchestrator, &AgentOrchestrator::responseError,
            this, [this](const QString &, const AgentError &error) {
        switch (error.code) {
        case AgentError::Code::AuthError:
            m_statusBarMgr->copilotStatusLabel()->setText(tr("\u26A0 Auth Error"));
            m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#f44747;"));
            m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("Authentication failed — check API key"));
            break;
        case AgentError::Code::RateLimited:
            m_statusBarMgr->copilotStatusLabel()->setText(tr("\u23F2 Rate Limited"));
            m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#ffb74d;"));
            m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("Rate limited — waiting for cooldown"));
            // Restore after 60s
            QTimer::singleShot(60000, this, [this]() {
                m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2713 AI Ready"));
                m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
                m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("AI Assistant status — click to open AI panel"));
            });
            break;
        case AgentError::Code::NetworkError:
            m_statusBarMgr->copilotStatusLabel()->setText(tr("\u26A0 Offline"));
            m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#f44747;"));
            m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("Network error — check connection"));
            // Try to auto-recover after 10s
            QTimer::singleShot(10000, this, [this]() {
                if (m_agentOrchestrator && m_agentOrchestrator->activeProvider()
                    && m_agentOrchestrator->activeProvider()->isAvailable()) {
                    m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2713 AI Ready"));
                    m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
                    m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("AI Assistant status — click to open AI panel"));
                }
            });
            break;
        default:
            break;
        }
    });

    // Restore status on successful response
    connect(m_agentOrchestrator, &AgentOrchestrator::responseFinished,
            this, [this](const QString &, const AgentResponse &) {
        if (m_statusBarMgr->copilotStatusLabel()->text() != tr("\u2713 AI Ready")) {
            m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2713 AI Ready"));
            m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
            m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("AI Assistant status — click to open AI panel"));
        }
    });

    // Populate tool toggles in settings panel
    if (m_settingsPanel) {
        QStringList toolNames = m_agentPlatform->toolRegistry()->toolNames();
        toolNames.sort();
        m_settingsPanel->setToolNames(toolNames);
    }

    if (loaded > 0) {
        statusBar()->showMessage(tr("Loaded %1 plugins").arg(loaded), 4000);
    }

    // Update status bar when provider becomes available
    connect(m_agentOrchestrator, &AgentOrchestrator::providerAvailabilityChanged,
            this, [this](bool available) {
        if (available) {
            m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2713 AI Ready"));
            m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
            m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("AI Assistant — connected"));
        } else {
            m_statusBarMgr->copilotStatusLabel()->setText(tr("\u26A0 AI Offline"));
            m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#f44747;"));
            m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("AI provider not available — click to configure"));
        }
    });

    // Update status bar when provider registers
    connect(m_agentOrchestrator, &AgentOrchestrator::providerRegistered,
            this, [this](const QString &) {
        const auto *active = m_agentOrchestrator->activeProvider();
        if (active) {
            if (active->isAvailable()) {
                m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2713 AI Ready"));
                m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
            } else {
                m_statusBarMgr->copilotStatusLabel()->setText(tr("\u2026 Connecting"));
                m_statusBarMgr->copilotStatusLabel()->setStyleSheet(QStringLiteral("padding: 0 8px; color:#75bfff;"));
                m_statusBarMgr->copilotStatusLabel()->setToolTip(tr("Connecting to AI provider..."));
            }
        }
    });

    // Auto-show AI panel when at least one provider is available
    if (!m_agentOrchestrator->providers().isEmpty()) {
        m_dockManager->showDock(dock(QStringLiteral("AIDock")), exdock::SideBarArea::Right);
    }
}
