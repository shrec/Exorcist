#include "luatool.h"

#include <QJsonArray>

LuaExecuteTool::LuaExecuteTool(LuaExecutor executor)
    : m_executor(std::move(executor)) {}

ToolSpec LuaExecuteTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("run_lua");
    s.description = QStringLiteral(
        "Execute a Lua script in a sandboxed LuaJIT environment. "
        "Use this to perform computations, transform text, process "
        "data, or interact with the IDE via host APIs:\n"
        "  ex.workspace.readFile(path) — read file contents\n"
        "  ex.workspace.root() — get workspace root\n"
        "  ex.workspace.exists(path) — check file exists\n"
        "  ex.workspace.listDir(path?) — list files/dirs (default: root)\n"
        "  ex.workspace.openFiles() — currently open file paths\n"
        "  ex.editor.activeFile() — current editor file\n"
        "  ex.editor.selectedText() — current selection\n"
        "  ex.editor.cursorLine() — cursor line number\n"
        "  ex.git.branch() — current git branch\n"
        "  ex.git.diff(staged) — git diff output\n"
        "  ex.diagnostics.errorCount() — compiler errors\n"
        "  print(...) — output captured and returned\n"
        "  return value — last expression returned as text\n\n"
        "Sandboxed: no file I/O, no OS access, no FFI. "
        "64MB memory limit, 10M instruction limit.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 30000;
    s.parallelSafe = true;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("script"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Lua script to execute. Can use print() for "
                                "output and return values for results.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("script")}}
    };
    return s;
}

ToolExecResult LuaExecuteTool::invoke(const QJsonObject &args)
{
    const QString script = args[QLatin1String("script")].toString();
    if (script.isEmpty())
        return {false, {}, {}, QStringLiteral("'script' parameter is required.")};

    const LuaResult result = m_executor(script);

    if (!result.ok)
        return {false, {}, {},
                QStringLiteral("Lua error: %1").arg(result.error)};

    QString text;
    if (!result.output.isEmpty())
        text += result.output;
    if (!result.returnValue.isEmpty()) {
        if (!text.isEmpty()) text += QLatin1Char('\n');
        text += QStringLiteral("=> %1").arg(result.returnValue);
    }
    if (text.isEmpty())
        text = QStringLiteral("(no output)");

    text += QStringLiteral("\n[Memory: %1 KB]")
                .arg(result.memoryUsedBytes / 1024);

    QJsonObject data;
    data[QLatin1String("output")]      = result.output;
    data[QLatin1String("returnValue")] = result.returnValue;
    data[QLatin1String("memoryBytes")] = result.memoryUsedBytes;

    return {true, data, text, {}};
}
