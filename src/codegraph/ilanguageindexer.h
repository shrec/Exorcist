#pragma once

#include "codegraphtypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

// ── ILanguageIndexer ─────────────────────────────────────────────────────────
// Interface for language-specific code indexing.
// Each programming language implements this to extract classes, functions,
// imports, connections, services, and test cases from source files.
//
// The CodeGraphIndexer orchestrator dispatches to the correct implementation
// based on the detected language of each file.

class ILanguageIndexer
{
public:
    virtual ~ILanguageIndexer() = default;

    // ── Identity ──
    virtual QString languageId() const = 0;
    virtual QStringList fileExtensions() const = 0;

    // ── File-level metadata (e.g. Q_OBJECT for C++) ──
    virtual CodeGraphFileMetadata extractFileMetadata(const QString &content) const = 0;

    // ── Structural parsing: classes, imports, namespaces ──
    virtual CodeGraphParseResult parseFile(const QString &content) const = 0;

    // ── Function/method extraction with exact line ranges ──
    virtual QVector<CodeGraphFunction> extractFunctions(const QString &content) const = 0;

    // ── First doc-comment for file summaries ──
    virtual QString extractFirstComment(const QStringList &lines) const = 0;

    // ── Framework-specific scanning ──
    virtual QVector<CodeGraphConnectionInfo> scanConnections(const QString &content) const = 0;
    virtual QVector<CodeGraphServiceRef> scanServiceRefs(const QString &content) const = 0;
    virtual QVector<CodeGraphTestCase> scanTestCases(const QString &content) const = 0;
};
