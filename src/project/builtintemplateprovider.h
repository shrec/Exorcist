#pragma once

#include "iprojecttemplateprovider.h"

#include <QObject>

/// Built-in project template provider — top 20 languages.
/// Ships with the IDE core — always available.
/// Individual template scaffolds are implemented incrementally.
class BuiltinTemplateProvider : public QObject, public IProjectTemplateProvider
{
    Q_OBJECT
    Q_INTERFACES(IProjectTemplateProvider)

public:
    explicit BuiltinTemplateProvider(QObject *parent = nullptr);

    QList<ProjectTemplate> templates() const override;
    bool createProject(const QString &templateId,
                       const QString &projectName,
                       const QString &location,
                       QString *error) override;

private:
    // Fully implemented
    bool createCppConsole(const QString &name, const QString &dir, QString *error);
    bool createCppLibrary(const QString &name, const QString &dir, QString *error);
    bool createCMakeProject(const QString &name, const QString &dir, QString *error);
    bool createCConsole(const QString &name, const QString &dir, QString *error);
    bool createRustBinary(const QString &name, const QString &dir, QString *error);
    bool createRustLibrary(const QString &name, const QString &dir, QString *error);
    bool createPythonScript(const QString &name, const QString &dir, QString *error);
    bool createPythonPackage(const QString &name, const QString &dir, QString *error);
    bool createGoModule(const QString &name, const QString &dir, QString *error);
    bool createNodeJs(const QString &name, const QString &dir, QString *error);
    bool createTypeScriptNode(const QString &name, const QString &dir, QString *error);
    bool createWebProject(const QString &name, const QString &dir, QString *error);
    bool createZigProject(const QString &name, const QString &dir, QString *error);

    // Generic scaffold (creates dir + single source file + project file)
    bool createGenericProject(const QString &name, const QString &dir,
                              const QString &filename, const QString &content,
                              const QString &language, const QString &templateId,
                              QString *error);
};
