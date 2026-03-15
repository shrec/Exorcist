#pragma once

#include <QList>
#include <QString>
#include <QtPlugin>

/// Describes a single project template that a plugin can contribute.
struct ProjectTemplate
{
    QString id;          ///< Unique template ID (e.g. "cpp-console", "rust-binary")
    QString language;    ///< Language group for the wizard (e.g. "C++", "Rust", "Python")
    QString name;        ///< Display name (e.g. "Console Application")
    QString description; ///< Short description
    QString icon;        ///< Optional icon path or theme icon name
};

/// Plugin interface for contributing project templates.
///
/// Plugins implement this to register new project types in the
/// New Project wizard. The IDE discovers providers via ServiceRegistry
/// and displays templates grouped by language.
///
/// Example:
///   A "C++ Language Pack" plugin provides templates for
///   Console App, Library, Qt Widget App, etc.
class IProjectTemplateProvider
{
public:
    virtual ~IProjectTemplateProvider() = default;

    /// Return all templates this provider contributes.
    virtual QList<ProjectTemplate> templates() const = 0;

    /// Create a project from the given template.
    /// @param templateId  The template ID from ProjectTemplate::id
    /// @param projectName User-chosen project name
    /// @param location    Directory where the project should be created
    /// @param error       Set on failure
    /// @return true on success
    virtual bool createProject(const QString &templateId,
                               const QString &projectName,
                               const QString &location,
                               QString *error) = 0;
};

#define EXORCIST_PROJECT_TEMPLATE_PROVIDER_IID "org.exorcist.IProjectTemplateProvider/1.0"
Q_DECLARE_INTERFACE(IProjectTemplateProvider, EXORCIST_PROJECT_TEMPLATE_PROVIDER_IID)
