#include "toolpresentation.h"

namespace ToolPresentation {

Info present(const QString &toolName)
{
    // ── File system tools ────────────────────────────────────────────
    if (toolName == QLatin1String("grep_search"))
        return { QStringLiteral("Search Files"),
                 QStringLiteral("Searching files\u2026"),
                 QStringLiteral("Searched files"),
                 QStringLiteral("Q") };

    if (toolName == QLatin1String("file_search"))
        return { QStringLiteral("Find Files"),
                 QStringLiteral("Finding files\u2026"),
                 QStringLiteral("Found files"),
                 QStringLiteral("Q") };

    if (toolName == QLatin1String("read_file"))
        return { QStringLiteral("Read File"),
                 QStringLiteral("Reading file\u2026"),
                 QStringLiteral("Read file"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("list_dir"))
        return { QStringLiteral("List Directory"),
                 QStringLiteral("Listing directory\u2026"),
                 QStringLiteral("Listed directory"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("create_file"))
        return { QStringLiteral("Create File"),
                 QStringLiteral("Creating file\u2026"),
                 QStringLiteral("Created file"),
                 QStringLiteral("+") };

    if (toolName == QLatin1String("replace_string_in_file")
        || toolName == QLatin1String("edit_file"))
        return { QStringLiteral("Edit File"),
                 QStringLiteral("Editing file\u2026"),
                 QStringLiteral("Edited file"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("delete_file"))
        return { QStringLiteral("Delete File"),
                 QStringLiteral("Deleting file\u2026"),
                 QStringLiteral("Deleted file"),
                 QStringLiteral("\u2716") };

    // ── Terminal tools ───────────────────────────────────────────────
    if (toolName == QLatin1String("run_in_terminal")
        || toolName == QLatin1String("run_command"))
        return { QStringLiteral("Run Command"),
                 QStringLiteral("Running command\u2026"),
                 QStringLiteral("Ran command"),
                 QStringLiteral(">") };

    if (toolName == QLatin1String("get_terminal_output"))
        return { QStringLiteral("Get Terminal Output"),
                 QStringLiteral("Getting output\u2026"),
                 QStringLiteral("Got output"),
                 QStringLiteral(">") };

    // ── Code intelligence ────────────────────────────────────────────
    if (toolName == QLatin1String("semantic_search")
        || toolName == QLatin1String("codebase_search"))
        return { QStringLiteral("Search Codebase"),
                 QStringLiteral("Searching codebase\u2026"),
                 QStringLiteral("Searched codebase"),
                 QStringLiteral("Q") };

    if (toolName == QLatin1String("get_errors")
        || toolName == QLatin1String("diagnostics"))
        return { QStringLiteral("Check Errors"),
                 QStringLiteral("Checking errors\u2026"),
                 QStringLiteral("Checked errors"),
                 QStringLiteral("\u26A0") };

    // ── Thinking ─────────────────────────────────────────────────────
    if (toolName == QLatin1String("think")
        || toolName == QLatin1String("thinking"))
        return { QStringLiteral("Thinking"),
                 QStringLiteral("Thinking\u2026"),
                 QStringLiteral("Thought through the problem"),
                 QStringLiteral("\U0001F4A1") };

    // ── Web / fetch ──────────────────────────────────────────────────
    if (toolName == QLatin1String("fetch_webpage")
        || toolName == QLatin1String("web_search"))
        return { QStringLiteral("Web Search"),
                 QStringLiteral("Searching the web\u2026"),
                 QStringLiteral("Searched the web"),
                 QStringLiteral("\U0001F310") };

    // ── Git tools ────────────────────────────────────────────────────
    if (toolName.startsWith(QLatin1String("git_")))
        return { QStringLiteral("Git: %1").arg(toolName.mid(4)),
                 QStringLiteral("Running git\u2026"),
                 QStringLiteral("Ran git command"),
                 QStringLiteral("\u2387") };

    // ── Memory tools ─────────────────────────────────────────────────
    if (toolName == QLatin1String("memory"))
        return { QStringLiteral("Memory"),
                 QStringLiteral("Accessing memory\u2026"),
                 QStringLiteral("Accessed memory"),
                 QStringLiteral("\u2630") };

    // ── Todo tools ───────────────────────────────────────────────────
    if (toolName == QLatin1String("manage_todo_list"))
        return { QStringLiteral("Todo List"),
                 QStringLiteral("Managing todo list\u2026"),
                 QStringLiteral("Updated todo list"),
                 QStringLiteral("\u2611") };

    // ── Apply patch ──────────────────────────────────────────────────
    if (toolName == QLatin1String("apply_patch"))
        return { QStringLiteral("Apply Patch"),
                 QStringLiteral("Applying patch\u2026"),
                 QStringLiteral("Applied patch"),
                 QStringLiteral("\u270E") };

    // ── Multi-replace ────────────────────────────────────────────────
    if (toolName == QLatin1String("multi_replace_string_in_file"))
        return { QStringLiteral("Multi-Edit File"),
                 QStringLiteral("Editing file\u2026"),
                 QStringLiteral("Edited file"),
                 QStringLiteral("\u270E") };

    // ── Insert edit ──────────────────────────────────────────────────
    if (toolName == QLatin1String("insert_edit_into_file"))
        return { QStringLiteral("Insert Into File"),
                 QStringLiteral("Inserting into file\u2026"),
                 QStringLiteral("Inserted into file"),
                 QStringLiteral("\u270E") };

    // ── Read project structure ───────────────────────────────────────
    if (toolName == QLatin1String("read_project_structure"))
        return { QStringLiteral("Read Project Structure"),
                 QStringLiteral("Reading project structure\u2026"),
                 QStringLiteral("Read project structure"),
                 QStringLiteral("\u2630") };

    // ── Create directory ─────────────────────────────────────────────
    if (toolName == QLatin1String("create_directory"))
        return { QStringLiteral("Create Directory"),
                 QStringLiteral("Creating directory\u2026"),
                 QStringLiteral("Created directory"),
                 QStringLiteral("+") };

    // ── Default ──────────────────────────────────────────────────────
    return { toolName,
             QStringLiteral("Running %1\u2026").arg(toolName),
             QStringLiteral("Ran %1").arg(toolName),
             QStringLiteral("\u25B6") };
}

} // namespace ToolPresentation
