#pragma once

#include "ilanguageindexer.h"

// ── PythonLanguageIndexer ────────────────────────────────────────────────────
// Extracts Python classes, functions (indentation-based), imports,
// docstrings, and pytest test cases.

class PythonLanguageIndexer : public ILanguageIndexer
{
public:
    QString languageId() const override;
    QStringList fileExtensions() const override;

    CodeGraphFileMetadata extractFileMetadata(const QString &content) const override;
    CodeGraphParseResult parseFile(const QString &content) const override;
    QVector<CodeGraphFunction> extractFunctions(const QString &content) const override;
    QString extractFirstComment(const QStringList &lines) const override;

    QVector<CodeGraphConnectionInfo> scanConnections(const QString &content) const override;
    QVector<CodeGraphServiceRef> scanServiceRefs(const QString &content) const override;
    QVector<CodeGraphTestCase> scanTestCases(const QString &content) const override;
};
