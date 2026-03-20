#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QString>

// ── CodeGraphQuery ───────────────────────────────────────────────────────────
// Query interface for the Code Graph database.
// Returns results as QJsonObject/QJsonArray for easy consumption by
// agent tools and IDE features.
//
// All methods are const and read-only.
// Target resolution: bare names try class first, then file path.

class CodeGraphQuery
{
public:
    explicit CodeGraphQuery(QSqlDatabase &db);

    QJsonObject context(const QString &target) const;
    QJsonArray  functions(const QString &target) const;
    QJsonArray  edges(const QString &target) const;
    QJsonArray  search(const QString &query, int limit = 20) const;
    QJsonObject projectSummary() const;
    QJsonArray  connections(const QString &target = {}) const;
    QJsonArray  services() const;
    QJsonArray  testCases(const QString &target = {}) const;
    QJsonArray  cmakeTargets() const;
    QJsonArray  subsystems() const;
    QJsonArray  semanticTags(const QString &target = {}) const;
    QJsonArray  searchByTag(const QString &tag, int limit = 50) const;

    // ── Analysis queries (from steps 13-18) ──
    QJsonArray  callees(const QString &funcName, int limit = 50) const;
    QJsonArray  callers(const QString &funcName, int limit = 50) const;
    QJsonArray  hotspots(int limit = 20) const;
    QJsonArray  deadCode(int limit = 50) const;
    QJsonArray  typos(int limit = 30) const;

    // ── AI-context optimisation queries ──
    QJsonArray  analysisScores(int limit = 20) const;
    QJsonObject symbolSlice(const QString &symbol) const;
    QJsonArray  callChain(const QString &symbol, int depth = 3) const;
    QJsonArray  aiTasks(const QString &taskType = {}) const;
    QJsonArray  duplicateFunctions() const;
    QJsonArray  optimizationPatterns() const;

private:
    struct FileMatch { qint64 id = 0; QString path; };

    FileMatch resolveTarget(const QString &target) const;
    bool tableExists(const QString &name) const;

    QSqlDatabase m_db;
};
