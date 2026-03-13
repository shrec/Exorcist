#include "githubplugin.h"
#include "ghservice.h"
#include "ghpanel.h"
#include "sdk/ihostservices.h"
#include "sdk/iworkspaceservice.h"

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

bool GitHubPlugin::initialize(IHostServices *host)
{
    m_host = host;
    m_service = new GhService(this);

    if (m_host && m_host->workspace()) {
        const QString root = m_host->workspace()->rootPath();
        if (!root.isEmpty())
            m_service->setWorkingDirectory(root);
    }

    return true;
}

void GitHubPlugin::shutdown()
{
    m_service = nullptr;
}

QWidget *GitHubPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QStringLiteral("GitHubDock")) {
        return new GhPanel(m_service, parent);
    }
    return nullptr;
}
