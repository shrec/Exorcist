#pragma once

#include "../sdk/ibuildsystem.h"

class CMakeIntegration;

// ── BuildSystemService ───────────────────────────────────────────────────────
//
// Adapts CMakeIntegration to the IBuildSystem SDK interface.
// Registered in ServiceRegistry as "buildSystem" so agent tools and plugins
// can access build operations without coupling to CMakeIntegration directly.

class BuildSystemService : public IBuildSystem
{
    Q_OBJECT

public:
    explicit BuildSystemService(CMakeIntegration *cmake, QObject *parent = nullptr);

    void setProjectRoot(const QString &root) override;
    bool hasProject() const override;
    void configure() override;
    void build(const QString &target = {}) override;
    void clean() override;
    void cancelBuild() override;
    bool isBuilding() const override;
    QStringList targets() const override;
    QString buildDirectory() const override;
    QString compileCommandsPath() const override;

private:
    CMakeIntegration *m_cmake;
};
