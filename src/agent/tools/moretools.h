#pragma once

#include <functional>

#include <QHash>
#include <QString>

#include "../itool.h"
#include "../../aiinterface.h"

// ─────────────────────────────────────────────────────────────────────────────
// ReplaceStringTool — replace_string_in_file
//
// Replaces ALL occurrences of old_string with new_string in a file.
// Falls back to a single-occurrence replace if old_string is unique.
// ─────────────────────────────────────────────────────────────────────────────

class ReplaceStringTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};

// ─────────────────────────────────────────────────────────────────────────────
// GetErrorsTool — get_errors
//
// Returns current diagnostics (errors/warnings) from the active file or
// the entire workspace. Uses a callback provided at construction.
// ─────────────────────────────────────────────────────────────────────────────

class GetErrorsTool : public ITool
{
public:
    using DiagnosticsGetter = std::function<QList<AgentDiagnostic>()>;

    explicit GetErrorsTool(DiagnosticsGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &) override;

private:
    DiagnosticsGetter m_getter;
};

// ─────────────────────────────────────────────────────────────────────────────
// GetChangedFilesTool — get_changed_files
//
// Returns git status (changed files) using a callback.
// ─────────────────────────────────────────────────────────────────────────────

class GetChangedFilesTool : public ITool
{
public:
    using StatusGetter = std::function<QHash<QString, QString>()>;  // absPath → XY

    explicit GetChangedFilesTool(StatusGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &) override;

private:
    StatusGetter m_getter;
};

// ─────────────────────────────────────────────────────────────────────────────
// FetchWebpageTool — fetch_webpage
//
// Fetches the content of a URL using curl (available on all platforms).
// Returns the text content (HTML stripped to plain text).
// ─────────────────────────────────────────────────────────────────────────────

class FetchWebpageTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};
