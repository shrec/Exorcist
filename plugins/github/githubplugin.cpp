#include "githubplugin.h"
#include "ghservice.h"
#include "ghpanel.h"
#include "sdk/icommandservice.h"
#include "core/imenumanager.h"

PluginInfo GitHubPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.github"),
        QStringLiteral("GitHub"),
        QStringLiteral("1.0.0"),
        QStringLiteral("GitHub CLI integration — PRs, Issues, Actions, Releases"),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {}
    };
}

bool GitHubPlugin::initializePlugin()
{
    m_service = new GhService(this);
    m_panel = new GhPanel(m_service, nullptr);

    const QString root = workspaceRoot();
    if (!root.isEmpty())
        m_service->setWorkingDirectory(root);

    registerCommands();
    installMenusAndToolBar();

    return true;
}

void GitHubPlugin::shutdownPlugin()
{
    m_service = nullptr;
    m_panel = nullptr;
}

QWidget *GitHubPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QStringLiteral("GitHubDock")) {
        m_panel->setParent(parent);
        return m_panel;
    }
    return nullptr;
}

void GitHubPlugin::registerCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(QStringLiteral("github.refresh"), tr("Refresh GitHub"), [this]() {
        if (m_panel)
            m_panel->refresh();
    });
}

void GitHubPlugin::installMenusAndToolBar()
{
    ensureMenu(QStringLiteral("github"), tr("GitHub"));
    addMenuCommand(QStringLiteral("github"), tr("&Refresh GitHub"),
                   QStringLiteral("github.refresh"), this);

    createToolBar(QStringLiteral("github"), tr("GitHub"));
    addToolBarCommand(QStringLiteral("github"), tr("Refresh"),
                      QStringLiteral("github.refresh"), this);
}
