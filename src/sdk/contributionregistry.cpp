#include "contributionregistry.h"
#include "hostservices.h"

#include "../mainwindow.h"
#include "../plugininterface.h"
#include "../plugin/icommandhandler.h"
#include "../plugin/iviewcontributor.h"
#include "../ui/dock/DockManager.h"
#include "../ui/dock/ExDockWidget.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>

// ── ContributionRegistry ─────────────────────────────────────────────────────

ContributionRegistry::ContributionRegistry(MainWindow *window,
                                           CommandServiceImpl *commands,
                                           QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_commands(commands)
{
}

void ContributionRegistry::registerManifest(const QString &pluginId,
                                            const PluginManifest &manifest,
                                            IPlugin *plugin)
{
    if (!manifest.commands.isEmpty())
        registerCommands(pluginId, manifest.commands, plugin);

    if (!manifest.menus.isEmpty())
        registerMenus(pluginId, manifest.menus);

    if (!manifest.views.isEmpty())
        registerViews(pluginId, manifest.views, plugin);

    if (!manifest.keybindings.isEmpty())
        registerKeybindings(pluginId, manifest.keybindings);

    if (!manifest.statusBarItems.isEmpty())
        registerStatusBarItems(pluginId, manifest.statusBarItems);
}

void ContributionRegistry::unregisterPlugin(const QString &pluginId)
{
    // Remove commands
    if (m_pluginCommands.contains(pluginId)) {
        for (const QString &cmdId : m_pluginCommands[pluginId])
            m_commands->unregisterCommand(cmdId);
        m_pluginCommands.remove(pluginId);
    }

    // Remove command palette entries for this plugin
    m_registeredCommands.erase(
        std::remove_if(m_registeredCommands.begin(), m_registeredCommands.end(),
                       [&](const RegisteredCommand &c) { return c.pluginId == pluginId; }),
        m_registeredCommands.end());

    // Remove views
    if (m_pluginViews.contains(pluginId)) {
        for (const QString &viewId : m_pluginViews[pluginId]) {
            auto it = m_registeredViews.find(viewId);
            if (it != m_registeredViews.end()) {
                if (it->dock) {
                    m_window->dockManager()->removeDockWidget(it->dock);
                }
                m_registeredViews.erase(it);
            }
        }
        m_pluginViews.remove(pluginId);
    }

    // Remove menu actions
    m_registeredMenus.erase(
        std::remove_if(m_registeredMenus.begin(), m_registeredMenus.end(),
                       [&](const RegisteredMenu &m) {
                           if (m.pluginId == pluginId) {
                               if (m.action) m.action->deleteLater();
                               return true;
                           }
                           return false;
                       }),
        m_registeredMenus.end());

    // Remove status bar widgets
    // (status bar widgets are tracked by pluginId prefix on objectName)

    emit commandsChanged();
    emit viewsChanged();
}

// ── Commands ─────────────────────────────────────────────────────────────────

void ContributionRegistry::registerCommands(const QString &pluginId,
                                            const QList<CommandContribution> &commands,
                                            IPlugin *plugin)
{
    auto *handler = dynamic_cast<ICommandHandler *>(plugin);
    QStringList ids;

    for (const CommandContribution &cmd : commands) {
        // Build the handler function
        std::function<void()> fn;
        if (handler) {
            fn = [handler, id = cmd.id]() { handler->execute(id); };
        } else {
            // No handler — command is declared but has no implementation
            fn = []() {};
        }

        m_commands->registerCommand(cmd.id, cmd.title, std::move(fn));

        RegisteredCommand rc;
        rc.pluginId = pluginId;
        rc.commandId = cmd.id;
        rc.title = cmd.title;
        rc.category = cmd.category;
        rc.keybinding = cmd.keybinding;
        m_registeredCommands.append(rc);

        ids.append(cmd.id);
    }

    m_pluginCommands[pluginId] = ids;
    emit commandsChanged();
}

// ── Menus ────────────────────────────────────────────────────────────────────

void ContributionRegistry::registerMenus(const QString &pluginId,
                                         const QList<MenuContribution> &menus)
{
    for (const MenuContribution &menu : menus) {
        QMenu *targetMenu = menuForLocation(menu.location);
        if (!targetMenu) continue;

        // Find the command title from our registered commands
        QString title = menu.commandId;
        for (const auto &rc : m_registeredCommands) {
            if (rc.commandId == menu.commandId) {
                title = rc.category.isEmpty()
                    ? rc.title
                    : rc.category + ": " + rc.title;
                break;
            }
        }

        auto *action = targetMenu->addAction(title);
        connect(action, &QAction::triggered, this, [this, cmdId = menu.commandId]() {
            m_commands->executeCommand(cmdId);
        });

        m_registeredMenus.append({pluginId, action});
    }
}

QMenu *ContributionRegistry::menuForLocation(MenuContribution::Location loc) const
{
    QMenuBar *bar = m_window->menuBar();
    if (!bar) return nullptr;

    // Find existing menus by title
    const QList<QAction *> actions = bar->actions();
    for (QAction *a : actions) {
        QMenu *menu = a->menu();
        if (!menu) continue;

        switch (loc) {
        case MenuContribution::MainMenuFile:
            if (menu->title().contains(tr("File"), Qt::CaseInsensitive))
                return menu;
            break;
        case MenuContribution::MainMenuEdit:
            if (menu->title().contains(tr("Edit"), Qt::CaseInsensitive))
                return menu;
            break;
        case MenuContribution::MainMenuView:
            if (menu->title().contains(tr("View"), Qt::CaseInsensitive))
                return menu;
            break;
        case MenuContribution::MainMenuTools:
            if (menu->title().contains(tr("Tools"), Qt::CaseInsensitive))
                return menu;
            break;
        case MenuContribution::MainMenuHelp:
            if (menu->title().contains(tr("Help"), Qt::CaseInsensitive))
                return menu;
            break;
        default:
            break;
        }
    }

    return nullptr;
}

// ── Views ────────────────────────────────────────────────────────────────────

static exdock::SideBarArea viewLocationToSide(ViewContribution::Location loc)
{
    switch (loc) {
    case ViewContribution::SidebarLeft:   return exdock::SideBarArea::Left;
    case ViewContribution::SidebarRight:  return exdock::SideBarArea::Right;
    case ViewContribution::BottomPanel:   return exdock::SideBarArea::Bottom;
    case ViewContribution::TopPanel:      return exdock::SideBarArea::Top;
    }
    return exdock::SideBarArea::Bottom;
}

void ContributionRegistry::registerViews(const QString &pluginId,
                                         const QList<ViewContribution> &views,
                                         IPlugin *plugin)
{
    auto *viewContributor = dynamic_cast<IViewContributor *>(plugin);
    QStringList ids;

    auto *dm = m_window->dockManager();
    if (!dm) return;

    for (const ViewContribution &view : views) {
        RegisteredView rv;
        rv.pluginId = pluginId;
        rv.viewId = view.id;
        rv.title = view.title;
        rv.location = view.location;

        // Create the dock widget
        auto *dock = new exdock::ExDockWidget(view.title, m_window);
        dock->setDockId(view.id);

        // If the plugin implements IViewContributor, ask it for the content
        if (viewContributor) {
            QWidget *content = viewContributor->createView(view.id, dock);
            if (content) {
                dock->setContentWidget(content);
            }
        } else {
            // Placeholder — plugin can populate later via IViewService
            auto *placeholder = new QLabel(tr("Loading %1...").arg(view.title), dock);
            placeholder->setAlignment(Qt::AlignCenter);
            dock->setContentWidget(placeholder);
        }

        const auto side = viewLocationToSide(view.location);
        dm->addDockWidget(dock, side, view.defaultVisible);

        rv.dock = dock;
        m_registeredViews[view.id] = rv;
        ids.append(view.id);
    }

    m_pluginViews[pluginId] = ids;
    emit viewsChanged();
}

// ── Keybindings ──────────────────────────────────────────────────────────────

void ContributionRegistry::registerKeybindings(const QString &pluginId,
                                               const QList<KeybindingContribution> &keybindings)
{
    Q_UNUSED(pluginId)

    for (const KeybindingContribution &kb : keybindings) {
        if (kb.key.isEmpty()) continue;

        // Create a shortcut action on the main window
        auto *action = new QAction(m_window);
        action->setShortcut(QKeySequence(kb.key));
        connect(action, &QAction::triggered, this, [this, cmdId = kb.commandId]() {
            m_commands->executeCommand(cmdId);
        });
        m_window->addAction(action);

        m_registeredMenus.append({pluginId, action});
    }
}

// ── Status bar items ─────────────────────────────────────────────────────────

void ContributionRegistry::registerStatusBarItems(const QString &pluginId,
                                                  const QList<StatusBarContribution> &items)
{
    auto *statusBar = m_window->statusBar();
    if (!statusBar) return;

    for (const StatusBarContribution &item : items) {
        auto *label = new QLabel(item.text, statusBar);
        label->setObjectName(pluginId + "." + item.id);
        if (!item.tooltip.isEmpty())
            label->setToolTip(item.tooltip);

        if (!item.command.isEmpty()) {
            label->setCursor(Qt::PointingHandCursor);
            label->installEventFilter(this); // handle click
            label->setProperty("_commandId", item.command);
        }

        if (item.alignment == StatusBarContribution::Left) {
            statusBar->addWidget(label);
        } else {
            statusBar->addPermanentWidget(label);
        }

        m_statusBarWidgets[item.id] = label;
    }
}

// ── Command palette integration ──────────────────────────────────────────────

QStringList ContributionRegistry::commandPaletteEntries() const
{
    QStringList entries;
    entries.reserve(m_registeredCommands.size());

    for (const RegisteredCommand &rc : m_registeredCommands) {
        QString entry;
        if (!rc.category.isEmpty())
            entry = rc.category + ": " + rc.title;
        else
            entry = rc.title;

        if (!rc.keybinding.isEmpty())
            entry += "\t" + rc.keybinding;

        entries.append(entry);
    }

    return entries;
}

bool ContributionRegistry::executeCommandByIndex(int index) const
{
    if (index < 0 || index >= m_registeredCommands.size())
        return false;

    return m_commands->executeCommand(m_registeredCommands[index].commandId);
}

QStringList ContributionRegistry::registeredViewIds() const
{
    return m_registeredViews.keys();
}
