#pragma once

#include <QObject>
#include <QList>

struct ProjectTemplate;
class IProjectTemplateProvider;

/// Collects project template providers and presents a unified template list.
/// Registered as "projectTemplateRegistry" in ServiceRegistry.
class ProjectTemplateRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ProjectTemplateRegistry(QObject *parent = nullptr);

    /// Register a template provider (plugin or built-in).
    void addProvider(IProjectTemplateProvider *provider);

    /// All available templates across all providers.
    QList<ProjectTemplate> allTemplates() const;

    /// All unique language names (sorted).
    QStringList languages() const;

    /// Templates for a specific language.
    QList<ProjectTemplate> templatesForLanguage(const QString &language) const;

    /// Create a project using the matching provider.
    bool createProject(const QString &templateId,
                       const QString &projectName,
                       const QString &location,
                       QString *error);

private:
    QList<IProjectTemplateProvider *> m_providers;
};
