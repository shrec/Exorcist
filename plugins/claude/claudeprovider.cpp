#include "claudeprovider.h"
#include "sseparser.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSettings>
#include <QSysInfo>
#include <QTimer>

// ── Endpoints / constants ─────────────────────────────────────────────────────

static const char kApiUrl[]        = "https://api.anthropic.com/v1/messages";
static const char kModelsUrl[]     = "https://api.anthropic.com/v1/models";
static const char kAnthropicVer[]  = "2023-06-01";

// Service name used in SecureKeyStorage.
static const char kApiKeyService[] = "claude.apiKey";

// Setting keys.
static const char kSettingModel[]            = "ai/claude/model";
static const char kSettingMaxTokens[]        = "ai/claude/max_tokens";
static const char kSettingTemperature[]      = "ai/claude/temperature";
static const char kSettingTemperatureSet[]   = "ai/claude/temperature_set";
static const char kSettingSystemPrompt[]     = "ai/claude/system_prompt";
static const char kSettingReasoningEffort[]  = "ai/claude/reasoning_effort";
static const char kSettingApiKeyMigration[]  = "ai/claude/api_key";  // legacy

// Default model — Claude Sonnet 4.5 is the recommended general-purpose
// coding model as of late 2025.  Opus / Haiku selectable via settings.
static const char kDefaultModel[] = "claude-sonnet-4-5-20250929";

// Curated default model list (used until /v1/models populates the real one).
static const char *kSeedModelIds[] = {
    "claude-opus-4-5-20250929",
    "claude-sonnet-4-5-20250929",
    "claude-haiku-4-5-20251001",
    "claude-opus-4-1-20250805",
    "claude-sonnet-4-20250514",
    "claude-3-5-haiku-latest",
    nullptr,
};

// ── System prompts ────────────────────────────────────────────────────────────

static const char kSystemPrompt[] =
    "You are Claude, an AI coding assistant integrated into the Exorcist IDE. "
    "Help users write, understand, debug, and refactor code. Be concise and "
    "provide working code. When the user shares file context or selected code, "
    "analyze it carefully before responding.";

static const char kAgentSystemPrompt[] =
    "You are Claude acting as a code agent in the Exorcist IDE — a fully-"
    "featured agentic coding assistant comparable to \"Claude Code\".\n"
    "\n"
    "You can read, search, edit, and create files in the user's workspace by "
    "calling the tools made available to you below.  Always prefer using tools "
    "to gather concrete information before proposing changes — never guess "
    "file contents or call sites.  When making edits, be precise: provide "
    "complete file paths, exact line numbers, and minimal diffs.\n"
    "\n"
    "Workflow:\n"
    "  1. Understand the request.  Ask clarifying questions only if blocked.\n"
    "  2. Use tools to inspect relevant files, search the codebase, or run "
    "commands.\n"
    "  3. Plan the change in 1-3 sentences before editing.\n"
    "  4. Apply edits via the appropriate edit tool.  Keep changes focused.\n"
    "  5. Verify (build/tests) when possible, then summarize what changed and "
    "why.\n"
    "\n"
    "Be honest about uncertainty.  If a tool returns an error, surface it and "
    "adapt.  Do not fabricate file contents, function signatures, or tool "
    "outputs.";

// ── Constructor ───────────────────────────────────────────────────────────────

ClaudeProvider::ClaudeProvider(QObject *parent)
    : m_sseParser(new SseParser(this))
{
    if (parent)
        setParent(parent);

    QSettings s;
    m_model = s.value(QLatin1String(kSettingModel),
                      QLatin1String(kDefaultModel)).toString();
    m_maxTokensOverride = s.value(QLatin1String(kSettingMaxTokens), 0).toInt();
    m_temperatureSet = s.value(QLatin1String(kSettingTemperatureSet), false).toBool();
    m_temperature = s.value(QLatin1String(kSettingTemperature), 1.0).toDouble();
    m_userSystemPromptOverride =
        s.value(QLatin1String(kSettingSystemPrompt)).toString();
    m_reasoningEffort =
        s.value(QLatin1String(kSettingReasoningEffort)).toString();

    seedDefaultModels();

    // Anthropic SSE event handler — parses text_delta, thinking_delta,
    // tool_use blocks, and message_stop.
    connect(m_sseParser, &SseParser::eventReceived,
            this, [this](const QString &eventType, const QString &data) {
        Q_UNUSED(eventType)
        const QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();
        const QString type = obj[QLatin1String("type")].toString();

        if (type == QLatin1String("message_start")) {
            // usage.input_tokens lands here for incremental accounting.
            const QJsonObject msg = obj[QLatin1String("message")].toObject();
            const QJsonObject usage = msg[QLatin1String("usage")].toObject();
            if (!usage.isEmpty())
                m_promptTokens = usage[QLatin1String("input_tokens")].toInt();
            return;
        }

        if (type == QLatin1String("content_block_start")) {
            const QJsonObject cb = obj[QLatin1String("content_block")].toObject();
            const QString blockType = cb[QLatin1String("type")].toString();
            if (blockType == QLatin1String("tool_use")) {
                m_inToolUse = true;
                m_currentToolUse = {};
                m_currentToolUse.id   = cb[QLatin1String("id")].toString();
                m_currentToolUse.name = cb[QLatin1String("name")].toString();
            }
        } else if (type == QLatin1String("content_block_delta")) {
            const QJsonObject delta = obj[QLatin1String("delta")].toObject();
            const QString deltaType = delta[QLatin1String("type")].toString();
            if (deltaType == QLatin1String("text_delta")) {
                const QString text = delta[QLatin1String("text")].toString();
                if (!text.isEmpty()) {
                    m_accumulated += text;
                    emit responseDelta(m_activeRequestId, text);
                }
            } else if (deltaType == QLatin1String("thinking_delta")) {
                const QString text = delta[QLatin1String("thinking")].toString();
                if (!text.isEmpty()) {
                    m_accumulatedThinking += text;
                    emit thinkingDelta(m_activeRequestId, text);
                }
            } else if (deltaType == QLatin1String("input_json_delta")) {
                m_currentToolUse.argumentsJson +=
                    delta[QLatin1String("partial_json")].toString();
            }
        } else if (type == QLatin1String("content_block_stop")) {
            if (m_inToolUse) {
                if (m_currentToolUse.argumentsJson.isEmpty())
                    m_currentToolUse.argumentsJson = QStringLiteral("{}");
                m_pendingToolCalls.append(m_currentToolUse);
                m_currentToolUse = {};
                m_inToolUse = false;
            }
        } else if (type == QLatin1String("message_delta")
                   || type == QLatin1String("message_stop")) {
            const QJsonObject delta = obj[QLatin1String("delta")].toObject();
            const QString stopReason = delta[QLatin1String("stop_reason")].toString();

            const QJsonObject usage = obj[QLatin1String("usage")].toObject();
            if (!usage.isEmpty())
                m_completionTokens = usage[QLatin1String("output_tokens")].toInt();

            if (type == QLatin1String("message_stop")
                || stopReason == QLatin1String("end_turn")
                || stopReason == QLatin1String("tool_use")
                || stopReason == QLatin1String("stop_sequence")
                || stopReason == QLatin1String("max_tokens")) {

                AgentResponse resp;
                resp.requestId       = m_activeRequestId;
                resp.text            = m_accumulated;
                resp.thinkingContent = m_accumulatedThinking;
                resp.promptTokens    = m_promptTokens;
                resp.completionTokens = m_completionTokens;
                resp.totalTokens     = m_promptTokens + m_completionTokens;

                for (const auto &ptc : m_pendingToolCalls) {
                    ToolCall tc;
                    tc.id        = ptc.id;
                    tc.name      = ptc.name;
                    tc.arguments = ptc.argumentsJson;
                    resp.toolCalls.append(tc);
                }

                // Clear state BEFORE emitting — the connected slot
                // (AgentController) may synchronously call sendRequest()
                // which creates a new QNetworkReply.  We must fully detach
                // the old reply first to prevent use-after-free in
                // Qt6Network's connection pool.
                const QString reqId = m_activeRequestId;
                m_activeRequestId.clear();
                cleanupActiveReply();
                m_accumulated.clear();
                m_accumulatedThinking.clear();
                m_pendingToolCalls.clear();
                m_promptTokens = 0;
                m_completionTokens = 0;

                if (!resp.toolCalls.isEmpty()) {
                    // Defer emission so the old reply is fully destroyed
                    // before AgentController issues the tool-continuation
                    // call.
                    QTimer::singleShot(0, this, [this, reqId, resp] {
                        emit responseFinished(reqId, resp);
                    });
                } else {
                    emit responseFinished(reqId, resp);
                }
            }
        }
    });
}

// ── IAgentProvider identity ───────────────────────────────────────────────────

QString ClaudeProvider::id() const
{
    return QStringLiteral("claude");
}

QString ClaudeProvider::displayName() const
{
    return QStringLiteral("Anthropic Claude");
}

AgentCapabilities ClaudeProvider::capabilities() const
{
    AgentCapabilities caps =
          AgentCapability::Chat
        | AgentCapability::Streaming
        | AgentCapability::InlineCompletion
        | AgentCapability::CodeEdit
        | AgentCapability::ToolCalling
        | AgentCapability::TestGeneration;

    if (modelSupportsVision(m_model))
        caps |= AgentCapability::Vision;
    if (modelSupportsThinking(m_model))
        caps |= AgentCapability::Thinking;
    return caps;
}

bool ClaudeProvider::isAvailable() const
{
    return m_available;
}

// ── Models ────────────────────────────────────────────────────────────────────

QStringList ClaudeProvider::availableModels() const
{
    if (!m_models.isEmpty())
        return m_models;
    return {m_model};
}

QString ClaudeProvider::currentModel() const
{
    return m_model;
}

void ClaudeProvider::setModel(const QString &model)
{
    if (m_model == model)
        return;
    m_model = model;
    QSettings().setValue(QLatin1String(kSettingModel), model);
}

QList<ModelInfo> ClaudeProvider::modelInfoList() const
{
    return m_modelInfos;
}

ModelInfo ClaudeProvider::currentModelInfo() const
{
    for (const auto &mi : m_modelInfos) {
        if (mi.id == m_model)
            return mi;
    }
    ModelInfo fallback;
    fallback.id   = m_model;
    fallback.name = m_model;
    fallback.vendor = QStringLiteral("Anthropic");
    fallback.capabilities.streaming = true;
    fallback.capabilities.toolCalls = true;
    fallback.capabilities.vision    = modelSupportsVision(m_model);
    fallback.capabilities.thinking  = modelSupportsThinking(m_model);
    fallback.capabilities.maxOutputTokens = maxOutputTokensForModel(m_model);
    fallback.capabilities.maxContextWindowTokens = 200000;
    return fallback;
}

void ClaudeProvider::seedDefaultModels()
{
    m_models.clear();
    m_modelInfos.clear();
    for (int i = 0; kSeedModelIds[i] != nullptr; ++i) {
        const QString id = QString::fromLatin1(kSeedModelIds[i]);
        m_models.append(id);

        ModelInfo mi;
        mi.id     = id;
        mi.vendor = QStringLiteral("Anthropic");
        mi.modelPickerEnabled = true;
        mi.isChatDefault  = (id == QLatin1String(kDefaultModel));
        mi.isChatFallback = id.contains(QLatin1String("haiku"));

        if (id.contains(QLatin1String("opus"))) {
            mi.name = QStringLiteral("Claude Opus") +
                      (id.contains(QLatin1String("4-5")) ? QStringLiteral(" 4.5")
                       : id.contains(QLatin1String("4-1")) ? QStringLiteral(" 4.1")
                                                            : QStringLiteral(" 4"));
            mi.billing.isPremium  = true;
            mi.billing.multiplier = 5.0;
        } else if (id.contains(QLatin1String("sonnet"))) {
            mi.name = QStringLiteral("Claude Sonnet") +
                      (id.contains(QLatin1String("4-5")) ? QStringLiteral(" 4.5")
                                                          : QStringLiteral(" 4"));
            mi.billing.multiplier = 1.0;
        } else {
            mi.name = QStringLiteral("Claude Haiku 4.5");
            mi.billing.multiplier = 0.25;
            mi.isChatFallback = true;
        }

        mi.capabilities.type      = QStringLiteral("chat");
        mi.capabilities.family    = id.contains(QLatin1String("opus"))
                                  ? QStringLiteral("claude-opus")
                                  : id.contains(QLatin1String("sonnet"))
                                      ? QStringLiteral("claude-sonnet")
                                      : QStringLiteral("claude-haiku");
        mi.capabilities.streaming        = true;
        mi.capabilities.toolCalls        = true;
        mi.capabilities.vision           = modelSupportsVision(id);
        mi.capabilities.thinking         = modelSupportsThinking(id);
        mi.capabilities.adaptiveThinking = mi.capabilities.thinking;
        mi.capabilities.maxOutputTokens       = maxOutputTokensForModel(id);
        mi.capabilities.maxContextWindowTokens = 200000;
        mi.capabilities.maxPromptTokens        = 200000;
        mi.supportedEndpoints = {QStringLiteral("/v1/messages")};

        m_modelInfos.append(mi);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void ClaudeProvider::initialize()
{
    // Priority order for resolving the API key:
    //   1. ANTHROPIC_API_KEY env var (developer override)
    //   2. SecureKeyStorage (Windows Credential Manager, etc.)
    //   3. Legacy QSettings fallback (auto-migrates to secure storage)
    m_apiKey = QString::fromUtf8(qgetenv("ANTHROPIC_API_KEY"));

    if (m_apiKey.isEmpty() && m_keyRetrieve)
        m_apiKey = m_keyRetrieve(QString::fromLatin1(kApiKeyService));

    if (m_apiKey.isEmpty()) {
        QSettings s;
        const QString legacy = s.value(QLatin1String(kSettingApiKeyMigration)).toString();
        if (!legacy.isEmpty()) {
            m_apiKey = legacy;
            if (m_keyStore)
                m_keyStore(QString::fromLatin1(kApiKeyService), legacy);
            s.remove(QLatin1String(kSettingApiKeyMigration));
            s.sync();
        }
    }

    const bool was = m_available;
    m_available = !m_apiKey.isEmpty();
    if (m_available != was)
        emit availabilityChanged(m_available);

    if (m_available)
        fetchModels();
}

void ClaudeProvider::shutdown()
{
    cancelRequest(m_activeRequestId);
    m_available = false;
}

void ClaudeProvider::setKeyStorageCallbacks(KeyStoreFn store, KeyRetrieveFn retrieve, KeyDeleteFn remove)
{
    m_keyStore    = std::move(store);
    m_keyRetrieve = std::move(retrieve);
    m_keyDelete   = std::move(remove);
}

void ClaudeProvider::setApiKey(const QString &key)
{
    m_apiKey = key.trimmed();
    if (m_keyStore && !m_apiKey.isEmpty())
        m_keyStore(QString::fromLatin1(kApiKeyService), m_apiKey);

    const bool was = m_available;
    m_available = !m_apiKey.isEmpty();
    if (m_available != was)
        emit availabilityChanged(m_available);

    if (m_available)
        fetchModels();
}

void ClaudeProvider::clearApiKey()
{
    m_apiKey.clear();
    if (m_keyDelete)
        m_keyDelete(QString::fromLatin1(kApiKeyService));
    if (m_available) {
        m_available = false;
        emit availabilityChanged(false);
    }
}

void ClaudeProvider::fetchModels()
{
    QNetworkRequest req{QUrl{QString::fromLatin1(kModelsUrl)}};
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setRawHeader("x-api-key", m_apiKey.toUtf8());
    req.setRawHeader("anthropic-version", kAnthropicVer);
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QJsonArray data = obj[QLatin1String("data")].toArray();
        if (data.isEmpty())
            return;

        QStringList ids;
        QList<ModelInfo> infos;
        for (const QJsonValue &v : data) {
            const QJsonObject o = v.toObject();
            const QString id = o[QLatin1String("id")].toString();
            if (id.isEmpty())
                continue;
            ids.append(id);

            ModelInfo mi;
            mi.id      = id;
            mi.name    = o[QLatin1String("display_name")].toString();
            if (mi.name.isEmpty()) mi.name = id;
            mi.vendor  = QStringLiteral("Anthropic");
            mi.modelPickerEnabled = true;
            mi.capabilities.type      = QStringLiteral("chat");
            mi.capabilities.streaming = true;
            mi.capabilities.toolCalls = true;
            mi.capabilities.vision    = modelSupportsVision(id);
            mi.capabilities.thinking  = modelSupportsThinking(id);
            mi.capabilities.adaptiveThinking = mi.capabilities.thinking;
            mi.capabilities.maxOutputTokens       = maxOutputTokensForModel(id);
            mi.capabilities.maxContextWindowTokens = 200000;
            mi.supportedEndpoints = {QStringLiteral("/v1/messages")};
            if (id.contains(QLatin1String("opus"))) {
                mi.billing.isPremium  = true;
                mi.billing.multiplier = 5.0;
            } else if (id.contains(QLatin1String("haiku"))) {
                mi.billing.multiplier = 0.25;
            }
            infos.append(mi);
        }

        if (!ids.isEmpty()) {
            m_models     = ids;
            m_modelInfos = infos;
            if (!m_models.contains(m_model))
                m_model = m_models.first();
            emit modelsChanged();
        }
    });
}

// ── Request building helpers ──────────────────────────────────────────────────

int ClaudeProvider::maxOutputTokensForModel(const QString &model)
{
    // Opus 4 / Sonnet 4 / Sonnet 4.5: 64K output.
    // Haiku 4.5: 8192.
    // 3.5 Haiku and older: 8192.  Unknown: 16384.
    if (model.contains(QLatin1String("opus")) ||
        model.contains(QLatin1String("sonnet-4")))
        return 64000;
    if (model.contains(QLatin1String("haiku")))
        return 8192;
    return 16384;
}

bool ClaudeProvider::modelSupportsThinking(const QString &model)
{
    // Extended thinking is supported by Opus 4.x and Sonnet 4.x.
    return model.contains(QLatin1String("opus"))
        || model.contains(QLatin1String("sonnet-4"));
}

bool ClaudeProvider::modelSupportsVision(const QString &model)
{
    // All Claude 3+ multimodal models accept image input.  Haiku 3.5 onwards
    // and every 4.x model supports vision.  The pre-3.5 Haiku does not.
    return !model.contains(QLatin1String("3-haiku"));
}

QString ClaudeProvider::buildSystemPrompt(const AgentRequest &req) const
{
    QString prompt = QString::fromLatin1(
        req.agentMode ? kAgentSystemPrompt : kSystemPrompt);

    // Workspace context block — gives Claude project root, OS, language hint.
    if (!req.workspaceRoot.isEmpty() ||
        !req.activeFilePath.isEmpty() ||
        !req.languageId.isEmpty()) {
        prompt += QStringLiteral("\n\n<environment>\n");
        prompt += QStringLiteral("os: %1 %2\n")
                      .arg(QSysInfo::productType(),
                           QSysInfo::productVersion());
        if (!req.workspaceRoot.isEmpty()) {
            prompt += QStringLiteral("workspace_root: %1\n").arg(req.workspaceRoot);
            prompt += QStringLiteral("workspace_name: %1\n")
                          .arg(QFileInfo(req.workspaceRoot).fileName());
        }
        if (!req.activeFilePath.isEmpty())
            prompt += QStringLiteral("active_file: %1\n").arg(req.activeFilePath);
        if (!req.languageId.isEmpty())
            prompt += QStringLiteral("language: %1\n").arg(req.languageId);
        if (req.cursorLine >= 0)
            prompt += QStringLiteral("cursor_line: %1\n").arg(req.cursorLine + 1);
        prompt += QStringLiteral("</environment>");
    }

    // User-defined system prompt addendum (claude.systemPrompt setting).
    if (!m_userSystemPromptOverride.isEmpty()) {
        prompt += QStringLiteral("\n\n<user_instructions>\n");
        prompt += m_userSystemPromptOverride;
        prompt += QStringLiteral("\n</user_instructions>");
    }

    return prompt;
}

QString ClaudeProvider::buildUserContent(const AgentRequest &req) const
{
    QString content;

    if (!req.activeFilePath.isEmpty()) {
        content += QStringLiteral("Current file: %1\n").arg(req.activeFilePath);
        if (!req.languageId.isEmpty())
            content += QStringLiteral("Language: %1\n").arg(req.languageId);
        if (req.cursorLine >= 0) {
            content += QStringLiteral("Cursor: line %1, col %2\n")
                           .arg(req.cursorLine + 1)
                           .arg(req.cursorColumn + 1);
        }
    }

    if (!req.selectedText.isEmpty()) {
        content += QStringLiteral("\nSelected code:\n```%1\n%2\n```\n\n")
                       .arg(req.languageId, req.selectedText);
    } else if (!req.fullFileContent.isEmpty()) {
        content += QStringLiteral("\nFile content:\n```%1\n%2\n```\n\n")
                       .arg(req.languageId, req.fullFileContent);
    }

    if (!req.diagnostics.isEmpty()) {
        content += QStringLiteral("\nDiagnostics in current file:\n");
        for (const auto &d : req.diagnostics) {
            const char *sev = d.severity == AgentDiagnostic::Severity::Error
                                  ? "error"
                                  : d.severity == AgentDiagnostic::Severity::Warning
                                        ? "warning"
                                        : "info";
            content += QStringLiteral("  [%1] %2:%3 %4\n")
                           .arg(QString::fromLatin1(sev))
                           .arg(d.line + 1)
                           .arg(d.column + 1)
                           .arg(d.message);
        }
        content += QStringLiteral("\n");
    }

    content += req.userPrompt;
    return content;
}

QJsonArray ClaudeProvider::buildTools(const AgentRequest &req) const
{
    QJsonArray tools;
    for (const auto &td : req.tools) {
        QJsonObject toolDef;
        toolDef[QStringLiteral("name")]        = td.name;
        toolDef[QStringLiteral("description")] = td.description;

        // Anthropic expects an `input_schema` JSON Schema.  Prefer the
        // already-parsed schema; fall back to flat reconstruction.
        if (!td.inputSchema.isEmpty()) {
            toolDef[QStringLiteral("input_schema")] = td.inputSchema;
        } else {
            QJsonObject props;
            QJsonArray  required;
            for (const auto &p : td.parameters) {
                QJsonObject prop{
                    {QStringLiteral("type"),        p.type},
                    {QStringLiteral("description"), p.description}
                };
                props[p.name] = prop;
                if (p.required)
                    required.append(p.name);
            }
            QJsonObject schema{
                {QStringLiteral("type"),       QStringLiteral("object")},
                {QStringLiteral("properties"), props}
            };
            if (!required.isEmpty())
                schema[QStringLiteral("required")] = required;
            toolDef[QStringLiteral("input_schema")] = schema;
        }
        tools.append(toolDef);
    }
    return tools;
}

QJsonArray ClaudeProvider::buildMessages(const AgentRequest &request) const
{
    QJsonArray messages;

    auto pushAssistant = [&](const AgentMessage &msg) {
        QJsonArray contentBlocks;
        if (!msg.content.isEmpty()) {
            contentBlocks.append(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("text")},
                {QStringLiteral("text"), msg.content}
            });
        }
        for (const auto &tc : msg.toolCalls) {
            QJsonObject toolBlock;
            toolBlock[QStringLiteral("type")] = QStringLiteral("tool_use");
            toolBlock[QStringLiteral("id")]   = tc.id;
            toolBlock[QStringLiteral("name")] = tc.name;
            toolBlock[QStringLiteral("input")] =
                QJsonDocument::fromJson(tc.arguments.toUtf8()).object();
            contentBlocks.append(toolBlock);
        }
        if (!contentBlocks.isEmpty()) {
            messages.append(QJsonObject{
                {QStringLiteral("role"),    QStringLiteral("assistant")},
                {QStringLiteral("content"), contentBlocks}
            });
        }
    };

    for (const auto &msg : request.conversationHistory) {
        switch (msg.role) {
        case AgentMessage::Role::System:
            // Anthropic puts system text in a top-level field; merging is
            // handled by sendRequest().  Skip here.
            break;
        case AgentMessage::Role::User:
            messages.append(QJsonObject{
                {QStringLiteral("role"),    QStringLiteral("user")},
                {QStringLiteral("content"), msg.content}
            });
            break;
        case AgentMessage::Role::Assistant:
            pushAssistant(msg);
            break;
        case AgentMessage::Role::Tool:
            // Tool results are framed as user-role tool_result blocks.
            messages.append(QJsonObject{
                {QStringLiteral("role"), QStringLiteral("user")},
                {QStringLiteral("content"), QJsonArray{QJsonObject{
                    {QStringLiteral("type"),        QStringLiteral("tool_result")},
                    {QStringLiteral("tool_use_id"), msg.toolCallId},
                    {QStringLiteral("content"),     msg.content}
                }}}
            });
            break;
        }
    }

    if (request.appendUserMessage) {
        messages.append(QJsonObject{
            {QStringLiteral("role"),    QStringLiteral("user")},
            {QStringLiteral("content"), buildUserContent(request)}
        });
    }
    return messages;
}

// ── Send request ──────────────────────────────────────────────────────────────

void ClaudeProvider::sendRequest(const AgentRequest &request)
{
    if (!m_available) {
        AgentError err;
        err.requestId = request.requestId;
        err.code      = AgentError::Code::AuthError;
        err.message   = tr("Claude is not available. Set the API key via "
                           "claude.editApiKey or ANTHROPIC_API_KEY.");
        emit responseError(request.requestId, err);
        return;
    }

    // System prompt — agent vs chat, plus workspace + user override.
    QString systemText = buildSystemPrompt(request);
    // Merge any history-level System messages.
    for (const auto &msg : request.conversationHistory) {
        if (msg.role == AgentMessage::Role::System && !msg.content.isEmpty()) {
            systemText += QStringLiteral("\n\n");
            systemText += msg.content;
        }
    }

    const QJsonArray messages = buildMessages(request);
    const int maxTokens = m_maxTokensOverride > 0
                              ? m_maxTokensOverride
                              : maxOutputTokensForModel(m_model);

    QJsonObject body;
    body[QStringLiteral("model")]      = m_model;
    body[QStringLiteral("max_tokens")] = maxTokens;
    body[QStringLiteral("system")]     = systemText;
    body[QStringLiteral("messages")]   = messages;
    body[QStringLiteral("stream")]     = true;

    if (m_temperatureSet)
        body[QStringLiteral("temperature")] = m_temperature;

    // Tool list — assembled by AgentOrchestrator from ToolRegistry.
    if (!request.tools.isEmpty()) {
        body[QStringLiteral("tools")] = buildTools(request);
        // Allow Claude to choose which tool to call (default policy).
        body[QStringLiteral("tool_choice")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("auto")}
        };
    }

    // Extended thinking — supported by Opus/Sonnet 4 family.  Per-request
    // reasoningEffort overrides global setting.
    const QString effort = !request.reasoningEffort.isEmpty()
                               ? request.reasoningEffort
                               : m_reasoningEffort;
    if (!effort.isEmpty() && modelSupportsThinking(m_model)) {
        int budget = 0;
        if      (effort == QLatin1String("low"))    budget = 4096;
        else if (effort == QLatin1String("medium")) budget = 8192;
        else if (effort == QLatin1String("high"))   budget = 16384;
        if (budget > 0) {
            // Anthropic requires temperature=1 when extended thinking is on.
            body.remove(QStringLiteral("temperature"));
            body[QStringLiteral("thinking")] = QJsonObject{
                {QStringLiteral("type"),          QStringLiteral("enabled")},
                {QStringLiteral("budget_tokens"), budget}
            };
            // budget must be < max_tokens.
            if (maxTokens <= budget)
                body[QStringLiteral("max_tokens")] = budget + 4096;
        }
    }

    QNetworkRequest netReq{QUrl{QLatin1String(kApiUrl)}};
    netReq.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    netReq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("application/json"));
    netReq.setRawHeader("x-api-key",         m_apiKey.toUtf8());
    netReq.setRawHeader("anthropic-version", kAnthropicVer);
    netReq.setRawHeader("Accept",            "text/event-stream");

    m_activeRequestId = request.requestId;
    m_accumulated.clear();
    m_accumulatedThinking.clear();
    m_pendingToolCalls.clear();
    m_currentToolUse = {};
    m_inToolUse = false;
    m_promptTokens = 0;
    m_completionTokens = 0;
    m_sseParser->reset();

    QNetworkReply *reply = m_nam.post(netReq,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_activeReply = reply;
    connectReply(reply);
}

void ClaudeProvider::cancelRequest(const QString &requestId)
{
    if (requestId.isEmpty() || requestId != m_activeRequestId)
        return;
    cleanupActiveReply();
    m_activeRequestId.clear();
    m_accumulated.clear();
    m_accumulatedThinking.clear();
    m_pendingToolCalls.clear();
    m_promptTokens = 0;
    m_completionTokens = 0;
}

void ClaudeProvider::cleanupActiveReply()
{
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

// ── Reply wiring ──────────────────────────────────────────────────────────────

void ClaudeProvider::connectReply(QNetworkReply *reply)
{
    QPointer<QNetworkReply> safeReply(reply);

    connect(reply, &QNetworkReply::readyRead, this, [this, safeReply] {
        if (!safeReply || safeReply != m_activeReply)
            return;
        m_sseParser->feed(safeReply->readAll());
    });

    connect(reply, &QNetworkReply::finished, this, [this, safeReply] {
        if (!safeReply)
            return;
        if (m_activeReply != safeReply) {
            // Orphan reply — already cleaned up by message_stop handler.
            return;
        }

        const int status =
            safeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = safeReply->error();
        const QString errorStr = safeReply->errorString();
        const QByteArray body  = safeReply->readAll();

        cleanupActiveReply();

        if (m_activeRequestId.isEmpty())
            return; // Already handled by message_stop event

        if (error == QNetworkReply::NoError) {
            AgentResponse resp;
            resp.requestId = m_activeRequestId;
            resp.text      = m_accumulated;
            resp.thinkingContent = m_accumulatedThinking;
            const QString reqId = m_activeRequestId;
            m_activeRequestId.clear();
            m_accumulated.clear();
            m_accumulatedThinking.clear();
            emit responseFinished(reqId, resp);
        } else {
            AgentError err;
            err.requestId = m_activeRequestId;

            // Try to extract Anthropic's structured error message.
            QString detailMsg = errorStr;
            const QJsonObject errObj =
                QJsonDocument::fromJson(body).object()
                    [QLatin1String("error")].toObject();
            if (!errObj.isEmpty()) {
                const QString msg = errObj[QLatin1String("message")].toString();
                if (!msg.isEmpty())
                    detailMsg = msg;
            }
            err.message = detailMsg;

            if (status == 401 || status == 403)
                err.code = AgentError::Code::AuthError;
            else if (status == 429)
                err.code = AgentError::Code::RateLimited;
            else if (error == QNetworkReply::OperationCanceledError)
                err.code = AgentError::Code::Cancelled;
            else
                err.code = AgentError::Code::NetworkError;

            const QString reqId = m_activeRequestId;
            m_activeRequestId.clear();
            m_accumulated.clear();
            m_accumulatedThinking.clear();
            emit responseError(reqId, err);
        }
    });
}

// ── Inline completion ─────────────────────────────────────────────────────────

void ClaudeProvider::requestCompletion(const QString &filePath,
                                       const QString &languageId,
                                       const QString &textBefore,
                                       const QString &textAfter)
{
    if (!m_available)
        return;

    // Use a small Haiku model for inline completion when the user hasn't
    // explicitly asked for something else.  If the current model is Opus
    // or Sonnet, prefer Haiku for latency.
    QString completionModel = m_model;
    for (const auto &mi : m_modelInfos) {
        if (mi.id.contains(QLatin1String("haiku"))) {
            completionModel = mi.id;
            break;
        }
    }

    const QString systemPrompt = QStringLiteral(
        "You are a code completion engine.  Given the code context, output "
        "ONLY the code that should be inserted at the cursor position.  Do "
        "not include explanations, markdown, or backticks.  Output an empty "
        "string if no completion is appropriate.");

    const QString userMsg = QStringLiteral(
        "File: %1\nLanguage: %2\n\n"
        "Code before cursor:\n```\n%3\n```\n\n"
        "Code after cursor:\n```\n%4\n```\n\n"
        "Complete the code at the cursor position:")
        .arg(filePath, languageId, textBefore, textAfter);

    QJsonObject body;
    body[QStringLiteral("model")]      = completionModel;
    body[QStringLiteral("max_tokens")] = 256;
    body[QStringLiteral("system")]     = systemPrompt;
    body[QStringLiteral("stream")]     = false;
    body[QStringLiteral("messages")]   = QJsonArray{QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("user")},
        {QStringLiteral("content"), userMsg}
    }};

    QNetworkRequest netReq{QUrl{QLatin1String(kApiUrl)}};
    netReq.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    netReq.setHeader(QNetworkRequest::ContentTypeHeader,
                     QStringLiteral("application/json"));
    netReq.setRawHeader("x-api-key",         m_apiKey.toUtf8());
    netReq.setRawHeader("anthropic-version", kAnthropicVer);

    QNetworkReply *reply = m_nam.post(netReq,
        QJsonDocument(body).toJson(QJsonDocument::Compact));
    QPointer<QNetworkReply> safeReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, safeReply] {
        if (!safeReply) return;
        safeReply->deleteLater();
        if (safeReply->error() != QNetworkReply::NoError)
            return;

        const QJsonObject obj =
            QJsonDocument::fromJson(safeReply->readAll()).object();
        const QJsonArray content = obj[QLatin1String("content")].toArray();
        QString text;
        for (const QJsonValue &v : content) {
            const QJsonObject blk = v.toObject();
            if (blk[QLatin1String("type")].toString() == QLatin1String("text"))
                text += blk[QLatin1String("text")].toString();
        }
        text = text.trimmed();
        if (!text.isEmpty())
            emit completionReady(text);
    });
}
