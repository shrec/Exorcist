/*
 * exorcist_plugin_api.h — Exorcist IDE Plugin C ABI (v1.0)
 *
 * This header defines the stable C ABI for Exorcist plugins.
 * Plugins compiled against this ABI are binary-compatible across
 * compiler versions and do not require Qt or any C++ runtime.
 *
 * String ownership:
 *   - Input (const char*): Caller-owned. Callee reads, never frees.
 *   - Output (char*):      Host-allocated via its own allocator.
 *                           Caller MUST free via api->free_string().
 *
 * Threading:
 *   - ex_plugin_initialize / ex_plugin_shutdown: main thread only.
 *   - Host service calls (editor_*, workspace_*, ...): main thread only.
 *   - AI send_request / cancel_request: may be called from any thread.
 *   - AI response callbacks (ai_response_*): may be called from any thread.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Version ───────────────────────────────────────────────────────────────── */

#define EX_ABI_VERSION_MAJOR  1
#define EX_ABI_VERSION_MINOR  0
#define EX_ABI_VERSION        ((EX_ABI_VERSION_MAJOR << 16) | EX_ABI_VERSION_MINOR)

/* ── Export macro ──────────────────────────────────────────────────────────── */

#ifndef EX_EXPORT
#  ifdef _WIN32
#    define EX_EXPORT __declspec(dllexport)
#  else
#    define EX_EXPORT __attribute__((visibility("default")))
#  endif
#endif

/* ── Basic types ───────────────────────────────────────────────────────────── */

/** Owned byte buffer returned by host functions. Free with api->free_buffer(). */
typedef struct ExBuffer {
    const uint8_t *data;
    size_t         length;
} ExBuffer;

/* ── Enums ─────────────────────────────────────────────────────────────────── */

typedef enum {
    EX_PERM_FILESYSTEM_READ   = 0,
    EX_PERM_FILESYSTEM_WRITE  = 1,
    EX_PERM_TERMINAL_EXECUTE  = 2,
    EX_PERM_GIT_READ          = 3,
    EX_PERM_GIT_WRITE         = 4,
    EX_PERM_NETWORK_ACCESS    = 5,
    EX_PERM_DIAGNOSTICS_READ  = 6,
    EX_PERM_WORKSPACE_READ    = 7,
    EX_PERM_WORKSPACE_WRITE   = 8,
    EX_PERM_AGENT_TOOL_INVOKE = 9,
} ExPermission;

typedef enum {
    EX_CAP_CHAT              = 1 << 0,
    EX_CAP_INLINE_COMPLETION = 1 << 1,
    EX_CAP_STREAMING         = 1 << 2,
    EX_CAP_TOOL_CALLING      = 1 << 3,
    EX_CAP_CODE_EDIT         = 1 << 4,
    EX_CAP_TEST_GENERATION   = 1 << 5,
    EX_CAP_VISION            = 1 << 6,
    EX_CAP_THINKING          = 1 << 7,
} ExAgentCapability;

typedef enum {
    EX_INTENT_CHAT = 0,
    EX_INTENT_EXPLAIN_CODE,
    EX_INTENT_REFACTOR_SELECTION,
    EX_INTENT_GENERATE_TESTS,
    EX_INTENT_FIX_DIAGNOSTIC,
    EX_INTENT_SUGGEST_COMMIT_MSG,
    EX_INTENT_TERMINAL_ASSIST,
    EX_INTENT_CREATE_FILE,
    EX_INTENT_CODE_REVIEW,
} ExAgentIntent;

typedef enum {
    EX_ERR_NETWORK        = 0,
    EX_ERR_AUTH            = 1,
    EX_ERR_RATE_LIMITED    = 2,
    EX_ERR_CONTENT_FILTER  = 3,
    EX_ERR_CANCELLED       = 4,
    EX_ERR_UNKNOWN         = 5,
} ExErrorCode;

/* ── Plugin descriptor ─────────────────────────────────────────────────────── */

/** Static plugin info. All strings must remain valid for the plugin's lifetime. */
typedef struct ExPluginDescriptor {
    const char        *id;                /* "org.example.my-plugin" */
    const char        *name;              /* "My Plugin" */
    const char        *version;           /* "1.0.0" */
    const char        *description;
    const char        *author;
    const ExPermission *permissions;      /* array of permissions */
    int32_t            permissions_count;
} ExPluginDescriptor;

/* ── AI types ──────────────────────────────────────────────────────────────── */

typedef struct ExAIMessage {
    int32_t     role;           /* 0=System 1=User 2=Assistant 3=Tool */
    const char *content;
    const char *tool_call_id;   /* non-null for Tool role */
} ExAIMessage;

typedef struct ExToolParam {
    const char *name;
    const char *type;           /* "string", "number", "boolean", ... */
    const char *description;
    int32_t     required;       /* 0 or 1 */
} ExToolParam;

typedef struct ExToolDef {
    const char        *name;
    const char        *description;
    const ExToolParam *params;
    int32_t            param_count;
} ExToolDef;

typedef struct ExToolCall {
    const char *id;             /* server-assigned call id */
    const char *name;           /* tool name */
    const char *arguments;      /* JSON string */
} ExToolCall;

typedef struct ExAIRequest {
    const char        *request_id;
    int32_t            intent;          /* ExAgentIntent */
    int32_t            agent_mode;      /* 0 or 1 */
    const char        *user_prompt;
    const char        *workspace_root;
    const char        *active_file_path;
    const char        *language_id;
    const char        *selected_text;
    const char        *full_file_content;
    int32_t            cursor_line;
    int32_t            cursor_column;
    const char        *reasoning_effort; /* "low", "medium", "high" */
    const ExAIMessage *messages;
    int32_t            message_count;
    const ExToolDef   *tools;
    int32_t            tool_count;
} ExAIRequest;

typedef struct ExAIResponse {
    const char       *request_id;
    const char       *text;
    const char       *thinking_content;
    const ExToolCall *tool_calls;
    int32_t           tool_call_count;
    int32_t           prompt_tokens;
    int32_t           completion_tokens;
    int32_t           total_tokens;
} ExAIResponse;

/* ── AI provider vtable (plugin implements) ────────────────────────────────── */

/**
 * Plugins that provide AI capabilities fill out this vtable and register it
 * via api->ai_register_provider() during ex_plugin_initialize().
 *
 * `ctx` is a plugin-owned opaque pointer passed as the first argument to
 * every vtable function. The plugin manages its lifetime.
 */
typedef struct ExAIProviderVTable {
    void *ctx;

    /* Info — strings owned by plugin, valid for provider lifetime */
    const char *(*id)(void *ctx);
    const char *(*display_name)(void *ctx);
    uint32_t    (*capabilities)(void *ctx);  /* bitmask of ExAgentCapability */
    int32_t     (*is_available)(void *ctx);

    /* Lifecycle */
    void (*initialize)(void *ctx);
    void (*shutdown)(void *ctx);

    /* Models */
    int32_t     (*model_count)(void *ctx);
    const char *(*model_at)(void *ctx, int32_t index);   /* owned by plugin */
    const char *(*current_model)(void *ctx);             /* owned by plugin */
    void        (*set_model)(void *ctx, const char *model_id);

    /* Requests — host calls these; provider sends responses via ExHostAPI */
    void (*send_request)(void *ctx, const ExAIRequest *request);
    void (*cancel_request)(void *ctx, const char *request_id);

    /* Cleanup */
    void (*destroy)(void *ctx);
} ExAIProviderVTable;

/* ── Command callback ─────────────────────────────────────────────────────── */

typedef void (*ExCommandCallback)(void *user_data);

/* ── Host API (vtable passed to plugin during init) ────────────────────────── */

/**
 * The host populates this struct and passes it to ex_plugin_initialize().
 * The pointer is valid from initialization until ex_plugin_shutdown() returns.
 *
 * Every function takes `ctx` (opaque host context) as its first argument.
 */
typedef struct ExHostAPI {
    void *ctx;  /* pass as first arg to every function below */

    /* ── Memory ────────────────────────────────────────────────────────── */
    void (*free_string)(void *ctx, char *str);
    void (*free_buffer)(void *ctx, ExBuffer buf);

    /* ── Editor service ────────────────────────────────────────────────── */
    char    *(*editor_active_file)(void *ctx);       /* → free_string */
    char    *(*editor_active_language)(void *ctx);   /* → free_string */
    char    *(*editor_selected_text)(void *ctx);     /* → free_string */
    char    *(*editor_document_text)(void *ctx);     /* → free_string */
    void     (*editor_replace_selection)(void *ctx, const char *text);
    void     (*editor_insert_text)(void *ctx, const char *text);
    void     (*editor_open_file)(void *ctx, const char *path,
                                 int32_t line, int32_t column);
    int32_t  (*editor_cursor_line)(void *ctx);
    int32_t  (*editor_cursor_column)(void *ctx);

    /* ── Workspace service ─────────────────────────────────────────────── */
    char    *(*workspace_root)(void *ctx);           /* → free_string */
    int32_t  (*workspace_read_file)(void *ctx, const char *path,
                                    ExBuffer *out);  /* 0=error, 1=ok */
    int32_t  (*workspace_write_file)(void *ctx, const char *path,
                                     const void *data, size_t len);
    int32_t  (*workspace_exists)(void *ctx, const char *path);

    /* ── Notification service ──────────────────────────────────────────── */
    void (*notify_info)(void *ctx, const char *text);
    void (*notify_warning)(void *ctx, const char *text);
    void (*notify_error)(void *ctx, const char *text);
    void (*notify_status)(void *ctx, const char *text, int32_t timeout_ms);

    /* ── Command service ───────────────────────────────────────────────── */
    void     (*command_register)(void *ctx, const char *id, const char *title,
                                 ExCommandCallback callback, void *user_data);
    void     (*command_unregister)(void *ctx, const char *id);
    int32_t  (*command_execute)(void *ctx, const char *id);

    /* ── Git service ───────────────────────────────────────────────────── */
    int32_t  (*git_is_repo)(void *ctx);
    char    *(*git_current_branch)(void *ctx);       /* → free_string */
    char    *(*git_diff)(void *ctx, int32_t staged); /* → free_string */

    /* ── Terminal service ──────────────────────────────────────────────── */
    void     (*terminal_run_command)(void *ctx, const char *command);
    void     (*terminal_send_input)(void *ctx, const char *text);
    char    *(*terminal_recent_output)(void *ctx, int32_t max_lines);

    /* ── Diagnostics service ───────────────────────────────────────────── */
    int32_t  (*diagnostics_error_count)(void *ctx);
    int32_t  (*diagnostics_warning_count)(void *ctx);

    /* ── AI provider registration ──────────────────────────────────────── */
    void (*ai_register_provider)(void *ctx, const ExAIProviderVTable *vtable);

    /* ── AI response callbacks (provider → host) ───────────────────────── */
    void (*ai_response_delta)(void *ctx, const char *provider_id,
                              const char *request_id, const char *text);
    void (*ai_thinking_delta)(void *ctx, const char *provider_id,
                              const char *request_id, const char *text);
    void (*ai_response_finished)(void *ctx, const char *provider_id,
                                 const char *request_id,
                                 const ExAIResponse *response);
    void (*ai_response_error)(void *ctx, const char *provider_id,
                              const char *request_id,
                              int32_t code, const char *message);
    void (*ai_availability_changed)(void *ctx, const char *provider_id,
                                    int32_t available);
    void (*ai_models_changed)(void *ctx, const char *provider_id);

    /* ── Logging ───────────────────────────────────────────────────────── */
    void (*log_debug)(void *ctx, const char *message);
    void (*log_info)(void *ctx, const char *message);
    void (*log_warning)(void *ctx, const char *message);
    void (*log_error)(void *ctx, const char *message);

} ExHostAPI;

/* ── Plugin entry points ───────────────────────────────────────────────────── */
/*
 * A C ABI plugin MUST export all four functions below.
 * The host resolves them via dlsym / GetProcAddress at load time.
 */

/** Return EX_ABI_VERSION. Host checks major version for compatibility. */
EX_EXPORT int32_t ex_plugin_api_version(void);

/** Return a static descriptor. All strings must outlive the plugin. */
EX_EXPORT ExPluginDescriptor ex_plugin_describe(void);

/** Initialize the plugin. Returns non-zero on success, 0 on failure.
 *  `api` remains valid until ex_plugin_shutdown() returns. */
EX_EXPORT int32_t ex_plugin_initialize(const ExHostAPI *api);

/** Shutdown the plugin. Release all resources. */
EX_EXPORT void ex_plugin_shutdown(void);

#ifdef __cplusplus
}
#endif
