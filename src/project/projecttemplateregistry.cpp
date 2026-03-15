#include "projecttemplateregistry.h"
#include "iprojecttemplateprovider.h"

#include <QSet>
#include <algorithm>

ProjectTemplateRegistry::ProjectTemplateRegistry(QObject *parent)
    : QObject(parent)
{
}

void ProjectTemplateRegistry::addProvider(IProjectTemplateProvider *provider)
{
    if (provider && !m_providers.contains(provider))
        m_providers.append(provider);
}

QList<ProjectTemplate> ProjectTemplateRegistry::allTemplates() const
{
    QList<ProjectTemplate> result;
    for (auto *p : m_providers)
        result.append(p->templates());
    return result;
}

QStringList ProjectTemplateRegistry::languages() const
{
    QSet<QString> langs;
    for (auto *p : m_providers)
        for (const auto &t : p->templates())
            langs.insert(t.language);

    QStringList sorted(langs.begin(), langs.end());
    sorted.sort(Qt::CaseInsensitive);
    return sorted;
}

QList<ProjectTemplate> ProjectTemplateRegistry::templatesForLanguage(const QString &language) const
{
    QList<ProjectTemplate> result;
    for (auto *p : m_providers)
        for (const auto &t : p->templates())
            if (t.language == language)
                result.append(t);
    return result;
}

bool ProjectTemplateRegistry::createProject(const QString &templateId,
                                            const QString &projectName,
                                            const QString &location,
                                            QString *error)
{
    for (auto *p : m_providers) {
        for (const auto &t : p->templates()) {
            if (t.id == templateId)
                return p->createProject(templateId, projectName, location, error);
        }
    }
    if (error)
        *error = tr("Template not found: %1").arg(templateId);
    return false;
}
