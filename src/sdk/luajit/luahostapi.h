#pragma once

/*
 * LuaHostAPI — Binds IHostServices into a sandboxed lua_State.
 *
 * Only read-only / lightweight operations are exposed. No write access to
 * editor, workspace, terminal, or AI provider registration. Gated by
 * permission flags from plugin.json.
 *
 * Lua API surface:
 *
 *   ex.commands.register(id, title, callback)
 *   ex.commands.execute(id) → bool
 *
 *   ex.editor.activeFile() → string|nil
 *   ex.editor.language() → string|nil
 *   ex.editor.selectedText() → string|nil
 *   ex.editor.cursorLine() → int
 *   ex.editor.cursorColumn() → int
 *
 *   ex.notify.info(text)
 *   ex.notify.warning(text)
 *   ex.notify.error(text)
 *   ex.notify.status(text, timeout_ms)
 *
 *   ex.workspace.root() → string|nil
 *   ex.workspace.readFile(path) → string|nil
 *   ex.workspace.exists(path) → bool
 *
 *   ex.git.isRepo() → bool
 *   ex.git.branch() → string|nil
 *   ex.git.diff(staged) → string|nil
 *
 *   ex.diagnostics.errorCount() → int
 *   ex.diagnostics.warningCount() → int
 *
 *   ex.log.debug(msg)
 *   ex.log.info(msg)
 *   ex.log.warning(msg)
 *   ex.log.error(msg)
 *
 *   ex.events.on(name, callback)   -- subscribe to IDE event
 *   ex.events.off(name)            -- unsubscribe from IDE event
 *
 *   ex.runtime.memoryUsed()  → int   -- bytes currently allocated
 *   ex.runtime.memoryLimit() → int   -- max bytes allowed
 */

#include "luascriptengine.h"  // LuaPermission, hasPermission

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <QString>
#include <QByteArray>
#include <functional>
#include <vector>

class IHostServices;

namespace luabridge {

class LuaHostAPI
{
public:
    /// Register all permitted API tables into the lua_State.
    static void registerAll(lua_State *L, IHostServices *host, uint32_t perms);

private:
    // ── Helpers ──────────────────────────────────────────────────────────

    /// Store IHostServices* in the registry so C functions can retrieve it.
    static void storeHost(lua_State *L, IHostServices *host);

    /// Retrieve IHostServices* from the registry.
    static IHostServices *getHost(lua_State *L);

    /// Push a QString onto the Lua stack (nil if empty).
    static void pushQString(lua_State *L, const QString &str);

    /// Get a string argument from Lua, converting to QString.
    static QString checkQString(lua_State *L, int idx);

    // ── Module registration ──────────────────────────────────────────────

    static void registerCommands(lua_State *L);
    static void registerEditor(lua_State *L);
    static void registerNotify(lua_State *L);
    static void registerWorkspace(lua_State *L);
    static void registerGit(lua_State *L);
    static void registerDiagnostics(lua_State *L);
    static void registerLog(lua_State *L);
    static void registerEvents(lua_State *L);
    static void registerRuntime(lua_State *L);

    // ── C function implementations ───────────────────────────────────────

    // Commands
    static int l_command_register(lua_State *L);
    static int l_command_execute(lua_State *L);

    // Editor (read-only)
    static int l_editor_activeFile(lua_State *L);
    static int l_editor_language(lua_State *L);
    static int l_editor_selectedText(lua_State *L);
    static int l_editor_cursorLine(lua_State *L);
    static int l_editor_cursorColumn(lua_State *L);

    // Notifications
    static int l_notify_info(lua_State *L);
    static int l_notify_warning(lua_State *L);
    static int l_notify_error(lua_State *L);
    static int l_notify_status(lua_State *L);

    // Workspace (read-only)
    static int l_workspace_root(lua_State *L);
    static int l_workspace_readFile(lua_State *L);
    static int l_workspace_exists(lua_State *L);

    // Git (read-only)
    static int l_git_isRepo(lua_State *L);
    static int l_git_branch(lua_State *L);
    static int l_git_diff(lua_State *L);

    // Diagnostics (read-only)
    static int l_diagnostics_errorCount(lua_State *L);
    static int l_diagnostics_warningCount(lua_State *L);

    // Logging
    static int l_log_debug(lua_State *L);
    static int l_log_info(lua_State *L);
    static int l_log_warning(lua_State *L);
    static int l_log_error(lua_State *L);

    // Events
    static int l_events_on(lua_State *L);
    static int l_events_off(lua_State *L);

    // Runtime / memory
    static int l_runtime_memoryUsed(lua_State *L);
    static int l_runtime_memoryLimit(lua_State *L);

    // ── Registry key for IHostServices* ──────────────────────────────────
    static const char *const kHostKey;
};

} // namespace luabridge
