#pragma once

#include <QString>
#include <functional>

// ── Command Service ──────────────────────────────────────────────────────────
//
// Stable SDK interface for command registration and invocation.
// Plugins register commands here; the IDE wires them to menus, keybindings,
// and the command palette automatically via PluginManifest contributions.

class ICommandService
{
public:
    virtual ~ICommandService() = default;

    /// Register a command handler. Replaces any previous handler for this id.
    virtual void registerCommand(const QString &id,
                                 const QString &title,
                                 std::function<void()> handler) = 0;

    /// Remove a previously registered command.
    virtual void unregisterCommand(const QString &id) = 0;

    /// Execute a command by id. Returns false if command not found.
    virtual bool executeCommand(const QString &id) = 0;

    /// Check whether a command is registered.
    virtual bool hasCommand(const QString &id) const = 0;
};
