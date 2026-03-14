#include "difftool.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>

#include <algorithm>
#include <vector>

// ── DiffTool ─────────────────────────────────────────────────────────────────

DiffTool::DiffTool(DiffViewer viewer)
    : m_viewer(std::move(viewer))
{
}

ToolSpec DiffTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("diff");
    s.description = QStringLiteral(
        "Compare two texts or files and generate a unified diff. "
        "Can optionally display the diff visually in the IDE's "
        "side-by-side diff viewer for user review.\n\n"
        "Use cases:\n"
        "  - Compare two files: provide leftPath and rightPath\n"
        "  - Compare file with proposed changes: leftPath + rightContent\n"
        "  - Compare two text blocks: leftContent + rightContent\n"
        "  - Show diff for user approval before applying changes");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 15000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("leftPath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the left/original file.")}
            }},
            {QStringLiteral("rightPath"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Path to the right/modified file.")}
            }},
            {QStringLiteral("leftContent"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Left/original text content (instead of leftPath).")}
            }},
            {QStringLiteral("rightContent"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Right/modified text content (instead of rightPath).")}
            }},
            {QStringLiteral("leftTitle"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Display title for the left side.")}
            }},
            {QStringLiteral("rightTitle"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Display title for the right side.")}
            }},
            {QStringLiteral("showInEditor"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("boolean")},
                {QStringLiteral("description"),
                 QStringLiteral("If true, opens the diff in the IDE's visual diff viewer.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{}}
    };
    return s;
}

ToolExecResult DiffTool::invoke(const QJsonObject &args)
{
    // Resolve left content
    QString left;
    QString leftTitle = args[QLatin1String("leftTitle")].toString();
    if (args.contains(QLatin1String("leftPath"))) {
        const QString path = resolvePath(args[QLatin1String("leftPath")].toString());
        left = readFile(path);
        if (left.isNull())
            return {false, {}, {}, QStringLiteral("Cannot read left file: %1").arg(path)};
        if (leftTitle.isEmpty()) leftTitle = path;
    } else if (args.contains(QLatin1String("leftContent"))) {
        left = args[QLatin1String("leftContent")].toString();
        if (leftTitle.isEmpty()) leftTitle = QStringLiteral("Original");
    } else {
        return {false, {}, {},
            QStringLiteral("Provide either 'leftPath' or 'leftContent'.")};
    }

    // Resolve right content
    QString right;
    QString rightTitle = args[QLatin1String("rightTitle")].toString();
    if (args.contains(QLatin1String("rightPath"))) {
        const QString path = resolvePath(args[QLatin1String("rightPath")].toString());
        right = readFile(path);
        if (right.isNull())
            return {false, {}, {}, QStringLiteral("Cannot read right file: %1").arg(path)};
        if (rightTitle.isEmpty()) rightTitle = path;
    } else if (args.contains(QLatin1String("rightContent"))) {
        right = args[QLatin1String("rightContent")].toString();
        if (rightTitle.isEmpty()) rightTitle = QStringLiteral("Modified");
    } else {
        return {false, {}, {},
            QStringLiteral("Provide either 'rightPath' or 'rightContent'.")};
    }

    // Generate unified diff
    const QString diff = generateUnifiedDiff(left, right, leftTitle, rightTitle);

    // Optionally show in editor
    const bool showInEditor = args[QLatin1String("showInEditor")].toBool(false);
    if (showInEditor && m_viewer) {
        m_viewer(left, right, leftTitle, rightTitle);
    }

    if (diff.isEmpty())
        return {true, {}, QStringLiteral("No differences found."), {}};

    // Count changes
    int added = 0, removed = 0;
    for (const QString &line : diff.split(QLatin1Char('\n'))) {
        if (line.startsWith(QLatin1Char('+')) && !line.startsWith(QLatin1String("+++")))
            ++added;
        else if (line.startsWith(QLatin1Char('-')) && !line.startsWith(QLatin1String("---")))
            ++removed;
    }

    QString summary = QStringLiteral("--- %1 vs %2 ---\n+%3 -%4 lines\n\n%5")
        .arg(leftTitle, rightTitle).arg(added).arg(removed).arg(diff);

    return {true, {}, summary, {}};
}

QString DiffTool::resolvePath(const QString &raw) const
{
    if (raw.isEmpty() || QFileInfo(raw).isAbsolute() || m_workspaceRoot.isEmpty())
        return raw;
    return QDir(m_workspaceRoot).absoluteFilePath(raw);
}

QString DiffTool::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return f.readAll();
}

QString DiffTool::generateUnifiedDiff(const QString &left, const QString &right,
                                      const QString &leftTitle, const QString &rightTitle)
{
    const QStringList lLines = left.split(QLatin1Char('\n'));
    const QStringList rLines = right.split(QLatin1Char('\n'));

    if (lLines == rLines) return {};

    // Use a simple diff approach: output context around changes
    QString result;
    result += QStringLiteral("--- %1\n").arg(leftTitle);
    result += QStringLiteral("+++ %1\n").arg(rightTitle);

    // Simple LCS-based diff for hunks
    // Build edit script via dynamic programming
    const int m = lLines.size();
    const int n = rLines.size();

    // For large files, fall back to simple per-line comparison
    if (m > 10000 || n > 10000) {
        result += QStringLiteral("@@ -1,%1 +1,%2 @@\n").arg(m).arg(n);
        for (const QString &l : lLines)
            result += QLatin1Char('-') + l + QLatin1Char('\n');
        for (const QString &r : rLines)
            result += QLatin1Char('+') + r + QLatin1Char('\n');
        return result;
    }

    // Compute LCS table
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (int i = 1; i <= m; ++i)
        for (int j = 1; j <= n; ++j)
            dp[i][j] = (lLines[i-1] == rLines[j-1])
                ? dp[i-1][j-1] + 1
                : qMax(dp[i-1][j], dp[i][j-1]);

    // Backtrack to get edit script
    struct Edit { char type; int lineNum; QString text; };
    std::vector<Edit> edits;
    int i = m, j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && lLines[i-1] == rLines[j-1]) {
            edits.push_back({' ', i, lLines[i-1]});
            --i; --j;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            edits.push_back({'+', j, rLines[j-1]});
            --j;
        } else {
            edits.push_back({'-', i, lLines[i-1]});
            --i;
        }
    }
    std::reverse(edits.begin(), edits.end());

    // Group into hunks with 3 lines of context
    constexpr int ctx = 3;
    int idx = 0;
    const int total = static_cast<int>(edits.size());
    while (idx < total) {
        // Skip context-only lines
        while (idx < total && edits[idx].type == ' ') ++idx;
        if (idx >= total) break;

        // Found a change — build hunk
        int hunkStart = qMax(0, idx - ctx);
        int hunkEnd = idx;
        // Extend hunk to include nearby changes
        while (hunkEnd < total) {
            if (edits[hunkEnd].type != ' ') {
                hunkEnd++;
                continue;
            }
            // Check if next change is within context range
            int nextChange = hunkEnd;
            while (nextChange < total && edits[nextChange].type == ' ')
                ++nextChange;
            if (nextChange < total && nextChange - hunkEnd <= 2 * ctx)
                hunkEnd = nextChange;
            else
                break;
        }
        hunkEnd = qMin(total, hunkEnd + ctx);

        // Count lines for hunk header
        int oldCount = 0, newCount = 0;
        for (int k = hunkStart; k < hunkEnd; ++k) {
            if (edits[k].type != '+') ++oldCount;
            if (edits[k].type != '-') ++newCount;
        }

        result += QStringLiteral("@@ -%1,%2 +%3,%4 @@\n")
            .arg(hunkStart + 1).arg(oldCount).arg(hunkStart + 1).arg(newCount);

        for (int k = hunkStart; k < hunkEnd; ++k) {
            result += QLatin1Char(edits[k].type);
            result += edits[k].text;
            result += QLatin1Char('\n');
        }

        idx = hunkEnd;
    }

    return result;
}
