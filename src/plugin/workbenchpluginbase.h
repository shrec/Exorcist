#pragma once

#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "core/itoolbarmanager.h"
#include "plugininterface.h"
#include "sdk/ihostservices.h"

#include <QKeySequence>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <functional>

class QAction;
class QMenu;
class QObject;
class QWidget;
class ICommandService;
class IWorkspaceService;
class IEditorService;
class IViewService;
class INotificationService;
class IGitService;
class ITerminalService;
class IDiagnosticsService;
class ITaskService;
class IDockManager;
class IStatusBarManager;
class IWorkspaceManager;
class IProfileManager;
class IComponentService;

/// Shared base for workbench-facing plugins.
///
/// Keeps MainWindow as a shell/container while giving plugins a consistent
/// place to own their commands, menus, toolbars, and docks.
class WorkbenchPluginBase : public IPlugin
{
public:
    struct CommandSpec {
        QString text;
        QString commandId;
        QKeySequence shortcut;
        bool separatorBefore = false;
        QString iconName;    // optional: name of icon in ThemeIcons (e.g. "play")
    };

    bool initialize(IHostServices *host) final;
    void shutdown() final;

    // Workspace lifecycle — broadcast from PluginManager.  Default no-op.
    // Plugins override these to react to workspace open / close instead of
    // the container poking at plugin-owned state.  See CLAUDE.md rule L2.
    void onWorkspaceOpened(const QString &root) override { Q_UNUSED(root) }
    void onWorkspaceClosed() override {}

protected:
    WorkbenchPluginBase() = default;
    ~WorkbenchPluginBase() override = default;

    virtual bool initializePlugin() = 0;
    virtual void shutdownPlugin() {}

    IHostServices *host() const { return m_host; }

    // ── Lifecycle ownership tracking (rule L1, L7) ───────────────────────
    //
    // Every UI artifact created via the convenience helpers below is
    // automatically tracked here.  shutdown() iterates these lists and
    // unregisters each artifact before calling shutdownPlugin() — so the
    // plugin's UI state evaporates when the plugin unloads, with no
    // container-side cleanup loop required.
    //
    // Plugins that bypass these helpers (e.g. call docks()->addPanel()
    // directly) MUST track ownership themselves and clean up in
    // shutdownPlugin() per rule L1.

    void trackOwnedDock(const QString &id) const;
    void trackOwnedToolbar(const QString &id) const;
    void trackOwnedCommand(const QString &id) const;
    void trackOwnedMenu(const QString &id) const;
    void trackOwnedAction(QAction *action) const;

    // Adds a dock panel and tracks its id for auto-cleanup.  Returns true
    // on success.  Use this instead of docks()->addPanel(...) directly.
    bool addPanel(const QString &id,
                  const QString &title,
                  QWidget *widget,
                  IDockManager::Area area = IDockManager::Left,
                  bool startVisible = false) const;

    ICommandService *commands() const;
    IWorkspaceService *workspace() const;
    IEditorService *editor() const;
    IViewService *views() const;
    INotificationService *notifications() const;
    IGitService *git() const;
    ITerminalService *terminal() const;
    IDiagnosticsService *diagnostics() const;
    ITaskService *tasks() const;
    IDockManager *docks() const;
    IMenuManager *menus() const;
    IToolBarManager *toolbars() const;
    IStatusBarManager *statusBar() const;
    IWorkspaceManager *workspaceManager() const;
    IProfileManager *profiles() const;
    IComponentService *components() const;

    template<typename T>
    T *service(const QString &name) const
    {
        return m_host ? m_host->service<T>(name) : nullptr;
    }

    void registerService(const QString &name, QObject *service) const;
    QObject *queryService(const QString &name) const;

    bool executeCommand(const QString &commandId) const;

    QString workspaceRoot() const;
    QString activeFilePath() const;
    QString activeLanguageId() const;
    QString selectedText() const;

    void openFile(const QString &path, int line = -1, int column = -1) const;

    void showInfo(const QString &text) const;
    void showWarning(const QString &text) const;
    void showError(const QString &text) const;
    void showStatusMessage(const QString &text, int timeoutMs = 5000) const;

    void showPanel(const QString &panelId) const;
    void hidePanel(const QString &panelId) const;
    void togglePanel(const QString &panelId) const;

    QAction *makeCommandAction(const QString &text,
                               const QString &commandId,
                               QObject *owner,
                               const QKeySequence &shortcut = {}) const;

    QAction *addMenuCommand(IMenuManager::MenuLocation location,
                            const QString &text,
                            const QString &commandId,
                            QObject *owner,
                            const QKeySequence &shortcut = {}) const;

    QAction *addMenuCommand(const QString &menuId,
                            const QString &text,
                            const QString &commandId,
                            QObject *owner,
                            const QKeySequence &shortcut = {}) const;

    void addMenuSeparator(IMenuManager::MenuLocation location) const;
    QMenu *ensureMenu(const QString &menuId, const QString &title) const;

    bool createToolBar(const QString &id,
                       const QString &title,
                       IToolBarManager::Edge edge = IToolBarManager::Top) const;
    QAction *addToolBarCommand(const QString &toolBarId,
                               const QString &text,
                               const QString &commandId,
                               QObject *owner,
                               const QKeySequence &shortcut = {}) const;
    void addToolBarCommands(const QString &toolBarId,
                            const QList<CommandSpec> &commands,
                            QObject *owner) const;
    void addToolBarWidget(const QString &id, QWidget *widget) const;
    void addToolBarSeparator(const QString &id) const;

    void addMenuCommands(IMenuManager::MenuLocation location,
                         const QList<CommandSpec> &commands,
                         QObject *owner) const;

    /// Register a command via the host's command service AND track its id
    /// for auto-cleanup on shutdown.  Use this instead of
    /// commands()->registerCommand(...) directly so the command unregisters
    /// itself when the plugin unloads.
    void registerCommand(const QString &id,
                         const QString &title,
                         std::function<void()> handler) const;

private:
    /// Iterates the four owned-id lists and unregisters every artifact
    /// from its respective manager.  Called from shutdown() before
    /// shutdownPlugin() so the plugin's vtable still resolves.
    void cleanupOwnedArtifacts();

    IHostServices *m_host = nullptr;

    // mutable so const convenience helpers can record ownership.  These
    // are not real plugin state — they're accounting for cleanup, hidden
    // from the plugin's logical semantics.
    mutable QStringList               m_ownedDockIds;
    mutable QStringList               m_ownedToolbarIds;
    mutable QStringList               m_ownedCommandIds;
    mutable QStringList               m_ownedMenuIds;
    mutable QList<QPointer<QAction>>  m_ownedActions;
};
