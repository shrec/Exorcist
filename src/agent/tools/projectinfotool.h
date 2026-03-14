#pragma once

#include "../itool.h"

// ── GetProjectSetupInfoTool ───────────────────────────────────────────────────
//
// Provides project configuration / setup information by scanning the workspace
// root for common build and config files. Returns detected languages, build
// system, dependencies, and project structure hints.
//
// This is the Exorcist equivalent of Copilot's `get_project_setup_info`.

class GetProjectSetupInfoTool : public ITool
{
public:
    explicit GetProjectSetupInfoTool(const QString &workspaceRoot = {})
        : m_root(workspaceRoot) {}

    void setWorkspaceRoot(const QString &root) { m_root = root; }

    ToolSpec spec() const override;

    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_root;
};
