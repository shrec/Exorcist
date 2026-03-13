#pragma once

#include "../itool.h"
#include "luatool.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>

// ── Agent Tool Store ─────────────────────────────────────────────────────────
// Three tools that let the AI agent create, list, and run its own Lua tools.
// Tools are persisted to an "AgentTools" directory next to the IDE executable.
//
// Each tool is stored as two files:
//   <name>.lua   — the Lua script source
//   <name>.json  — metadata: { "name", "description", "created" }
//
// The agent can build a personal toolkit: save reusable scripts once, then
// invoke them by name in future sessions — the tools survive IDE restarts.

namespace AgentToolStoreDetail {

inline QString agentToolsDir()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/AgentTools");
}

inline bool ensureDir()
{
    return QDir().mkpath(agentToolsDir());
}

inline QString sanitizeName(const QString &name)
{
    QString safe;
    for (const QChar &c : name) {
        if (c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('-'))
            safe += c;
    }
    return safe.left(64);
}

} // namespace AgentToolStoreDetail


// ── save_lua_tool ────────────────────────────────────────────────────────────
// Saves a Lua script as a named reusable tool.

class SaveLuaToolTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("save_lua_tool");
        s.description = QStringLiteral(
            "Save a Lua script as a reusable agent tool that persists across "
            "IDE sessions and projects. The tool is stored in the AgentTools "
            "directory next to the IDE executable — it survives restarts and "
            "works in any workspace.\n\n"
            "Use this to build a persistent personal toolkit: save useful "
            "scripts once, then run them by name with run_lua_tool in any "
            "future session without rewriting.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("name"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Unique tool name (letters, digits, underscore, hyphen). "
                                    "Max 64 characters.")}
                }},
                {QStringLiteral("description"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Short description of what the tool does.")}
                }},
                {QStringLiteral("script"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Lua script source code.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{
                QStringLiteral("name"),
                QStringLiteral("description"),
                QStringLiteral("script")
            }}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString rawName = args[QLatin1String("name")].toString();
        const QString desc    = args[QLatin1String("description")].toString();
        const QString script  = args[QLatin1String("script")].toString();

        if (rawName.isEmpty() || script.isEmpty())
            return {false, {}, {},
                    QStringLiteral("'name' and 'script' are required.")};

        const QString name = AgentToolStoreDetail::sanitizeName(rawName);
        if (name.isEmpty())
            return {false, {}, {},
                    QStringLiteral("Name must contain at least one letter or digit.")};

        if (!AgentToolStoreDetail::ensureDir())
            return {false, {}, {},
                    QStringLiteral("Failed to create AgentTools directory.")};

        const QString dir = AgentToolStoreDetail::agentToolsDir();

        // Write the Lua script
        QFile luaFile(dir + QLatin1Char('/') + name + QStringLiteral(".lua"));
        if (!luaFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return {false, {}, {},
                    QStringLiteral("Failed to write script file: %1").arg(luaFile.errorString())};
        luaFile.write(script.toUtf8());
        luaFile.close();

        // Write metadata JSON
        QJsonObject meta;
        meta[QLatin1String("name")]        = name;
        meta[QLatin1String("description")] = desc;
        meta[QLatin1String("created")]     = QDateTime::currentDateTime().toString(Qt::ISODate);

        QFile metaFile(dir + QLatin1Char('/') + name + QStringLiteral(".json"));
        if (!metaFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            return {false, {}, {},
                    QStringLiteral("Failed to write metadata file.")};
        metaFile.write(QJsonDocument(meta).toJson(QJsonDocument::Compact));
        metaFile.close();

        QJsonObject data;
        data[QLatin1String("name")] = name;
        data[QLatin1String("path")] = luaFile.fileName();

        return {true, data,
                QStringLiteral("Tool '%1' saved to AgentTools/. "
                               "Use run_lua_tool to execute it.").arg(name),
                {}};
    }
};


// ── list_lua_tools ───────────────────────────────────────────────────────────
// Lists all saved agent tools with their descriptions.

class ListLuaToolsTool : public ITool
{
public:
    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("list_lua_tools");
        s.description = QStringLiteral(
            "List all custom Lua tools saved in the AgentTools directory. "
            "Returns each tool's name, description, and full script source "
            "so you know exactly what's available.\n\n"
            "Call this at the start of sessions to discover your saved toolkit. "
            "Use run_lua_tool to execute a saved tool by name instead of "
            "rewriting scripts from scratch.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &) override
    {
        const QString dir = AgentToolStoreDetail::agentToolsDir();
        const QDir toolDir(dir);

        if (!toolDir.exists()) {
            return {true, QJsonObject{{QLatin1String("tools"), QJsonArray{}}},
                    QStringLiteral("No AgentTools directory yet. "
                                   "Use save_lua_tool to create your first tool."),
                    {}};
        }

        const QStringList luaFiles = toolDir.entryList(
            {QStringLiteral("*.lua")}, QDir::Files, QDir::Name);

        QJsonArray toolList;
        QStringList lines;

        for (const QString &luaFileName : luaFiles) {
            const QString baseName = luaFileName.chopped(4); // remove .lua
            const QString metaPath = dir + QLatin1Char('/') + baseName + QStringLiteral(".json");
            const QString scriptPath = dir + QLatin1Char('/') + luaFileName;

            QString description;
            QFile metaFile(metaPath);
            if (metaFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const auto doc = QJsonDocument::fromJson(metaFile.readAll());
                description = doc.object()[QLatin1String("description")].toString();
                metaFile.close();
            }

            QString scriptContent;
            QFile scriptFile(scriptPath);
            if (scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                scriptContent = QString::fromUtf8(scriptFile.readAll());
                scriptFile.close();
            }

            QJsonObject entry;
            entry[QLatin1String("name")]        = baseName;
            entry[QLatin1String("description")] = description;
            entry[QLatin1String("script")]      = scriptContent;
            toolList.append(entry);

            lines << QStringLiteral("\n### %1\n**Description:** %2\n```lua\n%3\n```")
                      .arg(baseName, description, scriptContent);
        }

        QJsonObject data;
        data[QLatin1String("tools")] = toolList;
        data[QLatin1String("count")] = toolList.size();

        const QString text = toolList.isEmpty()
            ? QStringLiteral("No saved tools yet. Use save_lua_tool to create one.")
            : QStringLiteral("Saved tools (%1):\n%2").arg(toolList.size()).arg(lines.join(QLatin1Char('\n')));

        return {true, data, text, {}};
    }
};


// ── run_lua_tool ─────────────────────────────────────────────────────────────
// Runs a previously saved Lua tool by name.

class RunLuaToolTool : public ITool
{
public:
    using LuaExecutor = LuaExecuteTool::LuaExecutor;

    explicit RunLuaToolTool(LuaExecutor executor)
        : m_executor(std::move(executor)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("run_lua_tool");
        s.description = QStringLiteral(
            "Run a previously saved Lua tool by name. The tool's script is "
            "loaded from the AgentTools directory and executed in the same "
            "sandboxed LuaJIT environment as run_lua — with all host APIs "
            "(ex.workspace.*, ex.editor.*, ex.git.*, etc.) available.\n\n"
            "If you're unsure what tools exist, call list_lua_tools first.");
        s.permission  = AgentToolPermission::SafeMutate;
        s.timeoutMs   = 30000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("name"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Name of the saved tool to run.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("name")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString rawName = args[QLatin1String("name")].toString();
        if (rawName.isEmpty())
            return {false, {}, {},
                    QStringLiteral("'name' parameter is required.")};

        const QString name = AgentToolStoreDetail::sanitizeName(rawName);
        const QString dir  = AgentToolStoreDetail::agentToolsDir();
        const QString path = dir + QLatin1Char('/') + name + QStringLiteral(".lua");

        QFile scriptFile(path);
        if (!scriptFile.open(QIODevice::ReadOnly | QIODevice::Text))
            return {false, {}, {},
                    QStringLiteral("Tool '%1' not found. Use list_lua_tools to see "
                                   "available tools.").arg(name)};

        const QString script = QString::fromUtf8(scriptFile.readAll());
        scriptFile.close();

        if (script.isEmpty())
            return {false, {}, {},
                    QStringLiteral("Tool '%1' has an empty script.").arg(name)};

        const auto result = m_executor(script);

        if (!result.ok)
            return {false, {}, {},
                    QStringLiteral("Tool '%1' error: %2").arg(name, result.error)};

        QString text;
        if (!result.output.isEmpty())
            text += result.output;
        if (!result.returnValue.isEmpty()) {
            if (!text.isEmpty()) text += QLatin1Char('\n');
            text += QStringLiteral("=> %1").arg(result.returnValue);
        }
        if (text.isEmpty())
            text = QStringLiteral("(no output)");

        text += QStringLiteral("\n[Tool: %1 | Memory: %2 KB]")
                    .arg(name)
                    .arg(result.memoryUsedBytes / 1024);

        QJsonObject data;
        data[QLatin1String("toolName")]    = name;
        data[QLatin1String("output")]      = result.output;
        data[QLatin1String("returnValue")] = result.returnValue;
        data[QLatin1String("memoryBytes")] = result.memoryUsedBytes;

        return {true, data, text, {}};
    }

private:
    LuaExecutor m_executor;
};
