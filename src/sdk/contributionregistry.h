#pragma once

#include "plugin/pluginmanifest.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QList>

#include <functional>

class IPlugin;
class IHostServices;
class MainWindow;
class CommandServiceImpl;

namespace exdock { class DockManager; class ExDockWidget; }

class QAction;
class QMenu;

// ── ContributionRegistry ─────────────────────────────────────────────────────
//
// Consumes PluginManifest declarations and wires them into the running IDE.
//
// For each contribution type, the registry:
//   1. Validates the contribution data
//   2. Registers it with the appropriate IDE subsystem
//   3. Tracks ownership so everything is cleaned up when a plugin unloads
//
// Contributions that require a plugin callback (views, commands) use the
// IViewContributor / ICommandHandler interfaces. Purely declarative
// contributions (keybindings, menus, themes) are wired directly.

class ContributionRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ContributionRegistry(MainWindow *window,
                                  CommandServiceImpl *commands,
                                  QObject *parent = nullptr);

    /// Process all contributions from a plugin's manifest.
    /// commandHandler (if provided) will be called for commands.
    /// If the plugin implements IViewContributor, views will be lazily created.
    void registerManifest(const QString &pluginId,
                          const PluginManifest &manifest,
                          IPlugin *plugin);

    /// Remove all contributions from a plugin.
    void unregisterPlugin(const QString &pluginId);

    /// Get all registered command entries for the command palette.
    /// Returns "Category: Title\tKeybinding" format strings.
    QStringList commandPaletteEntries() const;

    /// Execute a command palette entry by index.
    bool executeCommandByIndex(int index) const;

    /// All registered view IDs from plugins.
    QStringList registeredViewIds() const;

signals:
    void commandsChanged();
    void viewsChanged();

private:
    void registerCommands(const QString &pluginId,
                          const QList<CommandContribution> &commands,
                          IPlugin *plugin);
    void registerMenus(const QString &pluginId,
                       const QList<MenuContribution> &menus);
    void registerViews(const QString &pluginId,
                       const QList<ViewContribution> &views,
                       IPlugin *plugin);
    void registerKeybindings(const QString &pluginId,
                             const QList<KeybindingContribution> &keybindings);
    void registerStatusBarItems(const QString &pluginId,
                                const QList<StatusBarContribution> &items);

    QMenu *menuForLocation(MenuContribution::Location loc) const;

    struct RegisteredCommand {
        QString pluginId;
        QString commandId;
        QString title;
        QString category;
        QString keybinding;
    };

    struct RegisteredView {
        QString pluginId;
        QString viewId;
        QString title;
        ViewContribution::Location location;
        exdock::ExDockWidget *dock = nullptr;
    };

    struct RegisteredMenu {
        QString pluginId;
        QAction *action = nullptr;
    };

    MainWindow *m_window;
    CommandServiceImpl *m_commands;

    // Ordered list for command palette indexing
    QList<RegisteredCommand> m_registeredCommands;

    // View contributions
    QHash<QString, RegisteredView> m_registeredViews;

    // Menu actions (for cleanup)
    QList<RegisteredMenu> m_registeredMenus;

    // Status bar widgets (for cleanup)
    QHash<QString, QWidget *> m_statusBarWidgets;

    // Plugin → contribution IDs for cleanup
    QHash<QString, QStringList> m_pluginCommands;
    QHash<QString, QStringList> m_pluginViews;
};
