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

    if (toolName == QLatin1String("rename_file"))
        return { QStringLiteral("Rename File"),
                 QStringLiteral("Renaming file\u2026"),
                 QStringLiteral("Renamed file"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("copy_file"))
        return { QStringLiteral("Copy File"),
                 QStringLiteral("Copying file\u2026"),
                 QStringLiteral("Copied file"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("watch_files"))
        return { QStringLiteral("Watch Files"),
                 QStringLiteral("Watching files\u2026"),
                 QStringLiteral("Set up file watch"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("diff"))
        return { QStringLiteral("Diff Files"),
                 QStringLiteral("Comparing files\u2026"),
                 QStringLiteral("Compared files"),
                 QStringLiteral("\u2630") };

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

    if (toolName == QLatin1String("kill_terminal"))
        return { QStringLiteral("Kill Terminal"),
                 QStringLiteral("Killing terminal\u2026"),
                 QStringLiteral("Killed terminal"),
                 QStringLiteral(">") };

    if (toolName == QLatin1String("await_terminal"))
        return { QStringLiteral("Await Terminal"),
                 QStringLiteral("Waiting for terminal\u2026"),
                 QStringLiteral("Terminal completed"),
                 QStringLiteral(">") };

    if (toolName == QLatin1String("terminal_selection"))
        return { QStringLiteral("Terminal Selection"),
                 QStringLiteral("Getting terminal selection\u2026"),
                 QStringLiteral("Got terminal selection"),
                 QStringLiteral(">") };

    if (toolName == QLatin1String("terminal_last_command"))
        return { QStringLiteral("Last Command"),
                 QStringLiteral("Getting last command\u2026"),
                 QStringLiteral("Got last command"),
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

    // ── Editor / refactoring tools ───────────────────────────────────
    if (toolName == QLatin1String("open_file"))
        return { QStringLiteral("Open File"),
                 QStringLiteral("Opening file\u2026"),
                 QStringLiteral("Opened file"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("switch_header_source"))
        return { QStringLiteral("Switch Header/Source"),
                 QStringLiteral("Switching header/source\u2026"),
                 QStringLiteral("Switched header/source"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("format_code"))
        return { QStringLiteral("Format Code"),
                 QStringLiteral("Formatting code\u2026"),
                 QStringLiteral("Formatted code"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("refactor"))
        return { QStringLiteral("Refactor"),
                 QStringLiteral("Refactoring\u2026"),
                 QStringLiteral("Refactored code"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("rename_symbol"))
        return { QStringLiteral("Rename Symbol"),
                 QStringLiteral("Renaming symbol\u2026"),
                 QStringLiteral("Renamed symbol"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("list_code_usages"))
        return { QStringLiteral("Find Usages"),
                 QStringLiteral("Finding usages\u2026"),
                 QStringLiteral("Found usages"),
                 QStringLiteral("Q") };

    if (toolName == QLatin1String("get_editor_context"))
        return { QStringLiteral("Editor Context"),
                 QStringLiteral("Getting editor context\u2026"),
                 QStringLiteral("Got editor context"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("symbol_documentation"))
        return { QStringLiteral("Symbol Documentation"),
                 QStringLiteral("Looking up documentation\u2026"),
                 QStringLiteral("Found documentation"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("code_completion"))
        return { QStringLiteral("Code Completion"),
                 QStringLiteral("Getting completions\u2026"),
                 QStringLiteral("Got completions"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("tree_sitter_parse")
        || toolName == QLatin1String("tree_sitter_query"))
        return { QStringLiteral("Parse Syntax Tree"),
                 QStringLiteral("Parsing syntax tree\u2026"),
                 QStringLiteral("Parsed syntax tree"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("code_graph"))
        return { QStringLiteral("Code Graph"),
                 QStringLiteral("Analyzing code graph\u2026"),
                 QStringLiteral("Analyzed code graph"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("static_analysis"))
        return { QStringLiteral("Static Analysis"),
                 QStringLiteral("Running static analysis\u2026"),
                 QStringLiteral("Ran static analysis"),
                 QStringLiteral("\u26A0") };

    if (toolName == QLatin1String("analyze_change_impact")
        || toolName == QLatin1String("change_impact"))
        return { QStringLiteral("Analyze Impact"),
                 QStringLiteral("Analyzing change impact\u2026"),
                 QStringLiteral("Analyzed change impact"),
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
    if (toolName == QLatin1String("get_changed_files"))
        return { QStringLiteral("Changed Files"),
                 QStringLiteral("Getting changed files\u2026"),
                 QStringLiteral("Got changed files"),
                 QStringLiteral("\u2387") };

    if (toolName == QLatin1String("git_ops"))
        return { QStringLiteral("Git Operation"),
                 QStringLiteral("Running git\u2026"),
                 QStringLiteral("Ran git command"),
                 QStringLiteral("\u2387") };

    if (toolName.startsWith(QLatin1String("git_")))
        return { QStringLiteral("Git: %1").arg(toolName.mid(4)),
                 QStringLiteral("Running git\u2026"),
                 QStringLiteral("Ran git command"),
                 QStringLiteral("\u2387") };

    // ── Build / test tools ───────────────────────────────────────────
    if (toolName == QLatin1String("build_project"))
        return { QStringLiteral("Build Project"),
                 QStringLiteral("Building project\u2026"),
                 QStringLiteral("Built project"),
                 QStringLiteral("\u2692") };

    if (toolName == QLatin1String("run_tests"))
        return { QStringLiteral("Run Tests"),
                 QStringLiteral("Running tests\u2026"),
                 QStringLiteral("Ran tests"),
                 QStringLiteral("\u2611") };

    if (toolName == QLatin1String("get_build_targets"))
        return { QStringLiteral("Build Targets"),
                 QStringLiteral("Getting build targets\u2026"),
                 QStringLiteral("Got build targets"),
                 QStringLiteral("\u2692") };

    if (toolName == QLatin1String("test_failure"))
        return { QStringLiteral("Test Failure"),
                 QStringLiteral("Analyzing test failure\u2026"),
                 QStringLiteral("Analyzed test failure"),
                 QStringLiteral("\u2716") };

    if (toolName == QLatin1String("compile_and_run"))
        return { QStringLiteral("Compile & Run"),
                 QStringLiteral("Compiling and running\u2026"),
                 QStringLiteral("Compiled and ran"),
                 QStringLiteral("\u25B6") };

    if (toolName == QLatin1String("get_project_setup_info"))
        return { QStringLiteral("Project Setup"),
                 QStringLiteral("Getting project setup\u2026"),
                 QStringLiteral("Got project setup"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("workspace_config"))
        return { QStringLiteral("Workspace Config"),
                 QStringLiteral("Reading workspace config\u2026"),
                 QStringLiteral("Read workspace config"),
                 QStringLiteral("\u2630") };

    // ── Debug tools ──────────────────────────────────────────────────
    if (toolName == QLatin1String("debug_set_breakpoint"))
        return { QStringLiteral("Set Breakpoint"),
                 QStringLiteral("Setting breakpoint\u2026"),
                 QStringLiteral("Set breakpoint"),
                 QStringLiteral("\u25CF") };

    if (toolName == QLatin1String("debug_get_stack_trace"))
        return { QStringLiteral("Stack Trace"),
                 QStringLiteral("Getting stack trace\u2026"),
                 QStringLiteral("Got stack trace"),
                 QStringLiteral("\u25CF") };

    if (toolName == QLatin1String("debug_get_variables"))
        return { QStringLiteral("Debug Variables"),
                 QStringLiteral("Getting variables\u2026"),
                 QStringLiteral("Got variables"),
                 QStringLiteral("\u25CF") };

    if (toolName == QLatin1String("debug_step"))
        return { QStringLiteral("Debug Step"),
                 QStringLiteral("Stepping\u2026"),
                 QStringLiteral("Stepped"),
                 QStringLiteral("\u25CF") };

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

    // ── Lua tools ────────────────────────────────────────────────────
    if (toolName == QLatin1String("save_lua_tool"))
        return { QStringLiteral("Save Lua Tool"),
                 QStringLiteral("Saving Lua tool\u2026"),
                 QStringLiteral("Saved Lua tool"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("list_lua_tools"))
        return { QStringLiteral("List Lua Tools"),
                 QStringLiteral("Listing Lua tools\u2026"),
                 QStringLiteral("Listed Lua tools"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("run_lua_tool"))
        return { QStringLiteral("Run Lua Tool"),
                 QStringLiteral("Running Lua tool\u2026"),
                 QStringLiteral("Ran Lua tool"),
                 QStringLiteral("\u25B6") };

    if (toolName == QLatin1String("run_lua"))
        return { QStringLiteral("Run Lua"),
                 QStringLiteral("Running Lua script\u2026"),
                 QStringLiteral("Ran Lua script"),
                 QStringLiteral("\u25B6") };

    // ── UI / interaction tools ───────────────────────────────────────
    if (toolName == QLatin1String("screenshot"))
        return { QStringLiteral("Screenshot"),
                 QStringLiteral("Taking screenshot\u2026"),
                 QStringLiteral("Took screenshot"),
                 QStringLiteral("\U0001F4F7") };

    if (toolName == QLatin1String("introspect"))
        return { QStringLiteral("Introspect UI"),
                 QStringLiteral("Introspecting UI\u2026"),
                 QStringLiteral("Introspected UI"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("ask_user"))
        return { QStringLiteral("Ask User"),
                 QStringLiteral("Asking user\u2026"),
                 QStringLiteral("Asked user"),
                 QStringLiteral("?") };

    if (toolName == QLatin1String("run_ide_command"))
        return { QStringLiteral("IDE Command"),
                 QStringLiteral("Running IDE command\u2026"),
                 QStringLiteral("Ran IDE command"),
                 QStringLiteral(">") };

    if (toolName == QLatin1String("clipboard"))
        return { QStringLiteral("Clipboard"),
                 QStringLiteral("Accessing clipboard\u2026"),
                 QStringLiteral("Accessed clipboard"),
                 QStringLiteral("\u2630") };

    // ── Network / HTTP tools ─────────────────────────────────────────
    if (toolName == QLatin1String("http_request"))
        return { QStringLiteral("HTTP Request"),
                 QStringLiteral("Sending HTTP request\u2026"),
                 QStringLiteral("Sent HTTP request"),
                 QStringLiteral("\U0001F310") };

    if (toolName == QLatin1String("network"))
        return { QStringLiteral("Network"),
                 QStringLiteral("Network operation\u2026"),
                 QStringLiteral("Completed network operation"),
                 QStringLiteral("\U0001F310") };

    // ── Database tools ───────────────────────────────────────────────
    if (toolName == QLatin1String("database_query"))
        return { QStringLiteral("Database Query"),
                 QStringLiteral("Querying database\u2026"),
                 QStringLiteral("Queried database"),
                 QStringLiteral("\u2630") };

    // ── Subagent / orchestration ─────────────────────────────────────
    if (toolName == QLatin1String("run_subagent"))
        return { QStringLiteral("Run Subagent"),
                 QStringLiteral("Running subagent\u2026"),
                 QStringLiteral("Ran subagent"),
                 QStringLiteral("\u25B6") };

    if (toolName == QLatin1String("scratchpad"))
        return { QStringLiteral("Scratchpad"),
                 QStringLiteral("Using scratchpad\u2026"),
                 QStringLiteral("Used scratchpad"),
                 QStringLiteral("\u2630") };

    // ── Dashboard / mission tools ────────────────────────────────────
    if (toolName == QLatin1String("create_dashboard_mission"))
        return { QStringLiteral("Create Mission"),
                 QStringLiteral("Creating mission\u2026"),
                 QStringLiteral("Created mission"),
                 QStringLiteral("\u2611") };

    if (toolName == QLatin1String("update_dashboard_step"))
        return { QStringLiteral("Update Step"),
                 QStringLiteral("Updating step\u2026"),
                 QStringLiteral("Updated step"),
                 QStringLiteral("\u2611") };

    if (toolName == QLatin1String("update_dashboard_metric"))
        return { QStringLiteral("Update Metric"),
                 QStringLiteral("Updating metric\u2026"),
                 QStringLiteral("Updated metric"),
                 QStringLiteral("\u2611") };

    if (toolName == QLatin1String("add_dashboard_log"))
        return { QStringLiteral("Add Log"),
                 QStringLiteral("Adding log\u2026"),
                 QStringLiteral("Added log"),
                 QStringLiteral("\u2611") };

    if (toolName == QLatin1String("add_dashboard_artifact"))
        return { QStringLiteral("Add Artifact"),
                 QStringLiteral("Adding artifact\u2026"),
                 QStringLiteral("Added artifact"),
                 QStringLiteral("\u2611") };

    if (toolName == QLatin1String("complete_dashboard_mission"))
        return { QStringLiteral("Complete Mission"),
                 QStringLiteral("Completing mission\u2026"),
                 QStringLiteral("Completed mission"),
                 QStringLiteral("\u2611") };

    // ── Utility tools ────────────────────────────────────────────────
    if (toolName == QLatin1String("json_parse_format"))
        return { QStringLiteral("JSON Format"),
                 QStringLiteral("Formatting JSON\u2026"),
                 QStringLiteral("Formatted JSON"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("current_time"))
        return { QStringLiteral("Current Time"),
                 QStringLiteral("Getting time\u2026"),
                 QStringLiteral("Got time"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("regex_test"))
        return { QStringLiteral("Regex Test"),
                 QStringLiteral("Testing regex\u2026"),
                 QStringLiteral("Tested regex"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("environment_variables"))
        return { QStringLiteral("Environment Variables"),
                 QStringLiteral("Reading environment\u2026"),
                 QStringLiteral("Read environment"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("file_content_hash"))
        return { QStringLiteral("File Hash"),
                 QStringLiteral("Computing hash\u2026"),
                 QStringLiteral("Computed hash"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("archive"))
        return { QStringLiteral("Archive"),
                 QStringLiteral("Archiving\u2026"),
                 QStringLiteral("Archived"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("create_patch_file"))
        return { QStringLiteral("Create Patch"),
                 QStringLiteral("Creating patch\u2026"),
                 QStringLiteral("Created patch"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("image_info"))
        return { QStringLiteral("Image Info"),
                 QStringLiteral("Reading image info\u2026"),
                 QStringLiteral("Read image info"),
                 QStringLiteral("\U0001F4F7") };

    if (toolName == QLatin1String("generate_diagram"))
        return { QStringLiteral("Generate Diagram"),
                 QStringLiteral("Generating diagram\u2026"),
                 QStringLiteral("Generated diagram"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("performance_profile"))
        return { QStringLiteral("Performance Profile"),
                 QStringLiteral("Profiling\u2026"),
                 QStringLiteral("Profiled"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("process_manager"))
        return { QStringLiteral("Process Manager"),
                 QStringLiteral("Managing process\u2026"),
                 QStringLiteral("Managed process"),
                 QStringLiteral(">") };

    // ── Python tools ─────────────────────────────────────────────────
    if (toolName == QLatin1String("python_env"))
        return { QStringLiteral("Python Environment"),
                 QStringLiteral("Checking Python environment\u2026"),
                 QStringLiteral("Checked Python environment"),
                 QStringLiteral("\U0001F40D") };

    if (toolName == QLatin1String("install_python_packages"))
        return { QStringLiteral("Install Python Packages"),
                 QStringLiteral("Installing packages\u2026"),
                 QStringLiteral("Installed packages"),
                 QStringLiteral("\U0001F40D") };

    if (toolName == QLatin1String("run_python"))
        return { QStringLiteral("Run Python"),
                 QStringLiteral("Running Python\u2026"),
                 QStringLiteral("Ran Python"),
                 QStringLiteral("\U0001F40D") };

    // ── Node.js tools ────────────────────────────────────────────────
    if (toolName == QLatin1String("package_json_info"))
        return { QStringLiteral("Package.json Info"),
                 QStringLiteral("Reading package.json\u2026"),
                 QStringLiteral("Read package.json"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("npm_run"))
        return { QStringLiteral("NPM Run"),
                 QStringLiteral("Running npm script\u2026"),
                 QStringLiteral("Ran npm script"),
                 QStringLiteral(">") };

    if (toolName == QLatin1String("install_node_packages"))
        return { QStringLiteral("Install Node Packages"),
                 QStringLiteral("Installing packages\u2026"),
                 QStringLiteral("Installed packages"),
                 QStringLiteral(">") };

    // ── Notebook tools ───────────────────────────────────────────────
    if (toolName == QLatin1String("notebook_context"))
        return { QStringLiteral("Notebook Context"),
                 QStringLiteral("Reading notebook\u2026"),
                 QStringLiteral("Read notebook"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("read_cell_output"))
        return { QStringLiteral("Cell Output"),
                 QStringLiteral("Reading cell output\u2026"),
                 QStringLiteral("Read cell output"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("create_notebook"))
        return { QStringLiteral("Create Notebook"),
                 QStringLiteral("Creating notebook\u2026"),
                 QStringLiteral("Created notebook"),
                 QStringLiteral("+") };

    if (toolName == QLatin1String("edit_notebook_cells"))
        return { QStringLiteral("Edit Notebook"),
                 QStringLiteral("Editing notebook\u2026"),
                 QStringLiteral("Edited notebook"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("get_notebook_summary"))
        return { QStringLiteral("Notebook Summary"),
                 QStringLiteral("Getting notebook summary\u2026"),
                 QStringLiteral("Got notebook summary"),
                 QStringLiteral("\u2630") };

    // ── GitHub tools ─────────────────────────────────────────────────
    if (toolName == QLatin1String("github_issues"))
        return { QStringLiteral("GitHub Issues"),
                 QStringLiteral("Querying issues\u2026"),
                 QStringLiteral("Queried issues"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("github_pr"))
        return { QStringLiteral("GitHub PR"),
                 QStringLiteral("Querying pull requests\u2026"),
                 QStringLiteral("Queried pull requests"),
                 QStringLiteral("\u2630") };

    if (toolName == QLatin1String("github_code_search"))
        return { QStringLiteral("GitHub Code Search"),
                 QStringLiteral("Searching GitHub code\u2026"),
                 QStringLiteral("Searched GitHub code"),
                 QStringLiteral("Q") };

    if (toolName == QLatin1String("github_repo"))
        return { QStringLiteral("GitHub Repo"),
                 QStringLiteral("Querying repository\u2026"),
                 QStringLiteral("Queried repository"),
                 QStringLiteral("\u2630") };

    // ── Write / undo file tools ──────────────────────────────────────
    if (toolName == QLatin1String("write_file"))
        return { QStringLiteral("Write File"),
                 QStringLiteral("Writing file\u2026"),
                 QStringLiteral("Wrote file"),
                 QStringLiteral("\u270E") };

    if (toolName == QLatin1String("undo_file_edit"))
        return { QStringLiteral("Undo File Edit"),
                 QStringLiteral("Restoring file\u2026"),
                 QStringLiteral("Restored file"),
                 QStringLiteral("\u21B6") };

    // ── Docker ───────────────────────────────────────────────────────
    if (toolName == QLatin1String("docker"))
        return { QStringLiteral("Docker"),
                 QStringLiteral("Running Docker\u2026"),
                 QStringLiteral("Docker complete"),
                 QStringLiteral("\u2630") };

    // ── Default ──────────────────────────────────────────────────────
    return { toolName,
             QStringLiteral("Running %1\u2026").arg(toolName),
             QStringLiteral("Ran %1").arg(toolName),
             QStringLiteral("\u25B6") };
}

} // namespace ToolPresentation
