#include "copilotprovider.h"
#include "copilotauthdialog.h"
#include "sseparser.h"

#include <QApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrlQuery>

#include <memory>

Q_LOGGING_CATEGORY(lcCopilot, "exorcist.copilot")

// ── Endpoints (from Copilot Chat extension source) ────────────────────────────

static const char kTokenUrl[]   = "https://api.github.com/copilot_internal/v2/token";
static const char kChatUrl[]    = "https://api.githubcopilot.com/chat/completions";
static const char kResponsesUrl[] = "https://api.githubcopilot.com/responses";
static const char kModelsUrl[]  = "https://api.githubcopilot.com/models";

// Model refresh interval: 10 minutes (matches VS Code Copilot Chat)
static constexpr int kModelRefreshMs = 10 * 60 * 1000;
// Token refresh interval: 25 minutes (tokens expire at 30 min)
static constexpr int kTokenRefreshMs = 25 * 60 * 1000;
// Idle timeout: 60 seconds without data → cancel
static constexpr int kIdleTimeoutMs = 60 * 1000;
// Base delay for exponential backoff (ms)
static constexpr int kRetryBaseMs = 1000;

// Secure storage service keys
static const QString kGithubTokenKey  = QStringLiteral("copilot/github_token");
static const QString kRefreshTokenKey = QStringLiteral("copilot/refresh_token");

// ── System prompts ────────────────────────────────────────────────────────────

static const char kSystemPrompt[] =
    "You are GitHub Copilot, an AI coding assistant integrated into the "
    "Exorcist IDE. Help users write, understand, debug, and refactor code. "
    "Be concise and provide working code. When the user shares file context "
    "or selected code, analyze it carefully before responding.";

static const char kAgentSystemPrompt[] =
    "You are GitHub Copilot acting as a code agent in the Exorcist IDE. "
    "You can read files, propose edits, and help the user implement changes "
    "across their codebase. When asked to make changes, provide complete "
    "code with clear file paths and line references. Think step by step, "
    "consider the full project context, and make precise, targeted edits. "
    "Always explain what you changed and why.";

// ── Constructor ───────────────────────────────────────────────────────────────

CopilotProvider::CopilotProvider(QObject *parent)
    : m_sseParser(new SseParser(this))
{
    if (parent)
        setParent(parent);

    m_model = QSettings().value(QStringLiteral("ai/copilot/model"),
                                QStringLiteral("gpt-4o")).toString();

    // SSE events → streaming
    connect(m_sseParser, &SseParser::eventReceived,
            this, &CopilotProvider::handleSseEvent);
    connect(m_sseParser, &SseParser::done,
            this, &CopilotProvider::handleSseDone);

    // Periodic token refresh
    m_refreshTimer.setSingleShot(false);
    connect(&m_refreshTimer, &QTimer::timeout, this, [this] {
        if (!m_githubToken.isEmpty())
            refreshCopilotToken();
    });

    // Idle timeout for streaming requests
    m_idleTimer.setSingleShot(true);
    m_idleTimer.setInterval(kIdleTimeoutMs);
    connect(&m_idleTimer, &QTimer::timeout, this, [this] {
        if (!m_activeRequestId.isEmpty()) {
            qCWarning(lcCopilot) << "Request timed out (no data for"
                                 << kIdleTimeoutMs / 1000 << "seconds)";
            const QString reqId = m_activeRequestId;
            cancelRequest(reqId);
            AgentError err;
            err.requestId = reqId;
            err.code      = AgentError::Code::NetworkError;
            err.message   = tr("Request timed out — no response from server.");
            emit responseError(reqId, err);
        }
    });
}

// ── IAgentProvider identity ───────────────────────────────────────────────────

QString CopilotProvider::id() const
{
    return QStringLiteral("copilot");
}

QString CopilotProvider::displayName() const
{
    return QStringLiteral("GitHub Copilot");
}

AgentCapabilities CopilotProvider::capabilities() const
{
    return AgentCapability::Chat
         | AgentCapability::Streaming
         | AgentCapability::InlineCompletion
         | AgentCapability::CodeEdit
         | AgentCapability::ToolCalling;
}

bool CopilotProvider::isAvailable() const
{
    return m_available;
}

// ── Model selection ───────────────────────────────────────────────────────────

QStringList CopilotProvider::availableModels() const
{
    QStringList ids;
    for (const auto &mi : m_modelInfos) {
        if (mi.modelPickerEnabled)
            ids.append(mi.id);
    }
    if (ids.isEmpty())
        return {m_model};
    return ids;
}

QString CopilotProvider::currentModel() const
{
    return m_model;
}

void CopilotProvider::setModel(const QString &model)
{
    if (m_model != model) {
        m_model = model;
        QSettings().setValue(QStringLiteral("ai/copilot/model"), model);
    }
}

QList<ModelInfo> CopilotProvider::modelInfoList() const
{
    return m_modelInfos;
}

ModelInfo CopilotProvider::currentModelInfo() const
{
    for (const auto &mi : m_modelInfos) {
        if (mi.id == m_model)
            return mi;
    }
    // Fallback with minimal info
    ModelInfo fallback;
    fallback.id   = m_model;
    fallback.name = m_model;
    return fallback;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void CopilotProvider::initialize()
{
    // Skip re-initialization if we already have a valid token
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_available && m_token.isValid() && !m_token.isExpired(now))
        return;

    // Try environment variable first, then secure storage, then QSettings (migration)
    if (m_githubToken.isEmpty())
        m_githubToken = QString::fromUtf8(qgetenv("GITHUB_TOKEN"));
    if (m_githubToken.isEmpty() && m_keyRetrieve) {
        m_githubToken  = m_keyRetrieve(kGithubTokenKey);
        m_refreshToken = m_keyRetrieve(kRefreshTokenKey);
    }
    if (m_githubToken.isEmpty()) {
        // Migrate from old QSettings storage to secure storage
        QSettings settings;
        m_githubToken  = settings.value(QStringLiteral("ai/copilot/github_token")).toString();
        m_refreshToken = settings.value(QStringLiteral("ai/copilot/refresh_token")).toString();
        if (m_githubToken.isEmpty()) {
            m_githubToken = settings.value(QStringLiteral("ai/copilot/token")).toString();
        }
        if (!m_githubToken.isEmpty()) {
            // Migrate to secure storage and remove plaintext settings
            if (m_keyStore) {
                m_keyStore(kGithubTokenKey, m_githubToken);
                if (!m_refreshToken.isEmpty())
                    m_keyStore(kRefreshTokenKey, m_refreshToken);
            }
            settings.remove(QStringLiteral("ai/copilot/github_token"));
            settings.remove(QStringLiteral("ai/copilot/refresh_token"));
            settings.remove(QStringLiteral("ai/copilot/token"));
            settings.sync();
            qCInfo(lcCopilot) << "Migrated tokens from QSettings to secure storage";
        }
    }

    if (m_githubToken.isEmpty()) {
        qCInfo(lcCopilot) << "No GitHub token found, starting OAuth login";
        tryOAuthLogin();
        return;
    }

    qCInfo(lcCopilot) << "GitHub token loaded, exchanging for Copilot token";
    m_oauthRefreshAttempted = false;
    refreshCopilotToken();
}

void CopilotProvider::shutdown()
{
    m_refreshTimer.stop();
    cancelRequest(m_activeRequestId);
    m_available = false;
}

void CopilotProvider::setKeyStorageCallbacks(KeyStoreFn store, KeyRetrieveFn retrieve, KeyDeleteFn remove)
{
    m_keyStore    = std::move(store);
    m_keyRetrieve = std::move(retrieve);
    m_keyDelete   = std::move(remove);
}

// ── OAuth Device Flow ─────────────────────────────────────────────────────────

void CopilotProvider::tryOAuthLogin()
{
    QWidget *parentWidget = nullptr;
    for (QWidget *w : QApplication::topLevelWidgets()) {
        if (w->inherits("QMainWindow")) {
            parentWidget = w;
            break;
        }
    }

    auto *dialog = new CopilotAuthDialog(parentWidget);
    connect(dialog, &QDialog::accepted, this, [this, dialog] {
        m_githubToken  = dialog->githubToken();
        m_refreshToken = dialog->refreshToken();
        if (!m_githubToken.isEmpty()) {
            if (m_keyStore) {
                m_keyStore(kGithubTokenKey, m_githubToken);
                if (!m_refreshToken.isEmpty())
                    m_keyStore(kRefreshTokenKey, m_refreshToken);
            }
            qCInfo(lcCopilot) << "OAuth login succeeded, tokens saved";
            m_oauthRefreshAttempted = false;
            refreshCopilotToken();
        }
        dialog->deleteLater();
    });
    connect(dialog, &QDialog::rejected, this, [this, dialog] {
        m_available = false;
        emit availabilityChanged(false);
        qCInfo(lcCopilot) << "OAuth login cancelled";
        dialog->deleteLater();
    });
    dialog->show();
}

// ── Token exchange ────────────────────────────────────────────────────────────

void CopilotProvider::refreshCopilotToken()
{
    QNetworkRequest req{QUrl{QLatin1String(kTokenUrl)}};
    req.setRawHeader("Authorization", ("token " + m_githubToken).toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "GitHubCopilotChat/0.40.0");
    req.setRawHeader("Editor-Version", "vscode/1.100.0");
    req.setRawHeader("Editor-Plugin-Version", "copilot-chat/0.40.0");
    req.setRawHeader("Copilot-Integration-Id", "vscode-chat");
    req.setRawHeader("OpenAI-Intent", "conversation-panel");
    req.setRawHeader("X-GitHub-Api-Version", "2025-04-01");
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setAttribute(QNetworkRequest::Http2DirectAttribute, false);

    QNetworkReply *reply = m_nam.get(req);
    QPointer<QNetworkReply> safeReply(reply);
    connect(reply, &QNetworkReply::finished,
            this, [this, safeReply] {
        if (safeReply)
            handleTokenReply(safeReply);
    });
}

void CopilotProvider::handleTokenReply(QNetworkReply *reply)
{
    reply->deleteLater();

    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        const QByteArray errBody = reply->readAll();
        qCWarning(lcCopilot) << "Copilot token exchange failed:"
                             << status << reply->errorString()
                             << "body:" << errBody.left(300);

        // If 401/403, the GitHub OAuth token is expired or invalid
        if ((status == 401 || status == 403) && !m_refreshToken.isEmpty()
            && !m_oauthRefreshAttempted) {
            qCInfo(lcCopilot) << "GitHub token rejected, trying OAuth refresh";
            m_oauthRefreshAttempted = true;
            refreshOAuthToken();
            return;
        }

        // Transient network errors (DNS, timeout, etc.): keep current token
        // alive and retry on the next refresh cycle — don't flicker availability.
        if (m_token.isValid() && !m_token.isExpired(QDateTime::currentSecsSinceEpoch())) {
            qCInfo(lcCopilot) << "Token refresh failed but current token still valid, keeping available";
            return;
        }

        // No refresh token or refresh already tried — clear and re-auth
        qCWarning(lcCopilot) << "Cannot recover, clearing saved auth";
        clearSavedAuth();
        tryOAuthLogin();
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonObject obj = QJsonDocument::fromJson(data).object();
    const QString rawToken = obj[QLatin1String("token")].toString();
    const qint64 expiresAt = obj[QLatin1String("expires_at")].toVariant().toLongLong();

    if (rawToken.isEmpty()) {
        qCWarning(lcCopilot) << "Copilot token response has no token field:"
                             << data.left(200);
        m_available = false;
        emit availabilityChanged(false);
        return;
    }

    m_token = CopilotToken(rawToken, expiresAt);

    // Parse additional fields from the token envelope
    m_token.setSku(obj[QLatin1String("sku")].toString());

    // Organization list
    QStringList orgs;
    for (const auto &v : obj[QLatin1String("organization_list")].toArray())
        orgs.append(v.toString());
    m_token.setOrganizationList(orgs);

    // SKU-isolated endpoints
    const QJsonObject endpoints = obj[QLatin1String("endpoints")].toObject();
    if (endpoints.contains(QLatin1String("api")))
        m_token.setApiEndpoint(endpoints[QLatin1String("api")].toString());

    const bool wasAvailable = m_available;
    m_available = m_token.isValid();
    if (m_available != wasAvailable)
        emit availabilityChanged(m_available);

    qCInfo(lcCopilot) << "Copilot token obtained, available:" << m_available
                      << "sku:" << m_token.sku()
                      << "expires:" << expiresAt;

    if (m_available) {
        m_oauthRefreshAttempted = false;
        scheduleTokenRefresh();
        fetchModels();
    }
}

void CopilotProvider::scheduleTokenRefresh()
{
    if (!m_refreshTimer.isActive())
        m_refreshTimer.start(kTokenRefreshMs);
}

// ── OAuth token refresh ───────────────────────────────────────────────────────

void CopilotProvider::refreshOAuthToken()
{
    if (m_refreshToken.isEmpty()) {
        qCWarning(lcCopilot) << "No refresh token available, triggering re-auth";
        clearSavedAuth();
        tryOAuthLogin();
        return;
    }

    qCInfo(lcCopilot) << "Refreshing GitHub OAuth token";

    QNetworkRequest req{QUrl{QLatin1String("https://github.com/login/oauth/access_token")}};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setAttribute(QNetworkRequest::Http2DirectAttribute, false);

    const QByteArray body = QStringLiteral(
        "client_id=%1&grant_type=refresh_token&refresh_token=%2")
        .arg(QLatin1String("Iv1.b507a08c87ecfe98"), m_refreshToken).toUtf8();

    QNetworkReply *reply = m_nam.post(req, body);
    QPointer<QNetworkReply> safeReply(reply);
    connect(reply, &QNetworkReply::finished,
            this, [this, safeReply] {
        if (safeReply)
            handleOAuthRefreshReply(safeReply);
    });
}

void CopilotProvider::handleOAuthRefreshReply(QNetworkReply *reply)
{
    reply->deleteLater();

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();

    if (obj.contains(QLatin1String("access_token"))) {
        m_githubToken  = obj[QLatin1String("access_token")].toString();
        m_refreshToken = obj[QLatin1String("refresh_token")].toString();

        if (m_keyStore) {
            m_keyStore(kGithubTokenKey, m_githubToken);
            if (!m_refreshToken.isEmpty())
                m_keyStore(kRefreshTokenKey, m_refreshToken);
        }

        qCInfo(lcCopilot) << "OAuth token refreshed successfully";
        refreshCopilotToken();
        return;
    }

    const QString error = obj[QLatin1String("error")].toString();
    qCWarning(lcCopilot) << "OAuth refresh failed:" << error
                         << obj[QLatin1String("error_description")].toString();

    // Refresh failed — clear everything and re-auth
    clearSavedAuth();
    tryOAuthLogin();
}

void CopilotProvider::clearSavedAuth()
{
    m_githubToken.clear();
    m_refreshToken.clear();
    m_token = CopilotToken();
    m_available = false;
    m_oauthRefreshAttempted = false;

    if (m_keyDelete) {
        m_keyDelete(kGithubTokenKey);
        m_keyDelete(kRefreshTokenKey);
    }
    // Also clean up any legacy QSettings entries
    QSettings settings;
    settings.remove(QStringLiteral("ai/copilot/github_token"));
    settings.remove(QStringLiteral("ai/copilot/refresh_token"));
    settings.sync();

    emit availabilityChanged(false);
    qCInfo(lcCopilot) << "Saved auth cleared";
}

// ── Model discovery ───────────────────────────────────────────────────────────

void CopilotProvider::fetchModels()
{
    // Throttle: don't re-fetch within the refresh interval
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastModelFetch < kModelRefreshMs && !m_modelInfos.isEmpty())
        return;
    m_lastModelFetch = now;

    QNetworkReply *reply = m_nam.get(makeAuthRequest(QUrl{QLatin1String(kModelsUrl)}));
    QPointer<QNetworkReply> safeReply(reply);
    connect(reply, &QNetworkReply::finished,
            this, [this, safeReply] {
        if (safeReply)
            handleModelsReply(safeReply);
    });
}

void CopilotProvider::handleModelsReply(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(lcCopilot) << "Model fetch failed:" << reply->errorString();
        // Provide default models so the UI isn't empty
        if (m_modelInfos.isEmpty()) {
            const QStringList defaults = {
                QStringLiteral("gpt-4o"),
                QStringLiteral("claude-sonnet-4"),
                QStringLiteral("o4-mini"),
                QStringLiteral("gemini-2.5-pro"),
            };
            for (const QString &id : defaults) {
                ModelInfo mi;
                mi.id = id;
                mi.name = id;
                mi.modelPickerEnabled = true;
                mi.capabilities.type = QStringLiteral("chat");
                mi.capabilities.streaming = true;
                mi.capabilities.toolCalls = true;
                m_modelInfos.append(mi);
            }
            emit modelsChanged();
        }
        return;
    }

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    const QJsonArray data = obj[QLatin1String("data")].toArray();

    QList<ModelInfo> models;
    for (const QJsonValue &v : data) {
        ModelInfo mi = parseModelJson(v.toObject());
        if (mi.capabilities.type == QLatin1String("chat"))
            models.append(mi);
    }

    qCInfo(lcCopilot) << "Models endpoint returned" << data.size()
                      << "total," << models.size() << "chat models";

    if (!models.isEmpty()) {
        m_modelInfos = models;

        // Validate current model is still available
        bool found = false;
        for (const auto &mi : m_modelInfos) {
            if (mi.id == m_model) {
                found = true;
                break;
            }
        }
        if (!found) {
            // Fall back to the default model
            for (const auto &mi : m_modelInfos) {
                if (mi.isChatDefault) {
                    m_model = mi.id;
                    found = true;
                    break;
                }
            }
            if (!found && !m_modelInfos.isEmpty())
                m_model = m_modelInfos.first().id;
        }

        emit modelsChanged();
    }
}

ModelInfo CopilotProvider::parseModelJson(const QJsonObject &obj) const
{
    ModelInfo mi;
    mi.id      = obj[QLatin1String("id")].toString();
    mi.name    = obj[QLatin1String("name")].toString();
    mi.vendor  = obj[QLatin1String("vendor")].toString();
    mi.version = obj[QLatin1String("version")].toString();

    mi.modelPickerEnabled = obj[QLatin1String("model_picker_enabled")].toBool(true);
    mi.isChatDefault      = obj[QLatin1String("is_chat_default")].toBool(false);
    mi.isChatFallback     = obj[QLatin1String("is_chat_fallback")].toBool(false);
    mi.isPreview          = obj[QLatin1String("preview")].toBool(false);
    if (mi.isPreview && !mi.name.endsWith(QLatin1String(" (Preview)")))
        mi.name += QLatin1String(" (Preview)");

    // Capabilities
    const QJsonObject caps = obj[QLatin1String("capabilities")].toObject();
    mi.capabilities.type   = caps[QLatin1String("type")].toString();
    mi.capabilities.family = caps[QLatin1String("family")].toString();

    const QJsonObject supports = caps[QLatin1String("supports")].toObject();
    mi.capabilities.streaming        = supports[QLatin1String("streaming")].toBool(true);
    mi.capabilities.toolCalls        = supports[QLatin1String("tool_calls")].toBool(false);
    mi.capabilities.vision           = supports[QLatin1String("vision")].toBool(false);
    mi.capabilities.thinking         = supports[QLatin1String("thinking")].toBool(false);
    mi.capabilities.adaptiveThinking = supports[QLatin1String("adaptive_thinking")].toBool(false);

    const QJsonObject limits = caps[QLatin1String("limits")].toObject();
    mi.capabilities.maxPromptTokens         = limits[QLatin1String("max_prompt_tokens")].toInt(0);
    mi.capabilities.maxOutputTokens         = limits[QLatin1String("max_output_tokens")].toInt(0);
    mi.capabilities.maxContextWindowTokens  = limits[QLatin1String("max_context_window_tokens")].toInt(0);

    // Billing — check both "billing" and "policy" keys (API may use either)
    QJsonObject billing = obj[QLatin1String("billing")].toObject();
    if (billing.isEmpty())
        billing = obj[QLatin1String("policy")].toObject();
    mi.billing.isPremium  = billing[QLatin1String("is_premium")].toBool(false);
    mi.billing.multiplier = billing[QLatin1String("multiplier")].toDouble(0.0);

    // Debug: dump policy object to understand real structure
    const QJsonObject policy = obj[QLatin1String("policy")].toObject();
    qCInfo(lcCopilot) << "Model:" << mi.id
                      << "policy:" << QJsonDocument(policy).toJson(QJsonDocument::Compact);

    // Supported endpoints
    for (const auto &ep : obj[QLatin1String("supported_endpoints")].toArray())
        mi.supportedEndpoints.append(ep.toString());

    return mi;
}

// ── Common request helpers ────────────────────────────────────────────────────

QNetworkRequest CopilotProvider::makeAuthRequest(const QUrl &url) const
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Authorization", ("Bearer " + m_token.raw()).toUtf8());
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "GitHubCopilotChat/0.40.0");
    req.setRawHeader("Editor-Version", "vscode/1.100.0");
    req.setRawHeader("Editor-Plugin-Version", "copilot-chat/0.40.0");
    req.setRawHeader("Copilot-Integration-Id", "vscode-chat");
    req.setRawHeader("OpenAI-Intent", "conversation-panel");
    req.setRawHeader("X-GitHub-Api-Version", "2025-04-01");
    // Force HTTP/1.1 — avoids HTTP/2 GOAWAY issues with long-lived SSE streams
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setAttribute(QNetworkRequest::Http2DirectAttribute, false);
    return req;
}

// ── Requests ──────────────────────────────────────────────────────────────────

void CopilotProvider::sendRequest(const AgentRequest &request)
{
    if (!m_available) {
        AgentError err;
        err.requestId = request.requestId;
        err.code      = AgentError::Code::AuthError;
        err.message   = tr("Copilot is not available. Check your GitHub token.");
        emit responseError(request.requestId, err);
        return;
    }

    // Refresh token if needed — queue the request and auto-retry after refresh
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_token.isExpired(now)) {
        m_lastRequest = request;
        m_retryCount  = 0;
        refreshCopilotToken();
        // Wait for token refresh, then auto-retry
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(this, &CopilotProvider::availabilityChanged,
                        this, [this, conn](bool available) {
            disconnect(*conn);
            if (available && !m_lastRequest.requestId.isEmpty())
                sendRequest(m_lastRequest);
            else if (!available) {
                AgentError err;
                err.requestId = m_lastRequest.requestId;
                err.code      = AgentError::Code::AuthError;
                err.message   = tr("Token refresh failed. Please re-authenticate.");
                emit responseError(m_lastRequest.requestId, err);
            }
        });
        return;
    }

    // Save for potential retry
    m_lastRequest = request;
    m_retryCount  = 0;
    m_promptTokens     = 0;
    m_completionTokens = 0;
    m_totalTokens      = 0;

    // Check if current model supports Responses API
    const ModelInfo mi = currentModelInfo();
    const bool supportsResponses =
        mi.supportedEndpoints.contains(QLatin1String("/responses"));

    if (supportsResponses && request.agentMode) {
        m_useResponsesApi = true;
        sendResponsesApi(request);
    } else {
        m_useResponsesApi = false;
        sendChatCompletions(request);
    }
}

// ── Chat Completions API (/chat/completions) ──────────────────────────────────

void CopilotProvider::sendChatCompletions(const AgentRequest &request)
{
    QJsonObject body;
    const ModelInfo mi = currentModelInfo();
    const bool isOSeries = mi.capabilities.family.startsWith(QLatin1String("o"));

    body[QStringLiteral("model")]    = m_model;
    body[QStringLiteral("messages")] = buildMessages(request);
    body[QStringLiteral("stream")]   = true;

    // o-series models use max_completion_tokens instead of max_tokens
    if (mi.capabilities.maxOutputTokens > 0) {
        const QString key = isOSeries ? QStringLiteral("max_completion_tokens")
                                      : QStringLiteral("max_tokens");
        body[key] = mi.capabilities.maxOutputTokens;
    }

    // Thinking / reasoning effort for supported models
    if (!request.reasoningEffort.isEmpty()) {
        if (mi.capabilities.thinking || mi.capabilities.adaptiveThinking)
            body[QStringLiteral("reasoning_effort")] = request.reasoningEffort;
    }

    // Tool calling — only if model supports it
    QJsonArray tools;
    if (mi.capabilities.toolCalls)
        tools = buildTools(request);
    if (!tools.isEmpty())
        body[QStringLiteral("tools")] = tools;

    const QString url = m_token.apiEndpoint().isEmpty()
                            ? QLatin1String(kChatUrl)
                            : (m_token.apiEndpoint() + QLatin1String("/chat/completions"));

    QNetworkRequest netReq = makeAuthRequest(QUrl{url});
    m_activeRequestId = request.requestId;
    m_streamAccum.clear();
    m_pendingToolCalls.clear();
    m_pendingThinking.clear();
    m_sseParser->reset();

    // Clean up any leftover reply from a previous request before creating a
    // new one.  This prevents two QNetworkReply objects being alive at the
    // same time on the same connection, which causes use-after-free crashes
    // in Qt6Network's HTTP/2 multiplexer.
    cleanupActiveReply();

    const QByteArray bodyBytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
    qCInfo(lcCopilot) << "Chat Completions POST" << url
                      << "body size:" << bodyBytes.size() << "bytes"
                      << "tools:" << tools.size();
    QNetworkReply *reply = m_nam.post(netReq, bodyBytes);
    m_activeReply = reply;
    connectReply(reply);
    m_idleTimer.start();
}

// ── Responses API (/responses) ────────────────────────────────────────────────
// The Responses API is a newer, more efficient endpoint for certain models.

void CopilotProvider::sendResponsesApi(const AgentRequest &request)
{
    // Build input array from conversation history + new message
    QJsonArray input;
    for (const auto &msg : request.conversationHistory) {
        QJsonObject entry;
        switch (msg.role) {
        case AgentMessage::Role::System:    entry[QStringLiteral("role")] = QStringLiteral("system"); break;
        case AgentMessage::Role::User:      entry[QStringLiteral("role")] = QStringLiteral("user"); break;
        case AgentMessage::Role::Assistant: entry[QStringLiteral("role")] = QStringLiteral("assistant"); break;
        case AgentMessage::Role::Tool:      entry[QStringLiteral("role")] = QStringLiteral("tool"); break;
        }
        entry[QStringLiteral("content")] = msg.content;
        input.append(entry);
    }

    // System prompt
    QJsonObject systemMsg;
    systemMsg[QStringLiteral("role")]    = QStringLiteral("system");
    systemMsg[QStringLiteral("content")] = QString::fromLatin1(
        request.agentMode ? kAgentSystemPrompt : kSystemPrompt);
    input.prepend(systemMsg);

    // User message
    if (request.appendUserMessage) {
        QJsonObject userMsg;
        userMsg[QStringLiteral("role")]    = QStringLiteral("user");
        userMsg[QStringLiteral("content")] = buildUserContent(request);
        input.append(userMsg);
    }

    // Build tools array — only if model supports tool calls
    const ModelInfo mi = currentModelInfo();
    QJsonArray tools;
    if (mi.capabilities.toolCalls) {
        for (const auto &tool : request.tools) {
            QJsonObject params;
            if (!tool.inputSchema.isEmpty()) {
                params = tool.inputSchema;
            } else {
                QJsonObject props;
                QJsonArray required;
                for (const auto &p : tool.parameters) {
                    props[p.name] = QJsonObject{
                        {QStringLiteral("type"),        p.type},
                        {QStringLiteral("description"), p.description}
                    };
                    if (p.required)
                        required.append(p.name);
                }
                params = QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("object")},
                    {QStringLiteral("properties"), props},
                    {QStringLiteral("required"), required}
                };
            }
            tools.append(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("function")},
                {QStringLiteral("name"), tool.name},
                {QStringLiteral("description"), tool.description},
                {QStringLiteral("strict"), false},
                {QStringLiteral("parameters"), params}
            });
        }
    }

    QJsonObject body;
    body[QStringLiteral("model")]  = m_model;
    body[QStringLiteral("input")]  = input;
    body[QStringLiteral("stream")] = true;
    body[QStringLiteral("store")]  = false;
    if (mi.capabilities.maxOutputTokens > 0)
        body[QStringLiteral("max_output_tokens")] = mi.capabilities.maxOutputTokens;
    if (!tools.isEmpty())
        body[QStringLiteral("tools")] = tools;

    const QString url = m_token.apiEndpoint().isEmpty()
                            ? QLatin1String(kResponsesUrl)
                            : (m_token.apiEndpoint() + QLatin1String("/responses"));

    QNetworkRequest netReq = makeAuthRequest(QUrl{url});
    m_activeRequestId = request.requestId;
    m_streamAccum.clear();
    m_pendingToolCalls.clear();
    m_pendingThinking.clear();
    m_sseParser->reset();

    // Clean up any leftover reply before creating a new one (same reason as
    // in sendChatCompletions — prevents use-after-free in Qt6Network).
    cleanupActiveReply();

    const QByteArray bodyBytes = QJsonDocument(body).toJson(QJsonDocument::Compact);
    qCInfo(lcCopilot) << "Responses API POST" << url
                      << "body size:" << bodyBytes.size() << "bytes"
                      << "tools:" << tools.size();
    QNetworkReply *reply = m_nam.post(netReq, bodyBytes);
    m_activeReply = reply;
    connectReply(reply);
    m_idleTimer.start();
}

// ── Message building ──────────────────────────────────────────────────────────

QJsonArray CopilotProvider::buildMessages(const AgentRequest &request) const
{
    QJsonArray messages;

    // System prompt
    messages.append(QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("system")},
        {QStringLiteral("content"), QString::fromLatin1(
            request.agentMode ? kAgentSystemPrompt : kSystemPrompt)}
    });

    // Conversation history
    for (const auto &msg : request.conversationHistory) {
        QJsonObject entry;
        switch (msg.role) {
        case AgentMessage::Role::System:    entry[QStringLiteral("role")] = QStringLiteral("system"); break;
        case AgentMessage::Role::User:      entry[QStringLiteral("role")] = QStringLiteral("user"); break;
        case AgentMessage::Role::Assistant: entry[QStringLiteral("role")] = QStringLiteral("assistant"); break;
        case AgentMessage::Role::Tool:
            entry[QStringLiteral("role")]         = QStringLiteral("tool");
            entry[QStringLiteral("tool_call_id")] = msg.toolCallId;
            break;
        }
        entry[QStringLiteral("content")] = msg.content;

        // Assistant messages may contain tool calls
        if (!msg.toolCalls.isEmpty()) {
            QJsonArray tcArr;
            for (const auto &tc : msg.toolCalls) {
                tcArr.append(QJsonObject{
                    {QStringLiteral("id"), tc.id},
                    {QStringLiteral("type"), QStringLiteral("function")},
                    {QStringLiteral("function"), QJsonObject{
                        {QStringLiteral("name"),      tc.name},
                        {QStringLiteral("arguments"), tc.arguments}
                    }}
                });
            }
            entry[QStringLiteral("tool_calls")] = tcArr;
        }

        messages.append(entry);
    }

    // Current user message — with vision content array if images attached
    if (request.appendUserMessage) {
        if (!request.attachments.isEmpty()) {
            QJsonArray contentArray;
            contentArray.append(QJsonObject{
                {QStringLiteral("type"), QStringLiteral("text")},
                {QStringLiteral("text"), buildUserContent(request)}
            });
            for (const auto &att : request.attachments) {
                if (att.type == Attachment::Type::Image && !att.data.isEmpty()) {
                    const QString b64 = QString::fromLatin1(att.data.toBase64());
                    contentArray.append(QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("image_url")},
                        {QStringLiteral("image_url"), QJsonObject{
                            {QStringLiteral("url"),
                             QStringLiteral("data:%1;base64,%2").arg(att.mimeType, b64)}
                        }}
                    });
                }
            }
            messages.append(QJsonObject{
                {QStringLiteral("role"),    QStringLiteral("user")},
                {QStringLiteral("content"), contentArray}
            });
        } else {
            messages.append(QJsonObject{
                {QStringLiteral("role"),    QStringLiteral("user")},
                {QStringLiteral("content"), buildUserContent(request)}
            });
        }
    }

    return messages;
}

QJsonArray CopilotProvider::buildTools(const AgentRequest &request) const
{
    QJsonArray tools;
    for (const auto &td : request.tools) {
        // Use the original inputSchema if available (preserves items, enum, nested properties)
        QJsonObject params;
        if (!td.inputSchema.isEmpty()) {
            params = td.inputSchema;
        } else {
            // Fallback: reconstruct from flat ToolParameter list
            QJsonObject props;
            QJsonArray required;
            for (const auto &p : td.parameters) {
                props[p.name] = QJsonObject{
                    {QStringLiteral("type"),        p.type},
                    {QStringLiteral("description"), p.description}
                };
                if (p.required)
                    required.append(p.name);
            }
            params = QJsonObject{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), props},
                {QStringLiteral("required"), required}
            };
        }

        tools.append(QJsonObject{
            {QStringLiteral("type"), QStringLiteral("function")},
            {QStringLiteral("function"), QJsonObject{
                {QStringLiteral("name"),        td.name},
                {QStringLiteral("description"), td.description},
                {QStringLiteral("parameters"), params}
            }}
        });
    }
    return tools;
}

QString CopilotProvider::buildUserContent(const AgentRequest &req) const
{
    QString content;

    if (!req.activeFilePath.isEmpty()) {
        content += QStringLiteral("Current file: %1\n").arg(req.activeFilePath);
        if (!req.languageId.isEmpty())
            content += QStringLiteral("Language: %1\n").arg(req.languageId);
    }

    if (!req.selectedText.isEmpty())
        content += QStringLiteral("\nSelected code:\n```\n%1\n```\n\n").arg(req.selectedText);

    content += req.userPrompt;
    return content;
}

void CopilotProvider::cancelRequest(const QString &requestId)
{
    if (requestId.isEmpty() || requestId != m_activeRequestId)
        return;
    cleanupActiveReply();
    m_idleTimer.stop();
    m_activeRequestId.clear();
    m_pendingToolCalls.clear();
}

// ── Reply wiring ──────────────────────────────────────────────────────────────

void CopilotProvider::connectReply(QNetworkReply *reply)
{
    QPointer<QNetworkReply> safeReply(reply);

    connect(reply, &QNetworkReply::readyRead, this, [this, safeReply] {
        if (!safeReply || safeReply != m_activeReply)
            return;
        m_idleTimer.start();
        const QByteArray chunk = safeReply->readAll();
        m_sseParser->feed(chunk);
    });

    connect(reply, &QNetworkReply::errorOccurred, this,
            [this, safeReply](QNetworkReply::NetworkError code) {
        if (!safeReply || m_activeReply != safeReply)
            return;
        qCWarning(lcCopilot) << "Network error on active reply:"
                             << code << safeReply->errorString();
    });

    connect(reply, &QNetworkReply::finished, this, [this, safeReply] {
        if (!safeReply)
            return;
        m_idleTimer.stop();
        if (m_activeReply != safeReply) {
            // This reply is orphaned (superseded by a new one during SSE
            // finish_reason handling).  Just disconnect and schedule deletion
            // — do NOT touch m_activeReply or m_activeRequestId.
            safeReply->disconnect(this);
            safeReply->deleteLater();
            return;
        }

        // Read everything we need from the reply BEFORE any cleanup,
        // because cleanupActiveReply() will abort + disconnect it.
        const int status =
            safeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = safeReply->error();
        const QString errorStr = safeReply->errorString();

        // Parse rate limit headers while reply is still alive
        const QByteArray remaining  = safeReply->rawHeader("x-ratelimit-remaining");
        const QByteArray resetTs    = safeReply->rawHeader("x-ratelimit-reset");
        const QByteArray retryAfter = safeReply->rawHeader("retry-after");

        if (!remaining.isEmpty())
            m_rateLimit.remaining = remaining.toInt();
        if (!resetTs.isEmpty())
            m_rateLimit.resetEpoch = resetTs.toInt();

        qCWarning(lcCopilot) << "Reply finished: error=" << error
                             << errorStr << "HTTP" << status;

        // Clean up the reply immediately via cleanupActiveReply() which does
        // disconnect → abort → deleteLater in the correct order.  This
        // ensures the reply is fully detached BEFORE we emit any signals
        // that might trigger re-entrant sendRequest() calls from the
        // AgentController.  Without this, two replies can be alive
        // simultaneously and Qt6Network's HTTP/2 multiplexer crashes on
        // stale internal pointers (use-after-free at offset ~0x2f4).
        cleanupActiveReply();

        if (m_activeRequestId.isEmpty())
            return; // Already handled by SSE [DONE] / tool_calls finish_reason

        if (error == QNetworkReply::NoError) {
            m_rateLimit.throttled = false;
            handleSseDone(); // Stream ended without [DONE]
        } else {
            if (status == 429) {
                m_rateLimit.throttled = true;
                int secsUntilReset = 0;
                if (!retryAfter.isEmpty()) {
                    secsUntilReset = retryAfter.toInt();
                } else if (m_rateLimit.resetEpoch > 0) {
                    secsUntilReset = m_rateLimit.resetEpoch
                                     - static_cast<int>(QDateTime::currentSecsSinceEpoch());
                    if (secsUntilReset < 0) secsUntilReset = 0;
                }
                emit rateLimitHit(secsUntilReset);
            }
            retryOrFail(status, errorStr);
        }
    });
}

// ── SSE event handling ────────────────────────────────────────────────────────

void CopilotProvider::cleanupActiveReply()
{
    // Properly tear down the active reply so Qt6Network releases all internal
    // references to the HTTP stream.  Without this, the HTTP/2 multiplexer may
    // still hold a raw pointer to the reply; when deleteLater destroys it the
    // stale pointer causes a use-after-free crash in Qt6Network internals.
    if (m_activeReply) {
        m_activeReply->disconnect(this);
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
    }
}

void CopilotProvider::handleSseEvent(const QString &eventType, const QString &data)
{
    // Handle SSE error events from the server
    if (eventType == QLatin1String("error")) {
        qCWarning(lcCopilot) << "SSE error event:" << data.left(500);
        const QJsonObject errObj = QJsonDocument::fromJson(data.toUtf8()).object();
        AgentError err;
        err.requestId = m_activeRequestId;
        err.code      = AgentError::Code::Unknown;
        err.message   = errObj[QLatin1String("message")].toString(
                            tr("Server returned an error."));
        emit responseError(m_activeRequestId, err);
        m_activeRequestId.clear();
        m_idleTimer.stop();
        return;
    }

    m_idleTimer.start(); // reset idle timer on each event

    // Route to appropriate handler based on API used
    if (m_useResponsesApi)
        handleResponsesEvent(eventType, data);
    else
        handleChatCompletionsEvent(data);
}

// ── Chat Completions SSE parsing ──────────────────────────────────────────────

void CopilotProvider::handleChatCompletionsEvent(const QString &data)
{
    const QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();

    // Parse usage data (appears in the final chunk)
    const QJsonObject usage = obj[QLatin1String("usage")].toObject();
    if (!usage.isEmpty()) {
        m_promptTokens     = usage[QLatin1String("prompt_tokens")].toInt();
        m_completionTokens = usage[QLatin1String("completion_tokens")].toInt();
        m_totalTokens      = usage[QLatin1String("total_tokens")].toInt();
    }

    const QJsonArray choices = obj[QLatin1String("choices")].toArray();
    if (choices.isEmpty())
        return;

    const QJsonObject choice = choices[0].toObject();
    const QJsonObject delta  = choice[QLatin1String("delta")].toObject();

    // Text content
    const QString content = delta[QLatin1String("content")].toString();
    if (!content.isEmpty()) {
        m_streamAccum += content;
        emit responseDelta(m_activeRequestId, content);
    }

    // Thinking/reasoning content (for thinking models like o-series, Claude)
    const QString reasoning = delta[QLatin1String("reasoning_content")].toString();
    if (!reasoning.isEmpty()) {
        m_pendingThinking += reasoning;
        emit thinkingDelta(m_activeRequestId, reasoning);
    }

    // Tool calls (accumulated across chunks)
    const QJsonArray toolCallDeltas = delta[QLatin1String("tool_calls")].toArray();
    for (const QJsonValue &tcv : toolCallDeltas) {
        const QJsonObject tc    = tcv.toObject();
        const int         index = tc[QLatin1String("index")].toInt();
        const QJsonObject fn    = tc[QLatin1String("function")].toObject();

        if (!m_pendingToolCalls.contains(index)) {
            ToolCall newTc;
            newTc.id   = tc[QLatin1String("id")].toString();
            newTc.name = fn[QLatin1String("name")].toString();
            m_pendingToolCalls[index] = newTc;
        }
        // Accumulate arguments across chunks
        m_pendingToolCalls[index].arguments +=
            fn[QLatin1String("arguments")].toString();
    }

    // Check for finish_reason
    const QString finishReason = choice[QLatin1String("finish_reason")].toString();
    if (finishReason == QLatin1String("tool_calls")) {
        AgentResponse resp;
        resp.requestId = m_activeRequestId;
        resp.thinkingContent = m_pendingThinking;
        resp.promptTokens     = m_promptTokens;
        resp.completionTokens = m_completionTokens;
        resp.totalTokens      = m_totalTokens;
        for (auto it = m_pendingToolCalls.begin(); it != m_pendingToolCalls.end(); ++it)
            resp.toolCalls.append(it.value());
        // Clear state and disconnect/abort the old reply BEFORE emitting.
        // The controller's slot will call sendModelRequest() synchronously,
        // which creates a new QNetworkReply.  If the old reply is still alive
        // and connected, Qt6Network's HTTP/2 multiplexer can crash when it
        // later processes internal events for the destroyed old reply.
        const QString reqId = m_activeRequestId;
        m_activeRequestId.clear();
        cleanupActiveReply();
        m_idleTimer.stop();
        m_pendingToolCalls.clear();
        m_pendingThinking.clear();
        emit responseFinished(reqId, resp);
    } else if (finishReason == QLatin1String("content_filter")) {
        qCWarning(lcCopilot) << "Response blocked by content filter";
        AgentError err;
        err.requestId = m_activeRequestId;
        err.code      = AgentError::Code::ContentFilter;
        err.message   = tr("Response was blocked by the content safety filter.");
        const QString reqId = m_activeRequestId;
        m_activeRequestId.clear();
        cleanupActiveReply();
        m_idleTimer.stop();
        m_pendingToolCalls.clear();
        m_pendingThinking.clear();
        emit responseError(reqId, err);
    }
}

// ── Responses API SSE parsing ─────────────────────────────────────────────────
// Events: response.output_text.delta, response.output_text.done,
//         response.function_call_arguments.delta, response.function_call_arguments.done,
//         response.output_item.added, response.output_item.done,
//         response.completed, response.content_part.delta

void CopilotProvider::handleResponsesEvent(const QString &eventType, const QString &data)
{
    const QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();

    if (eventType == QLatin1String("response.output_text.delta")) {
        // Streaming text delta
        const QString delta = obj[QLatin1String("delta")].toString();
        if (!delta.isEmpty()) {
            m_streamAccum += delta;
            emit responseDelta(m_activeRequestId, delta);
        }
    } else if (eventType == QLatin1String("response.content_part.delta")) {
        // Alternative content delta format
        const QJsonObject deltaPart = obj[QLatin1String("delta")].toObject();
        const QString text = deltaPart[QLatin1String("text")].toString();
        if (!text.isEmpty()) {
            m_streamAccum += text;
            emit responseDelta(m_activeRequestId, text);
        }
        // Reasoning/thinking
        const QString reasoning = deltaPart[QLatin1String("reasoning")].toString();
        if (!reasoning.isEmpty()) {
            m_pendingThinking += reasoning;
            emit thinkingDelta(m_activeRequestId, reasoning);
        }
    } else if (eventType == QLatin1String("response.output_item.added")) {
        // New output item — could be function_call
        const QJsonObject item = obj[QLatin1String("item")].toObject();
        if (item[QLatin1String("type")].toString() == QLatin1String("function_call")) {
            m_responsesToolCall = ToolCall();
            m_responsesToolCall.id   = item[QLatin1String("call_id")].toString();
            m_responsesToolCall.name = item[QLatin1String("name")].toString();
            m_responsesToolCall.arguments.clear();
        }
    } else if (eventType == QLatin1String("response.function_call_arguments.delta")) {
        // Accumulate function call arguments
        const QString delta = obj[QLatin1String("delta")].toString();
        m_responsesToolCall.arguments += delta;
    } else if (eventType == QLatin1String("response.function_call_arguments.done")) {
        // Function call complete — add to pending
        if (!m_responsesToolCall.id.isEmpty())
            m_pendingToolCalls[m_pendingToolCalls.size()] = m_responsesToolCall;
    } else if (eventType == QLatin1String("response.output_item.done")) {
        // Output item finished — may be function_call with full data
        const QJsonObject item = obj[QLatin1String("item")].toObject();
        if (item[QLatin1String("type")].toString() == QLatin1String("function_call")) {
            ToolCall tc;
            tc.id        = item[QLatin1String("call_id")].toString();
            tc.name      = item[QLatin1String("name")].toString();
            tc.arguments = item[QLatin1String("arguments")].toString();
            // Overwrite if already accumulated via deltas
            bool found = false;
            for (auto it = m_pendingToolCalls.begin(); it != m_pendingToolCalls.end(); ++it) {
                if (it.value().id == tc.id) {
                    it.value() = tc;
                    found = true;
                    break;
                }
            }
            if (!found)
                m_pendingToolCalls[m_pendingToolCalls.size()] = tc;
        }
    } else if (eventType == QLatin1String("response.completed")) {
        // Stream finished — emit response with any accumulated tool calls
        const QJsonObject response = obj[QLatin1String("response")].toObject();
        const QString status = response[QLatin1String("status")].toString();

        if (status == QLatin1String("incomplete")) {
            const QJsonObject incompleteDetails =
                response[QLatin1String("incomplete_details")].toObject();
            const QString reason = incompleteDetails[QLatin1String("reason")].toString();

            if (reason == QLatin1String("content_filter")) {
                AgentError err;
                err.requestId = m_activeRequestId;
                err.code      = AgentError::Code::ContentFilter;
                err.message   = tr("Response was blocked by the content safety filter.");
                const QString reqId = m_activeRequestId;
                m_activeRequestId.clear();
                cleanupActiveReply();
                m_idleTimer.stop();
                m_pendingToolCalls.clear();
                m_pendingThinking.clear();
                emit responseError(reqId, err);
                return;
            }
        }

        // Parse usage from Responses API
        const QJsonObject usage = response[QLatin1String("usage")].toObject();
        if (!usage.isEmpty()) {
            m_promptTokens     = usage[QLatin1String("input_tokens")].toInt();
            m_completionTokens = usage[QLatin1String("output_tokens")].toInt();
            m_totalTokens      = m_promptTokens + m_completionTokens;
        }

        AgentResponse resp;
        resp.requestId = m_activeRequestId;
        resp.thinkingContent = m_pendingThinking;
        resp.promptTokens     = m_promptTokens;
        resp.completionTokens = m_completionTokens;
        resp.totalTokens      = m_totalTokens;
        for (auto it = m_pendingToolCalls.begin(); it != m_pendingToolCalls.end(); ++it)
            resp.toolCalls.append(it.value());
        // Clear state BEFORE emitting — see handleChatCompletionsEvent comment.
        const QString reqId = m_activeRequestId;
        m_activeRequestId.clear();
        cleanupActiveReply();
        m_idleTimer.stop();
        m_pendingToolCalls.clear();
        m_pendingThinking.clear();
        emit responseFinished(reqId, resp);
    }
}

void CopilotProvider::handleSseDone()
{
    if (m_activeRequestId.isEmpty())
        return;

    m_idleTimer.stop();

    AgentResponse resp;
    resp.requestId = m_activeRequestId;
    resp.thinkingContent = m_pendingThinking;
    resp.promptTokens     = m_promptTokens;
    resp.completionTokens = m_completionTokens;
    resp.totalTokens      = m_totalTokens;
    // Include any accumulated tool calls
    for (auto it = m_pendingToolCalls.begin(); it != m_pendingToolCalls.end(); ++it)
        resp.toolCalls.append(it.value());

    // Clear state BEFORE emitting — the slot connected to responseFinished()
    // (AgentController) may synchronously call sendRequest() which creates a
    // new QNetworkReply.  We must fully detach the old reply first to prevent
    // two replies being alive simultaneously in Qt6Network's connection pool.
    const QString reqId = m_activeRequestId;
    m_activeRequestId.clear();
    m_pendingToolCalls.clear();
    m_pendingThinking.clear();
    // cleanupActiveReply is safe to call even if already cleaned up by the
    // finished handler — it checks m_activeReply for null.
    cleanupActiveReply();
    emit responseFinished(reqId, resp);
}

// ── Retry with exponential backoff ────────────────────────────────────────────

void CopilotProvider::retryOrFail(int httpStatus, const QString &errorMsg)
{
    // Retry on 429 (rate limited), 5xx (server error), or 0 (network failure)
    const bool retryable = (httpStatus == 0 || httpStatus == 429 || httpStatus >= 500);

    if (retryable && m_retryCount < kMaxRetries) {
        ++m_retryCount;
        int delayMs;
        if (httpStatus == 429 && m_rateLimit.resetEpoch > 0) {
            // Use server-provided reset time
            const int secsLeft = m_rateLimit.resetEpoch
                                 - static_cast<int>(QDateTime::currentSecsSinceEpoch());
            delayMs = qMax(secsLeft * 1000, kRetryBaseMs);
        } else {
            delayMs = kRetryBaseMs * (1 << (m_retryCount - 1)); // 1s, 2s, 4s
        }
        qCInfo(lcCopilot) << "Retrying request (attempt" << m_retryCount
                          << "of" << kMaxRetries << ") after" << delayMs << "ms"
                          << "(HTTP" << httpStatus << ")";

        QTimer::singleShot(delayMs, this, [this] {
            if (m_lastRequest.requestId.isEmpty())
                return;
            m_activeRequestId = m_lastRequest.requestId;

            const ModelInfo mi = currentModelInfo();
            const bool supportsResponses =
                mi.supportedEndpoints.contains(QLatin1String("/responses"));

            if (supportsResponses && m_lastRequest.agentMode)
                sendResponsesApi(m_lastRequest);
            else
                sendChatCompletions(m_lastRequest);
        });
        return;
    }

    // Not retryable or exhausted retries
    AgentError err;
    err.requestId = m_activeRequestId;
    err.message   = errorMsg;

    if (httpStatus == 401 || httpStatus == 403)
        err.code = AgentError::Code::AuthError;
    else if (httpStatus == 429)
        err.code = AgentError::Code::RateLimited;
    else
        err.code = AgentError::Code::NetworkError;

    if (m_retryCount > 0)
        err.message += tr(" (after %1 retries)").arg(m_retryCount);

    const QString reqId = m_activeRequestId;
    m_activeRequestId.clear();
    emit responseError(reqId, err);
}

// ── Inline completion ─────────────────────────────────────────────────────────

void CopilotProvider::requestCompletion(const QString &filePath,
                                        const QString &languageId,
                                        const QString &textBefore,
                                        const QString &textAfter)
{
    if (!m_available)
        return;

    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (m_token.isExpired(now)) {
        refreshCopilotToken();
        return;
    }

    QJsonArray messages;
    messages.append(QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("system")},
        {QStringLiteral("content"),
         QStringLiteral("You are a code completion engine. Given code context, "
                        "output ONLY the code that should be inserted at the cursor position. "
                        "Do not include explanations, markdown, or backticks. "
                        "Output nothing if no completion is appropriate.")}
    });

    QString prompt = QStringLiteral("File: %1\nLanguage: %2\n\n"
                                    "Code before cursor:\n```\n%3\n```\n\n"
                                    "Code after cursor:\n```\n%4\n```\n\n"
                                    "Complete the code at the cursor position:")
                         .arg(filePath, languageId, textBefore, textAfter);

    messages.append(QJsonObject{
        {QStringLiteral("role"),    QStringLiteral("user")},
        {QStringLiteral("content"), prompt}
    });

    const QJsonObject body{
        {QStringLiteral("model"),      m_model},
        {QStringLiteral("messages"),   messages},
        {QStringLiteral("max_tokens"), 256},
        {QStringLiteral("stream"),     false}
    };

    QNetworkRequest netReq = makeAuthRequest(QUrl{QLatin1String(kChatUrl)});

    QNetworkReply *reply = m_nam.post(netReq,
        QJsonDocument(body).toJson(QJsonDocument::Compact));

    QPointer<QNetworkReply> safeReply(reply);
    connect(reply, &QNetworkReply::finished, this, [this, safeReply] {
        if (!safeReply) return;
        safeReply->deleteLater();
        if (safeReply->error() != QNetworkReply::NoError)
            return;

        const QJsonObject obj = QJsonDocument::fromJson(safeReply->readAll()).object();
        const QJsonArray choices = obj[QLatin1String("choices")].toArray();
        if (choices.isEmpty())
            return;

        const QString text = choices[0].toObject()
            [QLatin1String("message")].toObject()
            [QLatin1String("content")].toString().trimmed();

        if (!text.isEmpty())
            emit completionReady(text);
    });
}
