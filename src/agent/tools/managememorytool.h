#pragma once

#include "../itool.h"

class ProjectBrainService;

/// Tool that lets the AI model manage project-level memory facts and notes.
/// Wraps ProjectBrainService CRUD for facts and notes.
class ManageMemoryTool : public ITool
{
public:
    explicit ManageMemoryTool(ProjectBrainService *brain);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ProjectBrainService *m_brain;

    ToolExecResult doAddFact(const QJsonObject &args);
    ToolExecResult doUpdateFact(const QJsonObject &args);
    ToolExecResult doForgetFact(const QJsonObject &args);
    ToolExecResult doListFacts(const QJsonObject &args);
    ToolExecResult doReadNote(const QJsonObject &args);
    ToolExecResult doWriteNote(const QJsonObject &args);
    ToolExecResult doListNotes();
};
