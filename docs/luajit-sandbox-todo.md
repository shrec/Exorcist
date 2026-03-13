# LuaJIT Sandbox TODO

This document is based on the provided screenshots and tracks items to implement
or finalize for the LuaJIT sandbox API and wishlist.

## Core API (from screenshots)

1. ✅ `ex.workspace.readFile(path)` — agent tool: `read_file` (ReadFileTool).
2. ✅ `ex.workspace.root()` — agent tool: `get_editor_context`, `get_project_setup_info`.
3. ✅ `ex.editor.activeFile()` — agent tool: `get_editor_context` (EditorContextTool).
4. ✅ `ex.git.branch()` — agent tool: `git_status` (GitStatusTool).
5. ✅ `ex.json.decode()` / `ex.json.encode()` — agent tool: `json_parse_format` (JsonParseFormatTool).
6. ✅ `ex.http.get()` / `ex.http.post()` — agent tool: `http_request` (HttpRequestTool).
7. ✅ `ex.time.now()` — agent tool: `current_time` (CurrentTimeTool).
8. ✅ `ex.workspace.glob(pattern)` — agent tool: `file_search` (FileSearchTool).

Notes:
- Provide clear error messages for sandbox restrictions.
- Keep API names under `ex.*` namespace.

## Wishlist (from screenshots)

1. ✅ `run_cpp` / `compile_and_run` — agent tool: `compile_and_run` (CompileAndRunTool). Context: cpp/c.
2. ✅ `json_parse` / `json_format` — agent tool: `json_parse_format` (JsonParseFormatTool). Operations: parse/format/validate/query.
3. ✅ `file_content_hash` — agent tool: `file_content_hash` (FileHashTool). Algorithms: SHA256, MD5.
4. ✅ `archive` — agent tool: `archive` (ArchiveTool). Formats: zip, tar.gz. Operations: create/extract/list.
5. ✅ `environment_variables` — agent tool: `environment_variables` (EnvironmentVariablesTool). Operations: get/list/path.
6. ✅ `image_viewer` / `image_analyze` — agent tool: `image_info` (ImageInfoTool). Returns dimensions, format, depth, alpha, grayscale.
7. ✅ `tree_sitter_parse` — agent tool: `tree_sitter_parse` (TreeSitterParseTool). Callback-based, needs IDE integration.
8. ✅ `performance_profile` — agent tool: `performance_profile` (PerformanceProfileTool). Callback-based, needs profiler backend.
9. ✅ `generate_diagram` — agent tool: `generate_diagram` (GenerateDiagramTool). Callback-based, Mermaid/PlantUML.
10. ✅ `regex_test` — agent tool: `regex_test` (RegexTestTool). Operations: validate/match/matchAll/replace.
11. ✅ `symbol_documentation` — agent tool: `symbol_documentation` (SymbolDocTool). Callback-based, LSP hover.
12. ✅ `code_completion` — agent tool: `code_completion` (CodeCompletionTool). Callback-based, LSP completion.
13. ✅ `workspace_config` — agent tool: `workspace_config` (WorkspaceConfigTool). Operations: scan/read.
14. ✅ `create_patch_file` — agent tool: `create_patch_file` (CreatePatchTool). Operations: diff_files/diff_content/git_patch.

## Implementation Files

- `src/agent/tools/sandboxtools.h` — JsonParseFormatTool, CurrentTimeTool, RegexTestTool, EnvironmentVariablesTool, FileHashTool, WorkspaceConfigTool
- `src/agent/tools/devtools.h` — CompileAndRunTool, ArchiveTool, CreatePatchTool, ImageInfoTool, GenerateDiagramTool, TreeSitterParseTool, PerformanceProfileTool, SymbolDocTool, CodeCompletionTool
- `src/agent/agentplatformbootstrap.h` — Callbacks struct with new callback types
- `src/agent/agentplatformbootstrap.cpp` — Tool registration in registerCoreTools() + setWorkspaceRoot()

## Review Fixes (9/10 Audit)

1. ✅ **Hot-reload command cleanup (use-after-free)** — `l_command_register()` captured `lua_State*` in lambda; after `lua_close()` the pointer dangled. Fixed by tracking command IDs in Lua registry table (`_registered_commands`) and calling `unregisterCommand()` in both `reloadSinglePlugin()` and `shutdownAll()` before `lua_close()`.
   - `src/sdk/luajit/luascriptengine.h` — Added `QStringList registeredCommandIds` to `LoadedLuaPlugin`
   - `src/sdk/luajit/luahostapi.cpp` — `l_command_register()` tracks ID in `_registered_commands` registry table
   - `src/sdk/luajit/luascriptengine.cpp` — Cleanup in `reloadSinglePlugin()` and `shutdownAll()`

2. ✅ **Unit tests** — 29 QTest cases covering sandbox, plugin lifecycle, and command cleanup.
   - `tests/test_luascriptengine.cpp` — Mock `IHostServices`/`ICommandService`, tests for:
     - Ad-hoc execution (return, print capture, syntax/runtime errors, memory tracking)
     - Sandbox restrictions (os, io, dofile, loadstring, require, ffi, collectgarbage blocked)
     - Instruction limit (infinite loop abort)
     - Safe library availability (string, table, math)
     - Plugin loading (load, missing files, initialize, shutdown, permissions)
     - Command hot-reload cleanup (register, reload unregister, shutdown unregister)
     - Events, memory limit enforcement, plugin isolation, edge cases
   - `tests/CMakeLists.txt` — `test_luascriptengine` target linked against `libluajit`

