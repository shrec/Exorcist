#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QTemporaryDir>
#include <QTest>

#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "core/iprofilemanager.h"
#include "core/istatusbarmanager.h"
#include "core/itoolbarmanager.h"
#include "core/iworkspacemanager.h"
#include "lsp/lspclient.h"
#include "lsp/processlanguageserver.h"
#include "cpplanguageplugin.h"
#include "plugin/languageworkbenchpluginbase.h"
#include "plugin/workbenchpluginbase.h"
#include "sdk/ibuildsystem.h"
#include "sdk/icommandservice.h"
#include "sdk/icomponentservice.h"
#include "sdk/idebugservice.h"
#include "sdk/idiagnosticsservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/igitservice.h"
#include "sdk/ihostservices.h"
#include "sdk/ilaunchservice.h"
#include "sdk/ilspservice.h"
#include "sdk/inotificationservice.h"
#include "sdk/isearchservice.h"
#include "sdk/itaskservice.h"
#include "sdk/iterminalservice.h"
#include "sdk/itestrunner.h"
#include "sdk/iviewservice.h"
#include "sdk/iworkspaceservice.h"

#include <functional>

class FakeCommandService : public ICommandService
{
public:
    void registerCommand(const QString &id,
                         const QString &title,
                         std::function<void()> handler) override
    {
        m_titles[id] = title;
        m_handlers[id] = std::move(handler);
    }

    void unregisterCommand(const QString &id) override
    {
        m_titles.remove(id);
        m_handlers.remove(id);
    }

    bool executeCommand(const QString &id) override
    {
        if (!m_handlers.contains(id))
            return false;
        m_handlers[id]();
        m_lastExecuted = id;
        return true;
    }

    bool hasCommand(const QString &id) const override
    {
        return m_handlers.contains(id);
    }

    QString lastExecuted() const
    {
        return m_lastExecuted;
    }

private:
    QHash<QString, QString> m_titles;
    QHash<QString, std::function<void()>> m_handlers;
    QString m_lastExecuted;
};

class FakeWorkspaceService : public IWorkspaceService
{
public:
    QString rootPath() const override { return m_rootPath; }
    QStringList openFiles() const override { return m_openFiles; }
    QByteArray readFile(const QString &) const override { return {}; }
    bool writeFile(const QString &, const QByteArray &) override { return true; }
    QStringList listDirectory(const QString &) const override { return {}; }
    bool exists(const QString &) const override { return true; }

    QString m_rootPath = QStringLiteral("D:/workspace/sample");
    QStringList m_openFiles;
};

class FakeEditorService : public IEditorService
{
public:
    QString activeFilePath() const override { return m_activeFilePath; }
    QString activeLanguageId() const override { return m_activeLanguageId; }
    QString selectedText() const override { return m_selectedText; }
    QString activeDocumentText() const override { return {}; }
    void replaceSelection(const QString &) override {}
    void insertText(const QString &) override {}

    void openFile(const QString &path, int line, int column) override
    {
        m_lastOpenedPath = path;
        m_lastOpenedLine = line;
        m_lastOpenedColumn = column;
    }

    int cursorLine() const override { return 7; }
    int cursorColumn() const override { return 3; }

    QString m_activeFilePath = QStringLiteral("D:/workspace/sample/src/main.cpp");
    QString m_activeLanguageId = QStringLiteral("cpp");
    QString m_selectedText = QStringLiteral("Widget");
    QString m_lastOpenedPath;
    int m_lastOpenedLine = -1;
    int m_lastOpenedColumn = -1;
};

class FakeViewService : public IViewService
{
public:
    void registerPanel(const QString &, const QString &, QWidget *) override {}
    void unregisterPanel(const QString &) override {}
    void showView(const QString &viewId) override { m_lastShown = viewId; }
    void hideView(const QString &viewId) override { m_lastHidden = viewId; }
    bool isViewVisible(const QString &) const override { return false; }

    QString m_lastShown;
    QString m_lastHidden;
};

class FakeNotificationService : public INotificationService
{
public:
    void info(const QString &text) override { m_lastInfo = text; }
    void warning(const QString &text) override { m_lastWarning = text; }
    void error(const QString &text) override { m_lastError = text; }
    void statusMessage(const QString &text, int timeoutMs) override
    {
        m_lastStatus = text;
        m_lastTimeout = timeoutMs;
    }

    QString m_lastInfo;
    QString m_lastWarning;
    QString m_lastError;
    QString m_lastStatus;
    int m_lastTimeout = 0;
};

class FakeGitService : public IGitService
{
public:
    bool isGitRepo() const override { return true; }
    QString currentBranch() const override { return QStringLiteral("main"); }
    QString diff(bool) const override { return {}; }
    QStringList changedFiles() const override { return {}; }
    QStringList branches() const override { return {QStringLiteral("main")}; }
};

class FakeTerminalService : public ITerminalService
{
public:
    void runCommand(const QString &command) override { m_lastCommand = command; }
    void sendInput(const QString &text) override { m_lastInput = text; }
    QString recentOutput(int) const override { return {}; }
    void openTerminal() override {}
    QString selectedText() const override { return {}; }
    void setWorkingDirectory(const QString &dir) override { m_lastWorkingDirectory = dir; }

    QString m_lastCommand;
    QString m_lastInput;
    QString m_lastWorkingDirectory;
};

class FakeDiagnosticsService : public IDiagnosticsService
{
public:
    QList<SDKDiagnostic> diagnostics() const override { return {}; }
    QList<SDKDiagnostic> diagnosticsForFile(const QString &) const override { return {}; }
    int errorCount() const override { return 0; }
    int warningCount() const override { return 0; }
};

class FakeTaskService : public ITaskService
{
public:
    void runTask(const QString &taskId) override { m_lastTask = taskId; }
    void cancelTask(const QString &) override {}
    bool isTaskRunning(const QString &) const override { return false; }

    QString m_lastTask;
};

class FakeDockManager : public IDockManager
{
public:
    bool addPanel(const QString &, const QString &, QWidget *, Area, bool) override { return true; }
    bool removePanel(const QString &) override { return true; }
    void showPanel(const QString &id) override { m_lastShown = id; }
    void hidePanel(const QString &id) override { m_lastHidden = id; }
    void togglePanel(const QString &id) override { m_lastToggled = id; }
    bool isPanelVisible(const QString &) const override { return false; }
    void pinPanel(const QString &) override {}
    void unpinPanel(const QString &) override {}
    bool isPanelPinned(const QString &) const override { return false; }
    QStringList panelIds() const override { return {}; }
    QAction *panelToggleAction(const QString &) const override { return nullptr; }
    QWidget *panelWidget(const QString &) const override { return nullptr; }
    QByteArray saveLayout() const override { return {}; }
    bool restoreLayout(const QByteArray &) override { return true; }

    QString m_lastShown;
    QString m_lastHidden;
    QString m_lastToggled;
};

class FakeMenuManager : public IMenuManager
{
public:
    ~FakeMenuManager() override
    {
        qDeleteAll(m_customMenus);
        qDeleteAll(m_standardMenus);
    }

    QMenu *menu(MenuLocation location) override
    {
        if (!m_standardMenus.contains(location))
            m_standardMenus.insert(location, new QMenu);
        return m_standardMenus.value(location);
    }
    QMenu *createMenu(const QString &id, const QString &title) override
    {
        if (!m_customMenus.contains(id))
            m_customMenus.insert(id, new QMenu(title));
        return m_customMenus.value(id);
    }
    bool removeMenu(const QString &id) override
    {
        if (auto *m = m_customMenus.take(id)) m->deleteLater();
        return true;
    }
    void addAction(MenuLocation location, QAction *action, const QString & = {}) override
    {
        m_standardActions[location].append(action);
    }
    void addAction(const QString &menuId, QAction *action, const QString & = {}) override
    {
        m_customActions[menuId].append(action);
    }
    QMenu *addSubmenu(MenuLocation, const QString &, const QString & = {}) override { return nullptr; }
    void addSeparator(MenuLocation location) override { ++m_separatorCount[location]; }
    bool removeAction(QAction *) override { return true; }
    void addEditorContextAction(QAction *, const QString & = {}, const QString & = {}) override {}
    void addExplorerContextAction(QAction *, const QString & = {}, const QString & = {}) override {}
    QStringList customMenuIds() const override { return m_customMenus.keys(); }
    QMenu *customMenu(const QString &id) const override { return m_customMenus.value(id, nullptr); }

    bool hasAction(const QString &menuId, const QString &text) const
    {
        const auto actions = m_customActions.value(menuId);
        for (const auto *action : actions) {
            if (action && action->text() == text)
                return true;
        }
        return false;
    }

    QHash<QString, QList<QAction *>> m_customActions;
    QHash<MenuLocation, QList<QAction *>> m_standardActions;
    QHash<MenuLocation, int> m_separatorCount;

private:
    QHash<QString, QMenu *> m_customMenus;
    QHash<MenuLocation, QMenu *> m_standardMenus;
};

class FakeToolBarManager : public IToolBarManager
{
public:
    bool createToolBar(const QString &id, const QString &, Edge) override
    {
        m_toolBarIds.append(id);
        return true;
    }
    bool removeToolBar(const QString &) override { return true; }
    void addAction(const QString &toolBarId, QAction *action) override
    {
        m_actions[toolBarId].append(action);
    }
    void addWidget(const QString &toolBarId, QWidget *widget) override
    {
        m_widgets[toolBarId].append(widget);
    }
    void addSeparator(const QString &toolBarId) override { ++m_separatorCount[toolBarId]; }
    void setVisible(const QString &, bool) override {}
    bool isVisible(const QString &) const override { return true; }
    void setEdge(const QString &, Edge) override {}
    void setAllLocked(bool) override {}
    bool allLocked() const override { return false; }
    QStringList toolBarIds() const override { return m_toolBarIds; }
    QByteArray saveLayout() const override { return {}; }
    bool restoreLayout(const QByteArray &) override { return true; }

    QStringList m_toolBarIds;
    QHash<QString, QList<QAction *>> m_actions;
    QHash<QString, QList<QWidget *>> m_widgets;
    QHash<QString, int> m_separatorCount;
};

class FakeStatusBarManager : public IStatusBarManager
{
public:
    void addWidget(const QString &id, QWidget *widget, Alignment, int) override
    {
        m_widgets[id] = widget;
    }
    bool removeWidget(const QString &id) override
    {
        m_widgets.remove(id);
        m_texts.remove(id);
        return true;
    }
    void setText(const QString &id, const QString &text) override
    {
        m_texts[id] = text;
        if (auto *label = qobject_cast<QLabel *>(m_widgets.value(id)))
            label->setText(text);
    }
    void setTooltip(const QString &id, const QString &tooltip) override
    {
        m_tooltips[id] = tooltip;
        if (m_widgets.contains(id))
            m_widgets[id]->setToolTip(tooltip);
    }
    void setVisible(const QString &, bool) override {}
    void showMessage(const QString &text, int timeoutMs) override
    {
        m_lastMessage = text;
        m_lastTimeout = timeoutMs;
    }
    QStringList itemIds() const override { return m_widgets.keys(); }
    QWidget *widget(const QString &id) const override { return m_widgets.value(id, nullptr); }

    QString m_lastMessage;
    int m_lastTimeout = 0;
    QHash<QString, QWidget *> m_widgets;
    QHash<QString, QString> m_texts;
    QHash<QString, QString> m_tooltips;
};

class FakeWorkspaceManager : public IWorkspaceManager
{
public:
    void openFolder(const QString &path = {}) override { m_lastFolder = path; }
    void closeFolder() override {}
    QString rootPath() const override { return QStringLiteral("D:/workspace/sample"); }
    bool hasFolder() const override { return true; }
    void openFile(const QString &path) override { m_lastFile = path; }
    void openFile(const QString &path, int, int = 0) override { m_lastFile = path; }
    QStringList openFiles() const override { return {}; }
    QString activeFile() const override { return {}; }
    QStringList recentFolders() const override { return {}; }
    QStringList recentFiles() const override { return {}; }
    bool hasSolution() const override { return false; }
    QString solutionPath() const override { return {}; }

    QString m_lastFolder;
    QString m_lastFile;
};

class FakeProfileManager : public IProfileManager
{
public:
    QStringList availableProfiles() const override { return {}; }
    QString profileName(const QString &) const override { return {}; }
    QString profileDescription(const QString &) const override { return {}; }
    QString activeProfile() const override { return {}; }
    bool activateProfile(const QString &) override { return true; }
    QString detectProfile(const QString &) const override { return {}; }
    bool autoDetectEnabled() const override { return true; }
    void setAutoDetectEnabled(bool) override {}
};

class FakeComponentService : public IComponentService
{
public:
    QStringList availableComponents() const override { return {}; }
    QString componentDisplayName(const QString &) const override { return {}; }
    bool supportsMultipleInstances(const QString &) const override { return false; }
    bool hasComponent(const QString &) const override { return false; }
    QString createInstance(const QString &) override { return {}; }
    bool destroyInstance(const QString &) override { return true; }
    QStringList activeInstances() const override { return {}; }
    QStringList instancesOf(const QString &) const override { return {}; }
    int instanceCount(const QString &) const override { return 0; }
    QWidget *instanceWidget(const QString &) const override { return nullptr; }
};

class FakeBuildSystem : public IBuildSystem
{
    Q_OBJECT

public:
    explicit FakeBuildSystem(QObject *parent = nullptr)
        : IBuildSystem(parent)
    {
    }

    void setProjectRoot(const QString &) override {}
    bool hasProject() const override { return m_hasProject; }
    void configure() override { ++m_configureCalls; }
    void build(const QString &target = {}) override
    {
        ++m_buildCalls;
        m_lastBuildTarget = target;
    }
    void clean() override { ++m_cleanCalls; }
    void cancelBuild() override {}
    bool isBuilding() const override { return false; }
    QStringList targets() const override { return {}; }
    QString buildDirectory() const override { return m_buildDirectory; }
    QString compileCommandsPath() const override { return m_compileCommandsPath; }

    void emitConfigureFinished(bool success, const QString &error = {})
    {
        emit configureFinished(success, error);
    }

    bool m_hasProject = true;
    int m_configureCalls = 0;
    int m_buildCalls = 0;
    int m_cleanCalls = 0;
    QString m_lastBuildTarget;
    QString m_buildDirectory = QStringLiteral("D:/workspace/sample/build");
    QString m_compileCommandsPath =
        QStringLiteral("D:/workspace/sample/build/compile_commands.json");
};

class FakeLaunchService : public ILaunchService
{
    Q_OBJECT

public:
    explicit FakeLaunchService(QObject *parent = nullptr)
        : ILaunchService(parent)
    {
    }

    void startDebugging(const LaunchConfig &config) override
    {
        ++m_debugCalls;
        m_lastConfig = config;
    }

    void startWithoutDebugging(const LaunchConfig &config) override
    {
        ++m_runCalls;
        m_lastConfig = config;
    }

    void stopSession() override { ++m_stopCalls; }

    bool buildBeforeRun() const override { return m_buildBeforeRun; }
    void setBuildBeforeRun(bool enabled) override { m_buildBeforeRun = enabled; }

    int m_debugCalls = 0;
    int m_runCalls = 0;
    int m_stopCalls = 0;
    bool m_buildBeforeRun = true;
    LaunchConfig m_lastConfig;
};

class FakeTestRunner : public ITestRunner
{
    Q_OBJECT

public:
    explicit FakeTestRunner(QObject *parent = nullptr)
        : ITestRunner(parent)
    {
    }

    void discoverTests() override { ++m_discoverCalls; }
    void runAllTests() override { ++m_runAllCalls; }
    void runTest(int index) override { m_runTestIndices.append(index); }
    QList<TestItem> tests() const override { return m_tests; }
    bool hasTests() const override { return !m_tests.isEmpty(); }

    int m_discoverCalls = 0;
    int m_runAllCalls = 0;
    QList<int> m_runTestIndices;
    QList<TestItem> m_tests;
};

class FakeSearchService : public ISearchService
{
    Q_OBJECT

public:
    explicit FakeSearchService(QObject *parent = nullptr)
        : ISearchService(parent)
    {
    }

    void setRootPath(const QString &path) override { m_rootPath = path; }
    void activateSearch() override { ++m_activateCalls; }
    void indexWorkspace(const QString &rootPath) override
    {
        ++m_indexCalls;
        m_lastIndexedRoot = rootPath;
    }
    void reindexFile(const QString &filePath) override { m_lastReindexedFile = filePath; }
    QWidget *searchPanel() const override { return nullptr; }

    QString m_rootPath;
    QString m_lastIndexedRoot;
    QString m_lastReindexedFile;
    int m_activateCalls = 0;
    int m_indexCalls = 0;
};

class FakeDebugService : public IDebugService
{
    Q_OBJECT

public:
    explicit FakeDebugService(QObject *parent = nullptr)
        : IDebugService(parent)
    {
    }

    void addBreakpointEntry(const QString &filePath, int line) override
    {
        m_lastBreakpointFile = filePath;
        m_lastBreakpointLine = line;
    }

    void removeBreakpointEntry(const QString &filePath, int line) override
    {
        m_lastRemovedFile = filePath;
        m_lastRemovedLine = line;
    }

    QString m_lastBreakpointFile;
    int m_lastBreakpointLine = -1;
    QString m_lastRemovedFile;
    int m_lastRemovedLine = -1;
};

class FakeHostServices : public QObject, public IHostServices
{
    Q_OBJECT

public:
    ICommandService *commands() override { return &m_commandService; }
    IWorkspaceService *workspace() override { return &m_workspaceService; }
    IEditorService *editor() override { return &m_editorService; }
    IViewService *views() override { return &m_viewService; }
    INotificationService *notifications() override { return &m_notificationService; }
    IGitService *git() override { return &m_gitService; }
    ITerminalService *terminal() override { return &m_terminalService; }
    IDiagnosticsService *diagnostics() override { return &m_diagnosticsService; }
    ITaskService *tasks() override { return &m_taskService; }
    IDockManager *docks() override { return &m_dockManager; }
    IMenuManager *menus() override { return &m_menuManager; }
    IToolBarManager *toolbars() override { return &m_toolBarManager; }
    IStatusBarManager *statusBar() override { return &m_statusBarManager; }
    IWorkspaceManager *workspaceManager() override { return &m_workspaceManager; }
    IProfileManager *profiles() override { return &m_profileManager; }
    IComponentService *components() override { return &m_componentService; }

    void registerService(const QString &name, QObject *service) override
    {
        m_services[name] = service;
    }

    QObject *queryService(const QString &name) override
    {
        return m_services.value(name, nullptr);
    }

    FakeCommandService m_commandService;
    FakeWorkspaceService m_workspaceService;
    FakeEditorService m_editorService;
    FakeViewService m_viewService;
    FakeNotificationService m_notificationService;
    FakeGitService m_gitService;
    FakeTerminalService m_terminalService;
    FakeDiagnosticsService m_diagnosticsService;
    FakeTaskService m_taskService;
    FakeDockManager m_dockManager;
    FakeMenuManager m_menuManager;
    FakeToolBarManager m_toolBarManager;
    FakeStatusBarManager m_statusBarManager;
    FakeWorkspaceManager m_workspaceManager;
    FakeProfileManager m_profileManager;
    FakeComponentService m_componentService;

private:
    QHash<QString, QObject *> m_services;
};

class TestableWorkbenchPlugin : public QObject, public WorkbenchPluginBase
{
    Q_OBJECT

public:
    PluginInfo info() const override
    {
        return {QStringLiteral("test.workbench"),
                QStringLiteral("Test Workbench"),
                QStringLiteral("1.0.0")};
    }

    bool initializePlugin() override { return true; }

    QString pluginWorkspaceRoot() const { return workspaceRoot(); }
    QString pluginActiveFilePath() const { return activeFilePath(); }
    QString pluginActiveLanguageId() const { return activeLanguageId(); }
    QString pluginSelectedText() const { return selectedText(); }
    void pluginOpenFile(const QString &path, int line, int column) const { openFile(path, line, column); }
    void pluginShowInfo(const QString &text) const { showInfo(text); }
    void pluginShowWarning(const QString &text) const { showWarning(text); }
    void pluginShowError(const QString &text) const { showError(text); }
    void pluginShowStatusMessage(const QString &text, int timeoutMs) const { showStatusMessage(text, timeoutMs); }
    void pluginShowPanel(const QString &panelId) const { showPanel(panelId); }
    void pluginHidePanel(const QString &panelId) const { hidePanel(panelId); }
    void pluginTogglePanel(const QString &panelId) const { togglePanel(panelId); }
    bool pluginExecuteCommand(const QString &commandId) const { return executeCommand(commandId); }
    void pluginRegisterService(const QString &name, QObject *serviceObject) const { registerService(name, serviceObject); }
    QObject *pluginQueryService(const QString &name) const { return queryService(name); }
};

class TestableLanguageWorkbenchPlugin : public QObject, public LanguageWorkbenchPluginBase
{
    Q_OBJECT

public:
    PluginInfo info() const override
    {
        return {QStringLiteral("test.language-workbench"),
                QStringLiteral("Test Language Workbench"),
                QStringLiteral("1.0.0")};
    }

    bool initializePlugin() override { return true; }

    void pluginShowProjectTree() const { showProjectTree(); }
    void pluginShowProblemsPanel() const { showProblemsPanel(); }
    void pluginShowSearchPanel() const { showSearchPanel(); }
    void pluginShowTerminalPanel() const { showTerminalPanel(); }
    void pluginShowGitPanel() const { showGitPanel(); }
    void pluginShowStandardLanguagePanels() const { showStandardLanguagePanels(); }
    void pluginRegisterLanguageServices(QObject *client, QObject *serviceObject) const
    {
        registerLanguageServiceObjects(client, serviceObject);
    }
    std::unique_ptr<ProcessLanguageServer> pluginCreateProcessLanguageServer(
        const QString &program,
        const QString &displayName,
        const QStringList &baseArguments = {}) const
    {
        return createProcessLanguageServer(program, displayName, baseArguments);
    }
    void pluginAttachLanguageServer(ProcessLanguageServer *server) const
    {
        attachLanguageServerToWorkbench(server);
    }
};

class TestWorkbenchPluginBase : public QObject
{
    Q_OBJECT

private slots:
    void commonHelpersForwardToHostServices();
    void serviceRegistrationAndCommandExecutionUseHostBoundary();
    void languageWorkbenchHelpersUseStandardDockIds();
    void processLanguageServerHelpersCreateAndAttachToWorkbench();
    void cppLanguagePluginRegistersWorkbenchSurface();
};

void TestWorkbenchPluginBase::commonHelpersForwardToHostServices()
{
    FakeHostServices hostServices;
    TestableWorkbenchPlugin plugin;

    QVERIFY(plugin.initialize(&hostServices));

    QCOMPARE(plugin.pluginWorkspaceRoot(), QStringLiteral("D:/workspace/sample"));
    QCOMPARE(plugin.pluginActiveFilePath(), QStringLiteral("D:/workspace/sample/src/main.cpp"));
    QCOMPARE(plugin.pluginActiveLanguageId(), QStringLiteral("cpp"));
    QCOMPARE(plugin.pluginSelectedText(), QStringLiteral("Widget"));

    plugin.pluginOpenFile(QStringLiteral("D:/workspace/sample/include/widget.h"), 12, 4);
    QCOMPARE(hostServices.m_editorService.m_lastOpenedPath,
             QStringLiteral("D:/workspace/sample/include/widget.h"));
    QCOMPARE(hostServices.m_editorService.m_lastOpenedLine, 12);
    QCOMPARE(hostServices.m_editorService.m_lastOpenedColumn, 4);

    plugin.pluginShowInfo(QStringLiteral("info"));
    plugin.pluginShowWarning(QStringLiteral("warning"));
    plugin.pluginShowError(QStringLiteral("error"));
    plugin.pluginShowStatusMessage(QStringLiteral("ready"), 1234);
    QCOMPARE(hostServices.m_notificationService.m_lastInfo, QStringLiteral("info"));
    QCOMPARE(hostServices.m_notificationService.m_lastWarning, QStringLiteral("warning"));
    QCOMPARE(hostServices.m_notificationService.m_lastError, QStringLiteral("error"));
    QCOMPARE(hostServices.m_notificationService.m_lastStatus, QStringLiteral("ready"));
    QCOMPARE(hostServices.m_notificationService.m_lastTimeout, 1234);

    plugin.pluginShowPanel(QStringLiteral("ProjectDock"));
    plugin.pluginHidePanel(QStringLiteral("ProjectDock"));
    plugin.pluginTogglePanel(QStringLiteral("ProjectDock"));
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("ProjectDock"));
    QCOMPARE(hostServices.m_dockManager.m_lastHidden, QStringLiteral("ProjectDock"));
    QCOMPARE(hostServices.m_dockManager.m_lastToggled, QStringLiteral("ProjectDock"));

    plugin.shutdown();
}

void TestWorkbenchPluginBase::serviceRegistrationAndCommandExecutionUseHostBoundary()
{
    FakeHostServices hostServices;
    TestableWorkbenchPlugin plugin;
    QObject serviceObject;
    bool handlerTriggered = false;

    hostServices.m_commandService.registerCommand(
        QStringLiteral("plugin.test"),
        QStringLiteral("Test"),
        [&handlerTriggered]() { handlerTriggered = true; });

    QVERIFY(plugin.initialize(&hostServices));

    plugin.pluginRegisterService(QStringLiteral("shared.object"), &serviceObject);
    QCOMPARE(plugin.pluginQueryService(QStringLiteral("shared.object")), &serviceObject);

    QVERIFY(plugin.pluginExecuteCommand(QStringLiteral("plugin.test")));
    QVERIFY(handlerTriggered);
    QCOMPARE(hostServices.m_commandService.lastExecuted(), QStringLiteral("plugin.test"));

    plugin.shutdown();
}

void TestWorkbenchPluginBase::languageWorkbenchHelpersUseStandardDockIds()
{
    FakeHostServices hostServices;
    TestableLanguageWorkbenchPlugin plugin;
    QObject clientObject;
    QObject serviceObject;

    QVERIFY(plugin.initialize(&hostServices));

    plugin.pluginShowProjectTree();
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("ProjectDock"));

    plugin.pluginShowProblemsPanel();
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("ProblemsDock"));

    plugin.pluginShowSearchPanel();
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("SearchDock"));

    plugin.pluginShowTerminalPanel();
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("TerminalDock"));

    plugin.pluginShowGitPanel();
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("GitDock"));

    plugin.pluginShowStandardLanguagePanels();
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("ProblemsDock"));

    plugin.pluginRegisterLanguageServices(&clientObject, &serviceObject);
    QCOMPARE(hostServices.queryService(QStringLiteral("lspClient")), &clientObject);
    QCOMPARE(hostServices.queryService(QStringLiteral("lspService")), &serviceObject);

    plugin.shutdown();
}

void TestWorkbenchPluginBase::processLanguageServerHelpersCreateAndAttachToWorkbench()
{
    FakeHostServices hostServices;
    TestableLanguageWorkbenchPlugin plugin;

    QVERIFY(plugin.initialize(&hostServices));

    auto server = plugin.pluginCreateProcessLanguageServer(
        QStringLiteral("missing-language-server"),
        QStringLiteral("Missing Language Server"));
    QVERIFY(server);
    QVERIFY(server->client());
    QVERIFY(server->service());

    plugin.pluginAttachLanguageServer(server.get());

    QCOMPARE(hostServices.queryService(QStringLiteral("lspClient")),
             static_cast<QObject *>(server->client()));
    QCOMPARE(hostServices.queryService(QStringLiteral("lspService")),
             static_cast<QObject *>(server->service()));

    QMetaObject::invokeMethod(server.get(),
                              "statusMessage",
                              Qt::DirectConnection,
                              Q_ARG(QString, QStringLiteral("language ready")),
                              Q_ARG(int, 321));
    QCOMPARE(hostServices.m_notificationService.m_lastStatus,
             QStringLiteral("language ready"));
    QCOMPARE(hostServices.m_notificationService.m_lastTimeout, 321);

    plugin.shutdown();
}

void TestWorkbenchPluginBase::cppLanguagePluginRegistersWorkbenchSurface()
{
    FakeHostServices hostServices;
    FakeBuildSystem buildSystem;
    FakeLaunchService launchService;
    FakeTestRunner testRunner;
    FakeSearchService searchService;
    FakeDebugService debugService;
    CppLanguagePlugin plugin;
    QTemporaryDir workspaceDir;

    QVERIFY(workspaceDir.isValid());

    const QString compileCommandsPath =
        workspaceDir.filePath(QStringLiteral("build/compile_commands.json"));
    QDir().mkpath(QFileInfo(compileCommandsPath).absolutePath());
    QFile compileCommandsFile(compileCommandsPath);
    QVERIFY(compileCommandsFile.open(QIODevice::WriteOnly | QIODevice::Text));
    compileCommandsFile.write("[]");
    compileCommandsFile.close();

    hostServices.m_workspaceService.m_rootPath = workspaceDir.path();
    hostServices.m_workspaceManager.m_lastFolder.clear();
    buildSystem.m_buildDirectory = workspaceDir.filePath(QStringLiteral("build"));
    buildSystem.m_compileCommandsPath = compileCommandsPath;
    testRunner.m_tests = {
        TestItem{QStringLiteral("WidgetTests"), 0, {}, {}, {}, TestItem::Passed, {}, 0.2},
        TestItem{QStringLiteral("MathTests"), 1, {}, {}, {}, TestItem::Passed, {}, 0.1}
    };

    bool buildRunCommandTriggered = false;
    bool buildDebugCommandTriggered = false;

    hostServices.registerService(QStringLiteral("buildSystem"), &buildSystem);
    hostServices.registerService(QStringLiteral("launchService"), &launchService);
    hostServices.registerService(QStringLiteral("testRunner"), &testRunner);
    hostServices.registerService(QStringLiteral("searchService"), &searchService);
    hostServices.registerService(QStringLiteral("debugService"), &debugService);

    hostServices.m_commandService.registerCommand(
        QStringLiteral("build.run"),
        QStringLiteral("Run"),
        [&buildRunCommandTriggered]() { buildRunCommandTriggered = true; });
    hostServices.m_commandService.registerCommand(
        QStringLiteral("build.debug"),
        QStringLiteral("Debug"),
        [&buildDebugCommandTriggered]() { buildDebugCommandTriggered = true; });

    QVERIFY(plugin.initialize(&hostServices));

    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.goToDefinition")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.goToDeclaration")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.findReferences")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.renameSymbol")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.formatDocument")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.switchHeaderSource")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.openCompileCommands")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.restartLanguageServer")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.showWorkspace")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.buildProject")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.runProject")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.debugProject")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.stopSession")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.discoverTests")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.runAllTests")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.focusSearch")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.reindexWorkspace")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.openOutput")));
    QVERIFY(hostServices.m_commandService.hasCommand(QStringLiteral("cpp.openTests")));

    QVERIFY(hostServices.m_toolBarManager.toolBarIds().contains(QStringLiteral("cpp")));
    QVERIFY(hostServices.m_menuManager.customMenuIds().contains(QStringLiteral("cpp.language")));
    QVERIFY(hostServices.m_statusBarManager.itemIds().contains(QStringLiteral("cpp.language.status")));
    QVERIFY(hostServices.m_statusBarManager.widget(QStringLiteral("cpp.language.status")) != nullptr);

    QWidget parent;
    QWidget *workspaceView = plugin.createView(QStringLiteral("CppWorkspaceDock"), &parent);
    QVERIFY(workspaceView != nullptr);
    QCOMPARE(workspaceView->parentWidget(), &parent);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.openCompileCommands")));
    QCOMPARE(hostServices.m_editorService.m_lastOpenedPath, compileCommandsPath);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.showWorkspace")));
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("CppWorkspaceDock"));

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.focusSearch")));
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("SearchDock"));
    QCOMPARE(searchService.m_rootPath, workspaceDir.path());
    QCOMPARE(searchService.m_activateCalls, 1);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.reindexWorkspace")));
    QCOMPARE(searchService.m_lastIndexedRoot, workspaceDir.path());
    QCOMPARE(searchService.m_indexCalls, 1);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.buildProject")));
    QCOMPARE(buildSystem.m_buildCalls, 1);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.runProject")));
    QVERIFY(buildRunCommandTriggered);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.debugProject")));
    QVERIFY(buildDebugCommandTriggered);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.stopSession")));
    QCOMPARE(launchService.m_stopCalls, 1);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.discoverTests")));
    QCOMPARE(hostServices.m_dockManager.m_lastShown, QStringLiteral("TestExplorerDock"));
    QCOMPARE(testRunner.m_discoverCalls, 1);

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.runAllTests")));
    QCOMPARE(testRunner.m_runAllCalls, 1);

    buildSystem.emitConfigureFinished(true);
    const QString configureStatus =
        hostServices.m_statusBarManager.m_texts.value(QStringLiteral("cpp.language.status"));
    QVERIFY(configureStatus == QStringLiteral("C++: refreshing")
             || configureStatus == QStringLiteral("C++: restarting"));

    QVERIFY(hostServices.m_commandService.executeCommand(QStringLiteral("cpp.restartLanguageServer")));
    QCOMPARE(hostServices.m_statusBarManager.m_texts.value(QStringLiteral("cpp.language.status")),
             QStringLiteral("C++: restarting"));

    plugin.shutdown();
}

QTEST_MAIN(TestWorkbenchPluginBase)
#include "test_workbenchpluginbase.moc"
