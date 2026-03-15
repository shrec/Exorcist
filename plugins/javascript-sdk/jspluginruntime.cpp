#include "jspluginruntime.h"
#include "jshostapi.h"

#include "sdk/ihostservices.h"
#include "sdk/icommandservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

namespace jssdk {

// ── Helpers ──────────────────────────────────────────────────────────────────

static JSStringRef toJSString(const QString &s)
{
    const QByteArray utf8 = s.toUtf8();
    return JSStringCreateWithUTF8CString(utf8.constData());
}

static QString fromJSString(JSContextRef ctx, JSStringRef jsStr)
{
    Q_UNUSED(ctx)
    const size_t maxLen = JSStringGetMaximumUTF8CStringSize(jsStr);
    QByteArray buf(static_cast<int>(maxLen), Qt::Uninitialized);
    JSStringGetUTF8CString(jsStr, buf.data(), maxLen);
    return QString::fromUtf8(buf.constData());
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

JsPluginRuntime::JsPluginRuntime(IHostServices *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
}

JsPluginRuntime::~JsPluginRuntime()
{
    shutdownAll();
}

// ── Permission parsing ───────────────────────────────────────────────────────

uint32_t JsPluginRuntime::parsePermissions(const QStringList &permStrings)
{
    uint32_t flags = 0;
    for (const QString &p : permStrings) {
        const QString key = p.trimmed().toLower();
        if (key == "commands")
            flags |= static_cast<uint32_t>(Permission::Commands);
        else if (key == "editor.read")
            flags |= static_cast<uint32_t>(Permission::EditorRead);
        else if (key == "notifications")
            flags |= static_cast<uint32_t>(Permission::Notifications);
        else if (key == "workspace.read")
            flags |= static_cast<uint32_t>(Permission::WorkspaceRead);
        else if (key == "git.read")
            flags |= static_cast<uint32_t>(Permission::GitRead);
        else if (key == "diagnostics.read")
            flags |= static_cast<uint32_t>(Permission::DiagnosticsRead);
        else if (key == "logging")
            flags |= static_cast<uint32_t>(Permission::Logging);
        else if (key == "events")
            flags |= static_cast<uint32_t>(Permission::Events);
    }
    return flags;
}

// ── Exception handling ───────────────────────────────────────────────────────

QString JsPluginRuntime::getExceptionString(JSContextRef ctx, JSValueRef exc)
{
    if (!exc || JSValueIsUndefined(ctx, exc))
        return QStringLiteral("(unknown error)");

    JSStringRef str = JSValueToStringCopy(ctx, exc, nullptr);
    if (!str)
        return QStringLiteral("(unknown error)");

    QString result = fromJSString(ctx, str);
    JSStringRelease(str);
    return result;
}

// ── Load plugins from directory ──────────────────────────────────────────────

int JsPluginRuntime::loadPluginsFrom(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) return 0;

    const QFileInfoList subdirs = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &subdir : subdirs) {
        const QString pluginDir    = subdir.absoluteFilePath();
        const QString manifestPath = pluginDir + "/plugin.json";

        if (!QFile::exists(manifestPath))
            continue;

        // Parse manifest
        QFile mf(manifestPath);
        if (!mf.open(QIODevice::ReadOnly)) {
            m_errors << tr("Cannot open %1").arg(manifestPath);
            continue;
        }

        const QJsonObject obj = QJsonDocument::fromJson(mf.readAll()).object();

        if (obj.value("id").toString().isEmpty()) {
            m_errors << tr("Missing 'id' in %1").arg(manifestPath);
            continue;
        }

        const QString type = obj.value("type").toString(QStringLiteral("headless"));

        if (type == QStringLiteral("html")) {
            loadHtmlPlugin(pluginDir, obj);
        } else {
            loadHeadlessPlugin(pluginDir, obj);
        }
    }

    return static_cast<int>(m_plugins.size() + m_htmlPlugins.size());
}

// ── Load a headless JS plugin (main.js, no HTML) ─────────────────────────────

void JsPluginRuntime::loadHeadlessPlugin(const QString &pluginDir,
                                          const QJsonObject &obj)
{
    const QString mainJs = pluginDir + QStringLiteral("/main.js");
    if (!QFile::exists(mainJs)) {
        m_errors << tr("Missing main.js in %1").arg(pluginDir);
        return;
    }

    JsPluginInfo info;
    info.id          = obj.value("id").toString();
    info.name        = obj.value("name").toString();
    info.version     = obj.value("version").toString("0.0.0");
    info.description = obj.value("description").toString();
    info.author      = obj.value("author").toString();
    info.entryScript = mainJs;

    const QJsonArray perms = obj.value("permissions").toArray();
    for (const QJsonValue &v : perms)
        info.permissions.append(v.toString());

    const uint32_t permFlags = parsePermissions(info.permissions);

    auto pctx = std::make_unique<PluginContext>();
    pctx->host = m_host;
    pctx->permissions = permFlags;
    pctx->pluginId = info.id;

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    if (!ctx) {
        m_errors << tr("Failed to create JSC context for %1").arg(info.id);
        return;
    }

    JsHostAPI::registerAll(ctx, pctx.get());

    QFile scriptFile(mainJs);
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        m_errors << tr("Cannot read %1").arg(mainJs);
        JSGlobalContextRelease(ctx);
        return;
    }
    const QByteArray script = scriptFile.readAll();

    JSStringRef jsScript = JSStringCreateWithUTF8CString(script.constData());
    JSStringRef jsSourceURL = toJSString(info.id);
    JSValueRef exception = nullptr;

    JSEvaluateScript(ctx, jsScript, nullptr, jsSourceURL, 0, &exception);
    JSStringRelease(jsScript);
    JSStringRelease(jsSourceURL);

    if (exception && !JSValueIsUndefined(ctx, exception)) {
        m_errors << tr("JS exec error (%1): %2")
                        .arg(info.id, getExceptionString(ctx, exception));
        JSGlobalContextRelease(ctx);
        return;
    }

    LoadedPlugin jp;
    jp.info        = info;
    jp.ctx         = ctx;
    jp.permissions = permFlags;
    jp.pctx        = std::move(pctx);
    m_plugins.push_back(std::move(jp));

    qInfo("[JsSDK] Loaded headless plugin: %s (%s)",
          qUtf8Printable(info.id), qUtf8Printable(info.name));
}

// ── Load an HTML plugin (index.html + ex.* SDK in WebView) ───────────────────

void JsPluginRuntime::loadHtmlPlugin(const QString &pluginDir,
                                      const QJsonObject &obj)
{
    const QString htmlEntry = obj.value("htmlEntry").toString(QStringLiteral("ui/index.html"));
    const QString htmlPath = pluginDir + QStringLiteral("/") + htmlEntry;

    if (!QFile::exists(htmlPath)) {
        m_errors << tr("Missing HTML entry %1 in %2").arg(htmlEntry, pluginDir);
        return;
    }

    HtmlPluginInfo info;
    info.id          = obj.value("id").toString();
    info.name        = obj.value("name").toString();
    info.version     = obj.value("version").toString("0.0.0");
    info.description = obj.value("description").toString();
    info.author      = obj.value("author").toString();
    info.htmlEntry   = htmlPath;

    const QJsonArray perms = obj.value("permissions").toArray();
    for (const QJsonValue &v : perms)
        info.permissions.append(v.toString());

    // Parse view contributions
    const QJsonArray viewsArr = obj.value("contributions").toObject()
                                   .value("views").toArray();
    for (const QJsonValue &vv : viewsArr) {
        const QJsonObject vo = vv.toObject();
        JsViewContribution vc;
        vc.id             = vo.value("id").toString();
        vc.title          = vo.value("title").toString();
        vc.location       = vo.value("location").toString(QStringLiteral("bottom"));
        vc.defaultVisible = vo.value("defaultVisible").toBool(false);
        if (!vc.id.isEmpty())
            info.views.append(vc);
    }

    const uint32_t permFlags = parsePermissions(info.permissions);

    auto pctx = std::make_unique<PluginContext>();
    pctx->host = m_host;
    pctx->permissions = permFlags;
    pctx->pluginId = info.id;

    LoadedHtmlPlugin hp;
    hp.info        = info;
    hp.permissions = permFlags;
    hp.pctx        = std::move(pctx);
    m_htmlPlugins.push_back(std::move(hp));

    qInfo("[JsSDK] Loaded HTML plugin: %s (%s)",
          qUtf8Printable(info.id), qUtf8Printable(info.name));
}

// ── Call a global function ───────────────────────────────────────────────────

bool JsPluginRuntime::callGlobalFunction(JSGlobalContextRef ctx,
                                          const char *name,
                                          const QString &pluginId)
{
    // Check if function exists before trying to call it
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSStringRef fnName = JSStringCreateWithUTF8CString(name);
    JSValueRef fnVal = JSObjectGetProperty(ctx, global, fnName, nullptr);
    JSStringRelease(fnName);

    if (!fnVal || !JSValueIsObject(ctx, fnVal))
        return true;  // not defined — that's OK

    JSObjectRef fn = JSValueToObject(ctx, fnVal, nullptr);
    if (!fn || !JSObjectIsFunction(ctx, fn))
        return true;

    // Use JSEvaluateScript to call the function — this properly sets up
    // the VM entry scope that Ultralight's JSC build requires.
    const QByteArray script = QStringLiteral("if (typeof %1 === 'function') %1();")
                                  .arg(QString::fromUtf8(name)).toUtf8();
    JSStringRef jsScript = JSStringCreateWithUTF8CString(script.constData());
    JSValueRef exception = nullptr;
    JSEvaluateScript(ctx, jsScript, nullptr, nullptr, 0, &exception);
    JSStringRelease(jsScript);

    if (exception && !JSValueIsUndefined(ctx, exception)) {
        m_errors << tr("JS %1 error (%2): %3")
                        .arg(QString::fromUtf8(name), pluginId,
                             getExceptionString(ctx, exception));
        return false;
    }
    return true;
}

// ── Initialize all ───────────────────────────────────────────────────────────

void JsPluginRuntime::initializeAll()
{
    for (LoadedPlugin &jp : m_plugins) {
        if (callGlobalFunction(jp.ctx, "initialize", jp.info.id))
            jp.initialized = true;
    }
}

// ── Shutdown all ─────────────────────────────────────────────────────────────

void JsPluginRuntime::shutdownAll()
{
    for (LoadedPlugin &jp : m_plugins) {
        if (jp.ctx) {
            callGlobalFunction(jp.ctx, "shutdown", jp.info.id);

            // Unregister commands before releasing context
            if (m_host && m_host->commands() && jp.pctx) {
                for (const QString &cmdId : std::as_const(jp.pctx->registeredCommandIds))
                    m_host->commands()->unregisterCommand(cmdId);
            }

            JSGlobalContextRelease(jp.ctx);
            jp.ctx = nullptr;
        }
    }
    m_plugins.clear();
}

// ── Hot reload ───────────────────────────────────────────────────────────────

bool JsPluginRuntime::reloadPlugin(const QString &pluginId)
{
    for (LoadedPlugin &jp : m_plugins) {
        if (jp.info.id == pluginId)
            return reloadSinglePlugin(jp);
    }
    m_errors << tr("Plugin not found for reload: %1").arg(pluginId);
    return false;
}

bool JsPluginRuntime::reloadSinglePlugin(LoadedPlugin &jp)
{
    if (jp.ctx && jp.initialized)
        callGlobalFunction(jp.ctx, "shutdown", jp.info.id);

    // Unregister commands before releasing context
    if (m_host && m_host->commands() && jp.pctx) {
        for (const QString &cmdId : std::as_const(jp.pctx->registeredCommandIds))
            m_host->commands()->unregisterCommand(cmdId);
        jp.pctx->registeredCommandIds.clear();
    }

    if (jp.ctx) {
        JSGlobalContextRelease(jp.ctx);
        jp.ctx = nullptr;
    }
    jp.initialized = false;

    // Recreate context
    auto pctx = std::make_unique<PluginContext>();
    pctx->host = m_host;
    pctx->permissions = jp.permissions;
    pctx->pluginId = jp.info.id;

    JSGlobalContextRef ctx = JSGlobalContextCreate(nullptr);
    if (!ctx) {
        m_errors << tr("Failed to recreate JSC context for %1").arg(jp.info.id);
        return false;
    }

    JsHostAPI::registerAll(ctx, pctx.get());

    QFile scriptFile(jp.info.entryScript);
    if (!scriptFile.open(QIODevice::ReadOnly)) {
        m_errors << tr("Cannot read %1").arg(jp.info.entryScript);
        JSGlobalContextRelease(ctx);
        return false;
    }
    const QByteArray script = scriptFile.readAll();

    JSStringRef jsScript = JSStringCreateWithUTF8CString(script.constData());
    JSStringRef jsSourceURL = toJSString(jp.info.id);
    JSValueRef exception = nullptr;

    JSEvaluateScript(ctx, jsScript, nullptr, jsSourceURL, 0, &exception);
    JSStringRelease(jsScript);
    JSStringRelease(jsSourceURL);

    if (exception && !JSValueIsUndefined(ctx, exception)) {
        m_errors << tr("JS reload error (%1): %2")
                        .arg(jp.info.id, getExceptionString(ctx, exception));
        JSGlobalContextRelease(ctx);
        return false;
    }

    jp.ctx = ctx;
    jp.pctx = std::move(pctx);

    if (callGlobalFunction(jp.ctx, "initialize", jp.info.id))
        jp.initialized = true;

    qInfo("[JsSDK] Reloaded plugin: %s", qUtf8Printable(jp.info.id));
    return true;
}

// ── Enable hot reload ────────────────────────────────────────────────────────

void JsPluginRuntime::enableHotReload(const QString &pluginsDir)
{
    m_pluginsDir = pluginsDir;

    QDir dir(pluginsDir);
    if (!dir.exists()) return;

    const QFileInfoList subdirs = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subdir : subdirs)
        m_watcher.addPath(subdir.absoluteFilePath());

    m_reloadDebounce.setSingleShot(true);
    m_reloadDebounce.setInterval(300);

    connect(&m_reloadDebounce, &QTimer::timeout, this, [this]() {
        for (const QString &pluginId : std::as_const(m_pendingReloads))
            reloadPlugin(pluginId);
        m_pendingReloads.clear();
    });

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString &path) {
        for (const LoadedPlugin &jp : std::as_const(m_plugins)) {
            const QString pluginDir = QFileInfo(jp.info.entryScript).absolutePath();
            if (path == pluginDir && !m_pendingReloads.contains(jp.info.id)) {
                m_pendingReloads.append(jp.info.id);
                qInfo("[JsSDK] Change detected in %s, scheduling reload...",
                      qUtf8Printable(jp.info.id));
            }
        }
        m_reloadDebounce.start();
    });
}

// ── Fire event ───────────────────────────────────────────────────────────────

void JsPluginRuntime::fireEvent(const QString &eventName, const QStringList &args)
{
    for (LoadedPlugin &jp : m_plugins) {
        if (!jp.ctx || !jp.initialized) continue;
        if (!hasPermission(jp.permissions, Permission::Events)) continue;

        // Build script: __eventHandlers["eventName"]("arg1", "arg2", ...)
        // Use JSEvaluateScript for proper VM entry scope.
        QString scriptStr = QStringLiteral(
            "(function() { var h = __eventHandlers[\"%1\"]; if (typeof h === 'function') h(")
            .arg(eventName);

        for (int i = 0; i < args.size(); ++i) {
            if (i > 0) scriptStr += QStringLiteral(",");
            // Escape backslashes and quotes in args
            QString escaped = args[i];
            escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
            escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
            escaped.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
            scriptStr += QStringLiteral("\"%1\"").arg(escaped);
        }
        scriptStr += QStringLiteral("); })()");

        JSStringRef jsScript = toJSString(scriptStr);
        JSValueRef exception = nullptr;
        JSEvaluateScript(jp.ctx, jsScript, nullptr, nullptr, 0, &exception);
        JSStringRelease(jsScript);

        if (exception && !JSValueIsUndefined(jp.ctx, exception)) {
            m_errors << tr("JS event error (%1, %2): %3")
                            .arg(jp.info.id, eventName,
                                 getExceptionString(jp.ctx, exception));
        }
    }
}

} // namespace jssdk
