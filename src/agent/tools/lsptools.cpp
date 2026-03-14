#include "lsptools.h"

#include <QJsonArray>

// ── RenameSymbolTool ─────────────────────────────────────────────────────────

RenameSymbolTool::RenameSymbolTool(SymbolRenamer renamer)
    : m_renamer(std::move(renamer)) {}

ToolSpec RenameSymbolTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("rename_symbol");
    s.description = QStringLiteral(
        "Semantically rename a symbol across the entire workspace using "
        "the Language Server. All references, definitions, and declarations "
        "are updated consistently. Provide the file path, position of the "
        "symbol, and the new name.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 30000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the file containing the symbol.")}
            }},
            {QStringLiteral("line"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based line number of the symbol.")}
            }},
            {QStringLiteral("column"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based column number of the symbol.")}
            }},
            {QStringLiteral("newName"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The new name for the symbol.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("filePath"), QStringLiteral("line"),
            QStringLiteral("column"),   QStringLiteral("newName")
        }}
    };
    return s;
}

ToolExecResult RenameSymbolTool::invoke(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    const int line         = args[QLatin1String("line")].toInt(-1);
    const int column       = args[QLatin1String("column")].toInt(-1);
    const QString newName  = args[QLatin1String("newName")].toString();

    if (filePath.isEmpty() || line < 1 || column < 1 || newName.isEmpty())
        return {false, {}, {},
                QStringLiteral("All parameters required: filePath, line, column, newName")};

    if (!m_renamer)
        return {false, {}, {}, QStringLiteral("Rename functionality not available")};

    const QString result = m_renamer(filePath, line, column, newName);
    if (result.isEmpty())
        return {false, {}, {},
                QStringLiteral("Rename failed — no result from language server")};

    // Check if error
    if (result.startsWith(QLatin1String("Error")))
        return {false, {}, {}, result};

    QJsonObject data;
    data[QStringLiteral("result")] = result;

    return {true, data,
            QStringLiteral("Renamed to '%1'. %2").arg(newName, result),
            {}};
}

// ── ListCodeUsagesTool ───────────────────────────────────────────────────────

ListCodeUsagesTool::ListCodeUsagesTool(UsageFinder finder)
    : m_finder(std::move(finder)) {}

ToolSpec ListCodeUsagesTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("list_code_usages");
    s.description = QStringLiteral(
        "Find all usages (references, definitions, implementations) of a "
        "symbol at the given position. Returns a list of locations where "
        "the symbol is used across the workspace. Useful for understanding "
        "impact before refactoring.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 15000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the file containing the symbol.")}
            }},
            {QStringLiteral("line"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based line number of the symbol.")}
            }},
            {QStringLiteral("column"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based column number of the symbol.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{
            QStringLiteral("filePath"), QStringLiteral("line"),
            QStringLiteral("column")
        }}
    };
    return s;
}

ToolExecResult ListCodeUsagesTool::invoke(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    const int line         = args[QLatin1String("line")].toInt(-1);
    const int column       = args[QLatin1String("column")].toInt(-1);

    if (filePath.isEmpty() || line < 1 || column < 1)
        return {false, {}, {},
                QStringLiteral("Required: filePath, line, column")};

    if (!m_finder)
        return {false, {}, {},
                QStringLiteral("Code usages not available — no language server")};

    const QString result = m_finder(filePath, line, column);
    if (result.isEmpty())
        return {true, {}, QStringLiteral("No usages found."), {}};

    QJsonObject data;
    data[QStringLiteral("usages")] = result;

    return {true, data, result, {}};
}
