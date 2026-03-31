#include "devopsplugin.h"
#include "devopspanel.h"
#include "servicediscovery.h"

#include "sdk/icommandservice.h"
#include "core/imenumanager.h"

#include <QFileInfo>

DevOpsPlugin::DevOpsPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo DevOpsPlugin::info() const
{
    PluginInfo pi;
    pi.id          = QStringLiteral("org.exorcist.devops");
    pi.name        = QStringLiteral("DevOps Tools");
    pi.version     = QStringLiteral("1.0.0");
    pi.description = QStringLiteral(
        "Container, infrastructure-as-code, and CI/CD integration.");
    return pi;
}

bool DevOpsPlugin::initializePlugin()
{
    m_discovery = new ServiceDiscovery(this);
    m_panel     = new DevOpsPanel(m_discovery, nullptr);

    const QString root = workspaceRoot();
    if (!root.isEmpty()) {
        m_workDir = root;
        m_discovery->setWorkspaceRoot(root);
        m_panel->setWorkingDirectory(root);
    }

    registerCommands();
    return true;
}

void DevOpsPlugin::shutdownPlugin()
{
    if (m_panel && m_panel->isRunning())
        m_panel->stopCommand();
}

QWidget *DevOpsPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("DevOpsDock") && m_panel) {
        m_panel->setParent(parent);
        return m_panel;
    }
    return nullptr;
}

void DevOpsPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds) return;

    cmds->registerCommand(QStringLiteral("devops.showPanel"),
                          tr("Show DevOps Panel"), [this]() {
        showPanel(QStringLiteral("DevOpsDock"));
    });

    cmds->registerCommand(QStringLiteral("devops.dockerCompose"),
                          tr("Docker Compose Up"), [this]() {
        if (!m_discovery->hasDockerCompose()) {
            showInfo(tr("No docker-compose.yml found in workspace"));
            return;
        }
        showPanel(QStringLiteral("DevOpsDock"));
        const QString file = m_discovery->dockerComposeFile();
        m_panel->runCommand(tr("Docker Compose Up"),
                            QStringLiteral("docker compose -f \"%1\" up").arg(file));
    });

    cmds->registerCommand(QStringLiteral("devops.dockerBuild"),
                          tr("Docker Build"), [this]() {
        if (!m_discovery->hasDocker()) {
            showInfo(tr("No Dockerfile found in workspace"));
            return;
        }
        showPanel(QStringLiteral("DevOpsDock"));
        m_panel->runCommand(tr("Docker Build"),
                            QStringLiteral("docker build -t %1 .")
                                .arg(QFileInfo(m_workDir).fileName().toLower()));
    });

    cmds->registerCommand(QStringLiteral("devops.terraformPlan"),
                          tr("Terraform Plan"), [this]() {
        if (!m_discovery->hasTerraform()) {
            showInfo(tr("No .tf files found in workspace"));
            return;
        }
        showPanel(QStringLiteral("DevOpsDock"));
        m_panel->runCommand(tr("Terraform Plan"),
                            QStringLiteral("terraform plan"));
    });

    cmds->registerCommand(QStringLiteral("devops.terraformApply"),
                          tr("Terraform Apply"), [this]() {
        if (!m_discovery->hasTerraform()) {
            showInfo(tr("No .tf files found in workspace"));
            return;
        }
        showPanel(QStringLiteral("DevOpsDock"));
        m_panel->runCommand(tr("Terraform Apply"),
                            QStringLiteral("terraform apply -auto-approve"));
    });

    cmds->registerCommand(QStringLiteral("devops.refreshServices"),
                          tr("Refresh Services"), [this]() {
        if (m_discovery) m_discovery->refresh();
    });

    // Menu: DevOps
    auto *menu = ensureMenu(QStringLiteral("devops"), tr("&DevOps"));
    if (menu) {
        addMenuCommand(QStringLiteral("devops"),
                       tr("Docker Compose Up"), QStringLiteral("devops.dockerCompose"),
                       this);
        addMenuCommand(QStringLiteral("devops"),
                       tr("Docker Build"), QStringLiteral("devops.dockerBuild"),
                       this);
        addMenuCommand(QStringLiteral("devops"),
                       tr("Terraform Plan"), QStringLiteral("devops.terraformPlan"),
                       this);
        addMenuCommand(QStringLiteral("devops"),
                       tr("Terraform Apply"), QStringLiteral("devops.terraformApply"),
                       this);
    }
}
