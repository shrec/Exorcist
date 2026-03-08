#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QStringList>

#include "projectbraintypes.h"

class ProjectBrainService;

/// Assembled brain context ready for prompt injection.
struct AgentBrainContext {
    QList<ProjectRule>    relevantRules;
    QList<MemoryFact>     relevantFacts;
    QList<SessionSummary> relatedSessions;
    QString               architectureNotes;
    QString               buildNotes;
    QString               pitfallsNotes;
};

/// Builds a compact text representation of project knowledge
/// suitable for injection into the system prompt.
class BrainContextBuilder : public QObject
{
    Q_OBJECT

public:
    explicit BrainContextBuilder(ProjectBrainService *brain, QObject *parent = nullptr);

    /// Build the relevant brain context for a given task and active files.
    AgentBrainContext buildForTask(const QString &task,
                                  const QStringList &activeFiles) const;

    /// Serialize \a ctx to compact prompt text.
    QString toPromptText(const AgentBrainContext &ctx) const;

private:
    ProjectBrainService *m_brain;
};
