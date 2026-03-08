#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include "projectbraintypes.h"

class ProjectBrainService;

/// Extracts potential memory facts from completed agent turns.
///
/// After a turn finishes, call `suggestFacts()` with the user prompt,
/// assistant response, and list of files touched by tools. If anything
/// looks like a reusable fact, the engine emits `factSuggested()`.
class MemorySuggestionEngine : public QObject
{
    Q_OBJECT

public:
    explicit MemorySuggestionEngine(ProjectBrainService *brain,
                                    QObject *parent = nullptr);

    /// Analyse a completed turn and emit any suggested facts.
    void suggestFacts(const QString &userPrompt,
                      const QString &assistantResponse,
                      const QStringList &touchedFiles);

signals:
    /// Emitted for each candidate fact the user may want to remember.
    void factSuggested(const MemoryFact &candidate);

private:
    /// Extract "pitfall" facts from error-related turns.
    void extractPitfalls(const QString &userPrompt,
                         const QString &response,
                         QList<MemoryFact> &out);

    /// Extract build/test commands mentioned in the response.
    void extractBuildFacts(const QString &response,
                           QList<MemoryFact> &out);

    /// Extract architecture observations.
    void extractArchitectureFacts(const QString &userPrompt,
                                  const QString &response,
                                  const QStringList &files,
                                  QList<MemoryFact> &out);

    /// Check if a fact with the same key already exists.
    bool isDuplicate(const MemoryFact &fact) const;

    ProjectBrainService *m_brain;
};
