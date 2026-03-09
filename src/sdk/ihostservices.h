#pragma once

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

class ICommandService;
class IWorkspaceService;
class IEditorService;
class IViewService;
class INotificationService;
class IGitService;
class ITerminalService;
class IDiagnosticsService;
class ITaskService;

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
};
