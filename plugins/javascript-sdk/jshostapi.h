#pragma once
// ── JsHostAPI ────────────────────────────────────────────────────────────────
//
// Bridges IHostServices into a JSC global context as the `ex.*` API.
//
// Each registered function retrieves PluginContext* from a hidden global
// property, then calls the corresponding IHostServices method.
//
// Permission-gated: only api groups permitted by plugin.json are installed.
//
// JavaScript API surface:
//
//   ex.commands.register(id, title, callback)
//   ex.commands.execute(id) → bool
//
//   ex.editor.activeFile() → string|null
//   ex.editor.language() → string|null
//   ex.editor.selectedText() → string|null
//   ex.editor.cursorLine() → number
//   ex.editor.cursorColumn() → number
//
//   ex.notify.info(text)
//   ex.notify.warning(text)
//   ex.notify.error(text)
//   ex.notify.status(text, timeoutMs)
//
//   ex.workspace.root() → string|null
//   ex.workspace.readFile(path) → string|null
//   ex.workspace.exists(path) → bool
//   ex.workspace.listDir(path?) → string[]
//   ex.workspace.openFiles() → string[]
//
//   ex.git.isRepo() → bool
//   ex.git.branch() → string|null
//   ex.git.diff(staged) → string|null
//
//   ex.diagnostics.errorCount() → number
//   ex.diagnostics.warningCount() → number
//
//   ex.log.debug(msg)
//   ex.log.info(msg)
//   ex.log.warning(msg)
//   ex.log.error(msg)
//
//   ex.events.on(name, callback)
//   ex.events.off(name)

#include "jspluginruntime.h"   // PluginContext, Permission

#include <JavaScriptCore/JavaScript.h>

namespace jssdk {

class JsHostAPI
{
public:
    /// Register all permitted API namespaces into the JSC context.
    /// Works with both bare JSGlobalContextRef (headless) and
    /// JSContextRef from ulViewLockJSContext (WebView).
    static void registerAll(JSContextRef ctx, PluginContext *pctx);

private:
    // Store/retrieve PluginContext via hidden global property
    static void storeContext(JSContextRef ctx, PluginContext *pctx);
    static PluginContext *getContext(JSContextRef ctx);

    // Helpers
    static JSValueRef makeQString(JSContextRef ctx, const QString &str);
    static QString toQString(JSContextRef ctx, JSValueRef val);

    // Module registration
    static void registerCommands(JSContextRef ctx, JSObjectRef exObj);
    static void registerEditor(JSContextRef ctx, JSObjectRef exObj);
    static void registerNotify(JSContextRef ctx, JSObjectRef exObj);
    static void registerWorkspace(JSContextRef ctx, JSObjectRef exObj);
    static void registerGit(JSContextRef ctx, JSObjectRef exObj);
    static void registerDiagnostics(JSContextRef ctx, JSObjectRef exObj);
    static void registerLog(JSContextRef ctx, JSObjectRef exObj);
    static void registerEvents(JSContextRef ctx, JSObjectRef exObj);

    // JSC C function callbacks
    static JSValueRef js_command_register(JSContextRef ctx, JSObjectRef function,
                                          JSObjectRef thisObj, size_t argc,
                                          const JSValueRef argv[], JSValueRef *exc);
    static JSValueRef js_command_execute(JSContextRef ctx, JSObjectRef function,
                                         JSObjectRef thisObj, size_t argc,
                                         const JSValueRef argv[], JSValueRef *exc);

    static JSValueRef js_editor_activeFile(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                           size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_editor_language(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                         size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_editor_selectedText(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                             size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_editor_cursorLine(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                           size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_editor_cursorColumn(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                             size_t, const JSValueRef[], JSValueRef *);

    static JSValueRef js_notify_info(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                     size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_notify_warning(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                        size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_notify_error(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                      size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_notify_status(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                       size_t, const JSValueRef[], JSValueRef *);

    static JSValueRef js_workspace_root(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                        size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_workspace_readFile(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                            size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_workspace_exists(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                          size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_workspace_listDir(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                           size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_workspace_openFiles(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                             size_t, const JSValueRef[], JSValueRef *);

    static JSValueRef js_git_isRepo(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                    size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_git_branch(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                    size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_git_diff(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                  size_t, const JSValueRef[], JSValueRef *);

    static JSValueRef js_diagnostics_errorCount(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                                size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_diagnostics_warningCount(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                                  size_t, const JSValueRef[], JSValueRef *);

    static JSValueRef js_log_debug(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                   size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_log_info(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                  size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_log_warning(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                     size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_log_error(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                   size_t, const JSValueRef[], JSValueRef *);

    static JSValueRef js_events_on(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                   size_t, const JSValueRef[], JSValueRef *);
    static JSValueRef js_events_off(JSContextRef ctx, JSObjectRef, JSObjectRef,
                                    size_t, const JSValueRef[], JSValueRef *);
};

} // namespace jssdk
