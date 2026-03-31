#include "contributionregistry.h"
#include "hostservices.h"

#include "../core/imenumanager.h"
#include "../bootstrap/menumanagerimpl.h"
#include "../mainwindow.h"
#include "../plugininterface.h"
#include "../plugin/icommandhandler.h"
#include "../plugin/ilanguagecontributor.h"
#include "../plugin/isettingscontributor.h"
#include "../plugin/itaskcontributor.h"
#include "../plugin/iviewcontributor.h"
#include "../ui/dock/DockManager.h"
#include "../ui/dock/ExDockWidget.h"

#include <QAction>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
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

    if (!manifest.languages.isEmpty())
        registerLanguages(pluginId, manifest.languages, plugin);

    if (!manifest.tasks.isEmpty())
        registerTasks(pluginId, manifest.tasks, plugin);

    if (!manifest.settings.isEmpty())
        registerSettings(pluginId, manifest.settings, plugin);

    if (!manifest.themes.isEmpty())
        registerThemes(pluginId, manifest.themes);
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

    // Remove language contributions
    {
        bool changed = false;
        m_registeredLanguages.erase(
            std::remove_if(m_registeredLanguages.begin(), m_registeredLanguages.end(),
                           [&](const RegisteredLanguage &l) {
                               if (l.pluginId == pluginId) { changed = true; return true; }
                               return false;
                           }),
            m_registeredLanguages.end());
        if (changed) emit languagesChanged();
    }

    // Remove task contributions
    m_registeredTasks.erase(
        std::remove_if(m_registeredTasks.begin(), m_registeredTasks.end(),
                       [&](const RegisteredTask &t) { return t.pluginId == pluginId; }),
        m_registeredTasks.end());

    // Remove setting contributions
    {
        bool changed = false;
        m_registeredSettings.erase(
            std::remove_if(m_registeredSettings.begin(), m_registeredSettings.end(),
                           [&](const RegisteredSetting &s) {
                               if (s.pluginId == pluginId) { changed = true; return true; }
                               return false;
                           }),
            m_registeredSettings.end());
        if (changed) emit settingsChanged();
    }

    // Remove theme contributions
    {
        bool changed = false;
        m_registeredThemes.erase(
            std::remove_if(m_registeredThemes.begin(), m_registeredThemes.end(),
                           [&](const RegisteredTheme &t) {
                               if (t.pluginId == pluginId) { changed = true; return true; }
                               return false;
                           }),
            m_registeredThemes.end());
        if (changed) emit themesChanged();
    }

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
    if (!m_window)
        return nullptr;

    auto mapLocation = [](MenuContribution::Location location) -> std::optional<IMenuManager::MenuLocation> {
        switch (location) {
        case MenuContribution::MainMenuFile:       return IMenuManager::File;
        case MenuContribution::MainMenuEdit:       return IMenuManager::Edit;
        case MenuContribution::MainMenuView:       return IMenuManager::View;
        case MenuContribution::MainMenuGit:        return IMenuManager::Git;
        case MenuContribution::MainMenuProject:    return IMenuManager::Project;
        case MenuContribution::MainMenuSelection:  return IMenuManager::Selection;
        case MenuContribution::MainMenuBuild:      return IMenuManager::Build;
        case MenuContribution::MainMenuDebug:      return IMenuManager::Debug;
        case MenuContribution::MainMenuTest:       return IMenuManager::Test;
        case MenuContribution::MainMenuAnalyze:    return IMenuManager::Analyze;
        case MenuContribution::MainMenuRun:        return IMenuManager::Run;
        case MenuContribution::MainMenuTerminal:   return IMenuManager::Terminal;
        case MenuContribution::MainMenuTools:      return IMenuManager::Tools;
        case MenuContribution::MainMenuExtensions: return IMenuManager::Extensions;
        case MenuContribution::MainMenuWindow:     return IMenuManager::Window;
        case MenuContribution::MainMenuHelp:       return IMenuManager::Help;
        default:                                   return std::nullopt;
        }
    };

    const auto mapped = mapLocation(loc);
    if (!mapped.has_value())
        return nullptr;

    for (QObject *child : m_window->children()) {
        auto *menuMgr = qobject_cast<MenuManagerImpl *>(child);
        if (menuMgr)
            return menuMgr->menu(mapped.value());
    }

    QMenuBar *bar = m_window->menuBar();
    if (!bar)
        return nullptr;

    auto menuTitle = [](IMenuManager::MenuLocation location) -> QString {
        switch (location) {
        case IMenuManager::File:       return QObject::tr("&File");
        case IMenuManager::Edit:       return QObject::tr("&Edit");
        case IMenuManager::View:       return QObject::tr("&View");
        case IMenuManager::Git:        return QObject::tr("&Git");
        case IMenuManager::Project:    return QObject::tr("&Project");
        case IMenuManager::Selection:  return QObject::tr("&Selection");
        case IMenuManager::Build:      return QObject::tr("&Build");
        case IMenuManager::Debug:      return QObject::tr("&Debug");
        case IMenuManager::Test:       return QObject::tr("&Test");
        case IMenuManager::Analyze:    return QObject::tr("&Analyze");
        case IMenuManager::Run:        return QObject::tr("&Run");
        case IMenuManager::Terminal:   return QObject::tr("&Terminal");
        case IMenuManager::Tools:      return QObject::tr("&Tools");
        case IMenuManager::Extensions: return QObject::tr("E&xtensions");
        case IMenuManager::Window:     return QObject::tr("&Window");
        case IMenuManager::Help:       return QObject::tr("&Help");
        case IMenuManager::Custom:     return QObject::tr("Custom");
        }
        return {};
    };

    const QString wantedTitle = menuTitle(mapped.value());
    for (QAction *action : bar->actions()) {
        if (QMenu *menu = action->menu()) {
            if (menu->title() == wantedTitle)
                return menu;
        }
    }

    return bar->addMenu(wantedTitle);
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
        // If a dock with this ID already exists (e.g. registered by DockBootstrap
        // before plugins loaded), adopt it instead of creating a duplicate.
        // Creating a second dock with the same ID would reparent the content
        // widget away from the original dock, leaving it empty.
        if (auto *existing = dm->dockWidget(view.id)) {
            RegisteredView rv;
            rv.pluginId = pluginId;
            rv.viewId   = view.id;
            rv.title    = view.title;
            rv.location = view.location;
            rv.dock     = existing;
            m_registeredViews[view.id] = rv;
            ids.append(view.id);
            continue;
        }

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

// ── Languages ────────────────────────────────────────────────────────────────

void ContributionRegistry::registerLanguages(const QString &pluginId,
                                             const QList<LanguageContribution> &languages,
                                             IPlugin *plugin)
{
    auto *contributor = dynamic_cast<ILanguageContributor *>(plugin);

    for (const LanguageContribution &lang : languages) {
        RegisteredLanguage rl;
        rl.pluginId = pluginId;
        rl.contribution = lang;
        rl.contributor = contributor;
        m_registeredLanguages.append(rl);
    }

    emit languagesChanged();
}

const LanguageContribution *ContributionRegistry::languageForExtension(const QString &ext) const
{
    // Normalize: accept both ".rs" and "rs"
    QString normalized = ext.startsWith('.') ? ext : ('.' + ext);

    for (const RegisteredLanguage &rl : m_registeredLanguages) {
        for (const QString &langExt : rl.contribution.extensions) {
            if (langExt.compare(normalized, Qt::CaseInsensitive) == 0)
                return &rl.contribution;
        }
    }
    return nullptr;
}

const LanguageContribution *ContributionRegistry::languageById(const QString &id) const
{
    for (const RegisteredLanguage &rl : m_registeredLanguages) {
        if (rl.contribution.id == id)
            return &rl.contribution;
    }
    return nullptr;
}

QList<LanguageContribution> ContributionRegistry::registeredLanguages() const
{
    QList<LanguageContribution> result;
    result.reserve(m_registeredLanguages.size());
    for (const RegisteredLanguage &rl : m_registeredLanguages)
        result.append(rl.contribution);
    return result;
}

ILanguageContributor *ContributionRegistry::languageContributor(const QString &languageId) const
{
    for (const RegisteredLanguage &rl : m_registeredLanguages) {
        if (rl.contribution.id == languageId)
            return rl.contributor;
    }
    return nullptr;
}

// ── Tasks ────────────────────────────────────────────────────────────────────

void ContributionRegistry::registerTasks(const QString &pluginId,
                                         const QList<TaskContribution> &tasks,
                                         IPlugin *plugin)
{
    auto *contributor = dynamic_cast<ITaskContributor *>(plugin);

    for (const TaskContribution &task : tasks) {
        RegisteredTask rt;
        rt.pluginId = pluginId;
        rt.contribution = task;
        rt.contributor = contributor;
        m_registeredTasks.append(rt);
    }
}

QList<TaskContribution> ContributionRegistry::registeredTasks() const
{
    QList<TaskContribution> result;
    result.reserve(m_registeredTasks.size());
    for (const RegisteredTask &rt : m_registeredTasks)
        result.append(rt.contribution);
    return result;
}

ITaskContributor *ContributionRegistry::taskContributor(const QString &taskType) const
{
    for (const RegisteredTask &rt : m_registeredTasks) {
        if (rt.contributor && rt.contributor->taskType() == taskType)
            return rt.contributor;
    }
    return nullptr;
}

// ── Settings ─────────────────────────────────────────────────────────────────

void ContributionRegistry::registerSettings(const QString &pluginId,
                                            const QList<SettingContribution> &settings,
                                            IPlugin *plugin)
{
    auto *contributor = dynamic_cast<ISettingsContributor *>(plugin);

    QSettings appSettings;
    for (const SettingContribution &setting : settings) {
        RegisteredSetting rs;
        rs.pluginId = pluginId;
        rs.contribution = setting;
        rs.contributor = contributor;
        m_registeredSettings.append(rs);

        // Persist default value if not already set
        const QString fullKey = QStringLiteral("plugins/%1/%2").arg(pluginId, setting.key);
        if (!appSettings.contains(fullKey))
            appSettings.setValue(fullKey, setting.defaultValue);
    }

    emit settingsChanged();
}

QList<SettingContribution> ContributionRegistry::registeredSettings() const
{
    QList<SettingContribution> result;
    result.reserve(m_registeredSettings.size());
    for (const RegisteredSetting &rs : m_registeredSettings)
        result.append(rs.contribution);
    return result;
}

QList<SettingContribution> ContributionRegistry::settingsForPlugin(const QString &pluginId) const
{
    QList<SettingContribution> result;
    for (const RegisteredSetting &rs : m_registeredSettings) {
        if (rs.pluginId == pluginId)
            result.append(rs.contribution);
    }
    return result;
}

void ContributionRegistry::notifySettingChanged(const QString &pluginId,
                                                const QString &key,
                                                const QVariant &value)
{
    for (const RegisteredSetting &rs : m_registeredSettings) {
        if (rs.pluginId == pluginId && rs.contributor) {
            rs.contributor->settingChanged(key, value);
            return;
        }
    }
}

// ── Themes ───────────────────────────────────────────────────────────────────

void ContributionRegistry::registerThemes(const QString &pluginId,
                                          const QList<ThemeContribution> &themes)
{
    for (const ThemeContribution &theme : themes) {
        RegisteredTheme rt;
        rt.pluginId = pluginId;
        rt.contribution = theme;
        m_registeredThemes.append(rt);
    }

    emit themesChanged();
}

QList<ThemeContribution> ContributionRegistry::registeredThemes() const
{
    QList<ThemeContribution> result;
    result.reserve(m_registeredThemes.size());
    for (const RegisteredTheme &rt : m_registeredThemes)
        result.append(rt.contribution);
    return result;
}
