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

private:
    struct FileMatch { qint64 id = 0; QString path; };

    FileMatch resolveTarget(const QString &target) const;
    bool tableExists(const QString &name) const;

    QSqlDatabase m_db;
};
