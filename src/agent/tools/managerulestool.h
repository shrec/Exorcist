#pragma once

#include "../itool.h"

class ProjectBrainService;

/// Tool that lets the AI model manage project-level rules (conventions,
/// style guides, safety constraints). Wraps ProjectBrainService CRUD for rules.
class ManageRulesTool : public ITool
{
public:
    explicit ManageRulesTool(ProjectBrainService *brain);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ProjectBrainService *m_brain;

    ToolExecResult doAdd(const QJsonObject &args);
    ToolExecResult doRemove(const QJsonObject &args);
    ToolExecResult doList(const QJsonObject &args);
};
