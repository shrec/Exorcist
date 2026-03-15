#pragma once
// ── JsPluginRuntime ──────────────────────────────────────────────────────────
//
// Manages sandboxed JavaScriptCore contexts for lightweight JS plugins.
//
// Each JS plugin directory (plugin.json + main.js) gets its own JSC
// JSGlobalContext with the `ex.*` host API bridged to IHostServices.
//
// Uses the JavaScriptCore C API shipped with the Ultralight SDK — no
// separate WebView or DOM needed. Pure JavaScript execution only.

#include <QObject>
#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QStringList>
#include <QTimer>

#include <vector>
#include <memory>

#include <JavaScriptCore/JavaScript.h>

class IHostServices;

namespace jssdk {

// ── Plugin descriptor loaded from plugin.json ────────────────────────────────

struct JsPluginInfo
{
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
    QString entryScript;     // absolute path to main.js
    QStringList permissions;
};

// ── Permission flags ─────────────────────────────────────────────────────────

enum class Permission : uint32_t
{
    None             = 0,
    Commands         = 1 << 0,
    EditorRead       = 1 << 1,
    Notifications    = 1 << 2,
    WorkspaceRead    = 1 << 3,
    GitRead          = 1 << 4,
    DiagnosticsRead  = 1 << 5,
    Logging          = 1 << 6,
    Events           = 1 << 7,
};

inline uint32_t operator|(Permission a, Permission b) {
    return static_cast<uint32_t>(a) | static_cast<uint32_t>(b);
}
inline bool hasPermission(uint32_t flags, Permission p) {
    return (flags & static_cast<uint32_t>(p)) != 0;
}

// ── Per-plugin context data ──────────────────────────────────────────────────
// Stored as the JSGlobalContext's private data so C callbacks can retrieve it.

struct PluginContext
{
    IHostServices *host = nullptr;
    uint32_t       permissions = 0;
    QString        pluginId;
    QStringList    registeredCommandIds;
};

// ── View contribution from a JS plugin manifest ─────────────────────────────

struct JsViewContribution
{
    QString id;
    QString title;
    QString location;     // "left", "right", "bottom", "top"
    bool    defaultVisible = false;
};

// ── HTML plugin descriptor ───────────────────────────────────────────────────

struct HtmlPluginInfo
{
    QString     id;
    QString     name;
    QString     version;
    QString     description;
    QString     author;
    QString     htmlEntry;      // absolute path to index.html
    QStringList permissions;
    QList<JsViewContribution> views;
};

// ── Loaded HTML plugin ──────────────────────────────────────────────────────

struct LoadedHtmlPlugin
{
    HtmlPluginInfo              info;
    uint32_t                    permissions = 0;
    std::unique_ptr<PluginContext> pctx;
    // The view widget is created lazily by the plugin entry point
};

// ── Loaded plugin ────────────────────────────────────────────────────────────

struct LoadedPlugin
{
    JsPluginInfo        info;
    JSGlobalContextRef  ctx = nullptr;
    uint32_t            permissions = 0;
    bool                initialized = false;
    std::unique_ptr<PluginContext> pctx;
};

// ── Runtime engine ───────────────────────────────────────────────────────────

class JsPluginRuntime : public QObject
{
    Q_OBJECT

public:
    explicit JsPluginRuntime(IHostServices *host, QObject *parent = nullptr);
    ~JsPluginRuntime() override;

    /// Scan a directory for JS plugin subdirectories (plugin.json + main.js).
    int loadPluginsFrom(const QString &path);

    /// Call initialize() on all loaded plugins.
    void initializeAll();

    /// Call shutdown() and free all contexts.
    void shutdownAll();

    /// Hot-reload a single plugin by id.
    bool reloadPlugin(const QString &pluginId);

    /// Fire an event to all plugins that subscribed via ex.events.on().
    void fireEvent(const QString &eventName, const QStringList &args = {});

    /// Enable file watcher for hot reload.
    void enableHotReload(const QString &pluginsDir);

    /// Get error messages collected during load/init.
    const QStringList &errors() const { return m_errors; }

    /// Access loaded plugins (for testing).
    const std::vector<LoadedPlugin> &plugins() const { return m_plugins; }

    /// Access loaded HTML plugins.
    const std::vector<LoadedHtmlPlugin> &htmlPlugins() const { return m_htmlPlugins; }

private:
    static uint32_t parsePermissions(const QStringList &permStrings);

    bool callGlobalFunction(JSGlobalContextRef ctx, const char *name,
                            const QString &pluginId);

    static QString getExceptionString(JSContextRef ctx, JSValueRef exc);

    bool reloadSinglePlugin(LoadedPlugin &jp);

    void loadHeadlessPlugin(const QString &pluginDir, const QJsonObject &obj);
    void loadHtmlPlugin(const QString &pluginDir, const QJsonObject &obj);

    IHostServices                    *m_host;
    std::vector<LoadedPlugin>        m_plugins;
    std::vector<LoadedHtmlPlugin>    m_htmlPlugins;
    QStringList                      m_errors;
    QFileSystemWatcher           m_watcher;
    QTimer                       m_reloadDebounce;
    QStringList                  m_pendingReloads;
    QString                      m_pluginsDir;
};

} // namespace jssdk
