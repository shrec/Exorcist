#include "servicediscovery.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

ServiceDiscovery::ServiceDiscovery(QObject *parent)
    : QObject(parent)
{
}

void ServiceDiscovery::setWorkspaceRoot(const QString &root)
{
    if (m_root == root) return;
    m_root = root;
    refresh();
}

void ServiceDiscovery::refresh()
{
    m_services.clear();
    if (m_root.isEmpty()) {
        emit servicesChanged();
        return;
    }

    const QDir dir(m_root);

    // ── Docker ────────────────────────────────────────────────────────────
    if (dir.exists(QStringLiteral("Dockerfile"))) {
        m_services.append({
            DevOpsService::Docker,
            QStringLiteral("Docker"),
            dir.filePath(QStringLiteral("Dockerfile")),
            QStringLiteral("Container image definition")
        });
    }
    // Check for multi-stage Dockerfiles
    {
        QDirIterator it(m_root, { QStringLiteral("Dockerfile.*") },
                        QDir::Files, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            it.next();
            m_services.append({
                DevOpsService::Docker,
                QStringLiteral("Docker (%1)").arg(it.fileName()),
                it.filePath(),
                QStringLiteral("Variant Dockerfile")
            });
        }
    }

    // ── Docker Compose ────────────────────────────────────────────────────
    for (const auto &name : { "docker-compose.yml", "docker-compose.yaml",
                               "compose.yml", "compose.yaml" }) {
        const QString p = dir.filePath(QLatin1String(name));
        if (QFileInfo::exists(p)) {
            m_services.append({
                DevOpsService::DockerCompose,
                QStringLiteral("Docker Compose"),
                p,
                QStringLiteral("Multi-container orchestration (%1)").arg(QLatin1String(name))
            });
            break;  // one compose is enough
        }
    }

    // ── Terraform ─────────────────────────────────────────────────────────
    {
        QDirIterator it(m_root, { QStringLiteral("*.tf") },
                        QDir::Files, QDirIterator::NoIteratorFlags);
        if (it.hasNext()) {
            m_services.append({
                DevOpsService::Terraform,
                QStringLiteral("Terraform"),
                m_root,
                QStringLiteral("Infrastructure as Code (HCL)")
            });
        }
    }

    // ── Ansible ───────────────────────────────────────────────────────────
    if (dir.exists(QStringLiteral("ansible.cfg"))
        || dir.exists(QStringLiteral("playbook.yml"))
        || dir.exists(QStringLiteral("site.yml"))) {
        m_services.append({
            DevOpsService::Ansible,
            QStringLiteral("Ansible"),
            m_root,
            QStringLiteral("Configuration management / automation")
        });
    }

    // ── Kubernetes ────────────────────────────────────────────────────────
    if (dir.exists(QStringLiteral("k8s")) || dir.exists(QStringLiteral("kubernetes"))) {
        m_services.append({
            DevOpsService::Kubernetes,
            QStringLiteral("Kubernetes"),
            m_root,
            QStringLiteral("Container orchestration manifests")
        });
    }

    // ── GitHub Actions ────────────────────────────────────────────────────
    {
        const QString actionsDir = dir.filePath(QStringLiteral(".github/workflows"));
        if (QFileInfo(actionsDir).isDir()) {
            QDirIterator it(actionsDir, { QStringLiteral("*.yml"), QStringLiteral("*.yaml") },
                            QDir::Files);
            int count = 0;
            while (it.hasNext()) { it.next(); ++count; }
            if (count > 0) {
                m_services.append({
                    DevOpsService::GitHubActions,
                    QStringLiteral("GitHub Actions"),
                    actionsDir,
                    QStringLiteral("%1 workflow(s)").arg(count)
                });
            }
        }
    }

    emit servicesChanged();
}

bool ServiceDiscovery::hasDocker() const
{
    for (const auto &s : m_services)
        if (s.type == DevOpsService::Docker) return true;
    return false;
}

bool ServiceDiscovery::hasDockerCompose() const
{
    for (const auto &s : m_services)
        if (s.type == DevOpsService::DockerCompose) return true;
    return false;
}

bool ServiceDiscovery::hasTerraform() const
{
    for (const auto &s : m_services)
        if (s.type == DevOpsService::Terraform) return true;
    return false;
}

bool ServiceDiscovery::hasAnsible() const
{
    for (const auto &s : m_services)
        if (s.type == DevOpsService::Ansible) return true;
    return false;
}

bool ServiceDiscovery::hasKubernetes() const
{
    for (const auto &s : m_services)
        if (s.type == DevOpsService::Kubernetes) return true;
    return false;
}

bool ServiceDiscovery::hasGitHubActions() const
{
    for (const auto &s : m_services)
        if (s.type == DevOpsService::GitHubActions) return true;
    return false;
}

QString ServiceDiscovery::dockerComposeFile() const
{
    for (const auto &s : m_services)
        if (s.type == DevOpsService::DockerCompose)
            return s.configFile;
    return {};
}
