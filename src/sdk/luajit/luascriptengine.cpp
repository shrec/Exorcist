#include "luascriptengine.h"
#include "luahostapi.h"
#include "../ihostservices.h"
#include "../icommandservice.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>

#include <cstdlib>   // malloc, realloc, free
#include <cstring>   // memset
#include <ctime>     // clock()

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace luabridge {

// ── Custom allocator with memory cap ──────────────────────────────────────────
//
// lua_Alloc signature: void *(void *ud, void *ptr, size_t osize, size_t nsize)
//  - ptr==NULL, osize==type tag, nsize>0  → malloc(nsize)
//  - nsize==0                             → free(ptr)
//  - otherwise                            → realloc(ptr, nsize)
//
// We track `used` in MemoryBudget and refuse allocations that exceed `limit`.
// When the allocator returns NULL for a non-free request, LuaJIT internally
// raises LUA_ERRMEM which lua_pcall catches safely — the IDE never crashes.

void *LuaScriptEngine::limitedAlloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    auto *budget = static_cast<MemoryBudget *>(ud);

    // Free
    if (nsize == 0) {
        if (ptr) {
            budget->used -= osize;
            std::free(ptr);
        }
        return nullptr;
    }

    // Allocation or reallocation
    const size_t newUsed = budget->used - (ptr ? osize : 0) + nsize;
    if (newUsed > budget->limit) {
        budget->oomHit = true;
        qWarning("[LuaJIT] Memory limit exceeded: requested %zu bytes, "
                 "used %zu / %zu",
                 nsize, budget->used, budget->limit);
        return nullptr;  // LuaJIT raises LUA_ERRMEM internally
    }

    void *newPtr = ptr ? std::realloc(ptr, nsize) : std::malloc(nsize);
    if (newPtr) {
        budget->used = newUsed;
        budget->oomHit = false;
    }
    return newPtr;
}

// ── Instruction hook ─────────────────────────────────────────────────────────

void LuaScriptEngine::instructionHook(lua_State *L, lua_Debug * /*ar*/)
{
    luaL_error(L, "script exceeded instruction limit (%d)", kMaxInstructions);
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

LuaScriptEngine::LuaScriptEngine(IHostServices *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
}

LuaScriptEngine::~LuaScriptEngine()
{
    shutdownAll();
}

// ── Permission parsing ───────────────────────────────────────────────────────

uint32_t LuaScriptEngine::parsePermissions(const QStringList &permStrings)
{
    uint32_t flags = 0;

    for (const QString &p : permStrings) {
        const QString key = p.trimmed().toLower();
        if (key == "commands")
            flags |= static_cast<uint32_t>(LuaPermission::Commands);
        else if (key == "editor.read")
            flags |= static_cast<uint32_t>(LuaPermission::EditorRead);
        else if (key == "notifications")
            flags |= static_cast<uint32_t>(LuaPermission::Notifications);
        else if (key == "workspace.read")
            flags |= static_cast<uint32_t>(LuaPermission::WorkspaceRead);
        else if (key == "git.read")
            flags |= static_cast<uint32_t>(LuaPermission::GitRead);
        else if (key == "diagnostics.read")
            flags |= static_cast<uint32_t>(LuaPermission::DiagnosticsRead);
        else if (key == "logging")
            flags |= static_cast<uint32_t>(LuaPermission::Logging);
        else if (key == "events")
            flags |= static_cast<uint32_t>(LuaPermission::Events);
    }

    return flags;
}

// ── Built-in JSON module ─────────────────────────────────────────────────────
// Provides json.decode(str) → table and json.encode(tbl) → string inside the
// sandbox, so Lua plugins can work with structured data without leaving the VM.

// Forward: push a QJsonValue onto the Lua stack as a native Lua type.
static void pushJsonValue(lua_State *L, const QJsonValue &val)
{
    switch (val.type()) {
    case QJsonValue::Bool:
        lua_pushboolean(L, val.toBool() ? 1 : 0);
        break;
    case QJsonValue::Double:
        lua_pushnumber(L, val.toDouble());
        break;
    case QJsonValue::String: {
        const QByteArray utf8 = val.toString().toUtf8();
        lua_pushlstring(L, utf8.constData(), static_cast<size_t>(utf8.size()));
        break;
    }
    case QJsonValue::Array: {
        const QJsonArray arr = val.toArray();
        lua_createtable(L, arr.size(), 0);
        for (int i = 0; i < arr.size(); ++i) {
            pushJsonValue(L, arr[i]);
            lua_rawseti(L, -2, i + 1);  // Lua arrays are 1-based
        }
        break;
    }
    case QJsonValue::Object: {
        const QJsonObject obj = val.toObject();
        lua_createtable(L, 0, obj.size());
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QByteArray key = it.key().toUtf8();
            lua_pushlstring(L, key.constData(), static_cast<size_t>(key.size()));
            pushJsonValue(L, it.value());
            lua_rawset(L, -3);
        }
        break;
    }
    default:  // Null / Undefined
        lua_pushnil(L);
        break;
    }
}

// Forward: convert a Lua value at the given stack index to a QJsonValue.
static QJsonValue luaToJsonValue(lua_State *L, int idx, int depth = 0)
{
    if (depth > 64) return QJsonValue();   // prevent infinite recursion

    switch (lua_type(L, idx)) {
    case LUA_TBOOLEAN:
        return QJsonValue(lua_toboolean(L, idx) != 0);
    case LUA_TNUMBER:
        return QJsonValue(lua_tonumber(L, idx));
    case LUA_TSTRING:
        return QJsonValue(QString::fromUtf8(lua_tostring(L, idx)));
    case LUA_TTABLE: {
        // Detect array vs object: if all keys are consecutive integers → array
        const int absIdx = (idx > 0) ? idx : lua_gettop(L) + idx + 1;
        const int len = static_cast<int>(lua_objlen(L, absIdx));
        bool isArray = (len > 0);

        if (isArray) {
            // Verify it's a pure array (no non-integer keys)
            int count = 0;
            lua_pushnil(L);
            while (lua_next(L, absIdx) != 0) {
                lua_pop(L, 1);  // pop value, keep key
                ++count;
            }
            isArray = (count == len);
        }

        if (isArray) {
            QJsonArray arr;
            for (int i = 1; i <= len; ++i) {
                lua_rawgeti(L, absIdx, i);
                arr.append(luaToJsonValue(L, -1, depth + 1));
                lua_pop(L, 1);
            }
            return arr;
        } else {
            QJsonObject obj;
            lua_pushnil(L);
            while (lua_next(L, absIdx) != 0) {
                QString key;
                if (lua_type(L, -2) == LUA_TSTRING)
                    key = QString::fromUtf8(lua_tostring(L, -2));
                else if (lua_type(L, -2) == LUA_TNUMBER)
                    key = QString::number(lua_tonumber(L, -2));
                else {
                    lua_pop(L, 1);
                    continue;
                }
                obj[key] = luaToJsonValue(L, -1, depth + 1);
                lua_pop(L, 1);
            }
            return obj;
        }
    }
    default:
        return QJsonValue();
    }
}

void LuaScriptEngine::registerJsonModule(lua_State *L)
{
    lua_newtable(L);

    // json.decode(str) → table/value
    lua_pushcfunction(L, [](lua_State *Ls) -> int {
        size_t len = 0;
        const char *s = luaL_checklstring(Ls, 1, &len);
        const QByteArray data(s, static_cast<int>(len));

        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error != QJsonParseError::NoError) {
            lua_pushnil(Ls);
            const QByteArray msg = err.errorString().toUtf8();
            lua_pushlstring(Ls, msg.constData(), static_cast<size_t>(msg.size()));
            return 2;  // nil, errorString
        }

        if (doc.isArray())
            pushJsonValue(Ls, QJsonValue(doc.array()));
        else if (doc.isObject())
            pushJsonValue(Ls, QJsonValue(doc.object()));
        else
            lua_pushnil(Ls);

        return 1;
    });
    lua_setfield(L, -2, "decode");

    // json.encode(value [, pretty]) → string
    lua_pushcfunction(L, [](lua_State *Ls) -> int {
        luaL_checkany(Ls, 1);
        const bool pretty = lua_toboolean(Ls, 2) != 0;

        const QJsonValue val = luaToJsonValue(Ls, 1);
        QJsonDocument doc;
        if (val.isArray())
            doc.setArray(val.toArray());
        else if (val.isObject())
            doc.setObject(val.toObject());
        else {
            // Wrap scalar in array for valid JSON
            QJsonArray arr;
            arr.append(val);
            doc.setArray(arr);
        }

        const QByteArray out = doc.toJson(pretty ? QJsonDocument::Indented
                                                  : QJsonDocument::Compact);
        lua_pushlstring(Ls, out.constData(), static_cast<size_t>(out.size()));
        return 1;
    });
    lua_setfield(L, -2, "encode");

    lua_setglobal(L, "json");
}

// ── Sandbox creation ─────────────────────────────────────────────────────────

lua_State *LuaScriptEngine::createSandboxedState(uint32_t permissions,
                                                  MemoryBudget *budget)
{
    // Use our custom allocator so every byte is tracked and capped.
    lua_State *L = lua_newstate(limitedAlloc, budget);
    if (!L) return nullptr;

    // Open ONLY safe base libraries — no io, os, ffi, debug, package.
    luaopen_base(L);
    luaopen_table(L);
    luaopen_string(L);
    luaopen_math(L);
    luaopen_bit(L);

    applySandbox(L, permissions);
    registerHostAPI(L, permissions);

    return L;
}

void LuaScriptEngine::applySandbox(lua_State *L, uint32_t /*permissions*/)
{
    // ── Remove dangerous globals ─────────────────────────────────────────
    static const char *blocked[] = {
        "dofile", "loadfile", "load", "loadstring",
        "rawget", "rawset", "rawequal", "rawlen",
        "collectgarbage",
        "newproxy",
        "gcinfo",
        nullptr
    };

    for (int i = 0; blocked[i]; ++i) {
        lua_pushnil(L);
        lua_setglobal(L, blocked[i]);
    }

    // ── Ensure ffi / io / os / debug / package are nil ───────────────────
    static const char *blockedModules[] = {
        "ffi", "io", "os", "debug", "package", "jit",
        nullptr
    };
    for (int i = 0; blockedModules[i]; ++i) {
        lua_pushnil(L);
        lua_setglobal(L, blockedModules[i]);
    }

    // ── Block require ────────────────────────────────────────────────────
    lua_pushnil(L);
    lua_setglobal(L, "require");

    // ── Set instruction count hook ───────────────────────────────────────
    lua_sethook(L, instructionHook, LUA_MASKCOUNT, kMaxInstructions);

    // ── Provide os.clock() only (safe, no side effects) ──────────────────
    lua_newtable(L);
    lua_pushcfunction(L, [](lua_State *Ls) -> int {
        lua_pushnumber(Ls, static_cast<double>(std::clock()) / CLOCKS_PER_SEC);
        return 1;
    });
    lua_setfield(L, -2, "clock");
    lua_setglobal(L, "os");

    // ── Built-in json module (decode/encode) ─────────────────────────────
    registerJsonModule(L);
}

// ── Host API registration (delegated to luahostapi.h) ────────────────────────

void LuaScriptEngine::registerHostAPI(lua_State *L, uint32_t permissions)
{
    LuaHostAPI::registerAll(L, m_host, permissions);
}

// ── Load plugins from directory ──────────────────────────────────────────────

int LuaScriptEngine::loadPluginsFrom(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) return 0;

    const QFileInfoList subdirs = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &subdir : subdirs) {
        const QString pluginDir  = subdir.absoluteFilePath();
        const QString manifestPath = pluginDir + "/plugin.json";
        const QString mainLua      = pluginDir + "/main.lua";

        // Must have both plugin.json and main.lua.
        if (!QFile::exists(manifestPath) || !QFile::exists(mainLua))
            continue;

        // ── Parse manifest ───────────────────────────────────────────────
        QFile mf(manifestPath);
        if (!mf.open(QIODevice::ReadOnly)) {
            m_errors << QString("Cannot open %1").arg(manifestPath);
            continue;
        }

        const QJsonObject obj = QJsonDocument::fromJson(mf.readAll()).object();
        LuaPluginInfo info;
        info.id          = obj.value("id").toString();
        info.name        = obj.value("name").toString();
        info.version     = obj.value("version").toString("0.0.0");
        info.description = obj.value("description").toString();
        info.author      = obj.value("author").toString();
        info.entryScript = mainLua;

        const QJsonArray perms = obj.value("permissions").toArray();
        for (const QJsonValue &v : perms)
            info.permissions.append(v.toString());

        if (info.id.isEmpty()) {
            m_errors << QString("Missing 'id' in %1").arg(manifestPath);
            continue;
        }

        // ── Create sandboxed state with memory budget ─────────────────
        const uint32_t permFlags = parsePermissions(info.permissions);

        LoadedLuaPlugin lp;
        lp.info        = info;
        lp.permissions = permFlags;

        // Parse optional memoryLimit from manifest (in MB, default 16).
        const int memLimitMB = obj.value("memoryLimit").toInt(16);
        lp.memory->limit = static_cast<size_t>(memLimitMB) * 1024 * 1024;

        lua_State *L = createSandboxedState(permFlags, lp.memory.get());
        if (!L) {
            m_errors << QString("Failed to create Lua state for %1").arg(info.id);
            continue;
        }

        // ── Load the script ──────────────────────────────────────────────
        QFile scriptFile(mainLua);
        if (!scriptFile.open(QIODevice::ReadOnly)) {
            m_errors << QString("Cannot read %1").arg(mainLua);
            lua_close(L);
            continue;
        }
        const QByteArray script = scriptFile.readAll();

        const int loadErr = luaL_loadbuffer(L, script.constData(),
                                            static_cast<size_t>(script.size()),
                                            info.id.toUtf8().constData());
        if (loadErr != 0) {
            m_errors << QString("Lua load error (%1): %2")
                            .arg(info.id, lua_tostring(L, -1));
            lua_close(L);
            continue;
        }

        // Execute the script (defines globals like initialize/shutdown).
        if (lua_pcall(L, 0, 0, 0) != 0) {
            m_errors << QString("Lua exec error (%1): %2")
                            .arg(info.id, lua_tostring(L, -1));
            lua_close(L);
            continue;
        }

        lp.L = L;
        m_plugins.push_back(std::move(lp));

        qInfo("[LuaJIT] Loaded script plugin: %s (%s)",
              qUtf8Printable(info.id), qUtf8Printable(info.name));
    }

    return static_cast<int>(m_plugins.size());
}

// ── Initialize all ───────────────────────────────────────────────────────────

void LuaScriptEngine::initializeAll()
{
    for (LoadedLuaPlugin &lp : m_plugins) {
        lua_getglobal(lp.L, "initialize");
        if (lua_isfunction(lp.L, -1)) {
            if (lua_pcall(lp.L, 0, 0, 0) != 0) {
                m_errors << QString("Lua init error (%1): %2")
                                .arg(lp.info.id, lua_tostring(lp.L, -1));
                lua_pop(lp.L, 1);
                continue;
            }
            lp.initialized = true;
        } else {
            lua_pop(lp.L, 1);
            lp.initialized = true; // no initialize() is fine
        }
    }
}

// ── Shutdown all ─────────────────────────────────────────────────────────────

void LuaScriptEngine::shutdownAll()
{
    for (LoadedLuaPlugin &lp : m_plugins) {
        if (lp.L) {
            lua_getglobal(lp.L, "shutdown");
            if (lua_isfunction(lp.L, -1)) {
                if (lua_pcall(lp.L, 0, 0, 0) != 0)
                    lua_pop(lp.L, 1); // discard error
            } else {
                lua_pop(lp.L, 1);
            }

            // Unregister commands before closing the state
            if (m_host && m_host->commands()) {
                lua_getfield(lp.L, LUA_REGISTRYINDEX, "_registered_commands");
                if (lua_istable(lp.L, -1)) {
                    const int len = static_cast<int>(lua_objlen(lp.L, -1));
                    for (int i = 1; i <= len; ++i) {
                        lua_rawgeti(lp.L, -1, i);
                        if (lua_isstring(lp.L, -1)) {
                            const QString cmdId = QString::fromUtf8(lua_tostring(lp.L, -1));
                            m_host->commands()->unregisterCommand(cmdId);
                        }
                        lua_pop(lp.L, 1);
                    }
                }
                lua_pop(lp.L, 1);
            }

            lua_close(lp.L);
            lp.L = nullptr;
        }
    }
    m_plugins.clear();
}

// ── Traceback error handler ──────────────────────────────────────────────────
// Inspired by the game server's error handler pattern (CScriptLoader).
// Pushes a traceback function onto the stack and returns its index.

int LuaScriptEngine::pushTraceback(lua_State *L)
{
    lua_pushcfunction(L, [](lua_State *Ls) -> int {
        const char *msg = lua_tostring(Ls, 1);
        if (msg)
            luaL_traceback(Ls, Ls, msg, 1);
        else
            lua_pushliteral(Ls, "(no error message)");
        return 1;
    });
    return lua_gettop(L);
}

int LuaScriptEngine::pcallWithTraceback(lua_State *L, int nargs, int nresults,
                                         const QString &pluginId)
{
    const int errIdx = lua_gettop(L) - nargs;
    lua_pushcfunction(L, [](lua_State *Ls) -> int {
        const char *msg = lua_tostring(Ls, 1);
        if (msg)
            luaL_traceback(Ls, Ls, msg, 1);
        else
            lua_pushliteral(Ls, "(no error message)");
        return 1;
    });
    lua_insert(L, errIdx);  // move error handler below the function

    const int result = lua_pcall(L, nargs, nresults, errIdx);
    lua_remove(L, errIdx);  // remove error handler

    if (result != 0) {
        const char *err = lua_tostring(L, -1);
        m_errors << QString("Lua error (%1): %2")
                        .arg(pluginId, err ? QString::fromUtf8(err) : QStringLiteral("(unknown)"));
        qWarning("[LuaJIT] %s: %s", qUtf8Printable(pluginId),
                 err ? err : "(unknown)");
        lua_pop(L, 1);
    }

    return result;
}

// ── Hot reload ───────────────────────────────────────────────────────────────

bool LuaScriptEngine::reloadPlugin(const QString &pluginId)
{
    for (LoadedLuaPlugin &lp : m_plugins) {
        if (lp.info.id == pluginId)
            return reloadSinglePlugin(lp);
    }
    m_errors << QString("Plugin not found for reload: %1").arg(pluginId);
    return false;
}

bool LuaScriptEngine::reloadSinglePlugin(LoadedLuaPlugin &lp)
{
    // Call shutdown() if the plugin was initialized.
    if (lp.L && lp.initialized) {
        lua_getglobal(lp.L, "shutdown");
        if (lua_isfunction(lp.L, -1))
            lua_pcall(lp.L, 0, 0, 0);
        else
            lua_pop(lp.L, 1);
    }

    // ── Unregister commands BEFORE closing the state ─────────────────
    // Commands registered via ex.commands.register() hold lambdas that
    // capture the lua_State*. We must remove them before lua_close()
    // to avoid use-after-free when the command is invoked later.
    if (lp.L && m_host && m_host->commands()) {
        // Read command IDs from the _registered_commands registry table
        lua_getfield(lp.L, LUA_REGISTRYINDEX, "_registered_commands");
        if (lua_istable(lp.L, -1)) {
            const int len = static_cast<int>(lua_objlen(lp.L, -1));
            for (int i = 1; i <= len; ++i) {
                lua_rawgeti(lp.L, -1, i);
                if (lua_isstring(lp.L, -1)) {
                    const QString cmdId = QString::fromUtf8(lua_tostring(lp.L, -1));
                    m_host->commands()->unregisterCommand(cmdId);
                }
                lua_pop(lp.L, 1);
            }
        }
        lua_pop(lp.L, 1);
    }
    lp.registeredCommandIds.clear();

    // Close old state (frees all memory through our custom allocator).
    if (lp.L) {
        lua_close(lp.L);
        lp.L = nullptr;
    }

    // Reset memory tracking.
    const size_t savedLimit = lp.memory->limit;
    *lp.memory = MemoryBudget{};
    lp.memory->limit = savedLimit;
    lp.initialized = false;

    // Re-create sandboxed state.
    lua_State *L = createSandboxedState(lp.permissions, lp.memory.get());
    if (!L) {
        m_errors << QString("Failed to recreate Lua state for %1").arg(lp.info.id);
        return false;
    }

    // Re-load the script file.
    QFile scriptFile(lp.info.entryScript);
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        m_errors << QString("Cannot read %1").arg(lp.info.entryScript);
        lua_close(L);
        return false;
    }
    const QByteArray script = scriptFile.readAll();

    if (luaL_loadbuffer(L, script.constData(),
                        static_cast<size_t>(script.size()),
                        lp.info.id.toUtf8().constData()) != 0) {
        m_errors << QString("Lua reload load error (%1): %2")
                        .arg(lp.info.id, lua_tostring(L, -1));
        lua_close(L);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != 0) {
        m_errors << QString("Lua reload exec error (%1): %2")
                        .arg(lp.info.id, lua_tostring(L, -1));
        lua_close(L);
        return false;
    }

    lp.L = L;

    // Call initialize() on the reloaded plugin.
    lua_getglobal(L, "initialize");
    if (lua_isfunction(L, -1)) {
        if (pcallWithTraceback(L, 0, 0, lp.info.id) == 0)
            lp.initialized = true;
    } else {
        lua_pop(L, 1);
        lp.initialized = true;
    }

    qInfo("[LuaJIT] Reloaded plugin: %s", qUtf8Printable(lp.info.id));
    return true;
}

void LuaScriptEngine::enableHotReload(const QString &pluginsDir)
{
    m_pluginsDir = pluginsDir;

    // Watch the plugins directory recursively for .lua file changes.
    QDir dir(pluginsDir);
    if (!dir.exists()) return;

    const QFileInfoList subdirs = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subdir : subdirs)
        m_watcher.addPath(subdir.absoluteFilePath());

    // Debounce: wait 300ms after last change before reloading.
    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(300);

    connect(&m_reloadDebounce, &QTimer::timeout, this, [this]() {
        for (const QString &pluginId : std::as_const(m_pendingReloads))
            reloadPlugin(pluginId);
        m_pendingReloads.clear();
    });

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &path) {
        // Find which plugin lives in this directory.
        for (const LoadedLuaPlugin &lp : std::as_const(m_plugins)) {
            const QString pluginDir = QFileInfo(lp.info.entryScript).absolutePath();
            if (path == pluginDir && !m_pendingReloads.contains(lp.info.id)) {
                m_pendingReloads.append(lp.info.id);
                qInfo("[LuaJIT] Change detected in %s, scheduling reload...",
                      qUtf8Printable(lp.info.id));
            }
        }
        m_reloadDebounce.start();
    });
}

// ── Fire event to all plugins ────────────────────────────────────────────────
// Similar to game server's BridgeFunction dispatch pattern, but per-plugin
// isolated states with traceback error handling.

void LuaScriptEngine::fireEvent(const QString &eventName, const QStringList &args)
{
    const QByteArray eventKey = eventName.toUtf8();

    for (LoadedLuaPlugin &lp : m_plugins) {
        if (!lp.L || !lp.initialized) continue;
        if (!hasPermission(lp.permissions, LuaPermission::Events)) continue;

        // Look up _event_handlers[eventName]
        lua_getfield(lp.L, LUA_REGISTRYINDEX, "_event_handlers");
        if (!lua_istable(lp.L, -1)) {
            lua_pop(lp.L, 1);
            continue;
        }

        lua_getfield(lp.L, -1, eventKey.constData());
        if (!lua_isfunction(lp.L, -1)) {
            lua_pop(lp.L, 2); // handler table + nil
            continue;
        }

        // Remove the handler table from under the function.
        lua_remove(lp.L, -2);

        // Push args.
        for (const QString &arg : args) {
            const QByteArray utf8 = arg.toUtf8();
            lua_pushlstring(lp.L, utf8.constData(), static_cast<size_t>(utf8.size()));
        }

        pcallWithTraceback(lp.L, args.size(), 0, lp.info.id);
    }
}

// ── Ad-hoc script execution ──────────────────────────────────────────────────
// Creates a temporary sandboxed state, overrides print() to capture output,
// runs the script, and tears everything down.

LuaScriptEngine::AdhocResult LuaScriptEngine::executeAdhoc(const QString &script)
{
    AdhocResult result;

    // Create a temporary memory budget (64 MB for agent use).
    auto budget = std::make_unique<MemoryBudget>();
    budget->limit = 64 * 1024 * 1024;

    // Create a fresh sandboxed state with read-only host API permissions
    // so agent scripts can use ex.workspace.readFile(), ex.editor.*, etc.
    constexpr uint32_t adhocPerms =
        static_cast<uint32_t>(LuaPermission::EditorRead) |
        static_cast<uint32_t>(LuaPermission::Notifications) |
        static_cast<uint32_t>(LuaPermission::WorkspaceRead) |
        static_cast<uint32_t>(LuaPermission::GitRead) |
        static_cast<uint32_t>(LuaPermission::DiagnosticsRead) |
        static_cast<uint32_t>(LuaPermission::Logging);
    lua_State *L = createSandboxedState(adhocPerms, budget.get());
    if (!L) {
        result.error = QStringLiteral("Failed to create Lua state.");
        return result;
    }

    // ── Override print() to capture output ───────────────────────────────
    // Store a string buffer in the registry for the print function.
    auto outputBuf = std::make_unique<QString>();
    lua_pushlightuserdata(L, outputBuf.get());
    lua_setfield(L, LUA_REGISTRYINDEX, "_adhoc_output");

    lua_pushcfunction(L, [](lua_State *Ls) -> int {
        lua_getfield(Ls, LUA_REGISTRYINDEX, "_adhoc_output");
        auto *buf = static_cast<QString *>(lua_touserdata(Ls, -1));
        lua_pop(Ls, 1);
        if (!buf) return 0;

        const int n = lua_gettop(Ls);
        for (int i = 1; i <= n; ++i) {
            if (i > 1) *buf += QLatin1Char('\t');
            const char *s = lua_tostring(Ls, i);
            *buf += s ? QString::fromUtf8(s) : QStringLiteral("nil");
        }
        *buf += QLatin1Char('\n');
        return 0;
    });
    lua_setglobal(L, "print");

    // ── Load and execute script ──────────────────────────────────────────
    const QByteArray scriptUtf8 = script.toUtf8();
    const int loadErr = luaL_loadbuffer(L, scriptUtf8.constData(),
                                        static_cast<size_t>(scriptUtf8.size()),
                                        "=adhoc");
    if (loadErr != 0) {
        result.error = QString::fromUtf8(lua_tostring(L, -1));
        lua_close(L);
        return result;
    }

    // Push traceback handler below the chunk.
    const int base = lua_gettop(L);
    lua_pushcfunction(L, [](lua_State *Ls) -> int {
        const char *msg = lua_tostring(Ls, 1);
        if (msg)
            luaL_traceback(Ls, Ls, msg, 1);
        else
            lua_pushliteral(Ls, "(no error message)");
        return 1;
    });
    lua_insert(L, base);  // move error handler below chunk

    const int execErr = lua_pcall(L, 0, 1, base);
    lua_remove(L, base);  // remove error handler

    if (execErr != 0) {
        result.error = QString::fromUtf8(lua_tostring(L, -1));
        lua_pop(L, 1);
    } else {
        result.ok = true;
        // Capture return value.
        if (!lua_isnil(L, -1)) {
            const char *rv = lua_tostring(L, -1);
            if (rv) result.returnValue = QString::fromUtf8(rv);
        }
        lua_pop(L, 1);
    }

    result.output = *outputBuf;
    if (result.output.endsWith(QLatin1Char('\n')))
        result.output.chop(1);
    result.memoryUsedBytes = static_cast<int>(budget->used);

    lua_close(L);
    return result;
}

} // namespace luabridge
