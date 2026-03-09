#include "cabi_bridge.h"

#include "../../logger.h"
#include "../ihostservices.h"
#include "../icommandservice.h"
#include "../idiagnosticsservice.h"
#include "../ieditorservice.h"
#include "../igitservice.h"
#include "../inotificationservice.h"
#include "../itaskservice.h"
#include "../iterminalservice.h"
#include "../iviewservice.h"
#include "../iworkspaceservice.h"

#include <QMetaObject>

#include <cstdlib>
#include <cstring>

namespace cabi {

// ── Helpers ──────────────────────────────────────────────────────────────────

static inline CAbiPluginBridge *B(void *ctx) {
    return static_cast<CAbiPluginBridge *>(ctx);
}

char *CAbiPluginBridge::dupString(const QString &s)
{
    if (s.isNull()) return nullptr;
    const QByteArray utf8 = s.toUtf8();
    auto *result = static_cast<char *>(std::malloc(
        static_cast<size_t>(utf8.size()) + 1));
    if (result)
        std::memcpy(result, utf8.constData(),
                    static_cast<size_t>(utf8.size()) + 1);
    return result;
}

// ── Memory ───────────────────────────────────────────────────────────────────

void CAbiPluginBridge::s_free_string(void *, char *str)   { std::free(str); }
void CAbiPluginBridge::s_free_buffer(void *, ExBuffer buf) {
    std::free(const_cast<uint8_t *>(buf.data));
}

// ── Editor ───────────────────────────────────────────────────────────────────

char *CAbiPluginBridge::s_editor_active_file(void *ctx) {
    auto *ed = B(ctx)->m_host->editor();
    return ed ? dupString(ed->activeFilePath()) : nullptr;
}
char *CAbiPluginBridge::s_editor_active_language(void *ctx) {
    auto *ed = B(ctx)->m_host->editor();
    return ed ? dupString(ed->activeLanguageId()) : nullptr;
}
char *CAbiPluginBridge::s_editor_selected_text(void *ctx) {
    auto *ed = B(ctx)->m_host->editor();
    return ed ? dupString(ed->selectedText()) : nullptr;
}
char *CAbiPluginBridge::s_editor_document_text(void *ctx) {
    auto *ed = B(ctx)->m_host->editor();
    return ed ? dupString(ed->activeDocumentText()) : nullptr;
}
void CAbiPluginBridge::s_editor_replace_selection(void *ctx, const char *text) {
    auto *ed = B(ctx)->m_host->editor();
    if (ed) ed->replaceSelection(QString::fromUtf8(text));
}
void CAbiPluginBridge::s_editor_insert_text(void *ctx, const char *text) {
    auto *ed = B(ctx)->m_host->editor();
    if (ed) ed->insertText(QString::fromUtf8(text));
}
void CAbiPluginBridge::s_editor_open_file(void *ctx, const char *path,
                                           int32_t line, int32_t col) {
    auto *ed = B(ctx)->m_host->editor();
    if (ed) ed->openFile(QString::fromUtf8(path), line, col);
}
int32_t CAbiPluginBridge::s_editor_cursor_line(void *ctx) {
    auto *ed = B(ctx)->m_host->editor();
    return ed ? ed->cursorLine() : -1;
}
int32_t CAbiPluginBridge::s_editor_cursor_column(void *ctx) {
    auto *ed = B(ctx)->m_host->editor();
    return ed ? ed->cursorColumn() : -1;
}

// ── Workspace ────────────────────────────────────────────────────────────────

char *CAbiPluginBridge::s_workspace_root(void *ctx) {
    auto *ws = B(ctx)->m_host->workspace();
    return ws ? dupString(ws->rootPath()) : nullptr;
}

int32_t CAbiPluginBridge::s_workspace_read_file(void *ctx, const char *path,
                                                 ExBuffer *out) {
    auto *ws = B(ctx)->m_host->workspace();
    if (!ws || !out) return 0;
    const QByteArray data = ws->readFile(QString::fromUtf8(path));
    if (data.isNull()) return 0;

    auto *copy = static_cast<uint8_t *>(std::malloc(
        static_cast<size_t>(data.size())));
    if (!copy) return 0;
    std::memcpy(copy, data.constData(), static_cast<size_t>(data.size()));
    out->data   = copy;
    out->length = static_cast<size_t>(data.size());
    return 1;
}

int32_t CAbiPluginBridge::s_workspace_write_file(void *ctx, const char *path,
                                                  const void *data, size_t len) {
    auto *ws = B(ctx)->m_host->workspace();
    if (!ws) return 0;
    return ws->writeFile(QString::fromUtf8(path),
                         QByteArray(static_cast<const char *>(data),
                                    static_cast<int>(len)))
        ? 1 : 0;
}

int32_t CAbiPluginBridge::s_workspace_exists(void *ctx, const char *path) {
    auto *ws = B(ctx)->m_host->workspace();
    return (ws && ws->exists(QString::fromUtf8(path))) ? 1 : 0;
}

// ── Notifications ────────────────────────────────────────────────────────────

void CAbiPluginBridge::s_notify_info(void *ctx, const char *text) {
    auto *n = B(ctx)->m_host->notifications();
    if (n) n->info(QString::fromUtf8(text));
}
void CAbiPluginBridge::s_notify_warning(void *ctx, const char *text) {
    auto *n = B(ctx)->m_host->notifications();
    if (n) n->warning(QString::fromUtf8(text));
}
void CAbiPluginBridge::s_notify_error(void *ctx, const char *text) {
    auto *n = B(ctx)->m_host->notifications();
    if (n) n->error(QString::fromUtf8(text));
}
void CAbiPluginBridge::s_notify_status(void *ctx, const char *text,
                                        int32_t timeout_ms) {
    auto *n = B(ctx)->m_host->notifications();
    if (n) n->statusMessage(QString::fromUtf8(text), timeout_ms);
}

// ── Commands ─────────────────────────────────────────────────────────────────

void CAbiPluginBridge::s_command_register(void *ctx, const char *id,
                                           const char *title,
                                           ExCommandCallback callback,
                                           void *userData) {
    auto *bridge = B(ctx);
    auto *cmd = bridge->m_host->commands();
    if (!cmd) return;

    const QString qid    = QString::fromUtf8(id);
    const QString qtitle = QString::fromUtf8(title);

    bridge->m_commands[qid] = {callback, userData};

    cmd->registerCommand(qid, qtitle, [callback, userData]() {
        if (callback) callback(userData);
    });
}

void CAbiPluginBridge::s_command_unregister(void *ctx, const char *id) {
    auto *bridge = B(ctx);
    const QString qid = QString::fromUtf8(id);
    bridge->m_commands.remove(qid);
    auto *cmd = bridge->m_host->commands();
    if (cmd) cmd->unregisterCommand(qid);
}

int32_t CAbiPluginBridge::s_command_execute(void *ctx, const char *id) {
    auto *cmd = B(ctx)->m_host->commands();
    return (cmd && cmd->executeCommand(QString::fromUtf8(id))) ? 1 : 0;
}

// ── Git ──────────────────────────────────────────────────────────────────────

int32_t CAbiPluginBridge::s_git_is_repo(void *ctx) {
    auto *g = B(ctx)->m_host->git();
    return (g && g->isGitRepo()) ? 1 : 0;
}
char *CAbiPluginBridge::s_git_current_branch(void *ctx) {
    auto *g = B(ctx)->m_host->git();
    return g ? dupString(g->currentBranch()) : nullptr;
}
char *CAbiPluginBridge::s_git_diff(void *ctx, int32_t staged) {
    auto *g = B(ctx)->m_host->git();
    return g ? dupString(g->diff(staged != 0)) : nullptr;
}

// ── Terminal ─────────────────────────────────────────────────────────────────

void CAbiPluginBridge::s_terminal_run_command(void *ctx, const char *cmd) {
    auto *t = B(ctx)->m_host->terminal();
    if (t) t->runCommand(QString::fromUtf8(cmd));
}
void CAbiPluginBridge::s_terminal_send_input(void *ctx, const char *text) {
    auto *t = B(ctx)->m_host->terminal();
    if (t) t->sendInput(QString::fromUtf8(text));
}
char *CAbiPluginBridge::s_terminal_recent_output(void *ctx, int32_t maxLines) {
    auto *t = B(ctx)->m_host->terminal();
    return t ? dupString(t->recentOutput(maxLines)) : nullptr;
}

// ── Diagnostics ──────────────────────────────────────────────────────────────

int32_t CAbiPluginBridge::s_diagnostics_error_count(void *ctx) {
    auto *d = B(ctx)->m_host->diagnostics();
    return d ? d->errorCount() : 0;
}
int32_t CAbiPluginBridge::s_diagnostics_warning_count(void *ctx) {
    auto *d = B(ctx)->m_host->diagnostics();
    return d ? d->warningCount() : 0;
}

// ── AI provider registration ─────────────────────────────────────────────────

void CAbiPluginBridge::s_ai_register_provider(void *ctx,
                                               const ExAIProviderVTable *vtable)
{
    auto *bridge = B(ctx);
    auto *adapter = new CAbiProviderAdapter(*vtable, &bridge->m_api, bridge);
    bridge->m_providers.append(adapter);
}

// ── AI response callbacks ────────────────────────────────────────────────────

void CAbiPluginBridge::s_ai_response_delta(void *ctx, const char *pid,
                                            const char *rid, const char *text)
{
    auto *adapter = B(ctx)->findAdapter(QString::fromUtf8(pid));
    if (!adapter) return;
    const QString reqId = QString::fromUtf8(rid);
    const QString chunk = QString::fromUtf8(text);
    QMetaObject::invokeMethod(adapter, [adapter, reqId, chunk]() {
        adapter->bridgeDelta(reqId, chunk);
    }, Qt::AutoConnection);
}

void CAbiPluginBridge::s_ai_thinking_delta(void *ctx, const char *pid,
                                            const char *rid, const char *text)
{
    auto *adapter = B(ctx)->findAdapter(QString::fromUtf8(pid));
    if (!adapter) return;
    const QString reqId = QString::fromUtf8(rid);
    const QString chunk = QString::fromUtf8(text);
    QMetaObject::invokeMethod(adapter, [adapter, reqId, chunk]() {
        adapter->bridgeThinking(reqId, chunk);
    }, Qt::AutoConnection);
}

void CAbiPluginBridge::s_ai_response_finished(void *ctx, const char *pid,
                                               const char *rid,
                                               const ExAIResponse *resp)
{
    auto *adapter = B(ctx)->findAdapter(QString::fromUtf8(pid));
    if (!adapter || !resp) return;

    const QString reqId = QString::fromUtf8(rid);
    AgentResponse ar;
    ar.requestId       = reqId;
    ar.text            = QString::fromUtf8(resp->text);
    ar.thinkingContent = QString::fromUtf8(resp->thinking_content);
    ar.promptTokens     = resp->prompt_tokens;
    ar.completionTokens = resp->completion_tokens;
    ar.totalTokens      = resp->total_tokens;

    for (int32_t i = 0; i < resp->tool_call_count; ++i) {
        ToolCall tc;
        tc.id        = QString::fromUtf8(resp->tool_calls[i].id);
        tc.name      = QString::fromUtf8(resp->tool_calls[i].name);
        tc.arguments = QString::fromUtf8(resp->tool_calls[i].arguments);
        ar.toolCalls.append(tc);
    }

    QMetaObject::invokeMethod(adapter, [adapter, reqId, ar]() {
        adapter->bridgeFinished(reqId, ar);
    }, Qt::AutoConnection);
}

void CAbiPluginBridge::s_ai_response_error(void *ctx, const char *pid,
                                            const char *rid, int32_t code,
                                            const char *msg)
{
    auto *adapter = B(ctx)->findAdapter(QString::fromUtf8(pid));
    if (!adapter) return;

    const QString reqId = QString::fromUtf8(rid);
    AgentError err;
    err.requestId = reqId;
    err.code      = static_cast<AgentError::Code>(code);
    err.message   = QString::fromUtf8(msg);

    QMetaObject::invokeMethod(adapter, [adapter, reqId, err]() {
        adapter->bridgeError(reqId, err);
    }, Qt::AutoConnection);
}

void CAbiPluginBridge::s_ai_availability_changed(void *ctx, const char *pid,
                                                  int32_t available)
{
    auto *adapter = B(ctx)->findAdapter(QString::fromUtf8(pid));
    if (!adapter) return;
    QMetaObject::invokeMethod(adapter, [adapter, available]() {
        adapter->bridgeAvailabilityChanged(available != 0);
    }, Qt::AutoConnection);
}

void CAbiPluginBridge::s_ai_models_changed(void *ctx, const char *pid)
{
    auto *adapter = B(ctx)->findAdapter(QString::fromUtf8(pid));
    if (!adapter) return;
    QMetaObject::invokeMethod(adapter, [adapter]() {
        adapter->bridgeModelsChanged();
    }, Qt::AutoConnection);
}

// ── Logging ──────────────────────────────────────────────────────────────────

void CAbiPluginBridge::s_log_debug(void *, const char *msg)   { qDebug("[CABI] %s", msg); }
void CAbiPluginBridge::s_log_info(void *, const char *msg)    { qInfo("[CABI] %s", msg); }
void CAbiPluginBridge::s_log_warning(void *, const char *msg) { qWarning("[CABI] %s", msg); }
void CAbiPluginBridge::s_log_error(void *, const char *msg)   { qCritical("[CABI] %s", msg); }

// ── CAbiPluginBridge constructor ─────────────────────────────────────────────

CAbiPluginBridge::CAbiPluginBridge(IHostServices *host, QObject *parent)
    : QObject(parent)
    , m_host(host)
{
    m_api.ctx = this;

    /* Memory */
    m_api.free_string = s_free_string;
    m_api.free_buffer = s_free_buffer;

    /* Editor */
    m_api.editor_active_file      = s_editor_active_file;
    m_api.editor_active_language  = s_editor_active_language;
    m_api.editor_selected_text    = s_editor_selected_text;
    m_api.editor_document_text    = s_editor_document_text;
    m_api.editor_replace_selection = s_editor_replace_selection;
    m_api.editor_insert_text      = s_editor_insert_text;
    m_api.editor_open_file        = s_editor_open_file;
    m_api.editor_cursor_line      = s_editor_cursor_line;
    m_api.editor_cursor_column    = s_editor_cursor_column;

    /* Workspace */
    m_api.workspace_root       = s_workspace_root;
    m_api.workspace_read_file  = s_workspace_read_file;
    m_api.workspace_write_file = s_workspace_write_file;
    m_api.workspace_exists     = s_workspace_exists;

    /* Notifications */
    m_api.notify_info    = s_notify_info;
    m_api.notify_warning = s_notify_warning;
    m_api.notify_error   = s_notify_error;
    m_api.notify_status  = s_notify_status;

    /* Commands */
    m_api.command_register   = s_command_register;
    m_api.command_unregister = s_command_unregister;
    m_api.command_execute    = s_command_execute;

    /* Git */
    m_api.git_is_repo        = s_git_is_repo;
    m_api.git_current_branch = s_git_current_branch;
    m_api.git_diff           = s_git_diff;

    /* Terminal */
    m_api.terminal_run_command    = s_terminal_run_command;
    m_api.terminal_send_input     = s_terminal_send_input;
    m_api.terminal_recent_output  = s_terminal_recent_output;

    /* Diagnostics */
    m_api.diagnostics_error_count   = s_diagnostics_error_count;
    m_api.diagnostics_warning_count = s_diagnostics_warning_count;

    /* AI */
    m_api.ai_register_provider     = s_ai_register_provider;
    m_api.ai_response_delta        = s_ai_response_delta;
    m_api.ai_thinking_delta        = s_ai_thinking_delta;
    m_api.ai_response_finished     = s_ai_response_finished;
    m_api.ai_response_error        = s_ai_response_error;
    m_api.ai_availability_changed  = s_ai_availability_changed;
    m_api.ai_models_changed        = s_ai_models_changed;

    /* Logging */
    m_api.log_debug   = s_log_debug;
    m_api.log_info    = s_log_info;
    m_api.log_warning = s_log_warning;
    m_api.log_error   = s_log_error;
}

CAbiPluginBridge::~CAbiPluginBridge() = default;

QList<IAgentProvider *> CAbiPluginBridge::providers() const
{
    QList<IAgentProvider *> result;
    for (auto *a : m_providers)
        result.append(a);
    return result;
}

CAbiProviderAdapter *CAbiPluginBridge::findAdapter(
    const QString &providerId) const
{
    for (auto *a : m_providers) {
        if (a->id() == providerId)
            return a;
    }
    return nullptr;
}

// ═════════════════════════════════════════════════════════════════════════════
// CAbiProviderAdapter
// ═════════════════════════════════════════════════════════════════════════════

CAbiProviderAdapter::CAbiProviderAdapter(ExAIProviderVTable vtable,
                                           const ExHostAPI *hostApi,
                                           QObject *parent)
    : m_vtable(vtable)
    , m_hostApi(hostApi)
{
    if (parent)
        setParent(parent);
    if (m_vtable.id && m_vtable.ctx)
        m_cachedId = QString::fromUtf8(m_vtable.id(m_vtable.ctx));
}

CAbiProviderAdapter::~CAbiProviderAdapter()
{
    if (m_vtable.destroy)
        m_vtable.destroy(m_vtable.ctx);
}

QString CAbiProviderAdapter::id() const { return m_cachedId; }

QString CAbiProviderAdapter::displayName() const {
    if (!m_vtable.display_name) return m_cachedId;
    return QString::fromUtf8(m_vtable.display_name(m_vtable.ctx));
}

AgentCapabilities CAbiProviderAdapter::capabilities() const {
    if (!m_vtable.capabilities) return {};
    const uint32_t bits = m_vtable.capabilities(m_vtable.ctx);
    AgentCapabilities caps;
    if (bits & EX_CAP_CHAT)              caps |= AgentCapability::Chat;
    if (bits & EX_CAP_INLINE_COMPLETION) caps |= AgentCapability::InlineCompletion;
    if (bits & EX_CAP_STREAMING)         caps |= AgentCapability::Streaming;
    if (bits & EX_CAP_TOOL_CALLING)      caps |= AgentCapability::ToolCalling;
    if (bits & EX_CAP_CODE_EDIT)         caps |= AgentCapability::CodeEdit;
    if (bits & EX_CAP_TEST_GENERATION)   caps |= AgentCapability::TestGeneration;
    if (bits & EX_CAP_VISION)            caps |= AgentCapability::Vision;
    if (bits & EX_CAP_THINKING)          caps |= AgentCapability::Thinking;
    return caps;
}

bool CAbiProviderAdapter::isAvailable() const {
    return m_vtable.is_available && m_vtable.is_available(m_vtable.ctx);
}

QStringList CAbiProviderAdapter::availableModels() const {
    QStringList result;
    if (!m_vtable.model_count || !m_vtable.model_at) return result;
    const int32_t n = m_vtable.model_count(m_vtable.ctx);
    for (int32_t i = 0; i < n; ++i) {
        const char *s = m_vtable.model_at(m_vtable.ctx, i);
        if (s) result.append(QString::fromUtf8(s));
    }
    return result;
}

QString CAbiProviderAdapter::currentModel() const {
    if (!m_vtable.current_model) return {};
    const char *s = m_vtable.current_model(m_vtable.ctx);
    return s ? QString::fromUtf8(s) : QString();
}

void CAbiProviderAdapter::setModel(const QString &model) {
    if (m_vtable.set_model)
        m_vtable.set_model(m_vtable.ctx, model.toUtf8().constData());
}

void CAbiProviderAdapter::initialize() {
    if (m_vtable.initialize) m_vtable.initialize(m_vtable.ctx);
}

void CAbiProviderAdapter::shutdown() {
    if (m_vtable.shutdown) m_vtable.shutdown(m_vtable.ctx);
}

void CAbiProviderAdapter::sendRequest(const AgentRequest &request)
{
    if (!m_vtable.send_request) return;

    // ── Convert AgentRequest → ExAIRequest ───────────────────────────────
    const QByteArray reqIdUtf8   = request.requestId.toUtf8();
    const QByteArray promptUtf8  = request.userPrompt.toUtf8();
    const QByteArray wsRootUtf8  = request.workspaceRoot.toUtf8();
    const QByteArray fileUtf8    = request.activeFilePath.toUtf8();
    const QByteArray langUtf8    = request.languageId.toUtf8();
    const QByteArray selUtf8     = request.selectedText.toUtf8();
    const QByteArray contentUtf8 = request.fullFileContent.toUtf8();
    const QByteArray effortUtf8  = request.reasoningEffort.toUtf8();

    // Convert messages
    std::vector<QByteArray> msgContentBufs, msgTcIdBufs;
    std::vector<ExAIMessage> msgs;
    msgs.reserve(static_cast<size_t>(request.conversationHistory.size()));
    msgContentBufs.reserve(msgs.capacity());
    msgTcIdBufs.reserve(msgs.capacity());

    for (const auto &m : request.conversationHistory) {
        msgContentBufs.push_back(m.content.toUtf8());
        msgTcIdBufs.push_back(m.toolCallId.toUtf8());
        ExAIMessage em{};
        em.role         = static_cast<int32_t>(m.role);
        em.content      = msgContentBufs.back().constData();
        em.tool_call_id = msgTcIdBufs.back().isEmpty() ? nullptr
                              : msgTcIdBufs.back().constData();
        msgs.push_back(em);
    }

    // Convert tool definitions
    std::vector<QByteArray> toolNameBufs, toolDescBufs;
    std::vector<std::vector<QByteArray>> paramBufs;
    std::vector<std::vector<ExToolParam>> paramStructs;
    std::vector<ExToolDef> tools;
    tools.reserve(static_cast<size_t>(request.tools.size()));

    for (const auto &td : request.tools) {
        toolNameBufs.push_back(td.name.toUtf8());
        toolDescBufs.push_back(td.description.toUtf8());

        std::vector<QByteArray> pbufs;
        std::vector<ExToolParam> pstructs;
        for (const auto &p : td.parameters) {
            pbufs.push_back(p.name.toUtf8());
            pbufs.push_back(p.type.toUtf8());
            pbufs.push_back(p.description.toUtf8());
            ExToolParam ep{};
            ep.name        = pbufs[pbufs.size() - 3].constData();
            ep.type        = pbufs[pbufs.size() - 2].constData();
            ep.description = pbufs[pbufs.size() - 1].constData();
            ep.required    = p.required ? 1 : 0;
            pstructs.push_back(ep);
        }
        paramBufs.push_back(std::move(pbufs));
        paramStructs.push_back(std::move(pstructs));

        ExToolDef etd{};
        etd.name        = toolNameBufs.back().constData();
        etd.description = toolDescBufs.back().constData();
        etd.params      = paramStructs.back().empty() ? nullptr
                              : paramStructs.back().data();
        etd.param_count = static_cast<int32_t>(paramStructs.back().size());
        tools.push_back(etd);
    }

    ExAIRequest exReq{};
    exReq.request_id        = reqIdUtf8.constData();
    exReq.intent            = static_cast<int32_t>(request.intent);
    exReq.agent_mode        = request.agentMode ? 1 : 0;
    exReq.user_prompt       = promptUtf8.constData();
    exReq.workspace_root    = wsRootUtf8.constData();
    exReq.active_file_path  = fileUtf8.constData();
    exReq.language_id       = langUtf8.constData();
    exReq.selected_text     = selUtf8.constData();
    exReq.full_file_content = contentUtf8.constData();
    exReq.cursor_line       = request.cursorLine;
    exReq.cursor_column     = request.cursorColumn;
    exReq.reasoning_effort  = effortUtf8.constData();
    exReq.messages          = msgs.empty() ? nullptr : msgs.data();
    exReq.message_count     = static_cast<int32_t>(msgs.size());
    exReq.tools             = tools.empty() ? nullptr : tools.data();
    exReq.tool_count        = static_cast<int32_t>(tools.size());

    m_vtable.send_request(m_vtable.ctx, &exReq);
}

void CAbiProviderAdapter::cancelRequest(const QString &requestId) {
    if (m_vtable.cancel_request)
        m_vtable.cancel_request(m_vtable.ctx, requestId.toUtf8().constData());
}

void CAbiProviderAdapter::bridgeDelta(const QString &requestId,
                                       const QString &text) {
    emit responseDelta(requestId, text);
}

void CAbiProviderAdapter::bridgeThinking(const QString &requestId,
                                          const QString &text) {
    emit thinkingDelta(requestId, text);
}

void CAbiProviderAdapter::bridgeFinished(const QString &requestId,
                                          const AgentResponse &resp) {
    emit responseFinished(requestId, resp);
}

void CAbiProviderAdapter::bridgeError(const QString &requestId,
                                       const AgentError &err) {
    emit responseError(requestId, err);
}

void CAbiProviderAdapter::bridgeAvailabilityChanged(bool available) {
    emit availabilityChanged(available);
}

void CAbiProviderAdapter::bridgeModelsChanged() {
    emit modelsChanged();
}

} // namespace cabi
