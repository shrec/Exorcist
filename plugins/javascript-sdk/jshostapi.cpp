#include "jshostapi.h"

#include "sdk/ihostservices.h"
#include "sdk/icommandservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/inotificationservice.h"
#include "sdk/iworkspaceservice.h"
#include "sdk/igitservice.h"
#include "sdk/idiagnosticsservice.h"

namespace jssdk {

// ── Hidden global key for PluginContext pointer ──────────────────────────────

static const char *const kCtxKey = "__exorcist_pctx";

// ── Helpers ──────────────────────────────────────────────────────────────────

static JSStringRef toJSStr(const char *s)
{
    return JSStringCreateWithUTF8CString(s);
}

static JSStringRef toJSStr(const QString &s)
{
    const QByteArray utf8 = s.toUtf8();
    return JSStringCreateWithUTF8CString(utf8.constData());
}

void JsHostAPI::storeContext(JSContextRef ctx, PluginContext *pctx)
{
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSStringRef key = toJSStr(kCtxKey);

    // Store as an opaque pointer inside a JS number (int64 fits a pointer)
    JSValueRef val = JSValueMakeNumber(ctx, static_cast<double>(
        reinterpret_cast<uintptr_t>(pctx)));
    JSObjectSetProperty(ctx, global, key, val,
                        kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontEnum,
                        nullptr);
    JSStringRelease(key);
}

PluginContext *JsHostAPI::getContext(JSContextRef ctx)
{
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSStringRef key = toJSStr(kCtxKey);
    JSValueRef val = JSObjectGetProperty(ctx, global, key, nullptr);
    JSStringRelease(key);

    if (!val || JSValueIsUndefined(ctx, val))
        return nullptr;

    auto ptr = static_cast<uintptr_t>(JSValueToNumber(ctx, val, nullptr));
    return reinterpret_cast<PluginContext *>(ptr);
}

JSValueRef JsHostAPI::makeQString(JSContextRef ctx, const QString &str)
{
    if (str.isNull())
        return JSValueMakeNull(ctx);
    JSStringRef s = toJSStr(str);
    JSValueRef val = JSValueMakeString(ctx, s);
    JSStringRelease(s);
    return val;
}

QString JsHostAPI::toQString(JSContextRef ctx, JSValueRef val)
{
    if (!val || JSValueIsNull(ctx, val) || JSValueIsUndefined(ctx, val))
        return {};
    JSStringRef s = JSValueToStringCopy(ctx, val, nullptr);
    if (!s) return {};
    const size_t maxLen = JSStringGetMaximumUTF8CStringSize(s);
    QByteArray buf(static_cast<int>(maxLen), Qt::Uninitialized);
    JSStringGetUTF8CString(s, buf.data(), maxLen);
    JSStringRelease(s);
    return QString::fromUtf8(buf.constData());
}

// ── Helper to make a JS function and set it on an object ─────────────────────

static void setFunction(JSContextRef ctx, JSObjectRef obj, const char *name,
                        JSObjectCallAsFunctionCallback cb)
{
    JSStringRef fnName = toJSStr(name);
    JSObjectRef fn = JSObjectMakeFunctionWithCallback(ctx, fnName, cb);
    JSObjectSetProperty(ctx, obj, fnName, fn, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(fnName);
}

// ── Registration ─────────────────────────────────────────────────────────────

void JsHostAPI::registerAll(JSContextRef ctx, PluginContext *pctx)
{
    storeContext(ctx, pctx);

    // Create "ex" global object
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSObjectRef exObj = JSObjectMake(ctx, nullptr, nullptr);

    if (hasPermission(pctx->permissions, Permission::Commands))
        registerCommands(ctx, exObj);

    if (hasPermission(pctx->permissions, Permission::EditorRead))
        registerEditor(ctx, exObj);

    if (hasPermission(pctx->permissions, Permission::Notifications))
        registerNotify(ctx, exObj);

    if (hasPermission(pctx->permissions, Permission::WorkspaceRead))
        registerWorkspace(ctx, exObj);

    if (hasPermission(pctx->permissions, Permission::GitRead))
        registerGit(ctx, exObj);

    if (hasPermission(pctx->permissions, Permission::DiagnosticsRead))
        registerDiagnostics(ctx, exObj);

    if (hasPermission(pctx->permissions, Permission::Logging))
        registerLog(ctx, exObj);

    if (hasPermission(pctx->permissions, Permission::Events))
        registerEvents(ctx, exObj);

    JSStringRef exKey = toJSStr("ex");
    JSObjectSetProperty(ctx, global, exKey, exObj,
                        kJSPropertyAttributeReadOnly, nullptr);
    JSStringRelease(exKey);

    // Create __eventHandlers object for event system
    JSObjectRef handlers = JSObjectMake(ctx, nullptr, nullptr);
    JSStringRef hKey = toJSStr("__eventHandlers");
    JSObjectSetProperty(ctx, global, hKey, handlers,
                        kJSPropertyAttributeDontEnum, nullptr);
    JSStringRelease(hKey);
}

// ── Module builders ──────────────────────────────────────────────────────────

void JsHostAPI::registerCommands(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef cmds = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, cmds, "register", js_command_register);
    setFunction(ctx, cmds, "execute", js_command_execute);
    JSStringRef key = toJSStr("commands");
    JSObjectSetProperty(ctx, exObj, key, cmds, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

void JsHostAPI::registerEditor(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef ed = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, ed, "activeFile", js_editor_activeFile);
    setFunction(ctx, ed, "language", js_editor_language);
    setFunction(ctx, ed, "selectedText", js_editor_selectedText);
    setFunction(ctx, ed, "cursorLine", js_editor_cursorLine);
    setFunction(ctx, ed, "cursorColumn", js_editor_cursorColumn);
    JSStringRef key = toJSStr("editor");
    JSObjectSetProperty(ctx, exObj, key, ed, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

void JsHostAPI::registerNotify(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef n = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, n, "info", js_notify_info);
    setFunction(ctx, n, "warning", js_notify_warning);
    setFunction(ctx, n, "error", js_notify_error);
    setFunction(ctx, n, "status", js_notify_status);
    JSStringRef key = toJSStr("notify");
    JSObjectSetProperty(ctx, exObj, key, n, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

void JsHostAPI::registerWorkspace(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef ws = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, ws, "root", js_workspace_root);
    setFunction(ctx, ws, "readFile", js_workspace_readFile);
    setFunction(ctx, ws, "exists", js_workspace_exists);
    setFunction(ctx, ws, "listDir", js_workspace_listDir);
    setFunction(ctx, ws, "openFiles", js_workspace_openFiles);
    JSStringRef key = toJSStr("workspace");
    JSObjectSetProperty(ctx, exObj, key, ws, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

void JsHostAPI::registerGit(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef g = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, g, "isRepo", js_git_isRepo);
    setFunction(ctx, g, "branch", js_git_branch);
    setFunction(ctx, g, "diff", js_git_diff);
    JSStringRef key = toJSStr("git");
    JSObjectSetProperty(ctx, exObj, key, g, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

void JsHostAPI::registerDiagnostics(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef d = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, d, "errorCount", js_diagnostics_errorCount);
    setFunction(ctx, d, "warningCount", js_diagnostics_warningCount);
    JSStringRef key = toJSStr("diagnostics");
    JSObjectSetProperty(ctx, exObj, key, d, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

void JsHostAPI::registerLog(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef l = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, l, "debug", js_log_debug);
    setFunction(ctx, l, "info", js_log_info);
    setFunction(ctx, l, "warning", js_log_warning);
    setFunction(ctx, l, "error", js_log_error);
    JSStringRef key = toJSStr("log");
    JSObjectSetProperty(ctx, exObj, key, l, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

void JsHostAPI::registerEvents(JSContextRef ctx, JSObjectRef exObj)
{
    JSObjectRef ev = JSObjectMake(ctx, nullptr, nullptr);
    setFunction(ctx, ev, "on", js_events_on);
    setFunction(ctx, ev, "off", js_events_off);
    JSStringRef key = toJSStr("events");
    JSObjectSetProperty(ctx, exObj, key, ev, kJSPropertyAttributeNone, nullptr);
    JSStringRelease(key);
}

// ══════════════════════════════════════════════════════════════════════════════
// C function implementations
// ══════════════════════════════════════════════════════════════════════════════

// ── Commands ─────────────────────────────────────────────────────────────────

JSValueRef JsHostAPI::js_command_register(JSContextRef ctx, JSObjectRef /*function*/,
                                           JSObjectRef /*thisObj*/, size_t argc,
                                           const JSValueRef argv[], JSValueRef * /*exc*/)
{
    if (argc < 3) return JSValueMakeUndefined(ctx);

    auto *pctx = getContext(ctx);
    if (!pctx || !pctx->host || !pctx->host->commands())
        return JSValueMakeUndefined(ctx);

    const QString id    = toQString(ctx, argv[0]);
    const QString title = toQString(ctx, argv[1]);

    // Protect the callback from GC
    JSObjectRef callback = JSValueToObject(ctx, argv[2], nullptr);
    if (!callback || !JSObjectIsFunction(ctx, callback))
        return JSValueMakeUndefined(ctx);

    // Store callback in __exo_cmds[id] so we can invoke it via
    // JSEvaluateScript (which properly sets up the VM entry scope).
    // Direct JSObjectCallAsFunction from external C++ crashes in
    // Ultralight's JSC build because no VM entry scope exists.
    JSObjectRef global = JSContextGetGlobalObject(ctx);

    // Ensure __exo_cmds registry object exists
    JSStringRef regKey = toJSStr("__exo_cmds");
    JSValueRef regVal = JSObjectGetProperty(ctx, global, regKey, nullptr);
    JSObjectRef registry;
    if (!regVal || JSValueIsUndefined(ctx, regVal)) {
        registry = JSObjectMake(ctx, nullptr, nullptr);
        JSObjectSetProperty(ctx, global, regKey, registry,
                            kJSPropertyAttributeDontEnum, nullptr);
    } else {
        registry = JSValueToObject(ctx, regVal, nullptr);
    }
    JSStringRelease(regKey);

    JSStringRef cmdKey = toJSStr(id);
    JSObjectSetProperty(ctx, registry, cmdKey, callback,
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(cmdKey);

    JSGlobalContextRef capturedCtx = JSContextGetGlobalContext(ctx);
    JSGlobalContextRetain(capturedCtx);

    // Escape the id for use inside a JS string literal
    QString escaped = id;
    escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    escaped.replace(QLatin1Char('\''), QStringLiteral("\\'"));
    const QByteArray callScript =
        QStringLiteral("__exo_cmds['%1']()").arg(escaped).toUtf8();

    pctx->host->commands()->registerCommand(id, title,
        [capturedCtx, callScript]() {
            JSStringRef script = JSStringCreateWithUTF8CString(callScript.constData());
            JSValueRef exc = nullptr;
            JSEvaluateScript(capturedCtx, script, nullptr, nullptr, 0, &exc);
            JSStringRelease(script);
            if (exc && !JSValueIsUndefined(capturedCtx, exc)) {
                JSStringRef msg = JSValueToStringCopy(capturedCtx, exc, nullptr);
                if (msg) {
                    const size_t len = JSStringGetMaximumUTF8CStringSize(msg);
                    QByteArray buf(static_cast<int>(len), Qt::Uninitialized);
                    JSStringGetUTF8CString(msg, buf.data(), len);
                    qWarning("[JsSDK] Command callback error: %s", buf.constData());
                    JSStringRelease(msg);
                }
            }
        });

    pctx->registeredCommandIds.append(id);
    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_command_execute(JSContextRef ctx, JSObjectRef /*function*/,
                                          JSObjectRef /*thisObj*/, size_t argc,
                                          const JSValueRef argv[], JSValueRef * /*exc*/)
{
    if (argc < 1) return JSValueMakeBoolean(ctx, false);

    auto *pctx = getContext(ctx);
    const QString id = toQString(ctx, argv[0]);
    bool ok = pctx && pctx->host && pctx->host->commands()
              && pctx->host->commands()->executeCommand(id);
    return JSValueMakeBoolean(ctx, ok);
}

// ── Editor (read-only) ──────────────────────────────────────────────────────

JSValueRef JsHostAPI::js_editor_activeFile(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                            size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return makeQString(ctx, pctx && pctx->host && pctx->host->editor()
                                ? pctx->host->editor()->activeFilePath() : QString());
}

JSValueRef JsHostAPI::js_editor_language(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                          size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return makeQString(ctx, pctx && pctx->host && pctx->host->editor()
                                ? pctx->host->editor()->activeLanguageId() : QString());
}

JSValueRef JsHostAPI::js_editor_selectedText(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                              size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return makeQString(ctx, pctx && pctx->host && pctx->host->editor()
                                ? pctx->host->editor()->selectedText() : QString());
}

JSValueRef JsHostAPI::js_editor_cursorLine(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                            size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return JSValueMakeNumber(ctx, pctx && pctx->host && pctx->host->editor()
                                      ? pctx->host->editor()->cursorLine() : -1);
}

JSValueRef JsHostAPI::js_editor_cursorColumn(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                              size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return JSValueMakeNumber(ctx, pctx && pctx->host && pctx->host->editor()
                                      ? pctx->host->editor()->cursorColumn() : -1);
}

// ── Notifications ────────────────────────────────────────────────────────────

JSValueRef JsHostAPI::js_notify_info(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                      size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (pctx && pctx->host && pctx->host->notifications() && argc >= 1)
        pctx->host->notifications()->info(toQString(ctx, argv[0]));
    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_notify_warning(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                         size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (pctx && pctx->host && pctx->host->notifications() && argc >= 1)
        pctx->host->notifications()->warning(toQString(ctx, argv[0]));
    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_notify_error(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                       size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (pctx && pctx->host && pctx->host->notifications() && argc >= 1)
        pctx->host->notifications()->error(toQString(ctx, argv[0]));
    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_notify_status(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                        size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (!pctx || !pctx->host || !pctx->host->notifications() || argc < 1)
        return JSValueMakeUndefined(ctx);

    const QString text = toQString(ctx, argv[0]);
    int timeout = 5000;
    if (argc >= 2)
        timeout = static_cast<int>(JSValueToNumber(ctx, argv[1], nullptr));
    pctx->host->notifications()->statusMessage(text, timeout);
    return JSValueMakeUndefined(ctx);
}

// ── Workspace (read-only) ────────────────────────────────────────────────────

JSValueRef JsHostAPI::js_workspace_root(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                         size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return makeQString(ctx, pctx && pctx->host && pctx->host->workspace()
                                ? pctx->host->workspace()->rootPath() : QString());
}

JSValueRef JsHostAPI::js_workspace_readFile(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                             size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (!pctx || !pctx->host || !pctx->host->workspace() || argc < 1)
        return JSValueMakeNull(ctx);

    const QString path = toQString(ctx, argv[0]);
    const QByteArray data = pctx->host->workspace()->readFile(path);
    if (data.isNull()) return JSValueMakeNull(ctx);

    JSStringRef s = JSStringCreateWithUTF8CString(data.constData());
    JSValueRef val = JSValueMakeString(ctx, s);
    JSStringRelease(s);
    return val;
}

JSValueRef JsHostAPI::js_workspace_exists(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                           size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (!pctx || !pctx->host || !pctx->host->workspace() || argc < 1)
        return JSValueMakeBoolean(ctx, false);
    return JSValueMakeBoolean(ctx, pctx->host->workspace()->exists(toQString(ctx, argv[0])));
}

JSValueRef JsHostAPI::js_workspace_listDir(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                            size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (!pctx || !pctx->host || !pctx->host->workspace())
        return JSObjectMakeArray(ctx, 0, nullptr, nullptr);

    const QString path = argc >= 1 ? toQString(ctx, argv[0]) : QString();
    const QStringList entries = pctx->host->workspace()->listDirectory(path);

    std::vector<JSValueRef> jsEntries;
    jsEntries.reserve(static_cast<size_t>(entries.size()));
    for (const QString &e : entries)
        jsEntries.push_back(makeQString(ctx, e));

    return JSObjectMakeArray(ctx, jsEntries.size(), jsEntries.data(), nullptr);
}

JSValueRef JsHostAPI::js_workspace_openFiles(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                              size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (!pctx || !pctx->host || !pctx->host->workspace())
        return JSObjectMakeArray(ctx, 0, nullptr, nullptr);

    const QStringList files = pctx->host->workspace()->openFiles();
    std::vector<JSValueRef> jsFiles;
    jsFiles.reserve(static_cast<size_t>(files.size()));
    for (const QString &f : files)
        jsFiles.push_back(makeQString(ctx, f));

    return JSObjectMakeArray(ctx, jsFiles.size(), jsFiles.data(), nullptr);
}

// ── Git (read-only) ──────────────────────────────────────────────────────────

JSValueRef JsHostAPI::js_git_isRepo(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                     size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return JSValueMakeBoolean(ctx, pctx && pctx->host && pctx->host->git()
                                       && pctx->host->git()->isGitRepo());
}

JSValueRef JsHostAPI::js_git_branch(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                     size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return makeQString(ctx, pctx && pctx->host && pctx->host->git()
                                ? pctx->host->git()->currentBranch() : QString());
}

JSValueRef JsHostAPI::js_git_diff(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                   size_t argc, const JSValueRef argv[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    if (!pctx || !pctx->host || !pctx->host->git())
        return JSValueMakeNull(ctx);
    const bool staged = argc >= 1 && JSValueToBoolean(ctx, argv[0]);
    return makeQString(ctx, pctx->host->git()->diff(staged));
}

// ── Diagnostics (read-only) ──────────────────────────────────────────────────

JSValueRef JsHostAPI::js_diagnostics_errorCount(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                                 size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return JSValueMakeNumber(ctx, pctx && pctx->host && pctx->host->diagnostics()
                                      ? pctx->host->diagnostics()->errorCount() : 0);
}

JSValueRef JsHostAPI::js_diagnostics_warningCount(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                                   size_t, const JSValueRef[], JSValueRef *)
{
    auto *pctx = getContext(ctx);
    return JSValueMakeNumber(ctx, pctx && pctx->host && pctx->host->diagnostics()
                                      ? pctx->host->diagnostics()->warningCount() : 0);
}

// ── Logging ──────────────────────────────────────────────────────────────────

JSValueRef JsHostAPI::js_log_debug(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                    size_t argc, const JSValueRef argv[], JSValueRef *)
{
    if (argc >= 1)
        qDebug("[JsPlugin] %s", qUtf8Printable(toQString(ctx, argv[0])));
    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_log_info(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                   size_t argc, const JSValueRef argv[], JSValueRef *)
{
    if (argc >= 1)
        qInfo("[JsPlugin] %s", qUtf8Printable(toQString(ctx, argv[0])));
    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_log_warning(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                      size_t argc, const JSValueRef argv[], JSValueRef *)
{
    if (argc >= 1)
        qWarning("[JsPlugin] %s", qUtf8Printable(toQString(ctx, argv[0])));
    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_log_error(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                    size_t argc, const JSValueRef argv[], JSValueRef *)
{
    if (argc >= 1)
        qCritical("[JsPlugin] %s", qUtf8Printable(toQString(ctx, argv[0])));
    return JSValueMakeUndefined(ctx);
}

// ── Events ───────────────────────────────────────────────────────────────────

JSValueRef JsHostAPI::js_events_on(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                    size_t argc, const JSValueRef argv[], JSValueRef *)
{
    if (argc < 2) return JSValueMakeUndefined(ctx);

    const QString name = toQString(ctx, argv[0]);
    JSObjectRef callback = JSValueToObject(ctx, argv[1], nullptr);
    if (!callback || !JSObjectIsFunction(ctx, callback))
        return JSValueMakeUndefined(ctx);

    // Store callback in __eventHandlers[name]
    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSStringRef hKey = toJSStr("__eventHandlers");
    JSValueRef hVal = JSObjectGetProperty(ctx, global, hKey, nullptr);
    JSStringRelease(hKey);

    JSObjectRef handlers = JSValueToObject(ctx, hVal, nullptr);
    if (!handlers) return JSValueMakeUndefined(ctx);

    JSStringRef evtKey = toJSStr(name);
    JSObjectSetProperty(ctx, handlers, evtKey, argv[1],
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(evtKey);

    return JSValueMakeUndefined(ctx);
}

JSValueRef JsHostAPI::js_events_off(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                     size_t argc, const JSValueRef argv[], JSValueRef *)
{
    if (argc < 1) return JSValueMakeUndefined(ctx);

    const QString name = toQString(ctx, argv[0]);

    JSObjectRef global = JSContextGetGlobalObject(ctx);
    JSStringRef hKey = toJSStr("__eventHandlers");
    JSValueRef hVal = JSObjectGetProperty(ctx, global, hKey, nullptr);
    JSStringRelease(hKey);

    JSObjectRef handlers = JSValueToObject(ctx, hVal, nullptr);
    if (!handlers) return JSValueMakeUndefined(ctx);

    JSStringRef evtKey = toJSStr(name);
    JSObjectSetProperty(ctx, handlers, evtKey, JSValueMakeUndefined(ctx),
                        kJSPropertyAttributeNone, nullptr);
    JSStringRelease(evtKey);

    return JSValueMakeUndefined(ctx);
}

} // namespace jssdk
