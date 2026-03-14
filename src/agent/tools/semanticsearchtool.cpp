#include "semanticsearchtool.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>

#include <algorithm>

SemanticSearchTool::SemanticSearchTool(const QString &workspaceRoot)
    : m_workspaceRoot(workspaceRoot)
{}

ToolSpec SemanticSearchTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("semantic_search");
    s.description = QStringLiteral(
        "Run a natural language search for relevant code or documentation "
        "comments from the workspace. Returns relevant code snippets. "
        "Use this when you are not sure of exact keywords — it splits "
        "the query into words and finds files with the best matches.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 30000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("query"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The natural language query to search for. "
                                "Should contain all relevant context such as "
                                "function names, variable names, or comments.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}}
    };
    return s;
}

ToolExecResult SemanticSearchTool::invoke(const QJsonObject &args)
{
    const QString query = args[QLatin1String("query")].toString().trimmed();
    if (query.isEmpty())
        return {false, {}, {},
                QStringLiteral("Missing required parameter: query")};

    if (m_workspaceRoot.isEmpty())
        return {false, {}, {},
                QStringLiteral("No workspace root set.")};

    // Extract keywords (split on non-alphanumeric, filter short words)
    const QStringList rawTokens = query.split(
        QRegularExpression(QStringLiteral("[^a-zA-Z0-9_]+")),
        Qt::SkipEmptyParts);
    QStringList keywords;
    for (const QString &tok : rawTokens) {
        if (tok.length() >= 2)
            keywords.append(tok.toLower());
    }
    if (keywords.isEmpty())
        return {false, {}, {},
                QStringLiteral("Query must contain at least one meaningful keyword.")};

    struct FileScore {
        QString relPath;
        int     score = 0;
        int     bestLine = -1;
        QString bestSnippet;
    };

    QList<FileScore> scored;

    QDirIterator it(m_workspaceRoot,
                    QStringList{QStringLiteral("*")},
                    QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        const QString filePath = it.next();

        // Skip build/hidden/binary directories
        if (filePath.contains(QLatin1String("/build")) ||
            filePath.contains(QLatin1String("/.git/")) ||
            filePath.contains(QLatin1String("/node_modules/")))
            continue;

        const QString relPath = QDir(m_workspaceRoot).relativeFilePath(filePath);

        // Score filename/path matches
        const QString lowerPath = relPath.toLower();
        int pathScore = 0;
        for (const QString &kw : keywords) {
            if (lowerPath.contains(kw))
                ++pathScore;
        }

        // Open and score file content
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        if (file.size() > 1024 * 1024)
            continue; // skip files > 1 MB

        const QString content = QString::fromUtf8(file.readAll());
        const QStringList lines = content.split(QLatin1Char('\n'));

        // Count distinct keyword hits in content and find best line
        QSet<int> hitKeywords;
        int bestLineNum = -1;
        int bestLineScore = 0;

        for (int lineNum = 0; lineNum < lines.size(); ++lineNum) {
            const QString lower = lines[lineNum].toLower();
            int lineScore = 0;
            for (int ki = 0; ki < keywords.size(); ++ki) {
                if (lower.contains(keywords[ki])) {
                    hitKeywords.insert(ki);
                    ++lineScore;
                }
            }
            if (lineScore > bestLineScore) {
                bestLineScore = lineScore;
                bestLineNum = lineNum;
            }
        }

        const int totalScore = pathScore * 3 + hitKeywords.size() * 2 + bestLineScore;
        if (totalScore == 0)
            continue;

        FileScore fs;
        fs.relPath = relPath;
        fs.score   = totalScore;
        fs.bestLine = bestLineNum;

        // Extract snippet around best matching line (±3 lines)
        if (bestLineNum >= 0) {
            const int start = std::max(0, bestLineNum - 3);
            const int end   = std::min(static_cast<int>(lines.size()) - 1, bestLineNum + 3);
            QStringList snippet;
            for (int i = start; i <= end; ++i)
                snippet.append(QStringLiteral("%1: %2").arg(i + 1).arg(lines[i]));
            fs.bestSnippet = snippet.join(QLatin1Char('\n'));
        }

        scored.append(fs);
    }

    if (scored.isEmpty())
        return {true, {},
                QStringLiteral("No relevant files found for: %1").arg(query), {}};

    // Sort by score descending
    std::sort(scored.begin(), scored.end(),
              [](const FileScore &a, const FileScore &b) {
                  return a.score > b.score;
              });

    // Return top 10 results
    const int limit = std::min(static_cast<int>(scored.size()), 10);
    QString result;
    for (int i = 0; i < limit; ++i) {
        const FileScore &fs = scored[i];
        result += QStringLiteral("## %1 (score: %2)\n").arg(fs.relPath).arg(fs.score);
        if (!fs.bestSnippet.isEmpty())
            result += fs.bestSnippet + QStringLiteral("\n\n");
    }

    return {true, {},
            QStringLiteral("Found %1 relevant file(s):\n\n%2")
                .arg(limit).arg(result), {}};
}
