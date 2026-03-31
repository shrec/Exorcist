#pragma once

#include <QObject>
#include <QList>
#include <QString>

/// Represents one discovered DevOps service/tool in the workspace.
struct DevOpsService {
    enum Type { Docker, DockerCompose, Terraform, Ansible, Kubernetes, GitHubActions };
    Type    type;
    QString name;        // display name
    QString configFile;  // path to the config file
    QString description;
};

/// Discovers DevOps tooling present in a workspace.
class ServiceDiscovery : public QObject
{
    Q_OBJECT
public:
    explicit ServiceDiscovery(QObject *parent = nullptr);

    void setWorkspaceRoot(const QString &root);
    void refresh();

    QList<DevOpsService> services() const { return m_services; }
    bool hasDocker() const;
    bool hasDockerCompose() const;
    bool hasTerraform() const;
    bool hasAnsible() const;
    bool hasKubernetes() const;
    bool hasGitHubActions() const;

    QString dockerComposeFile() const;

signals:
    void servicesChanged();

private:
    QString m_root;
    QList<DevOpsService> m_services;
};
