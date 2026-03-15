#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include "projectbraintypes.h"

/// Persistent project-level knowledge store.
///
/// Manages workspace rules, learned facts, and session summaries stored
/// under `.exorcist/` in the workspace root. Data is JSON-backed and
/// designed for retrieval during prompt construction.
class ProjectBrainService : public QObject
{
    Q_OBJECT

public:
    explicit ProjectBrainService(QObject *parent = nullptr);

    /// Load brain data from .exorcist/ directory under \a projectRoot.
    bool load(const QString &projectRoot);

    /// Persist current state back to disk. Returns false on I/O failure.
    bool save() const;

    /// True if a valid project root has been loaded.
    bool isLoaded() const { return !m_root.isEmpty(); }

    /// Return the workspace root path.
    QString root() const { return m_root; }

    // ── Rules ────────────────────────────────────────────────────────────

    const QList<ProjectRule> &rules() const { return m_rules; }
    void addRule(const ProjectRule &rule);
    void removeRule(const QString &id);

    /// Return rules whose scope patterns match any of \a activeFiles.
    QList<ProjectRule> relevantRules(const QStringList &activeFiles) const;

    // ── Memory Facts ─────────────────────────────────────────────────────

    const QList<MemoryFact> &facts() const { return m_facts; }
    void addFact(const MemoryFact &fact);
    void updateFact(const MemoryFact &fact);
    void forgetFact(const QString &id);

    /// Return facts matching \a query text or relevant to \a activeFiles.
    QList<MemoryFact> relevantFacts(const QString &query,
                                    const QStringList &activeFiles) const;

    // ── Session Summaries ────────────────────────────────────────────────

    const QList<SessionSummary> &summaries() const { return m_summaries; }
    void addSessionSummary(const SessionSummary &summary);
    QList<SessionSummary> recentSummaries(int n = 5) const;

    // ── Notes ────────────────────────────────────────────────────────────

    /// Read a markdown note file from .exorcist/notes/. Returns empty on error.
    QString readNote(const QString &name) const;

    /// Write or overwrite a note file.
    bool writeNote(const QString &name, const QString &content);

signals:
    void factsChanged();
    void rulesChanged();
    void summariesChanged();

private:
    bool loadJson(const QString &path, QJsonDocument &out) const;
    bool saveJson(const QString &path, const QJsonDocument &doc) const;
    bool ensureDirectoryExists(const QString &dirPath) const;
    bool scopeMatches(const QStringList &scope, const QStringList &files) const;

    QString               m_root;
    QList<ProjectRule>    m_rules;
    QList<MemoryFact>     m_facts;
    QList<SessionSummary> m_summaries;
};
