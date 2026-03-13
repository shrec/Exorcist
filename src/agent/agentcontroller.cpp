#include "agentcontroller.h"
#include "agentorchestrator.h"
#include "braincontextbuilder.h"
#include "diagnosticsnotifier.h"
#include "itool.h"
#include "promptfileloader.h"
#include "sessionstore.h"
#include "toolapprovalservice.h"

#include <QDir>
#include <QDirIterator>
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QSysInfo>
#include <QThreadPool>
#include <QUuid>

Q_LOGGING_CATEGORY(lcCtrl, "exorcist.agent.controller")

// ── Constructor / Destructor ──────────────────────────────────────────────────

AgentController::AgentController(AgentOrchestrator *orchestrator,
                                 ToolRegistry *toolRegistry,
                                 IContextProvider *contextProvider,
                                 QObject *parent)
    : QObject(parent)
    , m_orchestrator(orchestrator)
    , m_toolRegistry(toolRegistry)
    , m_contextProvider(contextProvider)
{
    // Wire orchestrator signals — we intercept them for the agent loop
    connect(m_orchestrator, &AgentOrchestrator::responseDelta,
            this, &AgentController::onResponseDelta);
    connect(m_orchestrator, &AgentOrchestrator::responseFinished,
            this, &AgentController::onResponseFinished);
    connect(m_orchestrator, &AgentOrchestrator::responseError,
            this, &AgentController::onResponseError);
}

AgentController::~AgentController()
{
    if (m_store && m_session)
        m_store->recordSessionEnd(m_session->id());
}

void AgentController::setToolConfirmationCallback(ConfirmToolFn fn)
{
    m_confirmFn = fn;

    // Forward to the centralised approval service so it can show UI
    if (m_approvalService && fn) {
        m_approvalService->setConfirmCallback(
            [fn](const QString &toolName, const QJsonObject &args,
                 const QString &description) -> ToolApprovalService::Decision {
                auto result = fn(toolName, args, description);
                switch (result) {
                case ToolApproval::AllowOnce:  return ToolApprovalService::Decision::AllowOnce;
                case ToolApproval::AllowAlways: return ToolApprovalService::Decision::AllowAlways;
                default:                        return ToolApprovalService::Decision::Deny;
                }
            });
    }
}

// ── Session management ────────────────────────────────────────────────────────

void AgentController::newSession(bool agentMode)
{
    if (m_busy)
        cancel();

    if (m_store && m_session)
        m_store->recordSessionEnd(m_session->id());

    m_session = std::make_unique<AgentSession>();
    m_session->setAgentMode(agentMode);
    m_alwaysAllowedTools.clear();

    auto *active = m_orchestrator->activeProvider();
    if (active) {
        m_session->setProviderId(active->id());
        m_session->setModel(active->currentModel());
    }

    if (m_store) {
        const QString mode = agentMode
            ? (m_maxPermission == AgentToolPermission::SafeMutate
                ? QStringLiteral("Edit")
                : QStringLiteral("Agent"))
            : QStringLiteral("Ask");
        m_store->recordSessionStart(m_session->id(), m_session->model(), mode);
    }
}

AgentSession *AgentController::session() const
{
    return m_session.get();
}

bool AgentController::isBusy() const
{
    return m_busy;
}

// ── Send message (starts a turn) ──────────────────────────────────────────────

void AgentController::sendMessage(const QString &userMessage,
                                  const QString &activeFilePath,
                                  const QString &selectedText,
                                  const QString &languageId,
                                  AgentIntent intent,
                                  const QList<Attachment> &attachments)
{
    if (m_busy) {
        qCWarning(lcCtrl) << "sendMessage called while busy, ignoring";
        return;
    }

    // Create session if none exists
    if (!m_session)
        newSession(false);

    m_activeFilePath = activeFilePath;
    m_selectedText   = selectedText;
    m_languageId     = languageId;
    m_pendingIntent  = intent;
    m_attachments    = attachments;

    m_session->beginTurn(userMessage);
    if (m_store)
        m_store->recordUserMessage(m_session->id(), userMessage);

    m_busy = true;
    m_currentStepCount = 0;
    m_streamAccum.clear();

    emit turnStarted(userMessage);

    qCInfo(lcCtrl) << "Turn started:" << userMessage.left(80);

    sendModelRequest();
}

void AgentController::cancel()
{
    if (!m_busy)
        return;

    if (!m_activeRequestId.isEmpty())
        m_orchestrator->cancelRequest(m_activeRequestId);

    m_activeRequestId.clear();
    m_busy = false;
    m_streamAccum.clear();

    emit turnError(tr("Cancelled by user."));
}

// ── Build and send model request ──────────────────────────────────────────────

void AgentController::sendModelRequest()
{
    IAgentProvider *provider = m_orchestrator->activeProvider();
    if (!provider || !provider->isAvailable()) {
        const QString errMsg = tr("AI provider is not available. The connection may have been lost temporarily. "
                                  "Please try again.");
        emit turnError(errMsg);
        finishTurn(errMsg);
        return;
    }

    m_activeRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_streamAccum.clear();

    AgentRequest req;
    req.requestId           = m_activeRequestId;
    req.intent              = m_pendingIntent;
    req.agentMode           = m_session->agentMode();
    req.userPrompt          = m_session->turns().last().userMessage;
    req.activeFilePath      = m_activeFilePath;
    req.selectedText        = m_selectedText;
    req.languageId          = m_languageId;
    req.conversationHistory = m_session->messages();
    req.attachments         = m_attachments;
    req.reasoningEffort     = m_reasoningEffort;

    if (m_currentStepCount == 0) {
        // First call of the turn: beginTurn already added the User message to
        // m_messages, but the provider will add it again via buildUserContent()
        // with file-context enrichment. Remove the duplicate from history.
        if (!req.conversationHistory.isEmpty() &&
            req.conversationHistory.last().role == AgentMessage::Role::User) {
            req.conversationHistory.removeLast();
        }
    } else {
        // Subsequent call (after tool execution): the history already contains
        // the user message in the right position followed by assistant/tool
        // messages. Don't let the provider append another user message.
        req.appendUserMessage = false;
    }
    if (!m_systemPrompt.trimmed().isEmpty() && m_session->turnCount() == 1) {
        bool hasSystem = false;
        for (const auto &msg : req.conversationHistory) {
            if (msg.role == AgentMessage::Role::System) {
                hasSystem = true;
                break;
            }
        }
        if (!hasSystem) {
            AgentMessage systemMsg;
            systemMsg.role = AgentMessage::Role::System;
            systemMsg.content = m_systemPrompt;
            req.conversationHistory.prepend(systemMsg);
        }
    }

    // Inject intent-specific custom instructions
    if (m_contextProvider) {
        const QString wsRoot = m_contextProvider->buildContext(
            {}, m_activeFilePath, {}, {}).workspaceRoot;
        QString intentInstructions;
        switch (m_pendingIntent) {
        case AgentIntent::GenerateTests:
            intentInstructions = PromptFileLoader::loadTestInstructions(wsRoot);
            break;
        case AgentIntent::CodeReview:
            intentInstructions = PromptFileLoader::loadReviewInstructions(wsRoot);
            break;
        case AgentIntent::SuggestCommitMessage:
            intentInstructions = PromptFileLoader::loadCommitInstructions(wsRoot);
            break;
        default:
            intentInstructions = PromptFileLoader::loadCodeGenInstructions(wsRoot);
            break;
        }
        if (!intentInstructions.isEmpty()) {
            AgentMessage instrMsg;
            instrMsg.role = AgentMessage::Role::System;
            instrMsg.content = QStringLiteral("<custom_instructions>\n%1\n</custom_instructions>")
                .arg(intentInstructions);
            // Insert after existing system message
            const int insertAt = (!req.conversationHistory.isEmpty() &&
                req.conversationHistory.first().role == AgentMessage::Role::System) ? 1 : 0;
            req.conversationHistory.insert(insertAt, instrMsg);
        }
    }

    // Inject project brain context (rules, facts, notes)
    if (m_brainBuilder) {
        const auto brainCtx = m_brainBuilder->buildForTask(
            req.userPrompt, {m_activeFilePath});
        const QString brainText = m_brainBuilder->toPromptText(brainCtx);
        if (!brainText.isEmpty()) {
            AgentMessage brainMsg;
            brainMsg.role = AgentMessage::Role::System;
            brainMsg.content = brainText;
            const int insertAt = (!req.conversationHistory.isEmpty() &&
                req.conversationHistory.first().role == AgentMessage::Role::System) ? 1 : 0;
            req.conversationHistory.insert(insertAt, brainMsg);
        }
    }

    // Inject real-time LSP diagnostic updates
    if (m_diagNotifier && m_diagNotifier->hasPendingNotification()) {
        const QString diagText = m_diagNotifier->consumeNotification();
        if (!diagText.isEmpty()) {
            AgentMessage diagMsg;
            diagMsg.role = AgentMessage::Role::System;
            diagMsg.content = diagText;
            req.conversationHistory.append(diagMsg);
        }
    }

    // Auto-compaction: prune oldest non-system messages when conversation is too large.
    // Rough estimate: 4 chars ≈ 1 token. Threshold: ~80% of a 128k context window.
    {
        qint64 totalChars = 0;
        for (const AgentMessage &m : std::as_const(req.conversationHistory))
            totalChars += m.content.size();

        constexpr qint64 kCharLimit = 400000; // ~100k tokens
        if (totalChars > kCharLimit && req.conversationHistory.size() > 4) {
            // Preserve: system messages (at front), last 4 messages, first user message
            int firstNonSystem = 0;
            while (firstNonSystem < req.conversationHistory.size() &&
                   req.conversationHistory[firstNonSystem].role == AgentMessage::Role::System)
                ++firstNonSystem;

            const int keepTail = 4;
            const int start = firstNonSystem + 1; // keep first user msg after system
            const int end = req.conversationHistory.size() - keepTail;
            if (start < end) {
                // Summarize removed messages with a placeholder
                const int removed = end - start;
                AgentMessage summary;
                summary.role = AgentMessage::Role::System;
                summary.content = QStringLiteral(
                    "[%1 earlier messages were auto-compacted to fit context window. "
                    "Key context has been preserved.]").arg(removed);

                for (int i = end - 1; i >= start; --i)
                    req.conversationHistory.removeAt(i);
                req.conversationHistory.insert(start, summary);

                qCInfo(lcCtrl) << "Auto-compacted" << removed << "messages";
            }
        }
    }

    // Inject global context as the first user message if this is a new conversation
    // (only inject once — when there's exactly one user turn so far)
    if (!req.workspaceRoot.isEmpty() || !m_session->turns().isEmpty()) {
        const QString wsRoot = !req.workspaceRoot.isEmpty()
            ? req.workspaceRoot
            : (m_contextProvider ? m_contextProvider
                ->buildContext({}, m_activeFilePath, {}, {}).workspaceRoot : QString{});

        if (!wsRoot.isEmpty()) {
            // Build compact directory tree (max 120 entries, skip build/.git)
            QStringList tree;
            QDirIterator it(wsRoot, QDir::AllEntries | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            int count = 0;
            while (it.hasNext() && count < 120) {
                QString p = it.next();
                if (p.contains(QLatin1String("/.git"))  || p.contains(QLatin1String("\\.git"))  ||
                    p.contains(QLatin1String("/build"))  || p.contains(QLatin1String("\\build")) ||
                    p.contains(QLatin1String("/node_modules")) || p.contains(QLatin1String("\\node_modules")))
                    continue;
                tree << QDir(wsRoot).relativeFilePath(p);
                ++count;
            }
            tree.sort();

            const QString osInfo = QStringLiteral("%1 %2 (%3)")
                .arg(QSysInfo::prettyProductName(),
                     QSysInfo::currentCpuArchitecture(),
                     QSysInfo::kernelVersion());

            // Check for saved agent tools — always tell the model about
            // the persistent toolkit capability, even if no tools exist yet.
            QString agentToolsSection;
            {
                const QDir atDir(QCoreApplication::applicationDirPath()
                                 + QStringLiteral("/AgentTools"));
                QStringList luaFiles;
                if (atDir.exists())
                    luaFiles = atDir.entryList(
                        {QStringLiteral("*.lua")}, QDir::Files, QDir::Name);

                if (!luaFiles.isEmpty()) {
                    QStringList summaries;
                    for (const QString &f : luaFiles) {
                        const QString base = f.chopped(4);
                        const QString metaPath = atDir.filePath(base + QStringLiteral(".json"));
                        QString desc;
                        QFile mf(metaPath);
                        if (mf.open(QIODevice::ReadOnly | QIODevice::Text)) {
                            const auto doc = QJsonDocument::fromJson(mf.readAll());
                            desc = doc.object()[QLatin1String("description")].toString();
                        }
                        summaries << QStringLiteral("  - %1: %2").arg(base, desc);
                    }
                    agentToolsSection = QStringLiteral(
                        "\nYou have a persistent Lua toolkit with %1 saved tool(s):\n%2\n"
                        "Call list_lua_tools to see full script contents. "
                        "Use run_lua_tool to execute them by name instead of "
                        "rewriting scripts. Use save_lua_tool to add new tools.\n")
                        .arg(luaFiles.size())
                        .arg(summaries.join(QLatin1Char('\n')));
                } else {
                    agentToolsSection = QStringLiteral(
                        "\nYou have a persistent Lua toolkit (currently empty). "
                        "Use save_lua_tool to save reusable Lua scripts that persist "
                        "across sessions and projects. Saved tools can be listed with "
                        "list_lua_tools and executed with run_lua_tool by name.\n");
                }
            }

            const QString contextMsg =
                QStringLiteral("<environment_info>\n")
                + QStringLiteral("OS: %1\n").arg(osInfo)
                + QStringLiteral("Workspace: %1\n").arg(wsRoot)
                + QStringLiteral("Directory tree:\n%1\n").arg(tree.join(QLatin1Char('\n')))
                + agentToolsSection
                + QStringLiteral("</environment_info>");

            // Only prepend if no environment_info block is already present
            bool alreadyPresent = false;
            for (const AgentMessage &m : req.conversationHistory) {
                if (m.content.contains(QLatin1String("<environment_info>")))
                    { alreadyPresent = true; break; }
            }
            if (!alreadyPresent) {
                AgentMessage ctxMsg;
                ctxMsg.role    = AgentMessage::Role::User;
                ctxMsg.content = contextMsg;
                // Insert right after system message (index 0 if present, else prepend)
                const int insertAt = (!req.conversationHistory.isEmpty() &&
                    req.conversationHistory.first().role == AgentMessage::Role::System) ? 1 : 0;
                req.conversationHistory.insert(insertAt, ctxMsg);
            }
        }
    }

    // In agent mode, attach available tools — send ALL tool definitions
    // so the model knows everything available. Permission enforcement
    // happens at execution time via ToolApprovalService.
    if (m_session->agentMode() && m_toolRegistry) {
        req.tools = m_toolRegistry->definitions(AgentToolPermission::Dangerous);
    }

    // Build context asynchronously if provider supports it
    if (m_contextProvider) {
        auto *watcher = new QFutureWatcher<ContextSnapshot>(this);
        connect(watcher, &QFutureWatcher<ContextSnapshot>::finished,
                this, [this, watcher, req]() mutable {
            ContextSnapshot ctx = watcher->result();
            watcher->deleteLater();

            req.workspaceRoot = ctx.workspaceRoot;
            if (req.activeFilePath.isEmpty())
                req.activeFilePath = ctx.activeFilePath;

            qCDebug(lcCtrl) << "Sending model request" << m_activeRequestId
                             << "step" << m_currentStepCount
                             << "tools:" << req.tools.size();

            m_orchestrator->sendRequest(req);
        });
        watcher->setFuture(m_contextProvider->buildContextAsync(
            req.userPrompt, m_activeFilePath, m_selectedText, m_languageId));
    } else {
        qCDebug(lcCtrl) << "Sending model request" << m_activeRequestId
                         << "step" << m_currentStepCount
                         << "tools:" << req.tools.size();

        m_orchestrator->sendRequest(req);
    }
}

// ── Response handling ─────────────────────────────────────────────────────────

void AgentController::onResponseDelta(const QString &requestId,
                                      const QString &textChunk)
{
    if (requestId != m_activeRequestId)
        return;
    m_streamAccum += textChunk;
    emit streamingDelta(textChunk);
}

void AgentController::onResponseFinished(const QString &requestId,
                                         const AgentResponse &response)
{
    if (requestId != m_activeRequestId)
        return;

    m_activeRequestId.clear();
    m_currentStepCount++;

    const QString responseText = m_streamAccum.isEmpty() ? response.text
                                                         : m_streamAccum;

    // Did the model request tool calls?
    if (!response.toolCalls.isEmpty() && m_session->agentMode()) {
        // Record the model's response as a step
        AgentStep modelStep;
        modelStep.type          = AgentStep::Type::ModelCall;
        modelStep.timestamp     = QDateTime::currentDateTime().toString(Qt::ISODate);
        modelStep.modelResponse = responseText;
        m_session->addStep(modelStep);
        emit stepCompleted(modelStep);

        // Check step limit
        if (m_currentStepCount >= m_maxSteps) {
            qCWarning(lcCtrl) << "Max steps reached (" << m_maxSteps << "), stopping";
            finishTurn(tr("[Agent reached maximum step limit (%1). "
                         "The response may be incomplete.]").arg(m_maxSteps));
            return;
        }

        // Execute tools and continue the loop
        processToolCalls(response.toolCalls);
        return;
    }

    // No tool calls — this is the final answer
    QList<PatchProposal> patches = extractPatches(responseText);

    if (m_store)
        m_store->recordAssistantMessage(m_session->id(), responseText);

    AgentStep finalStep;
    finalStep.type      = AgentStep::Type::FinalAnswer;
    finalStep.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
    finalStep.finalText = responseText;
    finalStep.patches   = patches;
    m_session->addStep(finalStep);
    emit stepCompleted(finalStep);

    finishTurn(responseText, patches);
}

void AgentController::onResponseError(const QString &requestId,
                                      const AgentError &error)
{
    if (requestId != m_activeRequestId)
        return;

    m_activeRequestId.clear();

    AgentStep errorStep;
    errorStep.type         = AgentStep::Type::Error;
    errorStep.timestamp    = QDateTime::currentDateTime().toString(Qt::ISODate);
    errorStep.errorMessage = error.message;
    m_session->addStep(errorStep);
    emit stepCompleted(errorStep);

    m_busy = false;
    emit turnError(error.message);
}

// ── Tool execution ────────────────────────────────────────────────────────────

void AgentController::processToolCalls(const QList<ToolCall> &toolCalls)
{
    struct PendingTool {
        ToolCall       tc;
        QJsonObject    args;
        ToolExecResult result;
        bool           denied = false;
    };

    QVector<PendingTool> work(toolCalls.size());

    // ── Phase 1: Permission checks on main thread ─────────────────────────
    int approvedCount = 0;
    for (int i = 0; i < toolCalls.size(); ++i) {
        work[i].tc   = toolCalls[i];
        work[i].args = QJsonDocument::fromJson(toolCalls[i].arguments.toUtf8()).object();

        qCInfo(lcCtrl) << "Tool call:" << toolCalls[i].name << "id:" << toolCalls[i].id;
        emit toolCallStarted(toolCalls[i].name, work[i].args);

        if (m_toolRegistry) {
            ITool *tool = m_toolRegistry->tool(toolCalls[i].name);
            if (tool) {
                const ToolSpec &spec = tool->spec();

                if (m_approvalService) {
                    // ── Centralised approval via ToolApprovalService ───────
                    const QString desc = tr("Tool '%1' wants to execute a potentially "
                                           "dangerous operation.").arg(toolCalls[i].name);
                    auto decision = m_approvalService->evaluate(
                        toolCalls[i].name, spec.permission, work[i].args, desc);
                    if (decision == ToolApprovalService::Decision::Deny) {
                        work[i].denied = true;
                        work[i].result = {false, {}, {},
                            tr("Permission denied: tool '%1' requires elevated permissions.")
                                .arg(toolCalls[i].name)};
                    }
                } else if (static_cast<int>(spec.permission) >
                    static_cast<int>(m_maxPermission)
                    && !m_alwaysAllowedTools.contains(toolCalls[i].name)) {
                    // ── Legacy built-in approval path ─────────────────────
                    ToolApproval approval = ToolApproval::Deny;
                    if (m_confirmFn) {
                        const QString desc = tr("Tool '%1' wants to execute a potentially "
                                               "dangerous operation.").arg(toolCalls[i].name);
                        approval = m_confirmFn(toolCalls[i].name, work[i].args, desc);
                    }
                    if (approval == ToolApproval::AllowAlways)
                        m_alwaysAllowedTools.insert(toolCalls[i].name);
                    if (approval == ToolApproval::Deny) {
                        work[i].denied = true;
                        work[i].result = {false, {}, {},
                            tr("Permission denied: tool '%1' requires elevated permissions.")
                                .arg(toolCalls[i].name)};
                    }
                }
            }
        }
        if (!work[i].denied)
            ++approvedCount;
    }

    // ── Phase 2: Execute tools (sequential) ─────────────────────────────
    // Sequential execution avoids nested QEventLoop issues that cause crashes
    // when combined with the tool confirmation callback's event loop.
    for (auto &w : work) {
        if (!w.denied)
            w.result = executeSingleTool(w.tc);
    }

    // ── Phase 3: Record results & emit signals (main thread) ──────────────
    for (const auto &w : work) {
        AgentStep step;
        step.type        = AgentStep::Type::ToolCall;
        step.timestamp   = QDateTime::currentDateTime().toString(Qt::ISODate);
        step.toolCall    = w.tc;
        step.toolResult  = w.result.ok ? w.result.textContent : w.result.error;
        step.toolSuccess = w.result.ok;
        m_session->addStep(step);
        emit stepCompleted(step);
        emit toolCallFinished(w.tc.name, w.result);

        if (m_store) {
            m_store->recordToolCall(m_session->id(), w.tc.name, w.args,
                                    w.result.ok,
                                    w.result.ok ? w.result.textContent : w.result.error);
        }

        m_currentStepCount++;
        if (m_currentStepCount >= m_maxSteps) {
            qCWarning(lcCtrl) << "Max steps reached during tool execution";
            finishTurn(tr("[Agent reached maximum step limit (%1). "
                         "The response may be incomplete.]").arg(m_maxSteps));
            return;
        }
    }

    // All tools executed — send results back to model for next step
    sendModelRequest();
}

ToolExecResult AgentController::executeSingleTool(const ToolCall &tc)
{
    if (!m_toolRegistry) {
        return {false, {}, {}, tr("Tool registry not available.")};
    }

    ITool *tool = m_toolRegistry->tool(tc.name);
    if (!tool) {
        return {false, {}, {},
                tr("Unknown tool: '%1'. Available tools: %2")
                    .arg(tc.name, m_toolRegistry->toolNames().join(QLatin1String(", ")))};
    }

    QJsonObject args = QJsonDocument::fromJson(tc.arguments.toUtf8()).object();

    qCDebug(lcCtrl) << "Executing tool:" << tc.name
                     << "args:" << QJsonDocument(args).toJson(QJsonDocument::Compact);

    return tool->invoke(args);
}

// ── Finish turn ───────────────────────────────────────────────────────────────

void AgentController::finishTurn(const QString &finalText,
                                 const QList<PatchProposal> &patches)
{
    m_busy = false;
    m_activeRequestId.clear();
    m_streamAccum.clear();

    // Emit patch proposals
    for (const auto &patch : patches)
        emit patchProposed(patch);

    if (m_session && !m_session->turns().isEmpty()) {
        emit turnFinished(m_session->turns().last());
    }

    qCInfo(lcCtrl) << "Turn finished. Steps:" << m_currentStepCount
                    << "Patches:" << patches.size();
}

// ── Patch extraction from model response ──────────────────────────────────────
//
// Looks for fenced code blocks with file paths in the response text.
// Format recognized:
//   ```filepath:path/to/file.cpp
//   ... code ...
//   ```
// This is a simple heuristic — more sophisticated diff parsing can be added.

QList<PatchProposal> AgentController::extractPatches(const QString &responseText)
{
    QList<PatchProposal> patches;

    // Pattern: ```language:filepath\n...code...\n```
    // or: ```filepath\n...code...\n```
    const QStringList lines = responseText.split(QLatin1Char('\n'));
    int i = 0;

    while (i < lines.size()) {
        const QString &line = lines[i];

        // Look for ``` with a filepath indicator
        if (line.startsWith(QLatin1String("```")) && line.contains(QLatin1Char(':'))) {
            // Try to extract filepath from the fence line
            // Format: ```lang:path or ```filepath:path
            QString rest = line.mid(3).trimmed();
            QString filePath;

            const int colonIdx = rest.indexOf(QLatin1Char(':'));
            if (colonIdx > 0) {
                filePath = rest.mid(colonIdx + 1).trimmed();
            }

            if (!filePath.isEmpty()) {
                // Collect content until closing ```
                QString content;
                ++i;
                while (i < lines.size() && !lines[i].startsWith(QLatin1String("```"))) {
                    if (!content.isEmpty())
                        content += QLatin1Char('\n');
                    content += lines[i];
                    ++i;
                }

                PatchProposal patch;
                patch.action      = PatchProposal::Action::Edit;
                patch.filePath    = filePath;
                patch.fullContent = content;
                patch.description = tr("Code block for %1").arg(filePath);
                patches.append(patch);
            }
        }
        ++i;
    }

    return patches;
}
