#include "searchworkspacetool.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

SearchWorkspaceTool::SearchWorkspaceTool(const QString &workspaceRoot)
    : m_workspaceRoot(workspaceRoot)
{}

ToolSpec SearchWorkspaceTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("grep_search");
    s.description = QStringLiteral(
        "Do a fast text search in the workspace. Use this tool when you want "
        "to search with an exact string or regex. Returns matching lines with "
        "file paths and line numbers. Use includePattern to search within files "
        "matching a specific pattern or in a specific file.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.parallelSafe = true;
    s.timeoutMs   = 30000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("query"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The pattern to search for. Use regex with alternation (e.g. 'word1|word2') to find multiple words at once.")}
            }},
            {QStringLiteral("isRegexp"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("Whether the pattern is a regex (default: false). Set this properly to declare whether it's a regex or plain text.")}
            }},
            {QStringLiteral("includePattern"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Glob pattern to filter files (e.g. '*.cpp'). "
                                "Default: search all text files.")}
            }},
            {QStringLiteral("maxResults"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum number of matches to return (default: 50).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}}
    };
    return s;
}

ToolExecResult SearchWorkspaceTool::invoke(const QJsonObject &args)
{
    const QString pattern = args[QLatin1String("query")].toString();
    if (pattern.isEmpty())
        return {false, {}, {},
                QStringLiteral("Missing required parameter: query")};

    if (m_workspaceRoot.isEmpty())
        return {false, {}, {},
                QStringLiteral("No workspace root set.")};

    const bool isRegex     = args[QLatin1String("isRegexp")].toBool(false);
    const QString include  = args[QLatin1String("includePattern")].toString();
    const int maxResults   = args[QLatin1String("maxResults")].toInt(50);

    QRegularExpression regex;
    if (isRegex) {
        regex.setPattern(pattern);
        regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid())
            return {false, {}, {},
                    QStringLiteral("Invalid regex: %1").arg(regex.errorString())};
    }

    // Collect matching files
    QStringList nameFilters;
    if (!include.isEmpty())
        nameFilters << include;

    QDirIterator it(m_workspaceRoot,
                    nameFilters.isEmpty()
                        ? QStringList{QStringLiteral("*")} : nameFilters,
                    QDir::Files,
                    QDirIterator::Subdirectories);

    int matchCount = 0;
    QString result;

    while (it.hasNext() && matchCount < maxResults) {
        const QString filePath = it.next();

        // Skip binary-ish files and build dirs
        if (filePath.contains(QLatin1String("/build")) ||
            filePath.contains(QLatin1String("/.git/")) ||
            filePath.contains(QLatin1String("/node_modules/")))
            continue;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        // Size limit: skip files > 1 MB
        if (file.size() > 1024 * 1024)
            continue;

        const QString content = QString::fromUtf8(file.readAll());
        const QStringList lines = content.split(QLatin1Char('\n'));

        const QString relPath = QDir(m_workspaceRoot).relativeFilePath(filePath);

        for (int lineNum = 0; lineNum < lines.size() && matchCount < maxResults; ++lineNum) {
            bool matches = false;
            if (isRegex) {
                matches = regex.match(lines[lineNum]).hasMatch();
            } else {
                matches = lines[lineNum].contains(pattern, Qt::CaseInsensitive);
            }

            if (matches) {
                result += QStringLiteral("%1:%2: %3\n")
                              .arg(relPath)
                              .arg(lineNum + 1)
                              .arg(lines[lineNum].trimmed());
                ++matchCount;
            }
        }
    }

    if (matchCount == 0)
        return {true, {}, QStringLiteral("No matches found for pattern: %1").arg(pattern), {}};

    return {true, {},
            QStringLiteral("%1 match(es) found:\n%2").arg(matchCount).arg(result), {}};
}
