#pragma once

#include <QJsonObject>
#include <QString>

// ── ICommandHandler ───────────────────────────────────────────────────────────
//
// Plugins implement this to handle command execution.
// Commands are registered via PluginManifest::commands and wired by
// ContributionRegistry into CommandPalette + keybindings + menus.
//
// A single plugin can handle multiple commands — the command ID is passed
// to execute() so one handler can dispatch many commands.
//
// Usage (C++ SDK):
//   class MyPlugin : public QObject, public IPlugin, public ICommandHandler {
//       void execute(const QString &commandId, const QJsonObject &args) override;
//   };

class ICommandHandler
{
public:
    virtual ~ICommandHandler() = default;

    /// Execute the command. `commandId` matches CommandContribution::id.
    /// `args` carries optional parameters (e.g., from a menu item).
    virtual void execute(const QString &commandId,
                         const QJsonObject &args = {}) = 0;

    /// Whether the command is currently enabled/available.
    /// Default: always enabled.
    virtual bool isEnabled(const QString &commandId) const
    {
        Q_UNUSED(commandId);
        return true;
    }

    /// Whether the command is visible in menus/palette.
    /// Default: always visible.
    virtual bool isVisible(const QString &commandId) const
    {
        Q_UNUSED(commandId);
        return true;
    }
};

#define EXORCIST_COMMAND_HANDLER_IID "org.exorcist.ICommandHandler/1.0"
Q_DECLARE_INTERFACE(ICommandHandler, EXORCIST_COMMAND_HANDLER_IID)
