#pragma once

#include <QString>

// ── Host Services ────────────────────────────────────────────────────────────
//
// The single root interface that every plugin receives on initialization.
// Provides typed access to all stable SDK services.
//
// Plugin code should look like:
//
//   bool initialize(IHostServices *host) override
//   {
//       host->commands()->registerCommand("myPlugin.run", "Run", [host]() {
//           host->notifications()->info("Running...");
//       });
//       return true;
//   }
//
// A plugin must NEVER cast IHostServices to a concrete type or access
// MainWindow / internal classes. The SDK boundary is the only stable contract.

class QObject;
class ICommandService;
class IWorkspaceService;
class IEditorService;
class IViewService;
class INotificationService;
class IGitService;
class ITerminalService;
class IDiagnosticsService;
class ITaskService;

// Core UI manager interfaces
class IDockManager;
class IMenuManager;
class IToolBarManager;
class IStatusBarManager;
class IWorkspaceManager;
class IProfileManager;
class IComponentService;

class IHostServices
{
public:
    virtual ~IHostServices() = default;

    // ── Core services ─────────────────────────────────────────────────────

    /// Command registration and execution.
    virtual ICommandService *commands() = 0;

    /// Workspace / filesystem queries.
    virtual IWorkspaceService *workspace() = 0;

    /// Active editor interaction.
    virtual IEditorService *editor() = 0;

    /// Panel / view management.
    virtual IViewService *views() = 0;

    /// Toast / status bar notifications.
    virtual INotificationService *notifications() = 0;

    // ── Subsystem services ────────────────────────────────────────────────

    /// Git repository queries.
    virtual IGitService *git() = 0;

    /// Terminal interaction.
    virtual ITerminalService *terminal() = 0;

    /// LSP diagnostics.
    virtual IDiagnosticsService *diagnostics() = 0;

    /// Build / test task runner.
    virtual ITaskService *tasks() = 0;

    // ── Core UI managers ──────────────────────────────────────────────────
    //
    // These are the plugin-first extension points. Plugins use these
    // to contribute dock panels, menus, toolbars, and status bar items
    // at runtime. The core IDE is just a shell — everything else comes
    // from plugins through these interfaces.

    /// Dock panel management (create, show, hide, pin panels).
    virtual IDockManager *docks() = 0;

    /// Menu bar management (add menus, actions, submenus).
    virtual IMenuManager *menus() = 0;

    /// Toolbar management (create toolbars, add actions/widgets).
    virtual IToolBarManager *toolbars() = 0;

    /// Status bar management (add items, show messages).
    virtual IStatusBarManager *statusBar() = 0;

    /// Workspace/folder management (open folder, files, solution).
    virtual IWorkspaceManager *workspaceManager() = 0;

    /// Development profile management (activate, detect, switch profiles).
    virtual IProfileManager *profiles() = 0;

    /// Component management (create, destroy, query dynamic component instances).
    virtual IComponentService *components() = 0;

    // ── Dynamic service registry ──────────────────────────────────────────

    /// Register a named service. Plugins use this to expose their own services.
    virtual void registerService(const QString &name, QObject *service) = 0;

    /// Query a named service by key. Returns nullptr if not found.
    virtual QObject *queryService(const QString &name) = 0;

    /// Typed convenience for queryService.
    template<typename T>
    T *service(const QString &name) { return qobject_cast<T *>(queryService(name)); }
};
