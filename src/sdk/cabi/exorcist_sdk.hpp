/*
 * exorcist_sdk.hpp — C++ convenience wrapper over the Exorcist C ABI.
 *
 * Header-only.  No Qt dependency — only standard C++ and the C ABI header.
 * Plugin authors include this instead of the raw C header for a nicer API.
 *
 * Usage:
 *   #include <exorcist_sdk.hpp>
 *
 *   static exorcist::PluginHost* g_host = nullptr;
 *
 *   extern "C" EX_EXPORT int32_t ex_plugin_initialize(const ExHostAPI* api) {
 *       g_host = new exorcist::PluginHost(api);
 *       g_host->notifications().info("Hello from my plugin!");
 *       return 1;
 *   }
 */
#pragma once

#include "exorcist_plugin_api.h"

#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace exorcist {

// ── RAII string returned by host ─────────────────────────────────────────────

class HostString {
public:
    HostString(char *raw, const ExHostAPI *api) : m_raw(raw), m_api(api) {}
    ~HostString() { if (m_raw) m_api->free_string(m_api->ctx, m_raw); }

    HostString(HostString &&o) noexcept : m_raw(o.m_raw), m_api(o.m_api) { o.m_raw = nullptr; }
    HostString &operator=(HostString &&o) noexcept {
        if (this != &o) {
            if (m_raw) m_api->free_string(m_api->ctx, m_raw);
            m_raw = o.m_raw;  m_api = o.m_api;  o.m_raw = nullptr;
        }
        return *this;
    }
    HostString(const HostString &) = delete;
    HostString &operator=(const HostString &) = delete;

    const char *c_str() const { return m_raw ? m_raw : ""; }
    std::string str()   const { return m_raw ? std::string(m_raw) : std::string(); }
    bool empty()        const { return !m_raw || m_raw[0] == '\0'; }
    explicit operator bool() const { return m_raw != nullptr; }

private:
    char           *m_raw;
    const ExHostAPI *m_api;
};

// ── Service wrappers ─────────────────────────────────────────────────────────

class EditorService {
public:
    explicit EditorService(const ExHostAPI *api) : m_api(api) {}

    HostString activeFile()     const { return {m_api->editor_active_file(m_api->ctx), m_api}; }
    HostString activeLanguage() const { return {m_api->editor_active_language(m_api->ctx), m_api}; }
    HostString selectedText()   const { return {m_api->editor_selected_text(m_api->ctx), m_api}; }
    HostString documentText()   const { return {m_api->editor_document_text(m_api->ctx), m_api}; }

    void replaceSelection(const char *text) { m_api->editor_replace_selection(m_api->ctx, text); }
    void insertText(const char *text)       { m_api->editor_insert_text(m_api->ctx, text); }
    void openFile(const char *path, int line = -1, int col = -1) {
        m_api->editor_open_file(m_api->ctx, path, line, col);
    }

    int cursorLine()   const { return m_api->editor_cursor_line(m_api->ctx); }
    int cursorColumn() const { return m_api->editor_cursor_column(m_api->ctx); }

private:
    const ExHostAPI *m_api;
};

class WorkspaceService {
public:
    explicit WorkspaceService(const ExHostAPI *api) : m_api(api) {}

    HostString root() const { return {m_api->workspace_root(m_api->ctx), m_api}; }

    std::vector<uint8_t> readFile(const char *path) const {
        ExBuffer buf{};
        if (!m_api->workspace_read_file(m_api->ctx, path, &buf) || !buf.data)
            return {};
        std::vector<uint8_t> result(buf.data, buf.data + buf.length);
        m_api->free_buffer(m_api->ctx, buf);
        return result;
    }

    bool writeFile(const char *path, const void *data, size_t len) {
        return m_api->workspace_write_file(m_api->ctx, path, data, len) != 0;
    }

    bool exists(const char *path) const {
        return m_api->workspace_exists(m_api->ctx, path) != 0;
    }

private:
    const ExHostAPI *m_api;
};

class NotificationService {
public:
    explicit NotificationService(const ExHostAPI *api) : m_api(api) {}

    void info(const char *text)    { m_api->notify_info(m_api->ctx, text); }
    void warning(const char *text) { m_api->notify_warning(m_api->ctx, text); }
    void error(const char *text)   { m_api->notify_error(m_api->ctx, text); }
    void status(const char *text, int timeout_ms = 5000) {
        m_api->notify_status(m_api->ctx, text, timeout_ms);
    }

private:
    const ExHostAPI *m_api;
};

class CommandService {
public:
    explicit CommandService(const ExHostAPI *api) : m_api(api) {}

    void registerCommand(const char *id, const char *title,
                         ExCommandCallback cb, void *user_data) {
        m_api->command_register(m_api->ctx, id, title, cb, user_data);
    }
    void unregisterCommand(const char *id) { m_api->command_unregister(m_api->ctx, id); }
    bool executeCommand(const char *id)    { return m_api->command_execute(m_api->ctx, id) != 0; }

private:
    const ExHostAPI *m_api;
};

class GitService {
public:
    explicit GitService(const ExHostAPI *api) : m_api(api) {}

    bool       isRepo()        const { return m_api->git_is_repo(m_api->ctx) != 0; }
    HostString currentBranch() const { return {m_api->git_current_branch(m_api->ctx), m_api}; }
    HostString diff(bool staged = false) const {
        return {m_api->git_diff(m_api->ctx, staged ? 1 : 0), m_api};
    }

private:
    const ExHostAPI *m_api;
};

class TerminalService {
public:
    explicit TerminalService(const ExHostAPI *api) : m_api(api) {}

    void runCommand(const char *cmd) { m_api->terminal_run_command(m_api->ctx, cmd); }
    void sendInput(const char *text) { m_api->terminal_send_input(m_api->ctx, text); }
    HostString recentOutput(int maxLines = 50) const {
        return {m_api->terminal_recent_output(m_api->ctx, maxLines), m_api};
    }

private:
    const ExHostAPI *m_api;
};

class DiagnosticsService {
public:
    explicit DiagnosticsService(const ExHostAPI *api) : m_api(api) {}

    int errorCount()   const { return m_api->diagnostics_error_count(m_api->ctx); }
    int warningCount() const { return m_api->diagnostics_warning_count(m_api->ctx); }

private:
    const ExHostAPI *m_api;
};

class LogService {
public:
    explicit LogService(const ExHostAPI *api) : m_api(api) {}

    void debug(const char *msg)   { m_api->log_debug(m_api->ctx, msg); }
    void info(const char *msg)    { m_api->log_info(m_api->ctx, msg); }
    void warning(const char *msg) { m_api->log_warning(m_api->ctx, msg); }
    void error(const char *msg)   { m_api->log_error(m_api->ctx, msg); }

private:
    const ExHostAPI *m_api;
};

// ── PluginHost — top-level wrapper ───────────────────────────────────────────

class PluginHost {
public:
    explicit PluginHost(const ExHostAPI *api)
        : m_api(api)
        , m_editor(api)
        , m_workspace(api)
        , m_notifications(api)
        , m_commands(api)
        , m_git(api)
        , m_terminal(api)
        , m_diagnostics(api)
        , m_log(api)
    {}

    const ExHostAPI *raw() const { return m_api; }

    EditorService       &editor()        { return m_editor; }
    WorkspaceService    &workspace()     { return m_workspace; }
    NotificationService &notifications() { return m_notifications; }
    CommandService      &commands()      { return m_commands; }
    GitService          &git()           { return m_git; }
    TerminalService     &terminal()      { return m_terminal; }
    DiagnosticsService  &diagnostics()   { return m_diagnostics; }
    LogService          &log()           { return m_log; }

    void registerAIProvider(const ExAIProviderVTable *vtable) {
        m_api->ai_register_provider(m_api->ctx, vtable);
    }

    /* AI response helpers — call from provider's worker thread */
    void aiDelta(const char *providerId, const char *requestId, const char *text) {
        m_api->ai_response_delta(m_api->ctx, providerId, requestId, text);
    }
    void aiThinkingDelta(const char *providerId, const char *requestId, const char *text) {
        m_api->ai_thinking_delta(m_api->ctx, providerId, requestId, text);
    }
    void aiFinished(const char *providerId, const char *requestId, const ExAIResponse *resp) {
        m_api->ai_response_finished(m_api->ctx, providerId, requestId, resp);
    }
    void aiError(const char *providerId, const char *requestId,
                 ExErrorCode code, const char *message) {
        m_api->ai_response_error(m_api->ctx, providerId, requestId, code, message);
    }
    void aiAvailabilityChanged(const char *providerId, bool available) {
        m_api->ai_availability_changed(m_api->ctx, providerId, available ? 1 : 0);
    }
    void aiModelsChanged(const char *providerId) {
        m_api->ai_models_changed(m_api->ctx, providerId);
    }

private:
    const ExHostAPI      *m_api;
    EditorService         m_editor;
    WorkspaceService      m_workspace;
    NotificationService   m_notifications;
    CommandService        m_commands;
    GitService            m_git;
    TerminalService       m_terminal;
    DiagnosticsService    m_diagnostics;
    LogService            m_log;
};

} // namespace exorcist
