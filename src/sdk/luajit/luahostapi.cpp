#include "luahostapi.h"

#include "../ihostservices.h"
#include "../icommandservice.h"
#include "../ieditorservice.h"
#include "../inotificationservice.h"
#include "../iworkspaceservice.h"
#include "../igitservice.h"
#include "../idiagnosticsservice.h"

namespace luabridge {

const char *const LuaHostAPI::kHostKey = "exorcist_host_services";

// ── Helpers ──────────────────────────────────────────────────────────────────

void LuaHostAPI::storeHost(lua_State *L, IHostServices *host)
{
    lua_pushlightuserdata(L, host);
    lua_setfield(L, LUA_REGISTRYINDEX, kHostKey);
}

IHostServices *LuaHostAPI::getHost(lua_State *L)
{
    lua_getfield(L, LUA_REGISTRYINDEX, kHostKey);
    auto *host = static_cast<IHostServices *>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    return host;
}

void LuaHostAPI::pushQString(lua_State *L, const QString &str)
{
    if (str.isNull()) {
        lua_pushnil(L);
    } else {
        const QByteArray utf8 = str.toUtf8();
        lua_pushlstring(L, utf8.constData(), static_cast<size_t>(utf8.size()));
    }
}

QString LuaHostAPI::checkQString(lua_State *L, int idx)
{
    size_t len = 0;
    const char *s = luaL_checklstring(L, idx, &len);
    return QString::fromUtf8(s, static_cast<int>(len));
}

// ── Registration ─────────────────────────────────────────────────────────────

void LuaHostAPI::registerAll(lua_State *L, IHostServices *host, uint32_t perms)
{
    storeHost(L, host);

    // Create the root "ex" table.
    lua_newtable(L);

    if (hasPermission(perms, LuaPermission::Commands))
        registerCommands(L);

    if (hasPermission(perms, LuaPermission::EditorRead))
        registerEditor(L);

    if (hasPermission(perms, LuaPermission::Notifications))
        registerNotify(L);

    if (hasPermission(perms, LuaPermission::WorkspaceRead))
        registerWorkspace(L);

    if (hasPermission(perms, LuaPermission::GitRead))
        registerGit(L);

    if (hasPermission(perms, LuaPermission::DiagnosticsRead))
        registerDiagnostics(L);

    if (hasPermission(perms, LuaPermission::Logging))
        registerLog(L);

    if (hasPermission(perms, LuaPermission::Events))
        registerEvents(L);

    // Runtime/memory is always available — no permission needed.
    registerRuntime(L);

    lua_setglobal(L, "ex");
}

// ── Module builders ──────────────────────────────────────────────────────────
// Each creates a sub-table on the "ex" table (which is at stack top).

void LuaHostAPI::registerCommands(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_command_register);
    lua_setfield(L, -2, "register");
    lua_pushcfunction(L, l_command_execute);
    lua_setfield(L, -2, "execute");
    lua_setfield(L, -2, "commands");
}

void LuaHostAPI::registerEditor(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_editor_activeFile);
    lua_setfield(L, -2, "activeFile");
    lua_pushcfunction(L, l_editor_language);
    lua_setfield(L, -2, "language");
    lua_pushcfunction(L, l_editor_selectedText);
    lua_setfield(L, -2, "selectedText");
    lua_pushcfunction(L, l_editor_cursorLine);
    lua_setfield(L, -2, "cursorLine");
    lua_pushcfunction(L, l_editor_cursorColumn);
    lua_setfield(L, -2, "cursorColumn");
    lua_setfield(L, -2, "editor");
}

void LuaHostAPI::registerNotify(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_notify_info);
    lua_setfield(L, -2, "info");
    lua_pushcfunction(L, l_notify_warning);
    lua_setfield(L, -2, "warning");
    lua_pushcfunction(L, l_notify_error);
    lua_setfield(L, -2, "error");
    lua_pushcfunction(L, l_notify_status);
    lua_setfield(L, -2, "status");
    lua_setfield(L, -2, "notify");
}

void LuaHostAPI::registerWorkspace(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_workspace_root);
    lua_setfield(L, -2, "root");
    lua_pushcfunction(L, l_workspace_readFile);
    lua_setfield(L, -2, "readFile");
    lua_pushcfunction(L, l_workspace_exists);
    lua_setfield(L, -2, "exists");
    lua_setfield(L, -2, "workspace");
}

void LuaHostAPI::registerGit(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_git_isRepo);
    lua_setfield(L, -2, "isRepo");
    lua_pushcfunction(L, l_git_branch);
    lua_setfield(L, -2, "branch");
    lua_pushcfunction(L, l_git_diff);
    lua_setfield(L, -2, "diff");
    lua_setfield(L, -2, "git");
}

void LuaHostAPI::registerDiagnostics(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_diagnostics_errorCount);
    lua_setfield(L, -2, "errorCount");
    lua_pushcfunction(L, l_diagnostics_warningCount);
    lua_setfield(L, -2, "warningCount");
    lua_setfield(L, -2, "diagnostics");
}

void LuaHostAPI::registerLog(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_log_debug);
    lua_setfield(L, -2, "debug");
    lua_pushcfunction(L, l_log_info);
    lua_setfield(L, -2, "info");
    lua_pushcfunction(L, l_log_warning);
    lua_setfield(L, -2, "warning");
    lua_pushcfunction(L, l_log_error);
    lua_setfield(L, -2, "error");
    lua_setfield(L, -2, "log");
}

void LuaHostAPI::registerEvents(lua_State *L)
{
    // Create _event_handlers table in registry for storing callbacks
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "_event_handlers");

    lua_newtable(L);
    lua_pushcfunction(L, l_events_on);
    lua_setfield(L, -2, "on");
    lua_pushcfunction(L, l_events_off);
    lua_setfield(L, -2, "off");
    lua_setfield(L, -2, "events");
}

// ═════════════════════════════════════════════════════════════════════════════
// C function implementations
// ═════════════════════════════════════════════════════════════════════════════

// ── Commands ─────────────────────────────────────────────────────────────────

// ex.commands.register(id, title, callback)
int LuaHostAPI::l_command_register(lua_State *L)
{
    const QString id    = checkQString(L, 1);
    const QString title = checkQString(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);

    // Store the callback in the registry so it survives.
    lua_pushvalue(L, 3);
    const int callbackRef = luaL_ref(L, LUA_REGISTRYINDEX);

    auto *host = getHost(L);
    if (!host || !host->commands()) return 0;

    // Track the command ID in a registry table so reloadSinglePlugin()
    // can unregister all commands before lua_close().
    lua_getfield(L, LUA_REGISTRYINDEX, "_registered_commands");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "_registered_commands");
    }
    const int tblLen = static_cast<int>(lua_objlen(L, -1));
    pushQString(L, id);
    lua_rawseti(L, -2, tblLen + 1);
    lua_pop(L, 1);  // pop the table

    // Capture L and the ref for the C++ lambda.
    // Safe because reloadSinglePlugin() unregisters commands before lua_close().
    lua_State *capturedL = L;
    host->commands()->registerCommand(id, title, [capturedL, callbackRef]() {
        lua_rawgeti(capturedL, LUA_REGISTRYINDEX, callbackRef);
        if (lua_pcall(capturedL, 0, 0, 0) != 0) {
            qWarning("[LuaJIT] Command callback error: %s",
                     lua_tostring(capturedL, -1));
            lua_pop(capturedL, 1);
        }
    });

    return 0;
}

// ex.commands.execute(id) → bool
int LuaHostAPI::l_command_execute(lua_State *L)
{
    const QString id = checkQString(L, 1);
    auto *host = getHost(L);
    const bool ok = host && host->commands() && host->commands()->executeCommand(id);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// ── Editor (read-only) ──────────────────────────────────────────────────────

int LuaHostAPI::l_editor_activeFile(lua_State *L)
{
    auto *host = getHost(L);
    pushQString(L, host && host->editor() ? host->editor()->activeFilePath() : QString());
    return 1;
}

int LuaHostAPI::l_editor_language(lua_State *L)
{
    auto *host = getHost(L);
    pushQString(L, host && host->editor() ? host->editor()->activeLanguageId() : QString());
    return 1;
}

int LuaHostAPI::l_editor_selectedText(lua_State *L)
{
    auto *host = getHost(L);
    pushQString(L, host && host->editor() ? host->editor()->selectedText() : QString());
    return 1;
}

int LuaHostAPI::l_editor_cursorLine(lua_State *L)
{
    auto *host = getHost(L);
    lua_pushinteger(L, host && host->editor() ? host->editor()->cursorLine() : -1);
    return 1;
}

int LuaHostAPI::l_editor_cursorColumn(lua_State *L)
{
    auto *host = getHost(L);
    lua_pushinteger(L, host && host->editor() ? host->editor()->cursorColumn() : -1);
    return 1;
}

// ── Notifications ────────────────────────────────────────────────────────────

int LuaHostAPI::l_notify_info(lua_State *L)
{
    auto *host = getHost(L);
    if (host && host->notifications())
        host->notifications()->info(checkQString(L, 1));
    return 0;
}

int LuaHostAPI::l_notify_warning(lua_State *L)
{
    auto *host = getHost(L);
    if (host && host->notifications())
        host->notifications()->warning(checkQString(L, 1));
    return 0;
}

int LuaHostAPI::l_notify_error(lua_State *L)
{
    auto *host = getHost(L);
    if (host && host->notifications())
        host->notifications()->error(checkQString(L, 1));
    return 0;
}

int LuaHostAPI::l_notify_status(lua_State *L)
{
    auto *host = getHost(L);
    if (!host || !host->notifications()) return 0;
    const QString text = checkQString(L, 1);
    const int timeout  = luaL_optinteger(L, 2, 5000);
    host->notifications()->statusMessage(text, timeout);
    return 0;
}

// ── Workspace (read-only) ────────────────────────────────────────────────────

int LuaHostAPI::l_workspace_root(lua_State *L)
{
    auto *host = getHost(L);
    pushQString(L, host && host->workspace() ? host->workspace()->rootPath() : QString());
    return 1;
}

int LuaHostAPI::l_workspace_readFile(lua_State *L)
{
    auto *host = getHost(L);
    if (!host || !host->workspace()) {
        lua_pushnil(L);
        return 1;
    }
    const QString path = checkQString(L, 1);
    const QByteArray data = host->workspace()->readFile(path);
    if (data.isNull()) {
        lua_pushnil(L);
    } else {
        lua_pushlstring(L, data.constData(), static_cast<size_t>(data.size()));
    }
    return 1;
}

int LuaHostAPI::l_workspace_exists(lua_State *L)
{
    auto *host = getHost(L);
    const bool ok = host && host->workspace()
                    && host->workspace()->exists(checkQString(L, 1));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

// ── Git (read-only) ──────────────────────────────────────────────────────────

int LuaHostAPI::l_git_isRepo(lua_State *L)
{
    auto *host = getHost(L);
    lua_pushboolean(L, (host && host->git() && host->git()->isGitRepo()) ? 1 : 0);
    return 1;
}

int LuaHostAPI::l_git_branch(lua_State *L)
{
    auto *host = getHost(L);
    pushQString(L, host && host->git() ? host->git()->currentBranch() : QString());
    return 1;
}

int LuaHostAPI::l_git_diff(lua_State *L)
{
    auto *host = getHost(L);
    if (!host || !host->git()) { lua_pushnil(L); return 1; }
    const bool staged = lua_toboolean(L, 1) != 0;
    pushQString(L, host->git()->diff(staged));
    return 1;
}

// ── Diagnostics (read-only) ──────────────────────────────────────────────────

int LuaHostAPI::l_diagnostics_errorCount(lua_State *L)
{
    auto *host = getHost(L);
    lua_pushinteger(L, host && host->diagnostics()
                           ? host->diagnostics()->errorCount() : 0);
    return 1;
}

int LuaHostAPI::l_diagnostics_warningCount(lua_State *L)
{
    auto *host = getHost(L);
    lua_pushinteger(L, host && host->diagnostics()
                           ? host->diagnostics()->warningCount() : 0);
    return 1;
}

// ── Logging ──────────────────────────────────────────────────────────────────

int LuaHostAPI::l_log_debug(lua_State *L)
{
    const QString msg = checkQString(L, 1);
    qDebug("[LuaPlugin] %s", qUtf8Printable(msg));
    return 0;
}

int LuaHostAPI::l_log_info(lua_State *L)
{
    const QString msg = checkQString(L, 1);
    qInfo("[LuaPlugin] %s", qUtf8Printable(msg));
    return 0;
}

int LuaHostAPI::l_log_warning(lua_State *L)
{
    const QString msg = checkQString(L, 1);
    qWarning("[LuaPlugin] %s", qUtf8Printable(msg));
    return 0;
}

int LuaHostAPI::l_log_error(lua_State *L)
{
    const QString msg = checkQString(L, 1);
    qCritical("[LuaPlugin] %s", qUtf8Printable(msg));
    return 0;
}

// ── Events ───────────────────────────────────────────────────────────────────

// ex.events.on(name, callback) — BridgeFunctionAttach pattern from game server
int LuaHostAPI::l_events_on(lua_State *L)
{
    const QString name = checkQString(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    // Get or create the _event_handlers table
    lua_getfield(L, LUA_REGISTRYINDEX, "_event_handlers");

    // Store callback: _event_handlers[name] = callback
    lua_pushvalue(L, 2);
    const QByteArray key = name.toUtf8();
    lua_setfield(L, -2, key.constData());

    lua_pop(L, 1); // pop _event_handlers
    return 0;
}

// ex.events.off(name) — unsubscribe from event
int LuaHostAPI::l_events_off(lua_State *L)
{
    const QString name = checkQString(L, 1);

    lua_getfield(L, LUA_REGISTRYINDEX, "_event_handlers");
    lua_pushnil(L);
    const QByteArray key = name.toUtf8();
    lua_setfield(L, -2, key.constData());

    lua_pop(L, 1);
    return 0;
}

// ── Runtime / Memory ─────────────────────────────────────────────────────────

void LuaHostAPI::registerRuntime(lua_State *L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_runtime_memoryUsed);
    lua_setfield(L, -2, "memoryUsed");
    lua_pushcfunction(L, l_runtime_memoryLimit);
    lua_setfield(L, -2, "memoryLimit");
    lua_setfield(L, -2, "runtime");
}

// ex.runtime.memoryUsed() → int (bytes)
int LuaHostAPI::l_runtime_memoryUsed(lua_State *L)
{
    // The MemoryBudget* is the allocator userdata set via lua_newstate.
    // Retrieve it through the Lua alloc function mechanism.
    void *ud = nullptr;
    lua_getallocf(L, &ud);
    if (ud) {
        auto *budget = static_cast<luabridge::MemoryBudget *>(ud);
        lua_pushinteger(L, static_cast<lua_Integer>(budget->used));
    } else {
        lua_pushinteger(L, lua_gc(L, LUA_GCCOUNT, 0) * 1024);
    }
    return 1;
}

// ex.runtime.memoryLimit() → int (bytes)
int LuaHostAPI::l_runtime_memoryLimit(lua_State *L)
{
    void *ud = nullptr;
    lua_getallocf(L, &ud);
    if (ud) {
        auto *budget = static_cast<luabridge::MemoryBudget *>(ud);
        lua_pushinteger(L, static_cast<lua_Integer>(budget->limit));
    } else {
        lua_pushinteger(L, 0);
    }
    return 1;
}

} // namespace luabridge
