#include "workbenchpluginbase.h"

#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "core/istatusbarmanager.h"
#include "core/iworkspacemanager.h"
#include "core/itoolbarmanager.h"
#include "sdk/icommandservice.h"
#include "sdk/icomponentservice.h"
#include "sdk/idiagnosticsservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/igitservice.h"
#include "sdk/ihostservices.h"
#include "sdk/inotificationservice.h"
#include "core/iprofilemanager.h"
#include "sdk/itaskservice.h"
#include "sdk/iterminalservice.h"
#include "sdk/iviewservice.h"
#include "sdk/iworkspaceservice.h"

#include "ui/themeicons.h"

#include <QAction>
#include <QDebug>
#include <QMenu>
#include <QObject>
#include <QWidget>

bool WorkbenchPluginBase::initialize(IHostServices *host)
{
    m_host = host;
    return initializePlugin();
}

void WorkbenchPluginBase::shutdown()
{
    // Phase 1 ownership cleanup: tear down every UI artifact the plugin
    // registered via the convenience helpers (rule L1, L7).  This runs
    // BEFORE shutdownPlugin() so the plugin still has access to host
    // services if it needs to do additional cleanup of un-tracked state.
    cleanupOwnedArtifacts();
    shutdownPlugin();
    m_host = nullptr;
}

void WorkbenchPluginBase::cleanupOwnedArtifacts()
{
    if (!m_host) return;

    // Order matters: actions first (so menus/toolbars don't hold dangling
    // pointers), then menus/toolbars/docks, then commands.
    if (auto *menuMgr = m_host->menus()) {
        for (auto &actPtr : m_ownedActions) {
            if (actPtr) menuMgr->removeAction(actPtr.data());
        }
    }
    m_ownedActions.clear();

    if (auto *toolMgr = m_host->toolbars()) {
        for (const QString &id : m_ownedToolbarIds)
            toolMgr->removeToolBar(id);
    }
    m_ownedToolbarIds.clear();

    if (auto *menuMgr = m_host->menus()) {
        for (const QString &id : m_ownedMenuIds)
            menuMgr->removeMenu(id);
    }
    m_ownedMenuIds.clear();

    if (auto *dockMgr = m_host->docks()) {
        for (const QString &id : m_ownedDockIds)
            dockMgr->removePanel(id);
    }
    m_ownedDockIds.clear();

    if (auto *cmdSvc = m_host->commands()) {
        for (const QString &id : m_ownedCommandIds)
            cmdSvc->unregisterCommand(id);
    }
    m_ownedCommandIds.clear();
}

void WorkbenchPluginBase::trackOwnedDock(const QString &id) const
{
    if (!id.isEmpty() && !m_ownedDockIds.contains(id))
        m_ownedDockIds.append(id);
}

void WorkbenchPluginBase::trackOwnedToolbar(const QString &id) const
{
    if (!id.isEmpty() && !m_ownedToolbarIds.contains(id))
        m_ownedToolbarIds.append(id);
}

void WorkbenchPluginBase::trackOwnedCommand(const QString &id) const
{
    if (!id.isEmpty() && !m_ownedCommandIds.contains(id))
        m_ownedCommandIds.append(id);
}

void WorkbenchPluginBase::trackOwnedMenu(const QString &id) const
{
    if (!id.isEmpty() && !m_ownedMenuIds.contains(id))
        m_ownedMenuIds.append(id);
}

void WorkbenchPluginBase::trackOwnedAction(QAction *action) const
{
    if (action)
        m_ownedActions.append(QPointer<QAction>(action));
}

bool WorkbenchPluginBase::addPanel(const QString &id,
                                   const QString &title,
                                   QWidget *widget,
                                   IDockManager::Area area,
                                   bool startVisible) const
{
    auto *dockMgr = docks();
    if (!dockMgr) return false;
    const bool ok = dockMgr->addPanel(id, title, widget, area, startVisible);
    if (ok) trackOwnedDock(id);
    return ok;
}

void WorkbenchPluginBase::registerCommand(const QString &id,
                                          const QString &title,
                                          std::function<void()> handler) const
{
    auto *cmdSvc = commands();
    if (!cmdSvc) return;
    cmdSvc->registerCommand(id, title, std::move(handler));
    trackOwnedCommand(id);
}

ICommandService *WorkbenchPluginBase::commands() const
{
    return m_host ? m_host->commands() : nullptr;
}

IWorkspaceService *WorkbenchPluginBase::workspace() const
{
    return m_host ? m_host->workspace() : nullptr;
}

IEditorService *WorkbenchPluginBase::editor() const
{
    return m_host ? m_host->editor() : nullptr;
}

IViewService *WorkbenchPluginBase::views() const
{
    return m_host ? m_host->views() : nullptr;
}

INotificationService *WorkbenchPluginBase::notifications() const
{
    return m_host ? m_host->notifications() : nullptr;
}

IGitService *WorkbenchPluginBase::git() const
{
    return m_host ? m_host->git() : nullptr;
}

ITerminalService *WorkbenchPluginBase::terminal() const
{
    return m_host ? m_host->terminal() : nullptr;
}

IDiagnosticsService *WorkbenchPluginBase::diagnostics() const
{
    return m_host ? m_host->diagnostics() : nullptr;
}

ITaskService *WorkbenchPluginBase::tasks() const
{
    return m_host ? m_host->tasks() : nullptr;
}

IDockManager *WorkbenchPluginBase::docks() const
{
    return m_host ? m_host->docks() : nullptr;
}

IMenuManager *WorkbenchPluginBase::menus() const
{
    return m_host ? m_host->menus() : nullptr;
}

IToolBarManager *WorkbenchPluginBase::toolbars() const
{
    return m_host ? m_host->toolbars() : nullptr;
}

IStatusBarManager *WorkbenchPluginBase::statusBar() const
{
    return m_host ? m_host->statusBar() : nullptr;
}

IWorkspaceManager *WorkbenchPluginBase::workspaceManager() const
{
    return m_host ? m_host->workspaceManager() : nullptr;
}

IProfileManager *WorkbenchPluginBase::profiles() const
{
    return m_host ? m_host->profiles() : nullptr;
}

IComponentService *WorkbenchPluginBase::components() const
{
    return m_host ? m_host->components() : nullptr;
}

void WorkbenchPluginBase::registerService(const QString &name, QObject *service) const
{
    if (m_host)
        m_host->registerService(name, service);
}

QObject *WorkbenchPluginBase::queryService(const QString &name) const
{
    return m_host ? m_host->queryService(name) : nullptr;
}

bool WorkbenchPluginBase::executeCommand(const QString &commandId) const
{
    auto *commandService = commands();
    return commandService ? commandService->executeCommand(commandId) : false;
}

QString WorkbenchPluginBase::workspaceRoot() const
{
    auto *workspaceService = workspace();
    return workspaceService ? workspaceService->rootPath() : QString();
}

QString WorkbenchPluginBase::activeFilePath() const
{
    auto *editorService = editor();
    return editorService ? editorService->activeFilePath() : QString();
}

QString WorkbenchPluginBase::activeLanguageId() const
{
    auto *editorService = editor();
    return editorService ? editorService->activeLanguageId() : QString();
}

QString WorkbenchPluginBase::selectedText() const
{
    auto *editorService = editor();
    return editorService ? editorService->selectedText() : QString();
}

void WorkbenchPluginBase::openFile(const QString &path, int line, int column) const
{
    auto *editorService = editor();
    if (editorService)
        editorService->openFile(path, line, column);
}

void WorkbenchPluginBase::showInfo(const QString &text) const
{
    auto *notificationService = notifications();
    if (notificationService)
        notificationService->info(text);
}

void WorkbenchPluginBase::showWarning(const QString &text) const
{
    auto *notificationService = notifications();
    if (notificationService)
        notificationService->warning(text);
}

void WorkbenchPluginBase::showError(const QString &text) const
{
    auto *notificationService = notifications();
    if (notificationService)
        notificationService->error(text);
}

void WorkbenchPluginBase::showStatusMessage(const QString &text, int timeoutMs) const
{
    auto *notificationService = notifications();
    if (notificationService)
        notificationService->statusMessage(text, timeoutMs);
}

void WorkbenchPluginBase::showPanel(const QString &panelId) const
{
    auto *dockManager = docks();
    if (dockManager)
        dockManager->showPanel(panelId);
}

void WorkbenchPluginBase::hidePanel(const QString &panelId) const
{
    auto *dockManager = docks();
    if (dockManager)
        dockManager->hidePanel(panelId);
}

void WorkbenchPluginBase::togglePanel(const QString &panelId) const
{
    auto *dockManager = docks();
    if (dockManager)
        dockManager->togglePanel(panelId);
}

QAction *WorkbenchPluginBase::makeCommandAction(const QString &text,
                                                const QString &commandId,
                                                QObject *owner,
                                                const QKeySequence &shortcut) const
{
    auto *commandService = commands();
    if (!owner || !commandService)
        return nullptr;

    auto *action = new QAction(text, owner);
    if (!shortcut.isEmpty()) {
        action->setShortcut(shortcut);
        // Application-wide so debug F5/F10/F11, build Ctrl+B, etc. fire even
        // when a chat input, dock, or sidebar widget has focus.
        action->setShortcutContext(Qt::ApplicationShortcut);
    }

    QObject::connect(action, &QAction::triggered, owner, [commandService, commandId]() {
        qDebug() << "[QAction] triggered → executeCommand:" << commandId;
        commandService->executeCommand(commandId);
    });
    return action;
}

QAction *WorkbenchPluginBase::addMenuCommand(IMenuManager::MenuLocation location,
                                             const QString &text,
                                             const QString &commandId,
                                             QObject *owner,
                                             const QKeySequence &shortcut) const
{
    auto *action = makeCommandAction(text, commandId, owner, shortcut);
    auto *menuManager = menus();
    if (action && menuManager) {
        menuManager->addAction(location, action);
        trackOwnedAction(action);  // auto-cleanup on shutdown (rule L7)
    }
    return action;
}

QAction *WorkbenchPluginBase::addMenuCommand(const QString &menuId,
                                             const QString &text,
                                             const QString &commandId,
                                             QObject *owner,
                                             const QKeySequence &shortcut) const
{
    auto *action = makeCommandAction(text, commandId, owner, shortcut);
    auto *menuManager = menus();
    if (action && menuManager) {
        menuManager->addAction(menuId, action);
        trackOwnedAction(action);
    }
    return action;
}

void WorkbenchPluginBase::addMenuSeparator(IMenuManager::MenuLocation location) const
{
    auto *menuManager = menus();
    if (menuManager)
        menuManager->addSeparator(location);
}

QMenu *WorkbenchPluginBase::ensureMenu(const QString &menuId, const QString &title) const
{
    auto *menuManager = menus();
    if (!menuManager) return nullptr;
    auto *menu = menuManager->createMenu(menuId, title);
    if (menu) trackOwnedMenu(menuId);
    return menu;
}

bool WorkbenchPluginBase::createToolBar(const QString &id,
                                        const QString &title,
                                        IToolBarManager::Edge edge) const
{
    auto *toolBarManager = toolbars();
    if (!toolBarManager) return false;
    const bool ok = toolBarManager->createToolBar(id, title, edge);
    if (ok) trackOwnedToolbar(id);
    return ok;
}

QAction *WorkbenchPluginBase::addToolBarCommand(const QString &toolBarId,
                                                const QString &text,
                                                const QString &commandId,
                                                QObject *owner,
                                                const QKeySequence &shortcut) const
{
    auto *action = makeCommandAction(text, commandId, owner, shortcut);
    auto *toolBarManager = toolbars();
    if (action && toolBarManager) {
        toolBarManager->addAction(toolBarId, action);
        trackOwnedAction(action);
    }
    return action;
}

void WorkbenchPluginBase::addToolBarCommands(const QString &toolBarId,
                                             const QList<CommandSpec> &commands,
                                             QObject *owner) const
{
    for (const CommandSpec &command : commands) {
        if (command.separatorBefore)
            addToolBarSeparator(toolBarId);
        auto *action = addToolBarCommand(toolBarId, command.text, command.commandId, owner,
                                         command.shortcut);
        if (action && !command.iconName.isEmpty())
            action->setIcon(ThemeIcons::icon(command.iconName));
    }
}

void WorkbenchPluginBase::addToolBarWidget(const QString &id, QWidget *widget) const
{
    auto *toolBarManager = toolbars();
    if (toolBarManager && widget)
        toolBarManager->addWidget(id, widget);
}

void WorkbenchPluginBase::addToolBarSeparator(const QString &id) const
{
    auto *toolBarManager = toolbars();
    if (toolBarManager)
        toolBarManager->addSeparator(id);
}

void WorkbenchPluginBase::addMenuCommands(IMenuManager::MenuLocation location,
                                          const QList<CommandSpec> &commands,
                                          QObject *owner) const
{
    for (const CommandSpec &command : commands) {
        if (command.separatorBefore)
            addMenuSeparator(location);
        addMenuCommand(location, command.text, command.commandId, owner,
                       command.shortcut);
    }
}
