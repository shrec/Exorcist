#pragma once

#include <QList>
#include <QString>

// ── Diagnostics Service ──────────────────────────────────────────────────────
//
// Stable SDK interface for reading LSP diagnostics.

struct SDKDiagnostic
{
    enum Severity { Error, Warning, Info, Hint };

    QString  filePath;
    int      line = 0;      // 1-based
    int      column = 0;    // 0-based
    Severity severity = Warning;
    QString  message;
    QString  source;        // "clangd", "pylsp", etc.
};

class IDiagnosticsService
{
public:
    virtual ~IDiagnosticsService() = default;

    /// All current diagnostics across open files.
    virtual QList<SDKDiagnostic> diagnostics() const = 0;

    /// Diagnostics for a specific file.
    virtual QList<SDKDiagnostic> diagnosticsForFile(const QString &filePath) const = 0;

    /// Count of errors across the workspace.
    virtual int errorCount() const = 0;

    /// Count of warnings across the workspace.
    virtual int warningCount() const = 0;
};
