#include "scratchpadtool.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

ToolSpec ScratchpadTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("scratchpad");
    s.description = QStringLiteral(
        "Project-linked notes system. Notes live outside the repo "
        "(gitignored) but are associated with the current project. "
        "Use for work-in-progress notes, architecture decisions, "
        "debugging logs, research, TODO lists, and meeting notes.\n\n"
        "Operations:\n"
        "  write — Create or overwrite a note. Args: {title, content, tags?}\n"
        "  append — Append to an existing note. Args: {title, content}\n"
        "  read — Read a note. Args: {title}\n"
        "  list — List all notes. Args: {tag?} to filter by tag\n"
        "  search — Full-text search across notes. Args: {query}\n"
        "  delete — Delete a note. Args: {title}\n"
        "  tags — List all tags used across notes.\n\n"
        "Notes are Markdown files. Titles become filenames (sanitized). "
        "Tags are stored as YAML front matter.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("operation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), QJsonArray{
                    QStringLiteral("write"),
                    QStringLiteral("append"),
                    QStringLiteral("read"),
                    QStringLiteral("list"),
                    QStringLiteral("search"),
                    QStringLiteral("delete"),
                    QStringLiteral("tags")
                }},
                {QStringLiteral("description"),
                 QStringLiteral("The scratchpad operation to perform.")}
            }},
            {QStringLiteral("title"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Note title. Used as filename.")}
            }},
            {QStringLiteral("content"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Markdown content to write/append.")}
            }},
            {QStringLiteral("tags"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")}
                }},
                {QStringLiteral("description"),
                 QStringLiteral("Tags for categorization (e.g. \"architecture\", "
                                "\"debug\", \"research\", \"todo\").")}
            }},
            {QStringLiteral("query"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Search query for full-text search.")}
            }},
            {QStringLiteral("tag"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Filter by tag when listing notes.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}}
    };
    return s;
}

ToolExecResult ScratchpadTool::invoke(const QJsonObject &args)
{
    const QString op = args[QLatin1String("operation")].toString();
    if (op.isEmpty())
        return {false, {}, {}, QStringLiteral("'operation' is required.")};

    ensureDir();

    if (op == QLatin1String("write"))       return doWrite(args);
    if (op == QLatin1String("append"))      return doAppend(args);
    if (op == QLatin1String("read"))        return doRead(args);
    if (op == QLatin1String("list"))        return doList(args);
    if (op == QLatin1String("search"))      return doSearch(args);
    if (op == QLatin1String("delete"))      return doDelete(args);
    if (op == QLatin1String("tags"))        return doTags();

    return {false, {}, {},
            QStringLiteral("Unknown operation: %1").arg(op)};
}

void ScratchpadTool::ensureDir()
{
    QDir dir(m_dir);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));
}

QString ScratchpadTool::sanitizeTitle(const QString &title)
{
    QString safe = title.toLower().trimmed();
    safe.replace(QRegularExpression(QStringLiteral("[^a-z0-9_\\-]")),
                 QStringLiteral("_"));
    if (safe.size() > 80)
        safe.truncate(80);
    return safe;
}

QString ScratchpadTool::notePath(const QString &title) const
{
    return m_dir + QLatin1Char('/') + sanitizeTitle(title)
           + QStringLiteral(".md");
}

QString ScratchpadTool::buildFrontMatter(const QJsonArray &tags)
{
    if (tags.isEmpty())
        return {};
    QString fm = QStringLiteral("---\ntags:");
    for (const auto &t : tags)
        fm += QStringLiteral(" ") + t.toString();
    fm += QStringLiteral("\ndate: ")
          + QDateTime::currentDateTime().toString(Qt::ISODate)
          + QStringLiteral("\n---\n\n");
    return fm;
}

QStringList ScratchpadTool::parseTags(const QString &content)
{
    QStringList tags;
    if (!content.startsWith(QLatin1String("---")))
        return tags;
    const int end = content.indexOf(QLatin1String("---"), 3);
    if (end < 0) return tags;
    const QString header = content.mid(3, end - 3);
    for (const QString &line : header.split(QLatin1Char('\n'))) {
        if (line.trimmed().startsWith(QLatin1String("tags:"))) {
            const QString tagStr = line.mid(line.indexOf(QLatin1Char(':')) + 1).trimmed();
            tags = tagStr.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        }
    }
    return tags;
}

QString ScratchpadTool::readFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QTextStream(&f).readAll();
}

ToolExecResult ScratchpadTool::doWrite(const QJsonObject &args)
{
    const QString title   = args[QLatin1String("title")].toString();
    const QString content = args[QLatin1String("content")].toString();
    if (title.isEmpty())
        return {false, {}, {}, QStringLiteral("'title' is required.")};

    const QString path = notePath(title);
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return {false, {}, {},
                QStringLiteral("Cannot write: %1").arg(f.errorString())};

    QTextStream out(&f);
    const QJsonArray tags = args[QLatin1String("tags")].toArray();
    out << buildFrontMatter(tags);
    out << QStringLiteral("# ") << title << QStringLiteral("\n\n");
    out << content;
    f.close();

    return {true, {}, QStringLiteral("Note saved: %1").arg(title), {}};
}

ToolExecResult ScratchpadTool::doAppend(const QJsonObject &args)
{
    const QString title   = args[QLatin1String("title")].toString();
    const QString content = args[QLatin1String("content")].toString();
    if (title.isEmpty())
        return {false, {}, {}, QStringLiteral("'title' is required.")};

    const QString path = notePath(title);
    if (!QFileInfo::exists(path))
        return {false, {}, {},
                QStringLiteral("Note '%1' does not exist. Use 'write' first.").arg(title)};

    QFile f(path);
    if (!f.open(QIODevice::Append | QIODevice::Text))
        return {false, {}, {},
                QStringLiteral("Cannot append: %1").arg(f.errorString())};

    QTextStream out(&f);
    out << QStringLiteral("\n") << content;
    f.close();

    return {true, {}, QStringLiteral("Appended to: %1").arg(title), {}};
}

ToolExecResult ScratchpadTool::doRead(const QJsonObject &args)
{
    const QString title = args[QLatin1String("title")].toString();
    if (title.isEmpty())
        return {false, {}, {}, QStringLiteral("'title' is required.")};

    const QString content = readFile(notePath(title));
    if (content.isEmpty())
        return {false, {}, {},
                QStringLiteral("Note '%1' not found.").arg(title)};

    QString truncated = content;
    if (truncated.size() > 50000) {
        truncated.truncate(50000);
        truncated += QStringLiteral("\n... (truncated)");
    }

    return {true, {}, truncated, {}};
}

ToolExecResult ScratchpadTool::doList(const QJsonObject &args)
{
    const QString filterTag = args[QLatin1String("tag")].toString();

    QDir dir(m_dir);
    const QFileInfoList entries = dir.entryInfoList(
        {QStringLiteral("*.md")}, QDir::Files, QDir::Time);

    if (entries.isEmpty())
        return {true, {}, QStringLiteral("No notes yet."), {}};

    QString text;
    int count = 0;
    for (const QFileInfo &fi : entries) {
        const QString content = readFile(fi.absoluteFilePath());
        const QStringList tags = parseTags(content);

        if (!filterTag.isEmpty() && !tags.contains(filterTag))
            continue;

        ++count;
        text += QStringLiteral("- **%1**").arg(fi.completeBaseName());
        if (!tags.isEmpty())
            text += QStringLiteral(" [%1]").arg(tags.join(QStringLiteral(", ")));
        text += QStringLiteral(" (%1)\n")
                    .arg(fi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    }

    if (count == 0)
        text = QStringLiteral("No notes matching tag '%1'.").arg(filterTag);
    else
        text = QStringLiteral("%1 note(s):\n").arg(count) + text;

    return {true, {}, text, {}};
}

ToolExecResult ScratchpadTool::doSearch(const QJsonObject &args)
{
    const QString query = args[QLatin1String("query")].toString();
    if (query.isEmpty())
        return {false, {}, {}, QStringLiteral("'query' is required.")};

    QDir dir(m_dir);
    const QFileInfoList entries = dir.entryInfoList(
        {QStringLiteral("*.md")}, QDir::Files);

    const QString lower = query.toLower();
    QString text;
    int hits = 0;

    for (const QFileInfo &fi : entries) {
        const QString content = readFile(fi.absoluteFilePath());
        if (!content.toLower().contains(lower))
            continue;

        ++hits;
        text += QStringLiteral("### %1\n").arg(fi.completeBaseName());

        // Show matching lines with context
        const QStringList lines = content.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].toLower().contains(lower)) {
                text += QStringLiteral("  L%1: %2\n").arg(i + 1).arg(lines[i].trimmed());
                if (text.size() > 20000) {
                    text += QStringLiteral("... (truncated)\n");
                    break;
                }
            }
        }
        text += QLatin1Char('\n');
    }

    if (hits == 0)
        text = QStringLiteral("No matches for '%1'.").arg(query);
    else
        text = QStringLiteral("%1 note(s) match '%2':\n\n").arg(hits).arg(query) + text;

    return {true, {}, text, {}};
}

ToolExecResult ScratchpadTool::doDelete(const QJsonObject &args)
{
    const QString title = args[QLatin1String("title")].toString();
    if (title.isEmpty())
        return {false, {}, {}, QStringLiteral("'title' is required.")};

    const QString path = notePath(title);
    if (!QFile::remove(path))
        return {false, {}, {},
                QStringLiteral("Note '%1' not found or cannot delete.").arg(title)};

    return {true, {}, QStringLiteral("Deleted: %1").arg(title), {}};
}

ToolExecResult ScratchpadTool::doTags()
{
    QDir dir(m_dir);
    const QFileInfoList entries = dir.entryInfoList(
        {QStringLiteral("*.md")}, QDir::Files);

    QHash<QString, int> tagCounts;
    for (const QFileInfo &fi : entries) {
        const QStringList tags = parseTags(readFile(fi.absoluteFilePath()));
        for (const QString &t : tags)
            tagCounts[t]++;
    }

    if (tagCounts.isEmpty())
        return {true, {}, QStringLiteral("No tags used yet."), {}};

    QString text = QStringLiteral("Tags:\n");
    for (auto it = tagCounts.cbegin(); it != tagCounts.cend(); ++it)
        text += QStringLiteral("  %1 (%2 notes)\n").arg(it.key()).arg(it.value());

    return {true, {}, text, {}};
}
