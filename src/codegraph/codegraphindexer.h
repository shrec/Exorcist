#pragma once

#include "codegraphtypes.h"

#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <memory>

class ILanguageIndexer;

// ── CodeGraphIndexer ─────────────────────────────────────────────────────────
// Workspace code indexer that populates a CodeGraph SQLite database.
// Ports the algorithms from tools/build_codegraph.py to C++ for runtime use.
//
// Language-specific parsing is delegated to ILanguageIndexer implementations.
// Built-in indexers are registered for C++, TypeScript/JS, and Python.
// Additional languages can be added via registerLanguageIndexer().
//
// Public API mirrors the Python 12-step pipeline:
//   1. scanFiles          — walk filesystem, index source files
//   2. parseAll           — extract classes, methods, includes
//   3. buildImplMap       — header↔source implementation pairing
//   4. buildSubsystemGraph — subsystem statistics + dependency edges
//   5. buildFunctionIndex — function/method extraction with exact line ranges
//   6. buildFileSummaries — 1-line file descriptions with categorization
//   7. buildEdges         — unified relationship graph
//   8. scanQtConnections  — signal/slot connection extraction (via indexers)
//   9. scanServices       — ServiceRegistry register/resolve calls (via indexers)
//  10. scanTestCases      — test method inventory (via indexers)
//  11. scanCMakeTargets   — CMake add_executable/library/test targets
//  12. buildFtsIndex      — FTS5 full-text search population

class CodeGraphIndexer
{
public:
    CodeGraphIndexer();

    void setWorkspaceRoot(const QString &root);
    void registerLanguageIndexer(std::shared_ptr<ILanguageIndexer> indexer);

    // ── Pipeline steps (each returns count of items processed) ──
    int scanFiles(QSqlDatabase &db);
    int parseAll(QSqlDatabase &db);
    int buildImplMap(QSqlDatabase &db);
    int buildSubsystemGraph(QSqlDatabase &db);
    int buildFunctionIndex(QSqlDatabase &db);
    int buildFileSummaries(QSqlDatabase &db);
    int buildEdges(QSqlDatabase &db);
    int scanQtConnections(QSqlDatabase &db);
    int scanServices(QSqlDatabase &db);
    int scanTestCases(QSqlDatabase &db);
    int scanCMakeTargets(QSqlDatabase &db);
    int buildFtsIndex(QSqlDatabase &db, bool hasFts5);

    CodeGraphStats fullRebuild(QSqlDatabase &db, bool hasFts5);

    const CodeGraphStats &lastStats() const { return m_stats; }

private:
    static QString detectLanguage(const QString &path);
    static QString detectSubsystem(const QString &relPath);
    static bool shouldSkipDir(const QString &name);

    ILanguageIndexer *indexerForLang(const QString &lang) const;

    QString m_workspaceRoot;
    CodeGraphStats m_stats;
    QHash<QString, std::shared_ptr<ILanguageIndexer>> m_indexers;
    QSet<QString> m_codeExtensions;
};
