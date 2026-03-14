#pragma once

#include "../itool.h"

#include <functional>

// ── LuaExecuteTool ───────────────────────────────────────────────────────────
// Executes a Lua script in a sandboxed LuaJIT environment.
// The AI agent uses this like a "code interpreter" — dynamically computing
// results, transforming data, querying the workspace via host APIs.
//
// Available host APIs inside the sandbox:
//   ex.editor.*       — active file, selection, cursor
//   ex.workspace.*    — root, readFile, exists
//   ex.git.*          — branch, diff, isRepo
//   ex.diagnostics.*  — errorCount, warningCount
//   ex.notify.*       — info, warning, error, status
//   ex.log.*          — debug, info, warning, error
//   ex.commands.*     — register, execute
//
// Blocked: FFI, io, os, loadfile, dofile, require
// Limits: 10M instructions, 64MB memory

class LuaExecuteTool : public ITool
{
public:
    // executor(script) → {output, returnValue, error, memoryUsed}
    struct LuaResult {
        bool    ok;
        QString output;       // captured print() output
        QString returnValue;  // tostring(result) of the last expression
        QString error;        // error message if failed
        int     memoryUsedBytes = 0;
    };
    using LuaExecutor = std::function<LuaResult(const QString &script)>;

    explicit LuaExecuteTool(LuaExecutor executor);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    LuaExecutor m_executor;
};
