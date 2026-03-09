#pragma once

#include "exorcist_plugin_api.h"
#include "../../aiinterface.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <vector>

class IHostServices;
class MainWindow;

namespace cabi {

// ── CAbiProviderAdapter ──────────────────────────────────────────────────────
//
// Wraps an ExAIProviderVTable (from a C ABI plugin) as a Qt IAgentProvider
// so the host's AgentOrchestrator can treat it like any other provider.

class CAbiProviderAdapter : public IAgentProvider
{
    Q_OBJECT

public:
    explicit CAbiProviderAdapter(ExAIProviderVTable vtable,
                                  const ExHostAPI *hostApi,
                                  QObject *parent = nullptr);
    ~CAbiProviderAdapter() override;

    /* IAgentProvider interface */
    QString           id()           const override;
    QString           displayName()  const override;
    AgentCapabilities capabilities() const override;
    bool              isAvailable()  const override;

    QStringList       availableModels() const override;
    QString           currentModel()    const override;
    void              setModel(const QString &model) override;

    void initialize() override;
    void shutdown()   override;

    void sendRequest(const AgentRequest &request)  override;
    void cancelRequest(const QString &requestId)   override;

    /* Bridge callbacks — called by CAbiPluginBridge when C plugin sends data */
    void bridgeDelta(const QString &requestId, const QString &text);
    void bridgeThinking(const QString &requestId, const QString &text);
    void bridgeFinished(const QString &requestId, const AgentResponse &resp);
    void bridgeError(const QString &requestId, const AgentError &err);
    void bridgeAvailabilityChanged(bool available);
    void bridgeModelsChanged();

private:
    ExAIProviderVTable  m_vtable;
    const ExHostAPI    *m_hostApi;
    QString             m_cachedId;
};

// ── CAbiPluginBridge ─────────────────────────────────────────────────────────
//
// Populates an ExHostAPI vtable with static callbacks that delegate to the
// C++ IHostServices. One bridge per loaded C ABI plugin.

class CAbiPluginBridge : public QObject
{
    Q_OBJECT

public:
    explicit CAbiPluginBridge(IHostServices *host, QObject *parent = nullptr);
    ~CAbiPluginBridge() override;

    /** Filled vtable — pass to ex_plugin_initialize(). */
    const ExHostAPI *api() const { return &m_api; }

    /** Providers registered by the plugin during initialization. */
    QList<IAgentProvider *> providers() const;

    /** Command callbacks registered by the plugin (cleaned up on destroy). */
    struct CmdEntry {
        ExCommandCallback callback;
        void             *userData;
    };

private:
    /* Helpers */
    static char *dupString(const QString &s);

    /* Static callbacks — ExHostAPI function pointers */
    static void    s_free_string(void *ctx, char *str);
    static void    s_free_buffer(void *ctx, ExBuffer buf);

    static char   *s_editor_active_file(void *ctx);
    static char   *s_editor_active_language(void *ctx);
    static char   *s_editor_selected_text(void *ctx);
    static char   *s_editor_document_text(void *ctx);
    static void    s_editor_replace_selection(void *ctx, const char *text);
    static void    s_editor_insert_text(void *ctx, const char *text);
    static void    s_editor_open_file(void *ctx, const char *path, int32_t line, int32_t col);
    static int32_t s_editor_cursor_line(void *ctx);
    static int32_t s_editor_cursor_column(void *ctx);

    static char   *s_workspace_root(void *ctx);
    static int32_t s_workspace_read_file(void *ctx, const char *path, ExBuffer *out);
    static int32_t s_workspace_write_file(void *ctx, const char *path, const void *data, size_t len);
    static int32_t s_workspace_exists(void *ctx, const char *path);

    static void    s_notify_info(void *ctx, const char *text);
    static void    s_notify_warning(void *ctx, const char *text);
    static void    s_notify_error(void *ctx, const char *text);
    static void    s_notify_status(void *ctx, const char *text, int32_t timeout_ms);

    static void    s_command_register(void *ctx, const char *id, const char *title,
                                      ExCommandCallback cb, void *ud);
    static void    s_command_unregister(void *ctx, const char *id);
    static int32_t s_command_execute(void *ctx, const char *id);

    static int32_t s_git_is_repo(void *ctx);
    static char   *s_git_current_branch(void *ctx);
    static char   *s_git_diff(void *ctx, int32_t staged);

    static void    s_terminal_run_command(void *ctx, const char *cmd);
    static void    s_terminal_send_input(void *ctx, const char *text);
    static char   *s_terminal_recent_output(void *ctx, int32_t maxLines);

    static int32_t s_diagnostics_error_count(void *ctx);
    static int32_t s_diagnostics_warning_count(void *ctx);

    static void    s_ai_register_provider(void *ctx, const ExAIProviderVTable *vtable);
    static void    s_ai_response_delta(void *ctx, const char *pid, const char *rid, const char *text);
    static void    s_ai_thinking_delta(void *ctx, const char *pid, const char *rid, const char *text);
    static void    s_ai_response_finished(void *ctx, const char *pid, const char *rid, const ExAIResponse *resp);
    static void    s_ai_response_error(void *ctx, const char *pid, const char *rid, int32_t code, const char *msg);
    static void    s_ai_availability_changed(void *ctx, const char *pid, int32_t available);
    static void    s_ai_models_changed(void *ctx, const char *pid);

    static void    s_log_debug(void *ctx, const char *msg);
    static void    s_log_info(void *ctx, const char *msg);
    static void    s_log_warning(void *ctx, const char *msg);
    static void    s_log_error(void *ctx, const char *msg);

    /* Instance data */
    ExHostAPI                            m_api{};
    IHostServices                       *m_host;
    QList<CAbiProviderAdapter *>         m_providers;
    QHash<QString, CmdEntry>             m_commands;

    CAbiProviderAdapter *findAdapter(const QString &providerId) const;
};

} // namespace cabi
