#include "navigationtools.h"

#include <QFileInfo>

// ── OpenFileTool ─────────────────────────────────────────────────────────────

OpenFileTool::OpenFileTool(FileOpener opener)
    : m_opener(std::move(opener))
{}

ToolSpec OpenFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("open_file");
    s.description = QStringLiteral(
        "Open a file in the editor at a specific line and column. "
        "Use this to navigate the user to the exact location of a "
        "bug, definition, or important code section.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the file to open.")}
            }},
            {QStringLiteral("line"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based line number to navigate to.")}
            }},
            {QStringLiteral("column"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("1-based column number (default: 1).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath")}}
    };
    return s;
}

ToolExecResult OpenFileTool::invoke(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};

    const int line   = args[QLatin1String("line")].toInt(1);
    const int column = args[QLatin1String("column")].toInt(1);

    const bool ok = m_opener(filePath, line, column);
    if (!ok)
        return {false, {}, {},
                QStringLiteral("Failed to open %1. File may not exist.").arg(filePath)};

    QString msg = QStringLiteral("Opened %1").arg(filePath);
    if (line > 1)
        msg += QStringLiteral(" at line %1").arg(line);
    return {true, {}, msg, {}};
}

// ── SwitchHeaderSourceTool ───────────────────────────────────────────────────

SwitchHeaderSourceTool::SwitchHeaderSourceTool(HeaderSourceSwitcher switcher)
    : m_switcher(std::move(switcher))
{}

ToolSpec SwitchHeaderSourceTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("switch_header_source");
    s.description = QStringLiteral(
        "Toggle between C/C++ header and source file. "
        "If the current file is .h/.hpp, opens the corresponding "
        ".cpp/.cxx/.cc file, and vice versa. Essential for C++ "
        "development workflow.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 5000;
    s.contexts    = {QStringLiteral("cpp"), QStringLiteral("c")};
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("filePath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the current header or source file.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("filePath")}}
    };
    return s;
}

ToolExecResult SwitchHeaderSourceTool::invoke(const QJsonObject &args)
{
    const QString filePath = args[QLatin1String("filePath")].toString();
    if (filePath.isEmpty())
        return {false, {}, {}, QStringLiteral("'filePath' is required.")};

    QString counterpart;

    if (m_switcher) {
        counterpart = m_switcher(filePath);
    } else {
        // Built-in fallback: try common extensions
        counterpart = findCounterpart(filePath);
    }

    if (counterpart.isEmpty())
        return {false, {}, {},
                QStringLiteral("No counterpart found for %1").arg(filePath)};

    return {true, {}, QStringLiteral("Counterpart: %1").arg(counterpart), {}};
}

QString SwitchHeaderSourceTool::findCounterpart(const QString &path)
{
    const QFileInfo fi(path);
    const QString base = fi.path() + QLatin1Char('/') + fi.completeBaseName();
    const QString ext  = fi.suffix().toLower();

    // Header → Source
    static const QStringList headerExts = {
        QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hxx"), QStringLiteral("hh")
    };
    static const QStringList sourceExts = {
        QStringLiteral("cpp"), QStringLiteral("cxx"), QStringLiteral("cc"), QStringLiteral("c")
    };

    const QStringList &candidates = headerExts.contains(ext) ? sourceExts : headerExts;

    for (const auto &candidate : candidates) {
        const QString tryPath = base + QLatin1Char('.') + candidate;
        if (QFileInfo::exists(tryPath))
            return tryPath;
    }
    return {};
}
