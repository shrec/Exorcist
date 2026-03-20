#include "cpplanguageplugin.h"
#include "clangdmanager.h"
#include "cppworkspacepanel.h"

#include "core/istatusbarmanager.h"
#include "lsp/lspclient.h"
#include "sdk/ibuildsystem.h"
#include "sdk/icommandservice.h"
#include "sdk/idebugservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/ilaunchservice.h"
#include "sdk/ilspservice.h"
#include "sdk/inotificationservice.h"
#include "sdk/isearchservice.h"
#include "sdk/iterminalservice.h"
#include "sdk/itestrunner.h"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <memory>

// ── LspServiceBridge ──────────────────────────────────────────────────────────
// Implements ILspService by delegating to ClangdManager + LspClient.
// Replaces LspBootstrap's signal-forwarding role.

class LspServiceBridge : public ILspService
{
    Q_OBJECT

public:
    LspServiceBridge(ClangdManager *clangd, LspClient *lspClient, QObject *parent)
        : ILspService(parent), m_clangd(clangd), m_lspClient(lspClient)
    {
        connect(m_clangd, &ClangdManager::serverReady,
                this, &ILspService::serverReady);

        // Go-to-Definition (F12) — also triggered from cpp.goToDefinition command
        connect(m_lspClient, &LspClient::definitionResult, this,
                [this](const QString & /*uri*/, const QJsonArray &locations) {
            if (locations.isEmpty()) {
                emit statusMessage(tr("No definition found"), 3000);
                return;
            }
            const QJsonObject loc = locations.first().toObject();
            const QString targetUri = loc.contains(QStringLiteral("targetUri"))
                ? loc.value(QStringLiteral("targetUri")).toString()
                : loc.value(QStringLiteral("uri")).toString();
            const QJsonObject range = loc.contains(QStringLiteral("targetSelectionRange"))
                ? loc.value(QStringLiteral("targetSelectionRange")).toObject()
                : loc.value(QStringLiteral("range")).toObject();
            const QJsonObject start = range.value(QStringLiteral("start")).toObject();
            const int line      = start.value(QStringLiteral("line")).toInt();
            const int character = start.value(QStringLiteral("character")).toInt();

            QString path = QUrl(targetUri).toLocalFile();
            if (path.isEmpty()) path = targetUri;
            emit navigateToLocation(path, line, character);
        });

        // Go-to-Declaration — same navigation path as definition
        connect(m_lspClient, &LspClient::declarationResult, this,
                [this](const QString & /*uri*/, const QJsonArray &locations) {
            if (locations.isEmpty()) {
                emit statusMessage(tr("No declaration found"), 3000);
                return;
            }
            const QJsonObject loc = locations.first().toObject();
            const QString targetUri = loc.contains(QStringLiteral("targetUri"))
                ? loc.value(QStringLiteral("targetUri")).toString()
                : loc.value(QStringLiteral("uri")).toString();
            const QJsonObject range = loc.contains(QStringLiteral("targetSelectionRange"))
                ? loc.value(QStringLiteral("targetSelectionRange")).toObject()
                : loc.value(QStringLiteral("range")).toObject();
            const QJsonObject start = range.value(QStringLiteral("start")).toObject();
            const int line      = start.value(QStringLiteral("line")).toInt();
            const int character = start.value(QStringLiteral("character")).toInt();

            QString path = QUrl(targetUri).toLocalFile();
            if (path.isEmpty()) path = targetUri;
            emit navigateToLocation(path, line, character);
        });

        // Find References
        connect(m_lspClient, &LspClient::referencesResult, this,
                [this](const QString & /*uri*/, const QJsonArray &locations) {
            if (locations.isEmpty()) {
                emit statusMessage(tr("No references found"), 3000);
                return;
            }
            emit referencesReady(locations);
        });

        // Rename Symbol
        connect(m_lspClient, &LspClient::renameResult, this,
                [this](const QString & /*uri*/, const QJsonObject &workspaceEdit) {
            emit workspaceEditRequested(workspaceEdit);
        });
    }

    void startServer(const QString &workspaceRoot,
                     const QStringList &args = {}) override
    {
        m_clangd->start(workspaceRoot, args);
    }

    void stopServer() override
    {
        m_clangd->stop();
    }

private:
    ClangdManager *m_clangd;
    LspClient     *m_lspClient;
};

// ── CppLanguagePlugin ─────────────────────────────────────────────────────────

namespace {

constexpr auto kCppMenuId = "cpp.language";
constexpr auto kCppToolBarId = "cpp";
constexpr auto kCppStatusItemId = "cpp.language.status";
constexpr auto kCppWorkspaceDockId = "CppWorkspaceDock";

}

CppLanguagePlugin::CppLanguagePlugin(QObject *parent)
    : QObject(parent)
{
    m_workspaceRefreshTimer.setInterval(1500);
    connect(&m_workspaceRefreshTimer, &QTimer::timeout,
            this, &CppLanguagePlugin::refreshWorkspaceSnapshot);
}

PluginInfo CppLanguagePlugin::info() const
{
    PluginInfo pi;
    pi.id      = QStringLiteral("org.exorcist.cpp-language");
    pi.name    = QStringLiteral("C/C++ Language Support");
    pi.version = QStringLiteral("1.0.0");
    return pi;
}

QWidget *CppLanguagePlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId != QLatin1String(kCppWorkspaceDockId))
        return nullptr;

    if (!m_workspacePanel) {
        m_workspacePanel = new CppWorkspacePanel(parent);
        connect(m_workspacePanel, &CppWorkspacePanel::commandRequested,
                this, [this](const QString &commandId) {
            executeCommand(commandId);
            refreshWorkspaceSnapshot();
        });
        connect(m_workspacePanel, &CppWorkspacePanel::panelRequested,
                this, [this](const QString &panelId) {
            showPanel(panelId);
        });
        connect(m_workspacePanel, &CppWorkspacePanel::buildTerminalRequested,
                this, [this]() {
            const QString buildDir = currentBuildDirectory();
            if (buildDir.isEmpty() || !terminal()) {
                showWarning(tr("Build directory is not available yet."));
                return;
            }
            terminal()->setWorkingDirectory(buildDir);
            showTerminalPanel();
            terminal()->openTerminal();
        });
        connect(m_workspacePanel, &CppWorkspacePanel::fileOpenRequested,
                this, [this](const QString &filePath) {
            if (editor())
                editor()->openFile(filePath, -1, -1);
        });
    }

    m_workspacePanel->setParent(parent);
    updateWorkspacePanel();
    return m_workspacePanel;
}

bool CppLanguagePlugin::initializePlugin()
{
    m_isShuttingDown = false;
    m_expectRestart = false;
    m_restartCount = 0;
    m_languageStatusTitle = tr("Starting");
    m_languageStatusDetail = tr("Preparing clangd for the active workspace.");
    m_buildStatusTitle = tr("Waiting");
    m_buildStatusDetail = tr("Build system service has not been resolved yet.");
    m_debugStatusTitle = tr("Idle");
    m_debugStatusDetail = tr("No debug session is active.");
    m_testStatusTitle = tr("Waiting");
    m_testStatusDetail = tr("Testing service has not been resolved yet.");
    m_searchStatusTitle = tr("Ready");
    m_searchStatusDetail = tr("Search and indexing services will appear when available.");

    m_clangd    = new ClangdManager(this);
    m_lspClient = new LspClient(m_clangd->transport(), this);

    auto *bridge = new LspServiceBridge(m_clangd, m_lspClient, this);
    m_lspService = bridge;

    registerLanguageServiceObjects(m_lspClient, bridge);
    installStatusItem();
    resolveOptionalServices();
    wireBuildSystem();
    wireLaunchService();
    wireTestRunner();
    wireSearchService();
    wireDebugService();

    // ── Status bar feedback ───────────────────────────────────────────────
    if (auto *notif = notifications()) {
        connect(m_clangd, &ClangdManager::serverReady, this, [this, notif]() {
            m_restartCount = 0;
            m_expectRestart = false;
            const QString compileCommandsPath = currentCompileCommandsPath();
            updateStatusPresentation(
                compileCommandsPath.isEmpty()
                    ? tr("C++: ready")
                    : tr("C++: ready (compile DB)"),
                compileCommandsPath.isEmpty()
                    ? tr("clangd is running. No compile_commands.json detected yet.")
                    : tr("clangd is running.\ncompile_commands.json: %1")
                          .arg(QDir::toNativeSeparators(compileCommandsPath)));
            updateWorkspaceCard(
                QStringLiteral("language"),
                compileCommandsPath.isEmpty()
                    ? tr("Ready")
                    : tr("Ready with compile DB"),
                compileCommandsPath.isEmpty()
                    ? tr("clangd is attached, but compile_commands.json is not available yet.")
                    : tr("clangd is attached to %1.")
                          .arg(QDir::toNativeSeparators(compileCommandsPath)));
            notif->statusMessage(tr("Clangd ready"), 3000);
        });
        connect(m_clangd, &ClangdManager::serverStopped, this, [this, notif]() {
            if (m_isShuttingDown)
                return;
            if (m_expectRestart)
                return;

            updateStatusPresentation(
                tr("C++: unavailable"),
                tr("clangd could not be started. Check that clangd is installed and available on PATH."));
            updateWorkspaceCard(
                QStringLiteral("language"),
                tr("Unavailable"),
                tr("clangd could not be started. Check that it is installed and reachable on PATH."));
            notif->warning(tr("Clangd is not installed or not on PATH."));
        });
        connect(m_clangd, &ClangdManager::serverCrashed, this, [this, notif]() {
            static constexpr int kMaxRestarts = 3;
            if (m_restartCount < kMaxRestarts) {
                ++m_restartCount;
                m_expectRestart = true;
                updateStatusPresentation(
                    tr("C++: restarting"),
                    tr("clangd crashed. Automatic restart %1 of %2 is in progress.")
                        .arg(m_restartCount).arg(kMaxRestarts));
                updateWorkspaceCard(
                    QStringLiteral("language"),
                    tr("Restarting"),
                    tr("clangd crashed. Automatic restart %1 of %2 is in progress.")
                        .arg(m_restartCount).arg(kMaxRestarts));
                notif->statusMessage(
                    tr("Clangd crashed \u2014 restarting (%1/%2)...")
                        .arg(m_restartCount).arg(kMaxRestarts), 4000);
                const QString root = workspaceRoot().isEmpty()
                    ? m_clangd->workspaceRoot() : workspaceRoot();
                QTimer::singleShot(3000, m_clangd, [this, root]() {
                    m_clangd->start(root);
                });
            } else {
                m_expectRestart = false;
                updateStatusPresentation(
                    tr("C++: degraded"),
                    tr("clangd crashed repeatedly. Language features are currently unavailable."));
                updateWorkspaceCard(
                    QStringLiteral("language"),
                    tr("Degraded"),
                    tr("clangd crashed repeatedly. Code intelligence is temporarily unavailable."));
                notif->warning(tr("Clangd crashed repeatedly. Language features unavailable. "
                                  "Check that clangd is installed and on PATH."));
            }
        });
    }

    // ── Register commands ─────────────────────────────────────────────────
    registerCommands();
    installMenus();
    installToolBar();

    // ── Start clangd ──────────────────────────────────────────────────────
    updateStatusPresentation(tr("C++: starting"),
                             tr("Starting clangd for the active workspace..."));
    updateWorkspaceCard(QStringLiteral("language"),
                        tr("Starting"),
                        tr("Starting clangd for the active workspace."));
    refreshWorkspaceSnapshot();
    m_workspaceRefreshTimer.start();
    m_clangd->start(languageWorkspaceRoot());

    return true;
}

void CppLanguagePlugin::shutdownPlugin()
{
    m_isShuttingDown = true;
    m_workspaceRefreshTimer.stop();

    if (m_lspClient && m_lspClient->isInitialized())
        m_lspClient->shutdown();
    if (m_clangd)
        m_clangd->stop();
    if (auto *sb = statusBar())
        sb->removeWidget(QLatin1String(kCppStatusItemId));
    m_workspacePanel = nullptr;
}

// ── Command registration ──────────────────────────────────────────────────────

void CppLanguagePlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds) return;

    auto *ed = editor();

    // Go to Definition — keyboard shortcut handled per-editor by LspEditorBridge (F12);
    // this registration makes the command accessible from the command palette.
    cmds->registerCommand(
        QStringLiteral("cpp.goToDefinition"),
        tr("Go to Definition"),
        [this, ed]() {
            if (!ed || !m_lspClient->isInitialized()) return;
            m_lspClient->requestDefinition(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn());
        });

    // Go to Declaration — Ctrl+F12 (not bound by LspEditorBridge by default)
    cmds->registerCommand(
        QStringLiteral("cpp.goToDeclaration"),
        tr("Go to Declaration"),
        [this, ed]() {
            if (!ed || !m_lspClient->isInitialized()) return;
            m_lspClient->requestDeclaration(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn());
        });

    // Find All References — keyboard shortcut handled per-editor (Shift+F12);
    // command palette entry for discoverability.
    cmds->registerCommand(
        QStringLiteral("cpp.findReferences"),
        tr("Find All References"),
        [this, ed]() {
            if (!ed || !m_lspClient->isInitialized()) return;
            m_lspClient->requestReferences(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn());
        });

    // Rename Symbol — keyboard shortcut handled per-editor (F2);
    // command palette entry shows an input dialog for the new name.
    cmds->registerCommand(
        QStringLiteral("cpp.renameSymbol"),
        tr("Rename Symbol"),
        [this, ed]() {
            if (!ed || !m_lspClient->isInitialized()) return;
            bool ok = false;
            const QString newName = QInputDialog::getText(
                nullptr, tr("Rename Symbol"), tr("New name:"),
                QLineEdit::Normal,
                ed->selectedText().isEmpty() ? QString() : ed->selectedText(),
                &ok);
            if (!ok || newName.trimmed().isEmpty()) return;
            m_lspClient->requestRename(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn(),
                newName.trimmed());
        });

    // Format Document — keyboard shortcut handled per-editor (Ctrl+Shift+F);
    // command palette entry for discoverability.
    cmds->registerCommand(
        QStringLiteral("cpp.formatDocument"),
        tr("Format Document"),
        [this, ed]() {
            if (!ed || !m_lspClient->isInitialized()) return;
            m_lspClient->requestFormatting(
                LspClient::pathToUri(ed->activeFilePath()));
        });

    // Go to Symbol in Workspace — shows a live-search dialog backed by workspace/symbol.
    // Ctrl+T shortcut registered here; not handled per-editor since it's workspace-wide.
    cmds->registerCommand(
        QStringLiteral("cpp.gotoWorkspaceSymbol"),
        tr("Go to Symbol in Workspace"),
        [this, ed]() {
            if (!m_lspClient->isInitialized()) return;

            auto *dlg = new QDialog(nullptr, Qt::Dialog);
            dlg->setWindowTitle(tr("Go to Symbol in Workspace"));
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->resize(540, 420);

            auto *layout    = new QVBoxLayout(dlg);
            auto *queryEdit = new QLineEdit(dlg);
            queryEdit->setPlaceholderText(tr("Type symbol name to search…"));
            auto *resultList = new QListWidget(dlg);
            layout->addWidget(queryEdit);
            layout->addWidget(resultList);

            // Shared location table indexed by list row
            auto locations = std::make_shared<QVector<QPair<QString, int>>>();

            // Populate list when server responds
            auto conn = connect(m_lspClient, &LspClient::workspaceSymbolsResult, dlg,
                [resultList, locations](const QJsonArray &symbols) {
                    resultList->clear();
                    locations->clear();
                    for (const QJsonValue &v : symbols) {
                        const QJsonObject sym = v.toObject();
                        const QString name    = sym[QLatin1String("name")].toString();
                        const QJsonObject loc = sym[QLatin1String("location")].toObject();
                        const QString uri     = loc[QLatin1String("uri")].toString();
                        const QString path    = QUrl(uri).toLocalFile();
                        const int line        = loc[QLatin1String("range")].toObject()
                                                [QLatin1String("start")].toObject()
                                                [QLatin1String("line")].toInt();
                        const QString label   = name + QLatin1String("  —  ")
                                              + QFileInfo(path).fileName();
                        resultList->addItem(label);
                        locations->append({path, line});
                    }
                });
            connect(dlg, &QDialog::destroyed, dlg, [conn]() {
                QObject::disconnect(conn);
            });

            // Debounced query (250 ms after last keystroke)
            auto *debounce = new QTimer(dlg);
            debounce->setSingleShot(true);
            debounce->setInterval(250);
            connect(queryEdit, &QLineEdit::textChanged, debounce,
                    [debounce]() { debounce->start(); });
            connect(debounce, &QTimer::timeout, dlg, [this, queryEdit]() {
                m_lspClient->requestWorkspaceSymbols(queryEdit->text());
            });

            // Navigate on double-click
            connect(resultList, &QListWidget::itemDoubleClicked, dlg,
                    [dlg, resultList, locations, ed](QListWidgetItem *item) {
                const int idx = resultList->row(item);
                if (idx >= 0 && idx < locations->size() && ed) {
                    const auto &[path, line] = (*locations)[idx];
                    ed->openFile(path, line + 1, 0);
                }
                dlg->accept();
            });

            // Initial search
            m_lspClient->requestWorkspaceSymbols({});
            queryEdit->setFocus();
            dlg->show();
        });

    // Switch Between Header and Source — unique to this plugin; no LspEditorBridge equivalent.
    // Declared with Alt+O keybinding in plugin.json.
    cmds->registerCommand(
        QStringLiteral("cpp.switchHeaderSource"),
        tr("Switch Between Header and Source"),
        [this, ed]() {
            if (!ed) return;
            switchHeaderSource(ed->activeFilePath());
        });

    cmds->registerCommand(
        QStringLiteral("cpp.restartLanguageServer"),
        tr("Restart Language Server"),
        [this]() {
            if (!m_clangd)
                return;
            m_expectRestart = true;
            updateStatusPresentation(
                tr("C++: restarting"),
                tr("clangd is restarting for the current workspace."));
            updateWorkspaceCard(QStringLiteral("language"),
                                tr("Restarting"),
                                tr("clangd is restarting for the current workspace."));
            m_clangd->restart();
            showStatusMessage(tr("Restarting clangd..."), 3000);
        });

    cmds->registerCommand(
        QStringLiteral("cpp.openCompileCommands"),
        tr("Open compile_commands.json"),
        [this]() {
            const QString path = currentCompileCommandsPath();
            if (path.isEmpty() || !QFile::exists(path)) {
                showWarning(tr("compile_commands.json not found. Configure the project first."));
                return;
            }
            openFile(path);
        });

    cmds->registerCommand(
        QStringLiteral("cpp.showWorkspace"),
        tr("Open C++ Workspace"),
        [this]() {
            showPanel(QLatin1String(kCppWorkspaceDockId));
            refreshWorkspaceSnapshot();
        });

    cmds->registerCommand(
        QStringLiteral("cpp.openProject"),
        tr("Open Project Explorer"),
        [this]() { showProjectTree(); });

    cmds->registerCommand(
        QStringLiteral("cpp.openOutline"),
        tr("Open Outline"),
        [this]() { showPanel(QStringLiteral("OutlineDock")); });

    cmds->registerCommand(
        QStringLiteral("cpp.openReferences"),
        tr("Open References"),
        [this]() { showPanel(QStringLiteral("ReferencesDock")); });

    cmds->registerCommand(
        QStringLiteral("cpp.openOutput"),
        tr("Open Output"),
        [this]() { showPanel(QStringLiteral("OutputDock")); });

    cmds->registerCommand(
        QStringLiteral("cpp.openRun"),
        tr("Open Run"),
        [this]() { showPanel(QStringLiteral("RunDock")); });

    cmds->registerCommand(
        QStringLiteral("cpp.openDebug"),
        tr("Open Debug"),
        [this]() { showPanel(QStringLiteral("DebugDock")); });

    cmds->registerCommand(
        QStringLiteral("cpp.openTests"),
        tr("Open Test Explorer"),
        [this]() { showPanel(QStringLiteral("TestExplorerDock")); });

    cmds->registerCommand(
        QStringLiteral("cpp.openProblems"),
        tr("Open Problems"),
        [this]() { showProblemsPanel(); });

    cmds->registerCommand(
        QStringLiteral("cpp.openSearch"),
        tr("Open Search"),
        [this]() { showSearchPanel(); });

    cmds->registerCommand(
        QStringLiteral("cpp.openTerminal"),
        tr("Open Terminal"),
        [this]() { showTerminalPanel(); });

    cmds->registerCommand(
        QStringLiteral("cpp.openGit"),
        tr("Open Git"),
        [this]() { showGitPanel(); });

    cmds->registerCommand(
        QStringLiteral("cpp.configureProject"),
        tr("Configure Project"),
        [this]() {
            if (m_buildSystem)
                m_buildSystem->configure();
            else
                executeCommand(QStringLiteral("build.configure"));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.buildProject"),
        tr("Build Project"),
        [this]() {
            if (m_buildSystem)
                m_buildSystem->build();
            else
                executeCommand(QStringLiteral("build.build"));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.cleanProject"),
        tr("Clean Project"),
        [this]() {
            if (m_buildSystem)
                m_buildSystem->clean();
            else
                executeCommand(QStringLiteral("build.clean"));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.runProject"),
        tr("Run Project"),
        [this]() {
            executeCommand(QStringLiteral("build.run"));
            updateWorkspaceCard(QStringLiteral("debug"),
                                tr("Launching"),
                                tr("Run without debugging has been requested."));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.debugProject"),
        tr("Debug Project"),
        [this]() {
            executeCommand(QStringLiteral("build.debug"));
            updateWorkspaceCard(QStringLiteral("debug"),
                                tr("Launching"),
                                tr("Debug launch has been requested."));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.stopSession"),
        tr("Stop Session"),
        [this]() {
            if (m_launchService)
                m_launchService->stopSession();
            else
                executeCommand(QStringLiteral("build.stop"));
            updateWorkspaceCard(QStringLiteral("debug"),
                                tr("Stopping"),
                                tr("Stopping the current run or debug session."));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.discoverTests"),
        tr("Discover Tests"),
        [this]() {
            showPanel(QStringLiteral("TestExplorerDock"));
            if (m_testRunner)
                m_testRunner->discoverTests();
            else
                executeCommand(QStringLiteral("testing.discover"));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.runAllTests"),
        tr("Run All Tests"),
        [this]() {
            showPanel(QStringLiteral("TestExplorerDock"));
            if (m_testRunner)
                m_testRunner->runAllTests();
            else
                executeCommand(QStringLiteral("testing.runAll"));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.focusSearch"),
        tr("Find in Files"),
        [this]() {
            showSearchPanel();
            if (!m_searchService)
                return;
            m_searchService->setRootPath(languageWorkspaceRoot());
            m_searchService->activateSearch();
            updateWorkspaceCard(QStringLiteral("search"),
                                tr("Focused"),
                                tr("Search panel is ready for the current workspace."));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.reindexWorkspace"),
        tr("Reindex Workspace"),
        [this]() {
            if (!m_searchService || languageWorkspaceRoot().isEmpty()) {
                showWarning(tr("Search service is not available for this workspace."));
                return;
            }
            showSearchPanel();
            m_searchService->indexWorkspace(languageWorkspaceRoot());
            updateWorkspaceCard(QStringLiteral("search"),
                                tr("Indexing"),
                                tr("Reindexing %1").arg(QDir::toNativeSeparators(languageWorkspaceRoot())));
        });

    cmds->registerCommand(
        QStringLiteral("cpp.openBuildTerminal"),
        tr("Open Build Terminal"),
        [this]() {
            const QString buildDir = currentBuildDirectory();
            if (buildDir.isEmpty() || !terminal()) {
                showWarning(tr("Build directory is not available yet."));
                return;
            }
            terminal()->setWorkingDirectory(buildDir);
            showTerminalPanel();
            terminal()->openTerminal();
        });
}

void CppLanguagePlugin::installMenus()
{
    if (!menus())
        return;

    ensureMenu(QLatin1String(kCppMenuId), tr("&C/C++"));

    addMenuCommand(QLatin1String(kCppMenuId), tr("Go to &Definition"),
                   QStringLiteral("cpp.goToDefinition"), this,
                   QKeySequence(Qt::Key_F12));
    addMenuCommand(QLatin1String(kCppMenuId), tr("Go to &Declaration"),
                   QStringLiteral("cpp.goToDeclaration"), this,
                   QKeySequence(Qt::CTRL | Qt::Key_F12));
    addMenuCommand(QLatin1String(kCppMenuId), tr("Find &References"),
                   QStringLiteral("cpp.findReferences"), this,
                   QKeySequence(Qt::SHIFT | Qt::Key_F12));
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Rename Symbol"),
                   QStringLiteral("cpp.renameSymbol"), this,
                   QKeySequence(Qt::Key_F2));
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Format Document"),
                   QStringLiteral("cpp.formatDocument"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    addMenuCommand(QLatin1String(kCppMenuId), tr("Go to Symbol in &Workspace"),
                   QStringLiteral("cpp.gotoWorkspaceSymbol"), this,
                   QKeySequence(Qt::CTRL | Qt::Key_T));
    addMenuCommand(QLatin1String(kCppMenuId), tr("Switch Header/&Source"),
                   QStringLiteral("cpp.switchHeaderSource"), this,
                   QKeySequence(Qt::ALT | Qt::Key_O));
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Find in Files"),
                   QStringLiteral("cpp.focusSearch"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open C++ &Workspace"),
                   QStringLiteral("cpp.showWorkspace"), this);

    addMenuSeparator(IMenuManager::Analyze);
    addMenuCommand(IMenuManager::Analyze, tr("&Find in Files"),
                   QStringLiteral("cpp.focusSearch"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    addMenuCommand(IMenuManager::Analyze, tr("&Reindex Workspace"),
                   QStringLiteral("cpp.reindexWorkspace"), this);

    if (auto *menu = ensureMenu(QLatin1String(kCppMenuId), tr("&C/C++")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Configure Project"),
                   QStringLiteral("cpp.configureProject"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Build Project"),
                   QStringLiteral("cpp.buildProject"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    addMenuCommand(QLatin1String(kCppMenuId), tr("Clean Pro&ject"),
                   QStringLiteral("cpp.cleanProject"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Run Project"),
                   QStringLiteral("cpp.runProject"), this,
                   QKeySequence(Qt::CTRL | Qt::Key_F5));
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Debug Project"),
                   QStringLiteral("cpp.debugProject"), this,
                   QKeySequence(Qt::Key_F5));
    addMenuCommand(QLatin1String(kCppMenuId), tr("S&top Session"),
                   QStringLiteral("cpp.stopSession"), this,
                   QKeySequence(Qt::SHIFT | Qt::Key_F5));

    if (auto *menu = ensureMenu(QLatin1String(kCppMenuId), tr("&C/C++")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Discover Tests"),
                   QStringLiteral("cpp.discoverTests"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Run &All Tests"),
                   QStringLiteral("cpp.runAllTests"), this);

    if (auto *menu = ensureMenu(QLatin1String(kCppMenuId), tr("&C/C++")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open &Outline"),
                   QStringLiteral("cpp.openOutline"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open &References"),
                   QStringLiteral("cpp.openReferences"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open &Output"),
                   QStringLiteral("cpp.openOutput"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open &Debug"),
                   QStringLiteral("cpp.openDebug"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open &Test Explorer"),
                   QStringLiteral("cpp.openTests"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open &Terminal"),
                   QStringLiteral("cpp.openTerminal"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open &compile_commands.json"),
                   QStringLiteral("cpp.openCompileCommands"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("Open Build Terminal"),
                   QStringLiteral("cpp.openBuildTerminal"), this);
    addMenuCommand(QLatin1String(kCppMenuId), tr("&Restart Language Server"),
                   QStringLiteral("cpp.restartLanguageServer"), this);
}

void CppLanguagePlugin::installToolBar()
{
    createToolBar(QLatin1String(kCppToolBarId), tr("C/C++"));
    addToolBarCommands(QLatin1String(kCppToolBarId), {
        { tr("Workspace"), QStringLiteral("cpp.showWorkspace") },
        { tr("Definition"), QStringLiteral("cpp.goToDefinition"), QKeySequence(Qt::Key_F12) },
        { tr("References"), QStringLiteral("cpp.findReferences"), QKeySequence(Qt::SHIFT | Qt::Key_F12) },
        { tr("Rename"), QStringLiteral("cpp.renameSymbol"), QKeySequence(Qt::Key_F2), true },
        { tr("Format"), QStringLiteral("cpp.formatDocument"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F) },
        { tr("Switch"), QStringLiteral("cpp.switchHeaderSource"), QKeySequence(Qt::ALT | Qt::Key_O) },
        { tr("Build"), QStringLiteral("cpp.buildProject"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B), true },
        { tr("Run"), QStringLiteral("cpp.runProject"), QKeySequence(Qt::CTRL | Qt::Key_F5) },
        { tr("Debug"), QStringLiteral("cpp.debugProject"), QKeySequence(Qt::Key_F5) },
        { tr("Tests"), QStringLiteral("cpp.runAllTests"), {}, true },
        { tr("Search"), QStringLiteral("cpp.focusSearch") },
        { tr("Compile DB"), QStringLiteral("cpp.openCompileCommands"), {}, true },
        { tr("Restart"), QStringLiteral("cpp.restartLanguageServer") },
    }, this);
}

void CppLanguagePlugin::installStatusItem()
{
    auto *sb = statusBar();
    if (!sb)
        return;

    auto label = std::make_unique<QLabel>(tr("C++: idle"));
    label->setToolTip(tr("C/C++ language environment status"));
    sb->addWidget(QLatin1String(kCppStatusItemId),
                  label.release(),
                  IStatusBarManager::Right,
                  70);
}

void CppLanguagePlugin::resolveOptionalServices()
{
    if (!m_buildSystem)
        m_buildSystem = service<IBuildSystem>(QStringLiteral("buildSystem"));
    if (!m_launchService)
        m_launchService = service<ILaunchService>(QStringLiteral("launchService"));
    if (!m_testRunner)
        m_testRunner = service<ITestRunner>(QStringLiteral("testRunner"));
    if (!m_searchService)
        m_searchService = service<ISearchService>(QStringLiteral("searchService"));
    if (!m_debugService)
        m_debugService = service<IDebugService>(QStringLiteral("debugService"));
}

void CppLanguagePlugin::wireBuildSystem()
{
    resolveOptionalServices();
    if (!m_buildSystem)
        return;
    if (m_buildSignalsWired)
        return;

    m_buildSignalsWired = true;
    updateWorkspaceCard(
        QStringLiteral("build"),
        tr("Ready"),
        currentBuildDirectory().isEmpty()
            ? tr("Build system detected. Configure the project to generate targets.")
            : tr("Build directory: %1").arg(QDir::toNativeSeparators(currentBuildDirectory())));

    connect(m_buildSystem, &IBuildSystem::configureFinished,
            this, [this](bool success, const QString &) {
                const QString compileCommandsPath = currentCompileCommandsPath();
                if (!success) {
                    updateWorkspaceCard(
                        QStringLiteral("build"),
                        tr("Configure failed"),
                        tr("Configuration failed. Check the Output dock for details."));
                    return;
                }

                updateWorkspaceCard(
                    QStringLiteral("build"),
                    tr("Configured"),
                    compileCommandsPath.isEmpty()
                        ? tr("Configuration completed. Waiting for compile_commands.json.")
                        : tr("Configuration completed with %1.")
                              .arg(QDir::toNativeSeparators(compileCommandsPath)));

                refreshWorkspaceSnapshot();

                if (!m_clangd)
                    return;
                updateStatusPresentation(
                    tr("C++: refreshing"),
                    compileCommandsPath.isEmpty()
                        ? tr("Build configuration changed. Refreshing clangd state.")
                        : tr("Build configuration changed.\ncompile_commands.json: %1")
                              .arg(QDir::toNativeSeparators(compileCommandsPath)));

                if (!compileCommandsPath.isEmpty())
                    showStatusMessage(tr("compile_commands.json updated — restarting clangd"), 4000);

                m_expectRestart = true;
                m_clangd->restart();
            });

    connect(m_buildSystem, &IBuildSystem::buildOutput,
            this, [this](const QString &, bool) {
                updateWorkspaceCard(
                    QStringLiteral("build"),
                    tr("Building"),
                    currentBuildDirectory().isEmpty()
                        ? tr("Build output is streaming to the Output dock.")
                        : tr("Building in %1.")
                              .arg(QDir::toNativeSeparators(currentBuildDirectory())));
            });

    connect(m_buildSystem, &IBuildSystem::buildFinished,
            this, [this](bool success, int exitCode) {
                updateWorkspaceCard(
                    QStringLiteral("build"),
                    success ? tr("Succeeded") : tr("Failed"),
                    success
                        ? tr("Build finished successfully.")
                        : tr("Build failed with exit code %1.").arg(exitCode));
            });
}

void CppLanguagePlugin::wireLaunchService()
{
    resolveOptionalServices();
    if (!m_launchService || m_launchSignalsWired)
        return;

    m_launchSignalsWired = true;
    connect(m_launchService, &ILaunchService::processStarted,
            this, [this](const QString &executable) {
                updateWorkspaceCard(
                    QStringLiteral("debug"),
                    tr("Running"),
                    executable.isEmpty()
                        ? tr("A run or debug session has started.")
                        : tr("Running %1").arg(QDir::toNativeSeparators(executable)));
            });

    connect(m_launchService, &ILaunchService::processFinished,
            this, [this](int exitCode) {
                updateWorkspaceCard(
                    QStringLiteral("debug"),
                    tr("Finished"),
                    tr("The last run or debug session exited with code %1.").arg(exitCode));
            });

    connect(m_launchService, &ILaunchService::launchError,
            this, [this](const QString &message) {
                updateWorkspaceCard(
                    QStringLiteral("debug"),
                    tr("Launch failed"),
                    message.isEmpty()
                        ? tr("The last launch request failed.")
                        : message);
            });
}

void CppLanguagePlugin::wireTestRunner()
{
    resolveOptionalServices();
    if (!m_testRunner || m_testSignalsWired)
        return;

    m_testSignalsWired = true;
    updateWorkspaceCard(
        QStringLiteral("tests"),
        m_testRunner->hasTests() ? tr("Ready") : tr("Waiting"),
        m_testRunner->hasTests()
            ? tr("%1 tests are currently discovered.").arg(m_testRunner->tests().size())
            : tr("No tests discovered yet."));

    connect(m_testRunner, &ITestRunner::discoveryFinished,
            this, [this]() {
                const int count = m_testRunner ? m_testRunner->tests().size() : 0;
                updateWorkspaceCard(
                    QStringLiteral("tests"),
                    count > 0 ? tr("Discovered") : tr("No tests"),
                    count > 0
                        ? tr("%1 tests are available in Test Explorer.").arg(count)
                        : tr("Test discovery completed, but no tests were found."));
            });

    connect(m_testRunner, &ITestRunner::testStarted,
            this, [this](int index) {
                updateWorkspaceCard(
                    QStringLiteral("tests"),
                    tr("Running"),
                    tr("Running test #%1.").arg(index));
            });

    connect(m_testRunner, &ITestRunner::allTestsFinished,
            this, [this]() {
                int passed = 0;
                int failed = 0;
                int skipped = 0;
                const auto tests = m_testRunner ? m_testRunner->tests() : QList<TestItem>{};
                for (const auto &test : tests) {
                    switch (test.status) {
                    case TestItem::Passed: ++passed; break;
                    case TestItem::Failed: ++failed; break;
                    case TestItem::Skipped: ++skipped; break;
                    default: break;
                    }
                }

                updateWorkspaceCard(
                    QStringLiteral("tests"),
                    failed > 0 ? tr("Attention") : tr("Completed"),
                    tr("%1 passed, %2 failed, %3 skipped.")
                        .arg(passed).arg(failed).arg(skipped));
            });
}

void CppLanguagePlugin::wireSearchService()
{
    resolveOptionalServices();
    if (!m_searchService || m_searchSignalsWired)
        return;

    m_searchSignalsWired = true;
    updateWorkspaceCard(
        QStringLiteral("search"),
        tr("Ready"),
        tr("Find in Files and workspace indexing are available."));
}

void CppLanguagePlugin::wireDebugService()
{
    resolveOptionalServices();
    if (!m_debugService || m_debugSignalsWired)
        return;

    m_debugSignalsWired = true;
    connect(m_debugService, &IDebugService::debugStopped,
            this, [this](const QList<DebugFrame> &frames) {
                updateWorkspaceCard(
                    QStringLiteral("debug"),
                    tr("Paused"),
                    frames.isEmpty()
                        ? tr("Debugger paused.")
                        : tr("Paused in %1.").arg(frames.first().name));
            });

    connect(m_debugService, &IDebugService::debugTerminated,
            this, [this]() {
                updateWorkspaceCard(
                    QStringLiteral("debug"),
                    tr("Stopped"),
                    tr("Debug session terminated."));
            });
}

void CppLanguagePlugin::updateWorkspacePanel()
{
    if (!m_workspacePanel)
        return;

    m_workspacePanel->setWorkspaceSummary(
        m_workspaceRootText,
        m_activeFileText,
        m_buildDirectoryText,
        currentCompileCommandsPath());
    m_workspacePanel->setCardStatus(QStringLiteral("language"),
                                    m_languageStatusTitle,
                                    m_languageStatusDetail);
    m_workspacePanel->setCardStatus(QStringLiteral("build"),
                                    m_buildStatusTitle,
                                    m_buildStatusDetail);
    m_workspacePanel->setCardStatus(QStringLiteral("debug"),
                                    m_debugStatusTitle,
                                    m_debugStatusDetail);
    m_workspacePanel->setCardStatus(QStringLiteral("tests"),
                                    m_testStatusTitle,
                                    m_testStatusDetail);
    m_workspacePanel->setCardStatus(QStringLiteral("search"),
                                    m_searchStatusTitle,
                                    m_searchStatusDetail);

    const bool buildReady = m_buildSystem != nullptr;
    const bool testsReady = m_testRunner != nullptr;
    const bool searchReady = m_searchService != nullptr;
    const bool canOpenBuildTerminal = terminal() != nullptr && !currentBuildDirectory().isEmpty();

    m_workspacePanel->setActionEnabled(QStringLiteral("cpp.configureProject"), buildReady);
    m_workspacePanel->setActionEnabled(QStringLiteral("cpp.buildProject"), buildReady);
    m_workspacePanel->setActionEnabled(QStringLiteral("cpp.cleanProject"), buildReady);
    m_workspacePanel->setActionEnabled(QStringLiteral("cpp.openBuildTerminal"), canOpenBuildTerminal);
    m_workspacePanel->setActionEnabled(QStringLiteral("cpp.discoverTests"), testsReady);
    m_workspacePanel->setActionEnabled(QStringLiteral("cpp.runAllTests"), testsReady);
    m_workspacePanel->setActionEnabled(QStringLiteral("cpp.reindexWorkspace"),
                                       searchReady && !languageWorkspaceRoot().isEmpty());
}

void CppLanguagePlugin::updateWorkspaceCard(const QString &cardId,
                                            const QString &title,
                                            const QString &detail)
{
    if (cardId == QLatin1String("language")) {
        m_languageStatusTitle = title;
        m_languageStatusDetail = detail;
    } else if (cardId == QLatin1String("build")) {
        m_buildStatusTitle = title;
        m_buildStatusDetail = detail;
    } else if (cardId == QLatin1String("debug")) {
        m_debugStatusTitle = title;
        m_debugStatusDetail = detail;
    } else if (cardId == QLatin1String("tests")) {
        m_testStatusTitle = title;
        m_testStatusDetail = detail;
    } else if (cardId == QLatin1String("search")) {
        m_searchStatusTitle = title;
        m_searchStatusDetail = detail;
    }

    updateWorkspacePanel();
}

void CppLanguagePlugin::refreshWorkspaceSnapshot()
{
    resolveOptionalServices();
    wireBuildSystem();
    wireLaunchService();
    wireTestRunner();
    wireSearchService();
    wireDebugService();
    m_workspaceRootText = languageWorkspaceRoot();
    m_activeFileText = activeFilePath();
    m_buildDirectoryText = currentBuildDirectory();
    updateWorkspacePanel();
}

void CppLanguagePlugin::updateStatusPresentation(const QString &text, const QString &tooltip)
{
    auto *sb = statusBar();
    if (!sb)
        return;

    sb->setText(QLatin1String(kCppStatusItemId), text);
    if (!tooltip.isEmpty())
        sb->setTooltip(QLatin1String(kCppStatusItemId), tooltip);
}

QString CppLanguagePlugin::currentCompileCommandsPath() const
{
    if (m_buildSystem) {
        const QString fromBuildSystem = m_buildSystem->compileCommandsPath();
        if (!fromBuildSystem.isEmpty())
            return fromBuildSystem;
    }

    if (m_clangd) {
        const QString dir = m_clangd->compileCommandsDir();
        if (!dir.isEmpty())
            return dir + QStringLiteral("/compile_commands.json");
    }

    return {};
}

QString CppLanguagePlugin::currentBuildDirectory() const
{
    return m_buildSystem ? m_buildSystem->buildDirectory() : QString();
}

// ── Header / Source switch ────────────────────────────────────────────────────

void CppLanguagePlugin::switchHeaderSource(const QString &filePath)
{
    if (filePath.isEmpty()) return;

    const QFileInfo fi(filePath);
    const QString baseName = fi.completeBaseName();
    const QString dir      = fi.absolutePath();
    const QString ext      = fi.suffix().toLower();

    static const QStringList kSrcExts = {
        QStringLiteral("cpp"), QStringLiteral("cxx"),
        QStringLiteral("cc"),  QStringLiteral("c"),
        QStringLiteral("mm")
    };
    static const QStringList kHdrExts = {
        QStringLiteral("h"),   QStringLiteral("hpp"),
        QStringLiteral("hxx"), QStringLiteral("hh")
    };

    const bool isHeader = kHdrExts.contains(ext);
    const bool isSource = kSrcExts.contains(ext);

    if (!isHeader && !isSource) {
        showStatusMessage(tr("Not a C/C++ source or header file"), 3000);
        return;
    }

    const QStringList &targetExts = isHeader ? kSrcExts : kHdrExts;

    // Candidate paths in priority order
    QStringList candidates;
    for (const QString &te : targetExts) {
        const QString name = baseName + QLatin1Char('.') + te;
        // Same directory (most common case)
        candidates << dir + QLatin1Char('/') + name;
        // src <-> include sibling directories
        if (isHeader)
            candidates << QFileInfo(dir + QStringLiteral("/../src/") + name).absoluteFilePath();
        else
            candidates << QFileInfo(dir + QStringLiteral("/../include/") + name).absoluteFilePath();
        // One level up (flat project structure)
        candidates << QFileInfo(dir + QStringLiteral("/../") + name).absoluteFilePath();
    }

    for (const QString &cand : candidates) {
        if (QFile::exists(cand)) {
            openFile(cand);
            return;
        }
    }

    showStatusMessage(
        tr("No corresponding %1 found")
            .arg(isHeader ? tr("source file") : tr("header file")),
        3000);
}

#include "cpplanguageplugin.moc"
