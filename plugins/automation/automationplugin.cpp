#include "automationplugin.h"
#include "taskdiscovery.h"
#include "taskrunnerpanel.h"

#include "sdk/icommandservice.h"
#include "core/imenumanager.h"

AutomationPlugin::AutomationPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo AutomationPlugin::info() const
{
    PluginInfo pi;
    pi.id          = QStringLiteral("org.exorcist.automation");
    pi.name        = QStringLiteral("Automation");
    pi.version     = QStringLiteral("1.0.0");
    pi.description = QStringLiteral(
        "Task runner integration: discovers tasks from Taskfile, justfile, "
        "Makefile, package.json and provides one-click execution.");
    return pi;
}

bool AutomationPlugin::initializePlugin()
{
    m_discovery = new TaskDiscovery(this);
    m_panel     = new TaskRunnerPanel(m_discovery, nullptr);

    const QString root = workspaceRoot();
    if (!root.isEmpty()) {
        m_workDir = root;
        m_discovery->setWorkspaceRoot(root);
        m_panel->setWorkingDirectory(root);
    }

    registerCommands();
    return true;
}

void AutomationPlugin::shutdownPlugin()
{
    if (m_panel && m_panel->isRunning())
        m_panel->stopTask();
}

QWidget *AutomationPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("TaskRunnerDock") && m_panel) {
        m_panel->setParent(parent);
        return m_panel;
    }
    return nullptr;
}

void AutomationPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds) return;

    cmds->registerCommand(QStringLiteral("automation.showTasks"),
                          tr("Show Task Runner"), [this]() {
        showPanel(QStringLiteral("TaskRunnerDock"));
    });

    cmds->registerCommand(QStringLiteral("automation.runTask"),
                          tr("Run Task..."), [this]() {
        showPanel(QStringLiteral("TaskRunnerDock"));
        // Panel tree is shown — user picks a task by double-clicking
    });

    cmds->registerCommand(QStringLiteral("automation.refreshTasks"),
                          tr("Refresh Tasks"), [this]() {
        if (m_discovery)
            m_discovery->refresh();
    });

    cmds->registerCommand(QStringLiteral("automation.runLastTask"),
                          tr("Run Last Task"), [this]() {
        if (m_panel) {
            showPanel(QStringLiteral("TaskRunnerDock"));
            m_panel->runLastTask();
        }
    });

    cmds->registerCommand(QStringLiteral("automation.stopTask"),
                          tr("Stop Running Task"), [this]() {
        if (m_panel)
            m_panel->stopTask();
    });

    // Menu: Run → Tasks
    auto *menu = ensureMenu(QStringLiteral("automation"),
                            tr("&Tasks"));
    if (menu) {
        addMenuCommand(QStringLiteral("automation"),
                       tr("Run Task..."), QStringLiteral("automation.runTask"),
                       this, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
        addMenuCommand(QStringLiteral("automation"),
                       tr("Run Last Task"), QStringLiteral("automation.runLastTask"),
                       this, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R));
        addMenuCommand(QStringLiteral("automation"),
                       tr("Stop Task"), QStringLiteral("automation.stopTask"),
                       this);
        addMenuSeparator(IMenuManager::MenuLocation::Run);
        addMenuCommand(QStringLiteral("automation"),
                       tr("Refresh Tasks"), QStringLiteral("automation.refreshTasks"),
                       this);
    }
}
