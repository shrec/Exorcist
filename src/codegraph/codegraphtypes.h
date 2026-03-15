#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// ── Code Graph Types ─────────────────────────────────────────────────────────
// POD structs used by the Code Graph indexer and query engine.
// Mirrors the schema from tools/build_codegraph.py for runtime use.

struct CodeGraphFunction
{
    QString name;
    QString qualifiedName;
    QString className;
    QString params;
    QString returnType;
    int startLine = 0;
    int endLine   = 0;
    int lineCount = 0;
    bool isMethod = false;
};

struct CodeGraphStats
{
    int filesIndexed       = 0;
    int classesParsed      = 0;
    int methodsParsed      = 0;
    int includesParsed     = 0;
    int implsMapped        = 0;
    int functionsExtracted = 0;
    int summariesBuilt     = 0;
    int edgesBuilt         = 0;
    int qtConnections      = 0;
    int servicesFound      = 0;
    int testCases          = 0;
    int cmakeTargets       = 0;
    int ftsEntries         = 0;
};

// ── Parsed data returned by ILanguageIndexer ─────────────────────────────────

struct CodeGraphClassInfo
{
    QString name;
    QString bases;
    int lineNum      = 0;
    bool isInterface = false;
    bool isStruct    = false;
};

struct CodeGraphImportInfo
{
    QString path;
    bool isSystem = false;
};

struct CodeGraphNamespaceInfo
{
    QString name;
};

struct CodeGraphFileMetadata
{
    bool hasQObject = false;
    bool hasSignals = false;
    bool hasSlots   = false;
};

struct CodeGraphParseResult
{
    QVector<CodeGraphClassInfo>     classes;
    QVector<CodeGraphImportInfo>    imports;
    QVector<CodeGraphNamespaceInfo> namespaces;
};

struct CodeGraphConnectionInfo
{
    QString sender;
    QString signalName;
    QString receiver;
    QString slotName;
    int lineNum = 0;
};

struct CodeGraphServiceRef
{
    QString key;
    int lineNum    = 0;
    QString regType; // "register" or "resolve"
};

struct CodeGraphTestCase
{
    QString className;
    QString methodName;
    int lineNum = 0;
};
