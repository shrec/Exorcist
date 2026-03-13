#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>
#include <vector>

struct lua_State;
struct lua_Debug;
class IHostServices;

namespace luabridge {

// ── Per-plugin memory budget ─────────────────────────────────────────────────
//
// Passed as userdata to lua_newstate's custom allocator.
// Tracks current allocation and enforces a hard ceiling.

struct MemoryBudget
{
    size_t used    = 0;
    size_t limit   = 16 * 1024 * 1024;  // default 16 MB per plugin
    bool   oomHit  = false;             // set when limit is exceeded
};

// ── Lua Plugin Descriptor ────────────────────────────────────────────────────
//
// Populated from plugin.json next to the .lua entry script.

struct LuaPluginInfo
{
    QString id;           // "org.example.my-lua-plugin"
    QString name;         // "My Lua Plugin"
    QString version;      // "1.0.0"
    QString description;
    QString author;
    QString entryScript;  // absolute path to main .lua file
    QStringList permissions; // "commands", "editor.read", "notifications", ...
};

// ── Permission flags for Lua sandbox ─────────────────────────────────────────

enum class LuaPermission : uint32_t
{
    None             = 0,
    Commands         = 1 << 0,   // register/execute commands
    EditorRead       = 1 << 1,   // read cursor, selection, file path
    Notifications    = 1 << 2,   // info/warning/error/status
    WorkspaceRead    = 1 << 3,   // root, readFile, exists, listDir
    GitRead          = 1 << 4,   // branch, diff, isRepo
    DiagnosticsRead  = 1 << 5,   // error/warning count
    Logging          = 1 << 6,   // log.debug/info/warning/error
    Events           = 1 << 7,   // ex.events.on/off — subscribe to IDE events
};

inline uint32_t operator|(LuaPermission a, LuaPermission b) {
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}
inline bool hasPermission(uint32_t flags, LuaPermission p) {
    return (flags & static_cast<uint32_t>(p)) != 0;
}

// ── Loaded Lua Plugin ────────────────────────────────────────────────────────

struct LoadedLuaPlugin
{
    LuaPluginInfo info;
    lua_State    *L = nullptr;    // sandboxed state
    uint32_t      permissions = 0;
    bool          initialized = false;
    std::unique_ptr<MemoryBudget> memory = std::make_unique<MemoryBudget>();
    QStringList   registeredCommandIds;  // tracked for cleanup on hot reload
};

// ── LuaScriptEngine ─────────────────────────────────────────────────────────
//
// Manages sandboxed LuaJIT states for lightweight scripting plugins.
//
// Design constraints:
//   - Each plugin gets its own lua_State (no shared globals)
//   - FFI module is blocked
//   - io/os/loadfile/dofile/require are removed
//   - Instruction count hook prevents infinite loops
//   - Only read-only host services exposed (no write, no terminal, no AI)
//   - Permissions from plugin.json control which APIs are available

class LuaScriptEngine : public QObject
{
    Q_OBJECT

public:
    explicit LuaScriptEngine(IHostServices *host, QObject *parent = nullptr);
    ~LuaScriptEngine() override;

    /// Load all Lua plugins from a directory.
    /// Each subdirectory should contain plugin.json + main.lua.
    int loadPluginsFrom(const QString &path);

    /// Initialize all loaded plugins (call their initialize() if defined).
    void initializeAll();

    /// Shutdown all plugins and close lua_States.
    void shutdownAll();

    /// Hot-reload a single plugin by id (close state, re-read files, re-init).
    bool reloadPlugin(const QString &pluginId);

    /// Fire an event to all plugins that registered a callback via ex.events.on().
    /// Calls every Lua callback registered for `eventName`.
    void fireEvent(const QString &eventName, const QStringList &args = {});

    /// Enable QFileSystemWatcher on the plugins directory for hot reload.
    void enableHotReload(const QString &pluginsDir);

    /// Execute an ad-hoc Lua script in a fresh sandboxed state.
    /// Returns captured output, return value, error, and memory used.
    struct AdhocResult {
        bool    ok = false;
        QString output;          // captured print() output
        QString returnValue;     // tostring() of script return value
        QString error;
        int     memoryUsedBytes = 0;
    };
    AdhocResult executeAdhoc(const QString &script);

    /// Get errors encountered during loading/init.
    const QStringList &errors() const { return m_errors; }

    /// Get list of loaded plugins.
    const std::vector<LoadedLuaPlugin> &plugins() const { return m_plugins; }

private:
    /// Create a sandboxed lua_State with only whitelisted globals.
    /// Uses the custom allocator tied to the plugin's MemoryBudget.
    lua_State *createSandboxedState(uint32_t permissions, MemoryBudget *budget);

    /// Apply the sandbox: remove dangerous globals, set instruction hook.
    void applySandbox(lua_State *L, uint32_t permissions);

    /// Register built-in json.decode()/json.encode() module.
    static void registerJsonModule(lua_State *L);

    /// Register host API functions into the lua_State based on permissions.
    void registerHostAPI(lua_State *L, uint32_t permissions);

    /// Parse permission strings from plugin.json into flags.
    static uint32_t parsePermissions(const QStringList &permStrings);

    /// Instruction count hook — aborts runaway scripts.
    static void instructionHook(lua_State *L, lua_Debug *ar);

    /// Custom allocator with per-plugin memory limit.
    /// Signature matches lua_Alloc: void*(void *ud, void *ptr, size_t osize, size_t nsize)
    static void *limitedAlloc(void *ud, void *ptr, size_t osize, size_t nsize);

    /// Push a traceback error handler and return its stack index.
    static int pushTraceback(lua_State *L);

    /// Safe pcall with traceback (like game server's error handler pattern).
    int pcallWithTraceback(lua_State *L, int nargs, int nresults,
                           const QString &pluginId);

    /// Reload a single loaded plugin entry (close old state, re-load script).
    bool reloadSinglePlugin(LoadedLuaPlugin &lp);

    /// Directory where plugins live (for hot reload watcher).
    QString m_pluginsDir;

    IHostServices         *m_host;
    std::vector<LoadedLuaPlugin>  m_plugins;
    QStringList             m_errors;
    QFileSystemWatcher      m_watcher;
    QTimer                  m_reloadDebounce;
    QStringList             m_pendingReloads;

    /// Max instructions per Lua call (10 million ≈ ~50ms on modern CPU).
    static constexpr int kMaxInstructions = 10'000'000;
};

} // namespace luabridge
