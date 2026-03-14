#include "advancedtools.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

// ── FileSearchTool ────────────────────────────────────────────────────────────

FileSearchTool::FileSearchTool(const QString &workspaceRoot)
    : m_workspaceRoot(workspaceRoot) {}

ToolSpec FileSearchTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("file_search");
    s.description = QStringLiteral(
        "Search for files in the workspace by glob pattern. "
        "This only returns the paths of matching files. "
        "Use this tool when you know the filename pattern of the files you're "
        "searching for. Glob patterns match recursively. "
        "Examples: **/*.{cpp,h} to match all cpp/h files, "
        "src/** to match all files under src/.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.parallelSafe = true;
    s.timeoutMs   = 15000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("query"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Glob pattern to match files (e.g. '**/*.cpp', 'src/**/*.h', '**/CMakeLists.txt').")}
            }},
            {QStringLiteral("maxResults"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum number of results to return (default: 50).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}}
    };
    return s;
}

ToolExecResult FileSearchTool::invoke(const QJsonObject &args)
{
    const QString query = args[QLatin1String("query")].toString();
    if (query.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: query")};

    if (m_workspaceRoot.isEmpty())
        return {false, {}, {}, QStringLiteral("No workspace root set.")};

    const int maxResults = args[QLatin1String("maxResults")].toInt(50);

    // Convert glob to QDir name filter + recursive search
    // Extract the filename portion of the glob pattern
    QString nameFilter = query;
    const int lastSlash = query.lastIndexOf(QLatin1Char('/'));
    if (lastSlash >= 0)
        nameFilter = query.mid(lastSlash + 1);

    // Remove ** prefix tokens
    if (nameFilter.startsWith(QLatin1String("**")))
        nameFilter = nameFilter.mid(2);
    if (nameFilter.startsWith(QLatin1Char('/')))
        nameFilter = nameFilter.mid(1);

    if (nameFilter.isEmpty())
        nameFilter = QStringLiteral("*");

    QDirIterator it(m_workspaceRoot,
                    QStringList{nameFilter},
                    QDir::Files,
                    QDirIterator::Subdirectories);

    QStringList results;
    while (it.hasNext() && results.size() < maxResults) {
        const QString filePath = it.next();
        // Skip build dirs and .git
        if (filePath.contains(QLatin1String("/.git/")) ||
            filePath.contains(QLatin1String("/build"))  ||
            filePath.contains(QLatin1String("\\build")) ||
            filePath.contains(QLatin1String("\\node_modules\\")))
            continue;
        results << QDir(m_workspaceRoot).relativeFilePath(filePath);
    }

    if (results.isEmpty())
        return {true, {}, QStringLiteral("No files found matching: %1").arg(query), {}};

    return {true, {}, results.join(QLatin1Char('\n')), {}};
}

// ── CreateDirectoryTool ───────────────────────────────────────────────────────

ToolSpec CreateDirectoryTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("create_directory");
    s.description = QStringLiteral(
        "Create a new directory. Creates all necessary parent directories. "
        "Has no effect if the directory already exists.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 5000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("The absolute path to the directory to create.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}
    };
    return s;
}

ToolExecResult CreateDirectoryTool::invoke(const QJsonObject &args)
{
    const QString path = args[QLatin1String("path")].toString();
    if (path.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

    QDir dir;
    if (!dir.mkpath(path))
        return {false, {}, {}, QStringLiteral("Failed to create directory: %1").arg(path)};

    return {true, {}, QStringLiteral("Directory created: %1").arg(path), {}};
}

// ── MultiReplaceStringTool ────────────────────────────────────────────────────

ToolSpec MultiReplaceStringTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("multi_replace_string_in_file");
    s.description = QStringLiteral(
        "Apply multiple replace_string_in_file operations in a single call, "
        "which is more efficient than calling replace_string_in_file multiple times. "
        "It takes an array of replacement operations and applies them sequentially. "
        "Use this when making multiple edits across different files or multiple edits "
        "in the same file.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 30000;

    QJsonObject replacementSchema{
        {QLatin1String("type"), QLatin1String("object")},
        {QLatin1String("properties"), QJsonObject{
            {QLatin1String("filePath"), QJsonObject{
                {QLatin1String("type"),        QLatin1String("string")},
                {QLatin1String("description"), QLatin1String("Absolute path to the file to edit.")}
            }},
            {QLatin1String("oldString"), QJsonObject{
                {QLatin1String("type"),        QLatin1String("string")},
                {QLatin1String("description"), QLatin1String("The exact literal text to replace. Include context lines to uniquely identify the location.")}
            }},
            {QLatin1String("newString"), QJsonObject{
                {QLatin1String("type"),        QLatin1String("string")},
                {QLatin1String("description"), QLatin1String("The exact literal text to replace oldString with.")}
            }}
        }},
        {QLatin1String("required"), QJsonArray{
            QLatin1String("filePath"), QLatin1String("oldString"), QLatin1String("newString")}}
    };

    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("replacements"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("description"), QStringLiteral("Array of replacement operations to apply sequentially.")},
                {QStringLiteral("items"), replacementSchema}
            }},
            {QStringLiteral("explanation"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("A brief explanation of what the multi-replace operation will accomplish.")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("replacements")}}
    };
    return s;
}

ToolExecResult MultiReplaceStringTool::invoke(const QJsonObject &args)
{
    const QJsonArray replacements = args[QLatin1String("replacements")].toArray();
    if (replacements.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: replacements")};

    int successCount = 0;
    int failCount    = 0;
    QStringList summary;

    for (const QJsonValue &val : replacements) {
        const QJsonObject op     = val.toObject();
        const QString filePath   = op[QLatin1String("filePath")].toString();
        const QString oldString  = op[QLatin1String("oldString")].toString();
        const QString newString  = op[QLatin1String("newString")].toString();

        if (filePath.isEmpty() || oldString.isEmpty()) {
            summary << QStringLiteral("[SKIP] Missing filePath or oldString");
            ++failCount;
            continue;
        }

        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            summary << QStringLiteral("[FAIL] Cannot read: %1").arg(filePath);
            ++failCount;
            continue;
        }
        QString content = QTextStream(&f).readAll();
        f.close();

        if (!content.contains(oldString)) {
            summary << QStringLiteral("[FAIL] oldString not found in: %1").arg(QFileInfo(filePath).fileName());
            ++failCount;
            continue;
        }

        content.replace(oldString, newString);

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            summary << QStringLiteral("[FAIL] Cannot write: %1").arg(filePath);
            ++failCount;
            continue;
        }
        QTextStream(&f) << content;
        f.close();

        summary << QStringLiteral("[OK] %1").arg(QFileInfo(filePath).fileName());
        ++successCount;
    }

    const bool allOk = failCount == 0;
    return {allOk, {},
            QStringLiteral("%1/%2 replacements successful:\n%3")
                .arg(successCount)
                .arg(replacements.size())
                .arg(summary.join(QLatin1Char('\n'))), {}};
}

// ── InsertEditIntoFileTool ────────────────────────────────────────────────────

ToolSpec InsertEditIntoFileTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("insert_edit_into_file");
    s.description = QStringLiteral(
        "Insert or replace content at a specific location in a file. "
        "Provide either a line range to replace, or just startLine to insert before "
        "that line. Line numbers are 1-based. Use this to add new code at a specific "
        "position, replace a block of lines, or append to the end of a file.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 10000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("Absolute path to the file to edit.")}
            }},
            {QStringLiteral("content"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"), QStringLiteral("The text to insert or use as replacement.")}
            }},
            {QStringLiteral("startLine"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"), QStringLiteral("1-based line number where insertion begins. 0 or omitted = append to end.")}
            }},
            {QStringLiteral("endLine"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"), QStringLiteral("1-based inclusive end line to replace. Omit or 0 to only insert (not replace).")}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("path"), QStringLiteral("content")}}
    };
    return s;
}

ToolExecResult InsertEditIntoFileTool::invoke(const QJsonObject &args)
{
    const QString path    = args[QLatin1String("path")].toString();
    const QString content = args[QLatin1String("content")].toString();

    if (path.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing required parameter: path")};

    const int startLine = args[QLatin1String("startLine")].toInt(0);
    const int endLine   = args[QLatin1String("endLine")].toInt(0);

    // If the file does not exist, just create it
    QFile f(path);
    if (!f.exists()) {
        // Create parent dir if needed
        QFileInfo fi(path);
        QDir().mkpath(fi.absolutePath());

        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            return {false, {}, {}, QStringLiteral("Cannot create file: %1").arg(path)};
        QTextStream(&f) << content;
        f.close();
        return {true, {}, QStringLiteral("Created %1").arg(path), {}};
    }

    // Read existing content
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {false, {}, {}, QStringLiteral("Cannot read file: %1").arg(path)};
    QStringList lines = QTextStream(&f).readAll().split(QLatin1Char('\n'));
    f.close();

    // Remove trailing empty line from split
    if (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    if (startLine <= 0) {
        // Append mode
        lines << content.split(QLatin1Char('\n'));
    } else {
        const int from = qBound(1, startLine, lines.size() + 1) - 1; // 0-based
        const int to   = endLine > 0 ? qBound(from, endLine - 1, lines.size() - 1) : from - 1;

        QStringList newLines = content.split(QLatin1Char('\n'));

        if (to >= from && endLine > 0) {
            // Replace range [from, to] inclusive
            lines.replace(from, newLines.first());
            // Remove extra replaced lines, insert remaining new lines
            for (int i = from + 1; i <= to; ++i)
                lines.removeAt(from + 1);
            for (int i = 1; i < newLines.size(); ++i)
                lines.insert(from + i, newLines[i]);
        } else {
            // Insert before startLine
            for (int i = 0; i < newLines.size(); ++i)
                lines.insert(from + i, newLines[i]);
        }
    }

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return {false, {}, {}, QStringLiteral("Cannot write file: %1").arg(path)};
    QTextStream(&f) << lines.join(QLatin1Char('\n')) << QLatin1Char('\n');
    f.close();

    return {true, {},
            QStringLiteral("Edited %1 (%2 lines)")
                .arg(QFileInfo(path).fileName())
                .arg(lines.size()), {}};
}

// ── ManageTodoListTool ────────────────────────────────────────────────────────

ManageTodoListTool::ManageTodoListTool(const QString &todoFile)
    : m_todoFile(todoFile) {}

ToolSpec ManageTodoListTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("manage_todo_list");
    s.description = QStringLiteral(
        "Manage a structured todo list to track progress and plan tasks. "
        "Use this frequently for complex multi-step work. "
        "Provide the complete array of all todo items with each call — it replaces the entire list. "
        "Each item has: id (number), title (3-7 words), status (not-started|in-progress|completed). "
        "Mark one todo as in-progress at a time. Mark completed immediately after finishing.");
    s.permission  = AgentToolPermission::SafeMutate;
    s.timeoutMs   = 5000;

    QJsonObject itemSchema{
        {QLatin1String("type"), QLatin1String("object")},
        {QLatin1String("properties"), QJsonObject{
            {QLatin1String("id"), QJsonObject{
                {QLatin1String("type"), QLatin1String("number")},
                {QLatin1String("description"), QLatin1String("Unique sequential identifier starting from 1.")}
            }},
            {QLatin1String("title"), QJsonObject{
                {QLatin1String("type"), QLatin1String("string")},
                {QLatin1String("description"), QLatin1String("Concise action-oriented label (3-7 words).")}
            }},
            {QLatin1String("status"), QJsonObject{
                {QLatin1String("type"), QLatin1String("string")},
                {QLatin1String("enum"), QJsonArray{QLatin1String("not-started"), QLatin1String("in-progress"), QLatin1String("completed")}},
                {QLatin1String("description"), QLatin1String("Current state of the todo.")}
            }}
        }},
        {QLatin1String("required"), QJsonArray{QLatin1String("id"), QLatin1String("title"), QLatin1String("status")}}
    };

    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("todoList"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("description"), QStringLiteral("Complete array of all todo items. Must include ALL items - both existing and new.")},
                {QStringLiteral("items"), itemSchema}
            }}
        }},
        {QStringLiteral("required"), QJsonArray{QStringLiteral("todoList")}}
    };
    return s;
}

ToolExecResult ManageTodoListTool::invoke(const QJsonObject &args)
{
    const QJsonArray todoList = args[QLatin1String("todoList")].toArray();

    // Persist to file
    if (!m_todoFile.isEmpty()) {
        QFile f(m_todoFile);
        QFileInfo fi(m_todoFile);
        QDir().mkpath(fi.absolutePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            QTextStream(&f) << QJsonDocument(todoList).toJson(QJsonDocument::Indented);
            f.close();
        }
    }

    // Build summary
    int notStarted = 0, inProgress = 0, completed = 0;
    QStringList lines;
    for (const QJsonValue &v : todoList) {
        const QJsonObject item = v.toObject();
        const QString status = item[QLatin1String("status")].toString();
        const QString title  = item[QLatin1String("title")].toString();
        const int id         = item[QLatin1String("id")].toInt();

        if (status == QLatin1String("not-started"))     { ++notStarted; }
        else if (status == QLatin1String("in-progress")) { ++inProgress; }
        else if (status == QLatin1String("completed"))  { ++completed; }

        const QString marker = (status == QLatin1String("completed"))   ? QStringLiteral("[x]")
                             : (status == QLatin1String("in-progress")) ? QStringLiteral("[~]")
                                                                        : QStringLiteral("[ ]");
        lines << QStringLiteral("%1 %2. %3").arg(marker).arg(id).arg(title);
    }

    return {true, {},
            QStringLiteral("Todo list updated (%1 total, %2 done, %3 in-progress, %4 pending):\n%5")
                .arg(todoList.size()).arg(completed).arg(inProgress).arg(notStarted)
                .arg(lines.join(QLatin1Char('\n'))), {}};
}

// ── ReadProjectStructureTool ──────────────────────────────────────────────────

ReadProjectStructureTool::ReadProjectStructureTool(const QString &workspaceRoot)
    : m_workspaceRoot(workspaceRoot) {}

ToolSpec ReadProjectStructureTool::spec() const
{
    ToolSpec s;
    s.name        = QStringLiteral("read_project_structure");
    s.description = QStringLiteral(
        "Read the workspace directory tree (depth-limited). Returns a list of files and folders "
        "with indentation showing hierarchy. Skips common ignore patterns like .git, node_modules, build.");
    s.permission  = AgentToolPermission::ReadOnly;
    s.timeoutMs   = 15000;
    s.inputSchema = QJsonObject{
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"), QJsonObject{
            {QStringLiteral("path"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("description"),
                 QStringLiteral("Subdirectory to list (relative to workspace root). Defaults to root.")}
            }},
            {QStringLiteral("maxDepth"), QJsonObject{
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("description"),
                 QStringLiteral("Maximum depth to recurse (default: 4).")}
            }}
        }}
    };
    return s;
}

ToolExecResult ReadProjectStructureTool::invoke(const QJsonObject &args)
{
    if (m_workspaceRoot.isEmpty())
        return {false, {}, {}, QStringLiteral("No workspace root configured.")};

    const QString subPath = args[QLatin1String("path")].toString();
    const int maxDepth = args[QLatin1String("maxDepth")].toInt(4);

    const QDir baseDir(subPath.isEmpty()
        ? m_workspaceRoot
        : QDir(m_workspaceRoot).absoluteFilePath(subPath));

    if (!baseDir.exists())
        return {false, {}, {}, QStringLiteral("Directory does not exist: %1").arg(baseDir.path())};

    QStringList output;
    listDir(baseDir, 0, maxDepth, output);

    const QString result = output.join(QLatin1Char('\n'));
    return {true, {}, result.isEmpty() ? QStringLiteral("(empty)") : result, {}};
}

void ReadProjectStructureTool::listDir(const QDir &dir, int depth, int maxDepth, QStringList &output) const
{
    if (depth > maxDepth)
        return;

    static const QStringList ignoreDirs = {
        QStringLiteral(".git"), QStringLiteral("node_modules"),
        QStringLiteral("build"), QStringLiteral("build-llvm"),
        QStringLiteral(".vs"), QStringLiteral("__pycache__"),
        QStringLiteral(".cache"), QStringLiteral("dist"),
        QStringLiteral("out"), QStringLiteral(".idea")
    };

    const QString indent(depth * 2, QLatin1Char(' '));
    const auto entries = dir.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::DirsFirst);

    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            if (ignoreDirs.contains(fi.fileName()))
                continue;
            output << QStringLiteral("%1%2/").arg(indent, fi.fileName());
            listDir(QDir(fi.absoluteFilePath()), depth + 1, maxDepth, output);
        } else {
            output << QStringLiteral("%1%2").arg(indent, fi.fileName());
        }
    }
}
