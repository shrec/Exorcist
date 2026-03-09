#include "chatpanelwidget.h"

#include <QComboBox>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMimeDatabase>
#include <QSaveFile>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include "../agentcontroller.h"
#include "../agentmodes.h"
#include "../agentorchestrator.h"
#include "../agentsession.h"
#include "../chatsessionservice.h"
#include "../contextbuilder.h"
#include "../markdownrenderer.h"
#include "../promptvariables.h"
#include "../sessionstore.h"

#include "chatinputwidget.h"
#include "chatsessionhistorypopup.h"
#include "chatsessionmodel.h"
#include "chatthemetokens.h"
#include "chattranscriptview.h"
#include "chatwelcomewidget.h"
#include "toolpresentationformatter.h"
#include "ui/notificationtoast.h"

// ═══════════════════════════════════════════════════════════════════════════════
//  Construction
// ═══════════════════════════════════════════════════════════════════════════════

ChatPanelWidget::ChatPanelWidget(AgentOrchestrator *orchestrator,
                                 QWidget *parent)
    : QWidget(parent)
    , m_orchestrator(orchestrator)
{
    m_sessionModel = new ChatSessionModel(this);
    buildUi();
    connectOrchestrator();
    showWelcomeOrTranscript();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  UI Layout
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::buildUi()
{
    m_rootLayout = new QVBoxLayout(this);
    m_rootLayout->setContentsMargins(0, 0, 0, 0);
    m_rootLayout->setSpacing(0);

    setStyleSheet(
        QStringLiteral("ChatPanelWidget { background:%1; }").arg(ChatTheme::PanelBg));

    // ── Header bar (session title + actions) ─────────────────────────
    m_headerBar = new QWidget(this);
    m_headerBar->setStyleSheet(
        QStringLiteral("background:%1; border-bottom:1px solid %2;")
            .arg(ChatTheme::SideBarBg, ChatTheme::Border));
    auto *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(10, 4, 6, 4);
    headerLayout->setSpacing(4);

    m_sessionTitleLabel = new QLabel(this);
    m_sessionTitleLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:12px; font-weight:600;")
            .arg(ChatTheme::FgPrimary));
    m_sessionTitleLabel->setTextFormat(Qt::PlainText);
    m_sessionTitleLabel->setText(tr("Copilot"));

    headerLayout->addWidget(m_sessionTitleLabel, 1);

    auto *historyHeaderBtn = new QToolButton(m_headerBar);
    historyHeaderBtn->setText(QStringLiteral("\u2630"));
    historyHeaderBtn->setToolTip(tr("Session History"));
    historyHeaderBtn->setAccessibleName(tr("Session history"));
    historyHeaderBtn->setStyleSheet(
        QStringLiteral("QToolButton { background:transparent; color:%1; border:none;"
                       " font-size:13px; padding:2px 6px; border-radius:%2px; }"
                       "QToolButton:hover { color:%3; background:%4; }")
            .arg(ChatTheme::FgSecondary, QString::number(ChatTheme::RadiusMedium),
                 ChatTheme::FgPrimary, ChatTheme::HoverBg));
    connect(historyHeaderBtn, &QToolButton::clicked,
            this, &ChatPanelWidget::onShowHistory);
    headerLayout->addWidget(historyHeaderBtn);

    m_newSessionHeaderBtn = new QToolButton(m_headerBar);
    m_newSessionHeaderBtn->setText(QStringLiteral("+"));
    m_newSessionHeaderBtn->setToolTip(tr("New Chat"));
    m_newSessionHeaderBtn->setAccessibleName(tr("New chat session"));
    m_newSessionHeaderBtn->setStyleSheet(
        QStringLiteral("QToolButton { background:transparent; color:%1; border:none;"
                       " font-size:16px; font-weight:300; padding:2px 6px;"
                       " border-radius:%2px; }"
                       "QToolButton:hover { color:%3; background:%4; }")
            .arg(ChatTheme::FgSecondary, QString::number(ChatTheme::RadiusMedium),
                 ChatTheme::FgPrimary, ChatTheme::HoverBg));
    connect(m_newSessionHeaderBtn, &QToolButton::clicked,
            this, &ChatPanelWidget::onNewSession);
    headerLayout->addWidget(m_newSessionHeaderBtn);

    m_gearHeaderBtn = new QToolButton(m_headerBar);
    m_gearHeaderBtn->setText(QStringLiteral("\u2699"));
    m_gearHeaderBtn->setToolTip(tr("AI Settings"));
    m_gearHeaderBtn->setAccessibleName(tr("AI Settings"));
    m_gearHeaderBtn->setStyleSheet(
        QStringLiteral("QToolButton { background:transparent; color:%1; border:none;"
                       " font-size:14px; padding:2px 6px; border-radius:%2px; }"
                       "QToolButton:hover { color:%3; background:%4; }")
            .arg(ChatTheme::FgSecondary, QString::number(ChatTheme::RadiusMedium),
                 ChatTheme::FgPrimary, ChatTheme::HoverBg));
    connect(m_gearHeaderBtn, &QToolButton::clicked,
            this, &ChatPanelWidget::settingsRequested);
    headerLayout->addWidget(m_gearHeaderBtn);

    m_rootLayout->addWidget(m_headerBar);

    // ── Changes bar (hidden by default) ──────────────────────────────
    m_changesBar = new QWidget(this);
    m_changesBar->hide();
    auto *changesLayout = new QHBoxLayout(m_changesBar);
    changesLayout->setContentsMargins(8, 4, 8, 4);
    changesLayout->setSpacing(6);

    m_changesLabel = new QLabel(this);
    m_changesLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:12px;").arg(ChatTheme::FgPrimary));

    m_keepBtn = new QToolButton(this);
    m_keepBtn->setText(tr("Keep All"));
    m_keepBtn->setStyleSheet(
        QStringLiteral("QToolButton { background:%1; color:%2; padding:3px 10px;"
                      " border-radius:4px; font-size:11px; border:none; }"
                      "QToolButton:hover { background:%3; }")
            .arg(ChatTheme::ButtonBg, ChatTheme::ButtonFg, ChatTheme::ButtonHover));

    m_undoBtn = new QToolButton(this);
    m_undoBtn->setText(tr("Undo All"));
    m_undoBtn->setStyleSheet(
        QStringLiteral("QToolButton { background:%1; color:%2; padding:3px 10px;"
                      " border-radius:4px; font-size:11px; border:none; }"
                      "QToolButton:hover { background:%3; }")
            .arg(ChatTheme::SecondaryBtnBg, ChatTheme::SecondaryBtnFg,
                 ChatTheme::SecondaryBtnHover));

    changesLayout->addWidget(m_changesLabel, 1);
    changesLayout->addWidget(m_keepBtn);
    changesLayout->addWidget(m_undoBtn);

    // Keep = accept changes as-is, clear patch list
    connect(m_keepBtn, &QToolButton::clicked, this, [this]() {
        m_pendingPatches.clear();
        hideChangesBar();
    });

    // Undo = restore file snapshots from agent session
    connect(m_undoBtn, &QToolButton::clicked, this, [this]() {
        if (m_pendingPatches.isEmpty()) {
            hideChangesBar();
            return;
        }
        if (m_agentController) {
            const auto *session = m_agentController->session();
            if (session) {
                const auto &snaps = session->fileSnapshots();
                for (auto it = snaps.begin(); it != snaps.end(); ++it) {
                    QSaveFile f(it.key());
                    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                        f.write(it.value().toUtf8());
                        f.commit();
                    }
                }
            }
        }
        m_pendingPatches.clear();
        hideChangesBar();
    });

    m_changesBar->setStyleSheet(
        QStringLiteral("background:%1; border-bottom:1px solid %2;")
            .arg(ChatTheme::InputBg, ChatTheme::Border));
    m_rootLayout->addWidget(m_changesBar);

    // ── Stacked area: welcome / transcript ───────────────────────────
    m_stack = new QStackedWidget(this);

    m_welcome = new ChatWelcomeWidget(m_stack);
    m_stack->addWidget(m_welcome);

    m_transcript = new ChatTranscriptView(m_sessionModel, m_stack);
    m_stack->addWidget(m_transcript);

    m_rootLayout->addWidget(m_stack, 1);

    // ── Input widget ─────────────────────────────────────────────────
    m_inputWidget = new ChatInputWidget(this);
    m_rootLayout->addWidget(m_inputWidget);

    // ── Wire input signals ───────────────────────────────────────────
    connect(m_inputWidget, &ChatInputWidget::sendRequested,
            this, &ChatPanelWidget::onSend);
    connect(m_inputWidget, &ChatInputWidget::cancelRequested,
            this, &ChatPanelWidget::onCancel);
    connect(m_inputWidget, &ChatInputWidget::newSessionRequested,
            this, &ChatPanelWidget::onNewSession);
    connect(m_inputWidget, &ChatInputWidget::historyRequested,
            this, &ChatPanelWidget::onShowHistory);
    connect(m_inputWidget, &ChatInputWidget::followupClicked,
            this, &ChatPanelWidget::onFollowupClicked);
    connect(m_inputWidget, &ChatInputWidget::modeChanged,
            this, [this](int mode) {
        m_currentMode = mode;
        if (m_agentController) {
            m_agentController->setSystemPrompt(AgentModes::systemPromptForMode(mode));
            m_agentController->setMaxToolPermission(AgentModes::maxPermissionForMode(mode));
        }
    });
    connect(m_inputWidget, &ChatInputWidget::modelSelected,
            this, [this](const QString &modelId) {
        if (auto *active = m_orchestrator->activeProvider())
            active->setModel(modelId);
    });

    // Wire welcome
    connect(m_welcome, &ChatWelcomeWidget::suggestionClicked,
            this, [this](const QString &msg) {
        m_inputWidget->setInputText(msg);
    });
    connect(m_welcome, &ChatWelcomeWidget::signInRequested,
            this, [this] {
        if (auto *active = m_orchestrator->activeProvider())
            active->initialize();
    });
    connect(m_welcome, &ChatWelcomeWidget::retryRequested,
            this, [this] {
        if (auto *active = m_orchestrator->activeProvider())
            active->initialize();
    });

    connectTranscript();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Orchestrator signals
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::connectOrchestrator()
{
    connect(m_orchestrator, &AgentOrchestrator::providerRegistered,
            this, &ChatPanelWidget::onProviderRegistered);
    connect(m_orchestrator, &AgentOrchestrator::providerRemoved,
            this, &ChatPanelWidget::onProviderRemoved);
    connect(m_orchestrator, &AgentOrchestrator::activeProviderChanged,
            this, &ChatPanelWidget::onActiveProviderChanged);
    connect(m_orchestrator, &AgentOrchestrator::responseDelta,
            this, &ChatPanelWidget::onResponseDelta);
    connect(m_orchestrator, &AgentOrchestrator::thinkingDelta,
            this, &ChatPanelWidget::onThinkingDelta);
    connect(m_orchestrator, &AgentOrchestrator::responseFinished,
            this, &ChatPanelWidget::onResponseFinished);
    connect(m_orchestrator, &AgentOrchestrator::responseError,
            this, &ChatPanelWidget::onResponseError);
    connect(m_orchestrator, &AgentOrchestrator::modelsChanged,
            this, &ChatPanelWidget::refreshModelList);
    connect(m_orchestrator, &AgentOrchestrator::providerAvailabilityChanged,
            this, [this](bool) {
        showWelcomeOrTranscript();
        refreshModelList();
    });

    refreshModelList();
}

void ChatPanelWidget::connectController()
{
    if (!m_agentController)
        return;

    connect(m_agentController, &AgentController::toolCallStarted,
            this, [this](const QString &toolName, const QJsonObject &args) {
        if (m_sessionModel->isEmpty())
            return;
        auto part = ChatContentPart::toolInvocation(
            toolName,
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            {}, {},
            ChatContentPart::ToolState::Streaming);
        ToolPresentationFormatter::enrichToolPart(part);
        part.toolInput = QString::fromUtf8(
            QJsonDocument(args).toJson(QJsonDocument::Compact));

        m_sessionModel->appendPart(part);
        int idx = m_sessionModel->turnCount() - 1;
        m_transcript->addContentPart(idx, part);
    });

    connect(m_agentController, &AgentController::toolCallFinished,
            this, [this](const QString &toolName, const ToolExecResult &result) {
        if (m_sessionModel->isEmpty())
            return;
        auto &turn = m_sessionModel->currentTurn();
        // find the most recent tool part matching this tool name with Streaming state
        for (int i = turn.parts.size() - 1; i >= 0; --i) {
            auto &p = turn.parts[i];
            if (p.type == ChatContentPart::Type::ToolInvocation
                && p.toolName == toolName
                && p.toolState == ChatContentPart::ToolState::Streaming)
            {
                p.toolState = result.ok
                    ? ChatContentPart::ToolState::CompleteSuccess
                    : ChatContentPart::ToolState::CompleteError;
                p.toolOutput = result.textContent.left(500);
                int idx = m_sessionModel->turnCount() - 1;
                m_transcript->updateToolState(idx, p.toolCallId, p);
                emit m_sessionModel->turnUpdated(idx);
                break;
            }
        }
    });

    connect(m_agentController, &AgentController::patchProposed,
            this, [this](const PatchProposal &patch) {
        m_pendingPatches.append(patch);
        showChangesBar(m_pendingPatches.size());
    });
}

void ChatPanelWidget::connectTranscript()
{
    connect(m_transcript, &ChatTranscriptView::followupClicked,
            this, &ChatPanelWidget::onFollowupClicked);
    connect(m_transcript, &ChatTranscriptView::feedbackGiven,
            this, &ChatPanelWidget::onFeedback);
    connect(m_transcript, &ChatTranscriptView::toolConfirmed,
            this, &ChatPanelWidget::onToolConfirmed);
    connect(m_transcript, &ChatTranscriptView::fileClicked,
            this, &ChatPanelWidget::openFileRequested);
    connect(m_transcript, &ChatTranscriptView::codeActionRequested,
            this, &ChatPanelWidget::onCodeAction);
    connect(m_transcript, &ChatTranscriptView::insertCodeRequested,
            this, [this](const QString &code) {
        if (m_insertAtCursorFn)
            m_insertAtCursorFn(code);
    });
    connect(m_transcript, &ChatTranscriptView::retryRequested,
            this, [this](const QString &turnId) {
        // Find the turn's user message and re-send
        for (int i = 0; i < m_sessionModel->turnCount(); ++i) {
            if (m_sessionModel->turn(i).id == turnId) {
                const QString userMsg = m_sessionModel->turn(i).userMessage;
                if (!userMsg.isEmpty())
                    onSend(userMsg, m_currentMode);
                break;
            }
        }
    });
    connect(m_transcript, &ChatTranscriptView::signInRequested,
            this, [this] {
        if (auto *active = m_orchestrator->activeProvider())
            active->initialize();
    });

    // Per-file keep/undo from workspace edit widgets in transcript
    connect(m_transcript, &ChatTranscriptView::keepFileRequested,
            this, [this](int, const QString &path) {
        // Remove patches for this file (accept as-is)
        m_pendingPatches.erase(
            std::remove_if(m_pendingPatches.begin(), m_pendingPatches.end(),
                [&path](const PatchProposal &p) { return p.filePath == path; }),
            m_pendingPatches.end());
        if (m_pendingPatches.isEmpty())
            hideChangesBar();
        else
            showChangesBar(m_pendingPatches.size());
    });
    connect(m_transcript, &ChatTranscriptView::undoFileRequested,
            this, [this](int, const QString &path) {
        if (!m_agentController)
            return;
        const auto *session = m_agentController->session();
        if (!session)
            return;
        const auto &snaps = session->fileSnapshots();
        auto it = snaps.find(path);
        if (it != snaps.end()) {
            QSaveFile f(it.key());
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(it.value().toUtf8());
                f.commit();
            }
        }
        m_pendingPatches.erase(
            std::remove_if(m_pendingPatches.begin(), m_pendingPatches.end(),
                [&path](const PatchProposal &p) { return p.filePath == path; }),
            m_pendingPatches.end());
        if (m_pendingPatches.isEmpty())
            hideChangesBar();
        else
            showChangesBar(m_pendingPatches.size());
    });
    connect(m_transcript, &ChatTranscriptView::keepAllRequested,
            this, [this]() { m_keepBtn->click(); });
    connect(m_transcript, &ChatTranscriptView::undoAllRequested,
            this, [this]() { m_undoBtn->click(); });
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Setters
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::setAgentController(AgentController *controller)
{
    m_agentController = controller;
    connectController();

    // Wire tool confirmation: AgentController uses a synchronous callback,
    // but our UI needs async confirmation via ChatToolInvocationWidget buttons.
    // We show a ConfirmationNeeded tool part and spin a local QEventLoop
    // until the user clicks Allow/Deny, which onToolConfirmed() resolves.
    if (m_agentController) {
        m_agentController->setToolConfirmationCallback(
            [this](const QString &toolName, const QJsonObject &args,
                   const QString &description) -> AgentController::ToolApproval
        {
            // Ask mode: read-only tools — auto-allow
            if (m_currentMode == 0)
                return AgentController::ToolApproval::AllowOnce;

            // Edit/Agent mode: show confirmation UI and block via event loop
            const QString callId =
                QUuid::createUuid().toString(QUuid::WithoutBraces);

            if (!m_sessionModel->isEmpty()) {
                auto part = ChatContentPart::toolInvocation(
                    toolName, callId,
                    description.isEmpty() ? toolName : description,
                    {},
                    ChatContentPart::ToolState::ConfirmationNeeded);
                ToolPresentationFormatter::enrichToolPart(part);
                part.toolInput = QString::fromUtf8(
                    QJsonDocument(args).toJson(QJsonDocument::Compact));
                m_sessionModel->appendPart(part);
                int idx = m_sessionModel->turnCount() - 1;
                m_transcript->addContentPart(idx, part);
            }

            // Spin a nested event loop until onToolConfirmed() is called
            AgentController::ToolApproval result =
                AgentController::ToolApproval::Deny;
            QEventLoop loop;
            m_pendingConfirmCallId = callId;
            m_pendingConfirmResolve = [&result, &loop](
                AgentController::ToolApproval decision) {
                result = decision;
                loop.quit();
            };
            loop.exec();
            return result;
        });
    }
}

void ChatPanelWidget::setSessionStore(SessionStore *store)
{
    m_sessionStore = store;
}

void ChatPanelWidget::setEditorContext(const QString &filePath,
                                       const QString &selectedText,
                                       const QString &languageId)
{
    m_activeFilePath = filePath;
    m_selectedText = selectedText;
    m_languageId = languageId;
}

void ChatPanelWidget::focusInput()
{
    m_inputWidget->focusInput();
}

void ChatPanelWidget::setInputText(const QString &text)
{
    m_inputWidget->setInputText(text);
}

void ChatPanelWidget::setInputEnabled(bool enabled)
{
    m_inputWidget->setEnabled(enabled);
}

void ChatPanelWidget::attachSelection(const QString &text, const QString &filePath,
                                      int startLine)
{
    Q_UNUSED(startLine)
    if (!text.isEmpty()) {
        m_selectedText = text;
        m_activeFilePath = filePath;
    }
}

void ChatPanelWidget::attachDiagnostics(const QList<AgentDiagnostic> &diagnostics)
{
    m_pendingDiagnostics = diagnostics;
    if (!diagnostics.isEmpty()) {
        QStringList lines;
        for (const auto &d : diagnostics) {
            lines.append(QStringLiteral("%1:%2 %3")
                .arg(d.line + 1).arg(d.column + 1).arg(d.message));
        }
        m_inputWidget->attachDiagnostics(
            lines.join(QLatin1Char('\n')),
            diagnostics.size());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Provider management
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::onProviderRegistered(const QString &id)
{
    Q_UNUSED(id)
    refreshModelList();
    showWelcomeOrTranscript();
}

void ChatPanelWidget::onProviderRemoved(const QString &id)
{
    Q_UNUSED(id)
    refreshModelList();
    showWelcomeOrTranscript();
}

void ChatPanelWidget::onActiveProviderChanged(const QString &id)
{
    Q_UNUSED(id)
    refreshModelList();
    showWelcomeOrTranscript();
}

void ChatPanelWidget::refreshModelList()
{
    m_inputWidget->clearModels();
    const auto *active = m_orchestrator->activeProvider();
    if (!active)
        return;

    const auto infoList = active->modelInfoList();
    if (!infoList.isEmpty()) {
        for (const auto &mi : infoList)
            m_inputWidget->addModel(mi.id, mi.name,
                                    mi.billing.isPremium,
                                    mi.billing.multiplier);
    } else {
        const auto models = active->availableModels();
        for (const auto &m : models)
            m_inputWidget->addModel(m, m);
    }
    m_inputWidget->setCurrentModel(active->currentModel());
}

void ChatPanelWidget::showWelcomeOrTranscript()
{
    if (m_sessionModel->isEmpty()) {
        // Check for error states
        const auto *active = m_orchestrator->activeProvider();
        if (!active) {
            m_welcome->showState(ChatWelcomeWidget::State::NoProvider);
        } else if (!active->isAvailable()) {
            m_welcome->showState(ChatWelcomeWidget::State::AuthRequired);
        } else {
            m_welcome->showState(ChatWelcomeWidget::State::Default);
        }
        m_stack->setCurrentWidget(m_welcome);
    } else {
        m_stack->setCurrentWidget(m_transcript);
    }
    updateSessionTitle();
}

void ChatPanelWidget::updateSessionTitle()
{
    if (m_sessionModel->isEmpty()) {
        m_sessionTitleLabel->setText(tr("Copilot"));
    } else {
        const QString title = m_sessionModel->title();
        m_sessionTitleLabel->setText(title.isEmpty() ? tr("Copilot") : title);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Send / Cancel
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::onSend(const QString &text, int mode)
{
    if (text.trimmed().isEmpty())
        return;

    // /new command → new session
    if (text.trimmed().startsWith(QLatin1String("/new"))) {
        onNewSession();
        return;
    }

    const auto *active = m_orchestrator->activeProvider();
    if (!active || !active->isAvailable()) {
        // Show error in transcript
        if (m_sessionModel->isEmpty())
            m_sessionModel->beginTurn(text);
        m_sessionModel->errorTurn(
            tr("No available provider. Select or configure a provider first."));
        showWelcomeOrTranscript();
        m_stack->setCurrentWidget(m_transcript);
        return;
    }

    m_currentMode = mode;

    // Resolve variables
    QString expandedPrompt = text;
    if (m_varResolver) {
        QStringList unresolved;
        expandedPrompt = m_varResolver->resolveInPrompt(text, &unresolved);
    }

    // Resolve slash commands
    AgentIntent intent = AgentIntent::Chat;
    const QString effectiveText = resolveSlashCommand(expandedPrompt, intent);

    // Extract the slash command prefix for display
    QString slashCmd;
    if (expandedPrompt.startsWith(QLatin1Char('/')) && effectiveText != expandedPrompt) {
        slashCmd = expandedPrompt.left(
            expandedPrompt.size() - effectiveText.size()).trimmed();
    }

    startRequest(effectiveText, mode, slashCmd);
}

void ChatPanelWidget::startRequest(const QString &text, int mode,
                                   const QString &slashCmd)
{
    // Begin a new turn in the session model
    const auto *prov = m_orchestrator->activeProvider();
    const QString modelId = prov ? prov->currentModel() : QString();
    const QString providerId = prov ? prov->id() : QString();

    // Collect attachments from input widget before they are cleared
    const auto &inputAtts = m_inputWidget->attachments();
    QStringList attachmentNames;
    QList<Attachment> reqAttachments;
    for (const auto &a : inputAtts) {
        attachmentNames << a.name;
        Attachment att;
        att.type = a.isImage ? Attachment::Type::Image : Attachment::Type::File;
        att.path = a.path;
        att.data = a.data;
        if (a.isImage) {
            QMimeDatabase db;
            att.mimeType = db.mimeTypeForFile(a.path).name();
        }
        reqAttachments.append(att);
    }

    m_sessionModel->beginTurn(text, attachmentNames, mode, modelId, providerId, slashCmd);

    // Populate context references for the turn
    if (m_contextBuilder) {
        ContextSnapshot ctx = m_contextBuilder->buildContext(
            text, m_activeFilePath, m_selectedText, m_languageId);
        auto &turn = m_sessionModel->currentTurn();
        for (const auto &item : std::as_const(ctx.items)) {
            ChatTurnModel::TurnReference ref;
            ref.filePath = item.filePath;
            ref.tooltip  = item.content.left(200);
            switch (item.type) {
            case ContextItem::Type::ActiveFile:
                ref.label = QStringLiteral("\U0001F4C4 %1").arg(item.label);
                break;
            case ContextItem::Type::Selection:
                ref.label = QStringLiteral("\u2702 selection");
                break;
            case ContextItem::Type::GitDiff:
            case ContextItem::Type::GitStatus:
                ref.label = QStringLiteral("\U0001F500 git");
                break;
            case ContextItem::Type::Diagnostics:
                ref.label = QStringLiteral("\u26A0 diagnostics");
                break;
            case ContextItem::Type::WorkspaceInfo:
                ref.label = QStringLiteral("\U0001F4C1 workspace");
                break;
            default:
                ref.label = item.label;
                break;
            }
            turn.references.append(ref);
        }
    }

    // Auto-generate session title from first user message
    if (m_sessionModel->turnCount() == 1 && m_sessionModel->title().isEmpty()) {
        QString title = text.simplified();
        if (title.length() > 50)
            title = title.left(47) + QStringLiteral("...");
        m_sessionModel->setTitle(title);
        updateSessionTitle();
    }

    m_stack->setCurrentWidget(m_transcript);
    m_transcript->scrollToBottom();

    // Add user message to conversation history
    m_conversationHistory.append({AgentMessage::Role::User, text});

    m_pendingRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingIntent = AgentIntent::Chat;

    m_inputWidget->setStreaming(true);

    const bool agentMode = AgentModes::usesAgentLoop(mode);

    // Session store recording
    if (m_chatSessionService) {
        m_chatSessionService->recordUserMessage(m_sessionModel->id(), text);
        if (m_sessionModel->turnCount() == 1)
            m_chatSessionService->setSessionTitle(m_sessionModel->id(),
                                                  m_sessionModel->title());
    } else if (m_sessionStore) {
        m_sessionStore->appendUserMessage(m_sessionModel->id(), text);
        if (m_sessionModel->turnCount() == 1)
            m_sessionStore->setSessionTitle(m_sessionModel->id(),
                                            m_sessionModel->title());
    }

    // Agent mode → delegate to controller
    if (agentMode && m_agentController) {
        m_agentController->newSession(true);

        auto deltaConn = std::make_shared<QMetaObject::Connection>();
        *deltaConn = connect(m_agentController, &AgentController::streamingDelta,
                             this, [this, deltaConn](const QString &chunk) {
            onResponseDelta(m_pendingRequestId, chunk);
        });

        auto finishConn = std::make_shared<QMetaObject::Connection>();
        *finishConn = connect(m_agentController, &AgentController::turnFinished,
                              this, [this, deltaConn, finishConn](const AgentTurn &turn) {
            disconnect(*deltaConn);
            disconnect(*finishConn);

            AgentResponse resp;
            resp.requestId = m_pendingRequestId;
            resp.text = turn.steps.isEmpty() ? QString()
                : turn.steps.last().finalText;
            onResponseFinished(m_pendingRequestId, resp);
        });

        auto errorConn = std::make_shared<QMetaObject::Connection>();
        *errorConn = connect(m_agentController, &AgentController::turnError,
                             this, [this, deltaConn, finishConn, errorConn](const QString &msg) {
            disconnect(*deltaConn);
            disconnect(*finishConn);
            disconnect(*errorConn);

            AgentError err;
            err.requestId = m_pendingRequestId;
            err.message = msg;
            onResponseError(m_pendingRequestId, err);
        });

        m_agentController->sendMessage(text, m_activeFilePath,
                                       m_selectedText, m_languageId,
                                       m_pendingIntent, reqAttachments);
        return;
    }

    // Standard ask mode → direct orchestrator request
    AgentRequest req;
    req.requestId           = m_pendingRequestId;
    req.intent              = m_pendingIntent;
    req.agentMode           = false;
    req.userPrompt          = text;
    req.activeFilePath      = m_activeFilePath;
    req.selectedText        = m_selectedText;
    req.languageId          = m_languageId;
    req.conversationHistory = m_conversationHistory;
    req.diagnostics         = m_pendingDiagnostics;
    req.attachments         = reqAttachments;
    m_pendingDiagnostics.clear();

    m_orchestrator->sendRequest(req);
}

void ChatPanelWidget::onCancel()
{
    if (m_pendingRequestId.isEmpty())
        return;

    if (m_agentController)
        m_agentController->cancel();
    m_orchestrator->cancelRequest(m_pendingRequestId);

    if (!m_sessionModel->isEmpty()) {
        m_sessionModel->cancelTurn();
        int idx = m_sessionModel->turnCount() - 1;
        if (auto *w = m_transcript->turnWidget(idx))
            w->finishTurn(ChatTurnModel::State::Cancelled);
    }

    m_pendingRequestId.clear();
    m_inputWidget->setStreaming(false);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Streaming responses
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::onResponseDelta(const QString &requestId,
                                      const QString &chunk)
{
    if (requestId != m_pendingRequestId)
        return;

    int idx = m_sessionModel->turnCount() - 1;
    if (idx < 0)
        return;

    m_sessionModel->appendMarkdownDelta(chunk);
    m_transcript->appendMarkdownDelta(idx, chunk);
}

void ChatPanelWidget::onThinkingDelta(const QString &requestId,
                                      const QString &chunk)
{
    if (requestId != m_pendingRequestId)
        return;

    int idx = m_sessionModel->turnCount() - 1;
    if (idx < 0)
        return;

    m_sessionModel->appendThinkingDelta(chunk);
    m_transcript->appendThinkingDelta(idx, chunk);
}

void ChatPanelWidget::onResponseFinished(const QString &requestId,
                                         const AgentResponse &response)
{
    if (requestId != m_pendingRequestId)
        return;

    m_pendingRequestId.clear();
    m_inputWidget->setStreaming(false);

    // Add assistant message to history
    m_conversationHistory.append({AgentMessage::Role::Assistant, response.text});

    // Record in session store
    if (m_chatSessionService)
        m_chatSessionService->recordAssistantMessage(m_sessionModel->id(), response.text);
    else if (m_sessionStore)
        m_sessionStore->appendAssistantMessage(m_sessionModel->id(), response.text);

    // If the response has proposed edits, show workspace edit part
    if (!response.proposedEdits.isEmpty()) {
        QList<ChatContentPart::EditedFile> files;
        for (const auto &edit : response.proposedEdits) {
            ChatContentPart::EditedFile f;
            f.filePath = edit.filePath;
            f.action = ChatContentPart::EditedFile::Action::Modified;
            f.state = ChatContentPart::EditedFile::State::Applied;

            // Build a diff preview from pending patches for this file
            for (const auto &patch : m_pendingPatches) {
                if (patch.filePath == edit.filePath) {
                    QStringList diffLines;
                    for (const auto &hunk : patch.hunks) {
                        diffLines << QStringLiteral("@@ -%1 +%1 @@")
                                         .arg(hunk.startLine);
                        if (!hunk.oldText.isEmpty()) {
                            for (const auto &l : hunk.oldText.split(QLatin1Char('\n')))
                                diffLines << (QLatin1Char('-') + l);
                            f.deletions += hunk.oldText.split(QLatin1Char('\n')).size();
                        }
                        if (!hunk.newText.isEmpty()) {
                            for (const auto &l : hunk.newText.split(QLatin1Char('\n')))
                                diffLines << (QLatin1Char('+') + l);
                            f.insertions += hunk.newText.split(QLatin1Char('\n')).size();
                        }
                    }
                    f.diffPreview = diffLines.join(QLatin1Char('\n'));
                    break;
                }
            }
            files.append(f);
        }
        auto part = ChatContentPart::workspaceEdit(files);
        m_sessionModel->appendPart(part);
        int idx = m_sessionModel->turnCount() - 1;
        m_transcript->addContentPart(idx, part);
    }

    // If the response includes followup suggestions, show them
    if (!response.followups.isEmpty()) {
        QVector<ChatContentPart::FollowupItem> items;
        for (const auto &msg : response.followups)
            items.append({msg, {}, {}});
        auto part = ChatContentPart::followup(items);
        m_sessionModel->appendPart(part);
        int idx = m_sessionModel->turnCount() - 1;
        m_transcript->addContentPart(idx, part);
    }

    // Complete the turn in the model
    m_sessionModel->completeTurn();
    int idx = m_sessionModel->turnCount() - 1;
    if (auto *w = m_transcript->turnWidget(idx)) {
        w->finishTurn(ChatTurnModel::State::Complete);
        if (response.totalTokens > 0)
            w->setTokenUsage(response.promptTokens, response.completionTokens,
                             response.totalTokens);
    }

    // Persist full turn data to session store
    persistCompletedTurn(idx);
}

void ChatPanelWidget::onResponseError(const QString &requestId,
                                      const AgentError &error)
{
    if (requestId != m_pendingRequestId)
        return;

    m_pendingRequestId.clear();
    m_inputWidget->setStreaming(false);

    int idx = m_sessionModel->turnCount() - 1;
    if (idx < 0)
        return;

    // Use error-code-specific messaging
    QString displayMsg;
    switch (error.code) {
    case AgentError::Code::AuthError:
        displayMsg = tr("Authentication failed. Please sign in again.");
        NotificationToast::show(this, displayMsg, NotificationToast::Error, 6000);
        break;
    case AgentError::Code::RateLimited:
        displayMsg = tr("Rate limit exceeded. Try again in a moment, or switch models.");
        NotificationToast::show(this, displayMsg, NotificationToast::Warning, 6000);
        break;
    case AgentError::Code::ContentFilter:
        displayMsg = tr("Response was blocked by the content safety filter.");
        break;
    case AgentError::Code::NetworkError:
        displayMsg = tr("Network error. Check your internet connection.");
        break;
    case AgentError::Code::Cancelled:
        displayMsg = tr("Request cancelled.");
        break;
    default:
        displayMsg = error.message;
        break;
    }

    m_sessionModel->errorTurn(displayMsg);
    if (auto *w = m_transcript->turnWidget(idx)) {
        w->showError(displayMsg, error.code);
        w->finishTurn(ChatTurnModel::State::Error);
    }

    // Persist full turn data to session store
    persistCompletedTurn(idx);

    if (m_chatSessionService)
        m_chatSessionService->recordError(m_sessionModel->id(), displayMsg);
    else if (m_sessionStore)
        m_sessionStore->appendError(m_sessionModel->id(), displayMsg);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Interaction slots
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::onFollowupClicked(const QString &message)
{
    m_inputWidget->setInputText(message);
    onSend(message, m_currentMode);
}

void ChatPanelWidget::onToolConfirmed(const QString &callId, bool allowed)
{
    // Update the tool part state in the model
    if (!m_sessionModel->isEmpty()) {
        auto &turn = m_sessionModel->currentTurn();
        for (auto &p : turn.parts) {
            if (p.type == ChatContentPart::Type::ToolInvocation
                && p.toolCallId == callId
                && p.toolState == ChatContentPart::ToolState::ConfirmationNeeded)
            {
                p.toolState = allowed
                    ? ChatContentPart::ToolState::Streaming
                    : ChatContentPart::ToolState::CompleteError;
                int idx = m_sessionModel->turnCount() - 1;
                m_transcript->updateToolState(idx, callId, p);
                emit m_sessionModel->turnUpdated(idx);
                break;
            }
        }
    }
    // Resolve the pending confirmation promise
    if (m_pendingConfirmCallId == callId && m_pendingConfirmResolve) {
        m_pendingConfirmResolve(allowed
            ? AgentController::ToolApproval::AllowOnce
            : AgentController::ToolApproval::Deny);
        m_pendingConfirmResolve = nullptr;
        m_pendingConfirmCallId.clear();
    }
}

void ChatPanelWidget::onFeedback(const QString &turnId, bool helpful)
{
    for (int i = 0; i < m_sessionModel->turnCount(); ++i) {
        if (m_sessionModel->turn(i).id == turnId) {
            m_sessionModel->setTurnFeedback(i,
                helpful ? ChatTurnModel::Feedback::Helpful
                        : ChatTurnModel::Feedback::Unhelpful);
            break;
        }
    }
}

void ChatPanelWidget::onNewSession()
{
    m_sessionModel->clear();
    m_conversationHistory.clear();
    m_pendingPatches.clear();
    m_pendingRequestId.clear();
    hideChangesBar();
    showWelcomeOrTranscript();
    m_inputWidget->setStreaming(false);
    m_inputWidget->clear();
    updateSessionTitle();
}

void ChatPanelWidget::persistCompletedTurn(int turnIndex)
{
    if (turnIndex < 0 || turnIndex >= m_sessionModel->turnCount())
        return;

    const auto &turn = m_sessionModel->turn(turnIndex);
    const QJsonObject turnJson = turn.toJson();

    if (m_chatSessionService)
        m_chatSessionService->store()->recordCompleteTurn(m_sessionModel->id(), turnJson);
    else if (m_sessionStore)
        m_sessionStore->recordCompleteTurn(m_sessionModel->id(), turnJson);
}

void ChatPanelWidget::restoreSessionTurns(const QJsonArray &completeTurns)
{
    for (const auto &v : completeTurns) {
        const ChatTurnModel turn = ChatTurnModel::fromJson(v.toObject());

        m_sessionModel->beginTurn(
            turn.userMessage, turn.attachmentNames, turn.mode,
            turn.modelId, turn.providerId, turn.slashCommand);

        // Add content parts to model and transcript
        int idx = m_sessionModel->turnCount() - 1;

        for (const auto &part : turn.parts) {
            m_sessionModel->appendPart(part);
            m_transcript->addContentPart(idx, part);
        }

        // Set final state
        auto &currentTurn = m_sessionModel->currentTurn();
        currentTurn.state = turn.state;
        currentTurn.feedback = turn.feedback;
        currentTurn.id = turn.id;
        currentTurn.timestamp = turn.timestamp;

        m_sessionModel->completeTurn();
        if (auto *w = m_transcript->turnWidget(idx))
            w->finishTurn(turn.state);

        // Rebuild conversation history for context
        m_conversationHistory.append({AgentMessage::Role::User, turn.userMessage});
        m_conversationHistory.append({AgentMessage::Role::Assistant, turn.fullMarkdownText()});
    }
}

void ChatPanelWidget::onShowHistory()
{
    if (!m_chatSessionService && !m_sessionStore)
        return;

    if (!m_historyPopup) {
        m_historyPopup = new ChatSessionHistoryPopup(this);
        connect(m_historyPopup, &ChatSessionHistoryPopup::sessionSelected,
                this, [this](const QString &sessionId) {
            // Load session from store
            auto session = m_chatSessionService
                ? m_chatSessionService->loadSession(sessionId)
                : m_sessionStore->loadSession(sessionId);
            onNewSession();

            // Prefer rich turn data if available
            if (!session.completeTurns.isEmpty()) {
                restoreSessionTurns(session.completeTurns);
            } else {
                // Fall back to simple message pairs
                for (const auto &msg : session.messages) {
                    if (msg.first == QLatin1String("user")) {
                        m_sessionModel->beginTurn(msg.second);
                        m_conversationHistory.append({AgentMessage::Role::User, msg.second});
                    } else if (msg.first == QLatin1String("assistant")) {
                        if (m_sessionModel->isEmpty())
                            continue;
                        m_sessionModel->appendMarkdownDelta(msg.second);
                        m_sessionModel->completeTurn();
                        int idx = m_sessionModel->turnCount() - 1;
                        m_transcript->appendMarkdownDelta(idx, msg.second);
                        if (auto *w = m_transcript->turnWidget(idx))
                            w->finishTurn(ChatTurnModel::State::Complete);
                        m_conversationHistory.append({AgentMessage::Role::Assistant, msg.second});
                    }
                }
            }
            m_stack->setCurrentWidget(m_transcript);
        });
        connect(m_historyPopup, &ChatSessionHistoryPopup::newSessionRequested,
                this, &ChatPanelWidget::onNewSession);

        connect(m_historyPopup, &ChatSessionHistoryPopup::sessionDeleted,
                this, [this](const QString &sessionId) {
            if (m_chatSessionService)
                m_chatSessionService->deleteSession(sessionId);
        });
        connect(m_historyPopup, &ChatSessionHistoryPopup::sessionRenamed,
                this, [this](const QString &sessionId, const QString &newTitle) {
            if (m_chatSessionService)
                m_chatSessionService->renameSession(sessionId, newTitle);
        });
    }

    // Populate sessions
    const auto summaries = m_chatSessionService
        ? m_chatSessionService->recentSessions(20)
        : m_sessionStore->recentSessions(20);
    QList<ChatSessionHistoryPopup::SessionEntry> entries;
    for (const auto &s : summaries) {
        entries.append({s.sessionId, s.title, s.created, s.turnCount});
    }
    m_historyPopup->setSessions(entries);

    // Position above the input widget
    const QPoint pos = m_inputWidget->mapToGlobal(QPoint(0, -400));
    m_historyPopup->showAt(pos);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Changes bar
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::showChangesBar(int editCount)
{
    m_changesLabel->setText(
        tr("%1 file%2 changed")
            .arg(editCount)
            .arg(editCount == 1 ? "" : "s"));
    m_changesBar->show();
}

void ChatPanelWidget::hideChangesBar()
{
    m_changesBar->hide();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Slash commands
// ═══════════════════════════════════════════════════════════════════════════════

QString ChatPanelWidget::resolveSlashCommand(const QString &text,
                                             AgentIntent &intent) const
{
    // Match basic slash commands
    if (text.startsWith(QLatin1String("/explain"))) {
        intent = AgentIntent::ExplainCode;
        return text.mid(8).trimmed();
    }
    if (text.startsWith(QLatin1String("/fix"))) {
        intent = AgentIntent::FixDiagnostic;
        return text.mid(4).trimmed();
    }
    if (text.startsWith(QLatin1String("/test"))) {
        intent = AgentIntent::GenerateTests;
        return text.mid(5).trimmed();
    }
    if (text.startsWith(QLatin1String("/doc"))) {
        intent = AgentIntent::Chat; // no dedicated docs intent yet
        return text.mid(4).trimmed();
    }
    if (text.startsWith(QLatin1String("/review"))) {
        intent = AgentIntent::CodeReview;
        return text.mid(7).trimmed();
    }
    if (text.startsWith(QLatin1String("/refactor"))) {
        intent = AgentIntent::RefactorSelection;
        return text.mid(9).trimmed();
    }
    if (text.startsWith(QLatin1String("/edit"))) {
        intent = AgentIntent::CreateFile;
        return text.mid(5).trimmed();
    }
    return text;
}

void ChatPanelWidget::onCodeAction(const QUrl &url)
{
    // Non-codeblock URLs (codeblock:// handled directly in ChatMarkdownWidget)
    if (url.scheme() == QLatin1String("file")) {
        emit openFileRequested(url.toLocalFile());
    }
}
