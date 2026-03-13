#include "chatpanelwidget.h"

#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMimeDatabase>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include "../agentcontroller.h"
#include "../agentmodes.h"
#include "../agentorchestrator.h"
#include "../modelregistry.h"
#include "../agentsession.h"
#include "../chatsessionservice.h"
#include "../contextbuilder.h"
#include "../../ui/markdownrenderer.h"
#include "../promptvariables.h"
#include "../sessionstore.h"
#include "chatinputwidget.h"
#include "chatsessionhistorypopup.h"
#include "chatsessionmodel.h"
#include "chatthemetokens.h"
#include "toolpresentationformatter.h"
#include "ui/notificationtoast.h"

#ifdef EXORCIST_HAS_ULTRALIGHT
#include "ultralight/ultralightwidget.h"
#include "ultralight/chatjsbridge.h"
#include <QFile>
#else
#include "chattranscriptview.h"
#include "chatwelcomewidget.h"
#endif

// ═══════════════════════════════════════════════════════════════════════════════
//  Construction
// ═══════════════════════════════════════════════════════════════════════════════

ChatPanelWidget::ChatPanelWidget(AgentOrchestrator *orchestrator,
                                 QWidget *parent)
    : QWidget(parent)
    , m_orchestrator(orchestrator)
{
    m_sessionModel = new ChatSessionModel(this);

    // Streaming delta throttle — flush buffered deltas every 50ms
    // instead of evaluating JS for every single token.
    m_deltaFlushTimer = new QTimer(this);
    m_deltaFlushTimer->setInterval(50);
    m_deltaFlushTimer->setSingleShot(false);
    connect(m_deltaFlushTimer, &QTimer::timeout, this, [this]() {
        const int idx = m_sessionModel->turnCount() - 1;
        if (idx < 0) {
            m_deltaFlushTimer->stop();
            return;
        }
        if (!m_deltaBuffer.isEmpty()) {
            const QString buf = m_deltaBuffer;
            m_deltaBuffer.clear();
#ifdef EXORCIST_HAS_ULTRALIGHT
            m_jsBridge->appendMarkdownDelta(idx, buf);
#else
            m_transcript->appendMarkdownDelta(idx, buf);
#endif
        }
        if (!m_thinkingDeltaBuffer.isEmpty()) {
            const QString buf = m_thinkingDeltaBuffer;
            m_thinkingDeltaBuffer.clear();
#ifdef EXORCIST_HAS_ULTRALIGHT
            m_jsBridge->appendThinkingDelta(idx, buf);
#else
            m_transcript->appendThinkingDelta(idx, buf);
#endif
        }
        if (m_deltaBuffer.isEmpty() && m_thinkingDeltaBuffer.isEmpty())
            m_deltaFlushTimer->stop();
    });

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

#ifdef EXORCIST_HAS_ULTRALIGHT
    // ── Full-HTML path: single Ultralight WebView fills the entire panel ────
    m_ultralightView = new exorcist::UltralightWidget(this);
    m_jsBridge = new exorcist::ChatJSBridge(m_ultralightView, this);

    {
        auto loadRes = [](const QString &path) -> QString {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) {
                qWarning("ChatPanel: failed to load resource %s", qPrintable(path));
                return {};
            }
            return QString::fromUtf8(f.readAll());
        };
        const QString css        = loadRes(QStringLiteral(":/chat/chat.css"));
        const QString markdownJs = loadRes(QStringLiteral(":/chat/markdown.js"));
        const QString chatJs     = loadRes(QStringLiteral(":/chat/chat.js"));
        QString htmlTemplate     = loadRes(QStringLiteral(":/chat/chat.html"));

        // Inject CSS and JS into the HTML template via placeholders
        htmlTemplate.replace(QLatin1String("%STYLE%"), css);
        htmlTemplate.replace(QLatin1String("%MARKDOWN_JS%"), markdownJs);
        htmlTemplate.replace(QLatin1String("%CHAT_JS%"), chatJs);

        // loadHTML uses about:blank + JS injection (no temp files needed)
        m_ultralightView->loadHTML(htmlTemplate);
    }

    // Apply theme once DOM is ready
    connect(m_ultralightView, &exorcist::UltralightWidget::domReady,
            this, [this]() {
        QJsonObject theme;
        const bool dark = ChatTheme::isDark();
        theme[QStringLiteral("panelBg")]       = dark ? ChatTheme::PanelBg : ChatTheme::L_PanelBg;
        theme[QStringLiteral("editorBg")]      = dark ? ChatTheme::EditorBg : ChatTheme::L_EditorBg;
        theme[QStringLiteral("fgPrimary")]     = dark ? ChatTheme::FgPrimary : ChatTheme::L_FgPrimary;
        theme[QStringLiteral("fgSecondary")]   = dark ? ChatTheme::FgSecondary : ChatTheme::L_FgSecondary;
        theme[QStringLiteral("fgDimmed")]      = dark ? ChatTheme::FgDimmed : ChatTheme::L_FgDimmed;
        theme[QStringLiteral("fgBright")]      = dark ? ChatTheme::FgBright : ChatTheme::L_FgBright;
        theme[QStringLiteral("border")]        = dark ? ChatTheme::Border : ChatTheme::L_Border;
        theme[QStringLiteral("sepLine")]       = dark ? ChatTheme::SepLine : ChatTheme::L_SepLine;
        theme[QStringLiteral("hoverBg")]       = dark ? ChatTheme::HoverBg : ChatTheme::L_HoverBg;
        theme[QStringLiteral("codeBg")]        = dark ? ChatTheme::CodeBg : ChatTheme::L_CodeBg;
        theme[QStringLiteral("codeHeaderBg")]  = dark ? ChatTheme::CodeHeaderBg : ChatTheme::L_CodeHeaderBg;
        theme[QStringLiteral("thinkingBg")]    = dark ? ChatTheme::ThinkingBg : ChatTheme::L_ThinkingBg;
        theme[QStringLiteral("thinkingBorder")]= dark ? ChatTheme::ThinkingBorder : ChatTheme::L_ThinkingBorder;
        theme[QStringLiteral("thinkingFg")]    = dark ? ChatTheme::ThinkingFg : ChatTheme::L_ThinkingFg;
        theme[QStringLiteral("scrollTrack")]   = dark ? ChatTheme::ScrollTrack : ChatTheme::L_ScrollTrack;
        theme[QStringLiteral("scrollThumb")]   = dark ? ChatTheme::ScrollHandle : ChatTheme::L_ScrollHandle;
        m_jsBridge->setTheme(theme);
    });

    m_rootLayout->addWidget(m_ultralightView, 1);

    // ── Wire JS→C++ signals for input, header, changes bar ─────────
    connect(m_jsBridge, &exorcist::ChatJSBridge::sendRequested,
            this, &ChatPanelWidget::onSend);
    connect(m_jsBridge, &exorcist::ChatJSBridge::cancelRequested,
            this, &ChatPanelWidget::onCancel);
    connect(m_jsBridge, &exorcist::ChatJSBridge::newSessionRequested,
            this, &ChatPanelWidget::onNewSession);
    connect(m_jsBridge, &exorcist::ChatJSBridge::historyRequested,
            this, &ChatPanelWidget::onShowHistory);
    connect(m_jsBridge, &exorcist::ChatJSBridge::settingsRequested,
            this, &ChatPanelWidget::settingsRequested);
    connect(m_jsBridge, &exorcist::ChatJSBridge::modeChanged,
            this, [this](int mode) {
        m_currentMode = mode;
        if (m_agentController) {
            m_agentController->setSystemPrompt(AgentModes::systemPromptForMode(mode));
            m_agentController->setMaxToolPermission(AgentModes::maxPermissionForMode(mode));
        }
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::modelSelected,
            this, [this](const QString &modelId) {
        if (auto *active = m_orchestrator->activeProvider())
            active->setModel(modelId);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::thinkingToggled,
            this, [this](bool enabled) {
        m_thinkingEnabled = enabled;
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::attachFileRequested,
            this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Attach File"), m_workspaceRoot,
            tr("All Files (*.*)"));
        if (path.isEmpty()) return;

        const QFileInfo fi(path);
        const int idx = m_pendingFileAttachments.size();
        m_pendingFileAttachments.append(path);
        m_jsBridge->addAttachmentChip(fi.fileName(), idx);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::mentionQueryRequested,
            this, [this](const QString &trigger, const QString &filter) {
        QJsonArray items;
        const QString lower = filter.toLower();

        if (trigger == QLatin1String("@")) {
            if (!m_workspaceFileFn) {
                m_jsBridge->setMentionItems(trigger, items);
                return;
            }
            const QStringList files = m_workspaceFileFn();
            int shown = 0;
            for (const QString &absPath : files) {
                const QFileInfo fi(absPath);
                const QString display = fi.fileName();
                if (!lower.isEmpty()
                    && !display.toLower().contains(lower)
                    && !absPath.toLower().contains(lower))
                    continue;
                QJsonObject o;
                o[QStringLiteral("label")] = display;
                o[QStringLiteral("desc")] = absPath;
                o[QStringLiteral("insertText")] = QStringLiteral("@file:%1").arg(absPath);
                items.append(o);
                if (++shown >= 20)
                    break;
            }
        } else if (trigger == QLatin1String("#")) {
            if (!m_varResolver) {
                m_jsBridge->setMentionItems(trigger, items);
                return;
            }
            const auto vars = m_varResolver->matchingVars(filter);
            for (const auto &v : vars) {
                QJsonObject o;
                o[QStringLiteral("label")] = QStringLiteral("#%1").arg(v.token);
                o[QStringLiteral("desc")] = v.description;
                o[QStringLiteral("insertText")] = QStringLiteral("#%1").arg(v.token);
                items.append(o);
                if (items.size() >= 20)
                    break;
            }
        }

        m_jsBridge->setMentionItems(trigger, items);
    });

    // Welcome / sign-in
    connect(m_jsBridge, &exorcist::ChatJSBridge::suggestionClicked,
            this, [this](const QString &msg) {
        m_jsBridge->setInputText(msg);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::signInRequested,
            this, [this] {
        if (auto *active = m_orchestrator->activeProvider())
            active->initialize();
    });

    connect(m_jsBridge, &exorcist::ChatJSBridge::openExternalUrlRequested,
            this, [](const QString &url) {
        QDesktopServices::openUrl(QUrl(url));
    });

    // Slash commands
    {
        QJsonArray cmds;
        const QStringList names = {
            QStringLiteral("/explain"), QStringLiteral("/fix"),
            QStringLiteral("/tests"),   QStringLiteral("/review"),
            QStringLiteral("/refactor"),QStringLiteral("/doc"),
            QStringLiteral("/generate"),QStringLiteral("/edit"),
            QStringLiteral("/search"),  QStringLiteral("/new"),
            QStringLiteral("/compact"),
        };
        for (const auto &n : names) {
            QJsonObject o;
            o[QStringLiteral("name")] = n;
            cmds.append(o);
        }
        m_jsBridge->setSlashCommands(cmds);
    }

#else
    // ── Qt-widget path ──────────────────────────────────────────────

    setStyleSheet(
        QStringLiteral("ChatPanelWidget { background:%1; }").arg(ChatTheme::pick(ChatTheme::PanelBg, ChatTheme::L_PanelBg)));

    // ── Header bar (session title + actions) ─────────────────────────
    m_headerBar = new QWidget(this);
    m_headerBar->setFixedHeight(34);
    m_headerBar->setStyleSheet(
        QStringLiteral("background:%1; border-bottom:1px solid %2;")
            .arg(ChatTheme::SideBarBg, ChatTheme::Border));
    auto *headerLayout = new QHBoxLayout(m_headerBar);
    headerLayout->setContentsMargins(12, 0, 8, 0);
    headerLayout->setSpacing(2);

    m_sessionTitleLabel = new QLabel(this);
    m_sessionTitleLabel->setStyleSheet(
        QStringLiteral("color:%1; font-size:12px; font-weight:600; background:transparent;")
            .arg(ChatTheme::FgPrimary));
    m_sessionTitleLabel->setTextFormat(Qt::PlainText);
    m_sessionTitleLabel->setText(tr("Copilot"));

    headerLayout->addWidget(m_sessionTitleLabel, 1);

    // Shared header button style — consistent 26x26 icons with 4px radius
    const QString headerBtnStyle = QStringLiteral(
        "QToolButton { background:transparent; color:%1; border:none;"
        " font-size:14px; padding:0; border-radius:4px; }"
        "QToolButton:hover { color:%2; background:%3; }")
            .arg(ChatTheme::FgSecondary, ChatTheme::FgPrimary, ChatTheme::HoverBg);
    constexpr int headerBtnSize = 26;

    auto *historyHeaderBtn = new QToolButton(m_headerBar);
    historyHeaderBtn->setText(QStringLiteral("\u2630"));
    historyHeaderBtn->setToolTip(tr("Session History"));
    historyHeaderBtn->setAccessibleName(tr("Session history"));
    historyHeaderBtn->setFixedSize(headerBtnSize, headerBtnSize);
    historyHeaderBtn->setStyleSheet(headerBtnStyle);
    connect(historyHeaderBtn, &QToolButton::clicked,
            this, &ChatPanelWidget::onShowHistory);
    headerLayout->addWidget(historyHeaderBtn);

    m_newSessionHeaderBtn = new QToolButton(m_headerBar);
    m_newSessionHeaderBtn->setText(QStringLiteral("+"));
    m_newSessionHeaderBtn->setToolTip(tr("New Chat"));
    m_newSessionHeaderBtn->setAccessibleName(tr("New chat session"));
    m_newSessionHeaderBtn->setFixedSize(headerBtnSize, headerBtnSize);
    m_newSessionHeaderBtn->setStyleSheet(
        QStringLiteral(
            "QToolButton { background:transparent; color:%1; border:none;"
            " font-size:17px; font-weight:300; padding:0; border-radius:4px; }"
            "QToolButton:hover { color:%2; background:%3; }")
            .arg(ChatTheme::FgSecondary, ChatTheme::FgPrimary, ChatTheme::HoverBg));
    connect(m_newSessionHeaderBtn, &QToolButton::clicked,
            this, &ChatPanelWidget::onNewSession);
    headerLayout->addWidget(m_newSessionHeaderBtn);

    m_gearHeaderBtn = new QToolButton(m_headerBar);
    m_gearHeaderBtn->setText(QStringLiteral("\u2699"));
    m_gearHeaderBtn->setToolTip(tr("AI Settings"));
    m_gearHeaderBtn->setAccessibleName(tr("AI Settings"));
    m_gearHeaderBtn->setFixedSize(headerBtnSize, headerBtnSize);
    m_gearHeaderBtn->setStyleSheet(headerBtnStyle);
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
                    if (it.value().isNull()) {
                        // File didn't exist before — remove it
                        QFile::remove(it.key());
                    } else {
                        QSaveFile f(it.key());
                        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                            f.write(it.value().toUtf8());
                            f.commit();
                        }
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

    // ── Main content area: welcome / transcript ─────────────────────
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

    // Wire welcome / transcript
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
#endif // EXORCIST_HAS_ULTRALIGHT

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

        // Build a human-readable description from tool arguments
        const QString desc = ToolPresentationFormatter::descriptionFromArgs(toolName, args);
        if (!desc.isEmpty())
            part.toolInvocationMsg = desc;

        m_sessionModel->appendPart(part);
        int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->addContentPart(idx, part.toJson());
#else
        m_transcript->addContentPart(idx, part);
#endif
    });

    connect(m_agentController, &AgentController::toolCallFinished,
            this, [this](const QString &toolName, const ToolExecResult &result) {
        if (m_sessionModel->isEmpty())
            return;

        // Auto-open newly created files in the editor
        if (result.ok && toolName == QLatin1String("create_file")) {
            const QString filePath = result.data[QLatin1String("filePath")].toString();
            if (!filePath.isEmpty())
                emit openFileRequested(filePath);
        }
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
                // Set past-tense message from presentation if not already set
                if (p.toolPastTenseMsg.isEmpty()) {
                    auto pres = ToolPresentationFormatter::present(toolName);
                    p.toolPastTenseMsg = pres.pastTenseMessage;
                }
                int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
                m_jsBridge->updateToolState(idx, p.toolCallId, p.toJson());
#else
                m_transcript->updateToolState(idx, p.toolCallId, p);
#endif
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

    // Show changes bar when tools modify files (checkpoint system)
    connect(m_agentController, &AgentController::filesChanged,
            this, [this](const QStringList &filePaths) {
        if (!filePaths.isEmpty()) {
            // Create synthetic PatchProposals so Keep/Undo knows about them
            for (const auto &fp : filePaths) {
                bool alreadyTracked = false;
                for (const auto &pp : m_pendingPatches) {
                    if (pp.filePath == fp) { alreadyTracked = true; break; }
                }
                if (!alreadyTracked) {
                    PatchProposal pp;
                    pp.filePath = fp;
                    m_pendingPatches.append(pp);
                }
            }
            showChangesBar(m_pendingPatches.size());
        }
    });

    // Token usage reporting — update input widget context token display
    connect(m_agentController, &AgentController::tokenUsageUpdated,
            this, [this](int promptTokens, int, int totalTokens) {
        if (m_inputWidget && totalTokens > 0)
            m_inputWidget->setContextTokenCount(promptTokens);
    });
}

void ChatPanelWidget::connectTranscript()
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    connect(m_jsBridge, &exorcist::ChatJSBridge::followupClicked,
            this, &ChatPanelWidget::onFollowupClicked);
    connect(m_jsBridge, &exorcist::ChatJSBridge::feedbackGiven,
            this, &ChatPanelWidget::onFeedback);
    connect(m_jsBridge, &exorcist::ChatJSBridge::toolConfirmed,
            this, &ChatPanelWidget::onToolConfirmed);
    connect(m_jsBridge, &exorcist::ChatJSBridge::fileClicked,
            this, &ChatPanelWidget::openFileRequested);
    connect(m_jsBridge, &exorcist::ChatJSBridge::insertCodeRequested,
            this, [this](const QString &code) {
        if (m_insertAtCursorFn)
            m_insertAtCursorFn(code);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::copyCodeRequested,
            this, [](const QString &code) {
        QGuiApplication::clipboard()->setText(code);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::copyTextRequested,
            this, [](const QString &text) {
        QGuiApplication::clipboard()->setText(text);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::applyCodeRequested,
            this, [this](const QString &code, const QString &, const QString &filePath) {
        QString target = filePath.trimmed();
        if (!target.isEmpty() && QFileInfo(target).isRelative() && !m_workspaceRoot.isEmpty())
            target = QDir(m_workspaceRoot).filePath(target);
        if (target.isEmpty())
            target = m_activeFilePath;

        if (!target.isEmpty()) {
            QSaveFile f(target);
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                f.write(code.toUtf8());
                f.commit();
                emit openFileRequested(target);
                return;
            }
        }

        if (m_insertAtCursorFn)
            m_insertAtCursorFn(code);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::runCodeRequested,
            this, [this](const QString &code, const QString &) {
        if (m_runInTerminalFn)
            m_runInTerminalFn(code);
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::retryRequested,
            this, [this](const QString &turnId) {
        for (int i = 0; i < m_sessionModel->turnCount(); ++i) {
            if (m_sessionModel->turn(i).id == turnId) {
                const QString userMsg = m_sessionModel->turn(i).userMessage;
                if (!userMsg.isEmpty())
                    onSend(userMsg, m_currentMode);
                break;
            }
        }
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::keepFileRequested,
            this, [this](int, const QString &path) {
        m_pendingPatches.erase(
            std::remove_if(m_pendingPatches.begin(), m_pendingPatches.end(),
                [&path](const PatchProposal &p) { return p.filePath == path; }),
            m_pendingPatches.end());
        if (m_pendingPatches.isEmpty()) hideChangesBar();
        else showChangesBar(m_pendingPatches.size());
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::undoFileRequested,
            this, [this](int, const QString &path) {
        if (!m_agentController) return;
        const auto *session = m_agentController->session();
        if (!session) return;
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
        if (m_pendingPatches.isEmpty()) hideChangesBar();
        else showChangesBar(m_pendingPatches.size());
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::keepAllRequested,
            this, [this]() {
        m_pendingPatches.clear();
        hideChangesBar();
    });
    connect(m_jsBridge, &exorcist::ChatJSBridge::undoAllRequested,
            this, [this]() {
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
#else
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
    connect(m_transcript, &ChatTranscriptView::keepFileRequested,
            this, [this](int, const QString &path) {
        m_pendingPatches.erase(
            std::remove_if(m_pendingPatches.begin(), m_pendingPatches.end(),
                [&path](const PatchProposal &p) { return p.filePath == path; }),
            m_pendingPatches.end());
        if (m_pendingPatches.isEmpty()) hideChangesBar();
        else showChangesBar(m_pendingPatches.size());
    });
    connect(m_transcript, &ChatTranscriptView::undoFileRequested,
            this, [this](int, const QString &path) {
        if (!m_agentController) return;
        const auto *session = m_agentController->session();
        if (!session) return;
        const auto &snaps = session->fileSnapshots();
        auto it = snaps.find(path);
        if (it != snaps.end()) {
            if (it.value().isNull()) {
                QFile::remove(it.key());
            } else {
                QSaveFile f(it.key());
                if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    f.write(it.value().toUtf8());
                    f.commit();
                }
            }
        }
        m_pendingPatches.erase(
            std::remove_if(m_pendingPatches.begin(), m_pendingPatches.end(),
                [&path](const PatchProposal &p) { return p.filePath == path; }),
            m_pendingPatches.end());
        if (m_pendingPatches.isEmpty()) hideChangesBar();
        else showChangesBar(m_pendingPatches.size());
    });
    connect(m_transcript, &ChatTranscriptView::keepAllRequested,
            this, [this]() { m_keepBtn->click(); });
    connect(m_transcript, &ChatTranscriptView::undoAllRequested,
            this, [this]() { m_undoBtn->click(); });
#endif
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

            // Guard: if a confirmation is already pending (re-entrant call
            // from nested event loop processing), auto-allow to prevent crash
            if (m_pendingConfirmResolve) {
                qWarning("ChatPanelWidget: re-entrant tool confirmation for '%s' — auto-allowing",
                         qPrintable(toolName));
                return AgentController::ToolApproval::AllowOnce;
            }

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
#ifdef EXORCIST_HAS_ULTRALIGHT
                m_jsBridge->addContentPart(idx, part.toJson());
#else
                m_transcript->addContentPart(idx, part);
#endif
            }

            // Spin a nested event loop until onToolConfirmed() is called.
            // Use QPointer to detect if `this` is destroyed during the loop.
            AgentController::ToolApproval result =
                AgentController::ToolApproval::Deny;
            QEventLoop loop;
            QPointer<ChatPanelWidget> guard(this);
            m_pendingConfirmCallId = callId;
            m_pendingConfirmResolve = [&result, &loop](
                AgentController::ToolApproval decision) {
                result = decision;
                loop.quit();
            };
            loop.exec();

            // Check if widget was destroyed during event loop
            if (!guard)
                return AgentController::ToolApproval::Deny;

            return result;
        });
    }
}

void ChatPanelWidget::setSessionStore(SessionStore *store)
{
    m_sessionStore = store;

    // Auto-restore last session if one exists
    if (store) {
        QTimer::singleShot(0, this, [this]() {
            if (!m_sessionStore || !m_sessionModel->isEmpty())
                return;
            const auto last = m_sessionStore->loadLastSession();
            if (last.isEmpty())
                return;
            if (!last.completeTurns.isEmpty()) {
                restoreSession(last.sessionId, last.completeTurns,
                               last.mode, last.title,
                               last.modelId, last.providerId);
            } else if (!last.messages.isEmpty()) {
                for (const auto &msg : last.messages) {
                    if (msg.first == QLatin1String("user")) {
                        m_sessionModel->beginTurn(msg.second);
#ifdef EXORCIST_HAS_ULTRALIGHT
                        {
                            int idx = m_sessionModel->turnCount() - 1;
                            m_jsBridge->addTurn(idx, m_sessionModel->turn(idx).toJson());
                        }
#endif
                        m_conversationHistory.append({AgentMessage::Role::User, msg.second});
                    } else {
                        ChatContentPart part;
                        part.type = ChatContentPart::Type::Markdown;
                        part.markdownText = msg.second;
                        m_sessionModel->appendPart(part);
                        int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
                        m_jsBridge->addContentPart(idx, part.toJson());
                        m_sessionModel->completeTurn();
                        m_jsBridge->finishTurn(idx, static_cast<int>(ChatTurnModel::State::Complete));
#else
                        m_transcript->addContentPart(idx, part);
                        m_sessionModel->completeTurn();
                        if (auto *w = m_transcript->turnWidget(idx))
                            w->finishTurn(ChatTurnModel::State::Complete);
#endif
                        m_conversationHistory.append({AgentMessage::Role::Assistant, msg.second});
                    }
                }
                showWelcomeOrTranscript();
            }
        });
    }
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
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->focusInput();
#else
    m_inputWidget->focusInput();
#endif
}

void ChatPanelWidget::setInputText(const QString &text)
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setInputText(text);
#else
    m_inputWidget->setInputText(text);
#endif
}

void ChatPanelWidget::setInputEnabled(bool enabled)
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setInputEnabled(enabled);
#else
    m_inputWidget->setEnabled(enabled);
#endif
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
#ifndef EXORCIST_HAS_ULTRALIGHT
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
#endif
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
    const auto *active = m_orchestrator->activeProvider();
    if (!active) {
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->clearModels();
#else
        m_inputWidget->clearModels();
#endif
        return;
    }

#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->clearModels();
    const auto infoList = active->modelInfoList();
    if (!infoList.isEmpty()) {
        for (const auto &mi : infoList) {
            bool premium = mi.billing.isPremium;
            double mult  = mi.billing.multiplier;
            if (mult <= 0.0 && m_modelRegistry) {
                const ModelInfo reg = m_modelRegistry->model(mi.id);
                if (!reg.id.isEmpty()) {
                    premium = reg.billing.isPremium;
                    mult    = reg.billing.multiplier;
                }
            }
            QJsonObject obj;
            obj[QStringLiteral("id")]       = mi.id;
            obj[QStringLiteral("name")]     = mi.name;
            obj[QStringLiteral("premium")]  = premium;
            obj[QStringLiteral("mult")]     = mult;
            obj[QStringLiteral("thinking")] = mi.capabilities.thinking;
            obj[QStringLiteral("vision")]   = mi.capabilities.vision;
            obj[QStringLiteral("tools")]    = mi.capabilities.toolCalls;
            m_jsBridge->addModel(obj);
        }
    } else {
        const auto models = active->availableModels();
        for (const auto &m : models) {
            QJsonObject obj;
            obj[QStringLiteral("id")]   = m;
            obj[QStringLiteral("name")] = m;
            if (m_modelRegistry) {
                const ModelInfo info = m_modelRegistry->model(m);
                if (!info.id.isEmpty()) {
                    obj[QStringLiteral("name")]     = info.name;
                    obj[QStringLiteral("premium")]  = info.billing.isPremium;
                    obj[QStringLiteral("mult")]     = info.billing.multiplier;
                    obj[QStringLiteral("thinking")] = info.capabilities.thinking;
                    obj[QStringLiteral("vision")]   = info.capabilities.vision;
                    obj[QStringLiteral("tools")]    = info.capabilities.toolCalls;
                }
            }
            m_jsBridge->addModel(obj);
        }
    }
    m_jsBridge->setCurrentModel(active->currentModel());
#else
    m_inputWidget->clearModels();
    const auto infoList = active->modelInfoList();
    if (!infoList.isEmpty()) {
        for (const auto &mi : infoList) {
            bool premium = mi.billing.isPremium;
            double mult  = mi.billing.multiplier;
            // If provider didn't populate billing, fall back to registry
            if (mult <= 0.0 && m_modelRegistry) {
                const ModelInfo reg = m_modelRegistry->model(mi.id);
                if (!reg.id.isEmpty()) {
                    premium = reg.billing.isPremium;
                    mult    = reg.billing.multiplier;
                }
            }
            m_inputWidget->addModel(mi.id, mi.name,
                                    premium, mult,
                                    mi.capabilities.thinking,
                                    mi.capabilities.vision,
                                    mi.capabilities.toolCalls);
        }
    } else {
        const auto models = active->availableModels();
        for (const auto &m : models) {
            // Look up billing/capabilities from ModelRegistry
            if (m_modelRegistry) {
                const ModelInfo info = m_modelRegistry->model(m);
                if (!info.id.isEmpty()) {
                    m_inputWidget->addModel(info.id, info.name,
                                            info.billing.isPremium,
                                            info.billing.multiplier,
                                            info.capabilities.thinking,
                                            info.capabilities.vision,
                                            info.capabilities.toolCalls);
                    continue;
                }
            }
            m_inputWidget->addModel(m, m);
        }
    }
    m_inputWidget->setCurrentModel(active->currentModel());
#endif
}

void ChatPanelWidget::showWelcomeOrTranscript()
{
    if (m_sessionModel->isEmpty()) {
        const auto *active = m_orchestrator->activeProvider();
        QString state = QStringLiteral("default");
        if (!active)
            state = QStringLiteral("noProvider");
        else if (!active->isAvailable())
            state = QStringLiteral("auth");

#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->showWelcome(state);
#else
        if (state == QLatin1String("noProvider"))
            m_welcome->showState(ChatWelcomeWidget::State::NoProvider);
        else if (state == QLatin1String("auth"))
            m_welcome->showState(ChatWelcomeWidget::State::AuthRequired);
        else
            m_welcome->showState(ChatWelcomeWidget::State::Default);
        m_stack->setCurrentWidget(m_welcome);
#endif
    } else {
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->showTranscript();
#else
        m_stack->setCurrentWidget(m_transcript);
#endif
    }
    updateSessionTitle();
}

void ChatPanelWidget::setToolCount(int count)
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setToolCount(count);
#else
    if (m_inputWidget)
        m_inputWidget->setToolCount(count);
#endif
}

void ChatPanelWidget::updateSessionTitle()
{
    const QString title = m_sessionModel->isEmpty()
        ? tr("Copilot")
        : (m_sessionModel->title().isEmpty() ? tr("Copilot") : m_sessionModel->title());
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setSessionTitle(title);
#else
    m_sessionTitleLabel->setText(title);
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Send / Cancel
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::onSend(const QString &text, int mode)
{
    qWarning("ChatPanelWidget::onSend called: text='%s' mode=%d", qPrintable(text.left(100)), mode);
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
        if (m_sessionModel->isEmpty()) {
            m_sessionModel->beginTurn(text);
#ifdef EXORCIST_HAS_ULTRALIGHT
            {
                int idx = m_sessionModel->turnCount() - 1;
                m_jsBridge->addTurn(idx, m_sessionModel->turn(idx).toJson());
            }
#endif
        }
        m_sessionModel->errorTurn(
            tr("No available provider. Select or configure a provider first."));
#ifdef EXORCIST_HAS_ULTRALIGHT
        {
            int idx = m_sessionModel->turnCount() - 1;
            const auto &turn = m_sessionModel->turn(idx);
            if (!turn.parts.isEmpty())
                m_jsBridge->addContentPart(idx, turn.parts.last().toJson());
            m_jsBridge->finishTurn(idx, static_cast<int>(ChatTurnModel::State::Error));
        }
#endif
        showWelcomeOrTranscript();
#ifndef EXORCIST_HAS_ULTRALIGHT
        m_stack->setCurrentWidget(m_transcript);
#endif
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
    QStringList attachmentNames;
    QList<Attachment> reqAttachments;
#ifdef EXORCIST_HAS_ULTRALIGHT
    for (const QString &path : m_pendingFileAttachments) {
        const QFileInfo fi(path);
        attachmentNames << fi.fileName();
        Attachment att;
        att.path = path;
        att.type = Attachment::Type::File;
        QMimeDatabase db;
        const QString mime = db.mimeTypeForFile(path).name();
        att.mimeType = mime;
        if (mime.startsWith(QLatin1String("image/"))) {
            att.type = Attachment::Type::Image;
            QFile f(path);
            if (f.open(QIODevice::ReadOnly))
                att.data = f.readAll();
        }
        reqAttachments.append(att);
    }
    m_pendingFileAttachments.clear();
#else
    const auto &inputAtts = m_inputWidget->attachments();
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
#endif

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

#ifdef EXORCIST_HAS_ULTRALIGHT
    {
        int idx = m_sessionModel->turnCount() - 1;
        m_jsBridge->addTurn(idx, m_sessionModel->turn(idx).toJson());
    }
    m_jsBridge->showTranscript();
    // Ultralight auto-scrolls via JS
#else
    m_stack->setCurrentWidget(m_transcript);
    m_transcript->scrollToBottom();
#endif

    m_pendingRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingIntent = AgentIntent::Chat;

#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setStreamingState(true);
#else
    m_inputWidget->setStreaming(true);
    m_transcript->setStreamingActive(true);
#endif

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
        // Only create a new session once — reuse for multi-turn context
        if (!m_agentSessionActive) {
            m_agentController->newSession(true);
            m_agentSessionActive = true;
        }

        // Pass current reasoning effort from settings (only if thinking toggle is on)
        const bool thinkOn =
#ifdef EXORCIST_HAS_ULTRALIGHT
            m_thinkingEnabled;
#else
            m_inputWidget->isThinkingEnabled();
#endif
        if (thinkOn) {
            QSettings s;
            s.beginGroup(QStringLiteral("AI"));
            const int idx = s.value(QStringLiteral("reasoningEffort"), 2).toInt();
            s.endGroup();
            static const char *levels[] = {"low", "medium", "high"};
            m_agentController->setReasoningEffort(
                QString::fromLatin1(levels[qBound(0, idx, 2)]));
        } else {
            m_agentController->setReasoningEffort(QString());
        }

        // Disconnect any stale agent connections from prior turns
        for (auto &c : m_agentConns)
            disconnect(c);
        m_agentConns.clear();

        const QString reqId = m_pendingRequestId;

        m_agentConns << connect(m_agentController, &AgentController::streamingDelta,
                                this, [this, reqId](const QString &chunk) {
            if (reqId != m_pendingRequestId) return;
            onResponseDelta(reqId, chunk);
        });

        m_agentConns << connect(m_agentController, &AgentController::turnFinished,
                                this, [this, reqId](const AgentTurn &turn) {
            if (reqId != m_pendingRequestId) return;
            for (auto &c : m_agentConns) disconnect(c);
            m_agentConns.clear();

            AgentResponse resp;
            resp.requestId = reqId;
            resp.text = turn.steps.isEmpty() ? QString()
                : turn.steps.last().finalText;
            onResponseFinished(reqId, resp);
        });

        m_agentConns << connect(m_agentController, &AgentController::turnError,
                                this, [this, reqId](const QString &msg) {
            if (reqId != m_pendingRequestId) return;
            for (auto &c : m_agentConns) disconnect(c);
            m_agentConns.clear();

            AgentError err;
            err.requestId = reqId;
            err.message = msg;
            onResponseError(reqId, err);
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
    // Read reasoning effort from settings, but only apply if thinking toggle is on
    {
        const bool thinkOn2 =
#ifdef EXORCIST_HAS_ULTRALIGHT
            m_thinkingEnabled;
#else
            m_inputWidget->isThinkingEnabled();
#endif
        if (thinkOn2) {
            QSettings s;
            s.beginGroup(QStringLiteral("AI"));
            const int idx = s.value(QStringLiteral("reasoningEffort"), 2).toInt();
            s.endGroup();
            static const char *levels[] = {"low", "medium", "high"};
            req.reasoningEffort = QString::fromLatin1(levels[qBound(0, idx, 2)]);
        }
    }
    m_pendingDiagnostics.clear();

    // Append user message to history AFTER building the request so it
    // is not duplicated — the provider adds it via buildUserContent().
    m_conversationHistory.append({AgentMessage::Role::User, text});

    m_orchestrator->sendRequest(req);
}

void ChatPanelWidget::onCancel()
{
    if (m_pendingRequestId.isEmpty())
        return;

    // Disconnect agent-mode signal connections
    for (auto &c : m_agentConns)
        disconnect(c);
    m_agentConns.clear();

    if (m_agentController)
        m_agentController->cancel();
    m_orchestrator->cancelRequest(m_pendingRequestId);

    if (!m_sessionModel->isEmpty()) {
        m_sessionModel->cancelTurn();
        int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->finishTurn(idx, static_cast<int>(ChatTurnModel::State::Cancelled));
#else
        if (auto *w = m_transcript->turnWidget(idx))
            w->finishTurn(ChatTurnModel::State::Cancelled);
#endif
    }

    m_pendingRequestId.clear();
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setStreamingState(false);
#else
    m_inputWidget->setStreaming(false);
    m_transcript->setStreamingActive(false);
#endif
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

    // Buffer the delta and start the flush timer.
    // JS evaluation happens at most every 50ms instead of per-token.
    m_deltaBuffer += chunk;
    if (!m_deltaFlushTimer->isActive())
        m_deltaFlushTimer->start();
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

    m_thinkingDeltaBuffer += chunk;
    if (!m_deltaFlushTimer->isActive())
        m_deltaFlushTimer->start();
}

void ChatPanelWidget::onResponseFinished(const QString &requestId,
                                         const AgentResponse &response)
{
    if (requestId != m_pendingRequestId)
        return;

    // Flush any buffered streaming deltas before finalizing the turn
    m_deltaFlushTimer->stop();
    {
        const int idx = m_sessionModel->turnCount() - 1;
        if (idx >= 0) {
            if (!m_deltaBuffer.isEmpty()) {
#ifdef EXORCIST_HAS_ULTRALIGHT
                m_jsBridge->appendMarkdownDelta(idx, m_deltaBuffer);
#else
                m_transcript->appendMarkdownDelta(idx, m_deltaBuffer);
#endif
                m_deltaBuffer.clear();
            }
            if (!m_thinkingDeltaBuffer.isEmpty()) {
#ifdef EXORCIST_HAS_ULTRALIGHT
                m_jsBridge->appendThinkingDelta(idx, m_thinkingDeltaBuffer);
#else
                m_transcript->appendThinkingDelta(idx, m_thinkingDeltaBuffer);
#endif
                m_thinkingDeltaBuffer.clear();
            }
        }
    }

    m_pendingRequestId.clear();
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setStreamingState(false);
#else
    m_inputWidget->setStreaming(false);
    m_transcript->setStreamingActive(false);
#endif

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
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->addContentPart(idx, part.toJson());
#else
        m_transcript->addContentPart(idx, part);
#endif
    }

    // If the response includes followup suggestions, show them
    if (!response.followups.isEmpty()) {
        QVector<ChatContentPart::FollowupItem> items;
        for (const auto &msg : response.followups)
            items.append({msg, {}, {}});
        auto part = ChatContentPart::followup(items);
        m_sessionModel->appendPart(part);
        int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->addContentPart(idx, part.toJson());
#else
        m_transcript->addContentPart(idx, part);
#endif
    }

    // Complete the turn in the model
    m_sessionModel->completeTurn();
    int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->finishTurn(idx, static_cast<int>(ChatTurnModel::State::Complete));
    if (response.totalTokens > 0)
        m_jsBridge->setTokenUsage(idx, response.promptTokens,
                                  response.completionTokens,
                                  response.totalTokens);
#else
    if (auto *w = m_transcript->turnWidget(idx)) {
        w->finishTurn(ChatTurnModel::State::Complete);
        if (response.totalTokens > 0)
            w->setTokenUsage(response.promptTokens, response.completionTokens,
                             response.totalTokens);
    }
#endif

    // Persist full turn data to session store
    persistCompletedTurn(idx);
}

void ChatPanelWidget::onResponseError(const QString &requestId,
                                      const AgentError &error)
{
    if (requestId != m_pendingRequestId)
        return;

    m_pendingRequestId.clear();
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setStreamingState(false);
#else
    m_inputWidget->setStreaming(false);
    m_transcript->setStreamingActive(false);
#endif

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
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->finishTurn(idx, static_cast<int>(ChatTurnModel::State::Error));
#else
    if (auto *w = m_transcript->turnWidget(idx)) {
        w->showError(displayMsg, error.code);
        w->finishTurn(ChatTurnModel::State::Error);
    }
#endif

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
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setInputText(message);
#else
    m_inputWidget->setInputText(message);
#endif
    onSend(message, m_currentMode);
}

void ChatPanelWidget::onToolConfirmed(const QString &callId, int approval)
{
    const bool allowed = (approval > 0);

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
#ifdef EXORCIST_HAS_ULTRALIGHT
                m_jsBridge->updateToolState(idx, callId, p.toJson());
#else
                m_transcript->updateToolState(idx, callId, p);
#endif
                emit m_sessionModel->turnUpdated(idx);
                break;
            }
        }
    }
    // Resolve the pending confirmation promise
    // approval: 0=Deny, 1=AllowOnce, 2=AllowAlways
    if (m_pendingConfirmCallId == callId && m_pendingConfirmResolve) {
        AgentController::ToolApproval decision;
        switch (approval) {
        case 2:  decision = AgentController::ToolApproval::AllowAlways; break;
        case 1:  decision = AgentController::ToolApproval::AllowOnce;  break;
        default: decision = AgentController::ToolApproval::Deny;       break;
        }
        m_pendingConfirmResolve(decision);
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

    // Reset agent session so next message creates a fresh one
    for (auto &c : m_agentConns)
        disconnect(c);
    m_agentConns.clear();
    m_agentSessionActive = false;

    hideChangesBar();
    showWelcomeOrTranscript();
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->setStreamingState(false);
    m_jsBridge->clearInput();
#else
    m_inputWidget->setStreaming(false);
    m_transcript->setStreamingActive(false);
    m_inputWidget->clear();
#endif
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
    restoreSession({}, completeTurns, 0, {}, {}, {});
}

void ChatPanelWidget::restoreSession(const QString &sessionId,
                                     const QJsonArray &completeTurns,
                                     int mode,
                                     const QString &title,
                                     const QString &modelId,
                                     const QString &providerId)
{
    // Start fresh
    onNewSession();

    // Restore session metadata
    if (!sessionId.isEmpty())
        m_sessionModel->setId(sessionId);
    if (!title.isEmpty())
        m_sessionModel->setTitle(title);

    // Determine the active mode: use session-level mode, or infer from last turn
    int effectiveMode = mode;
    if (effectiveMode == 0 && !completeTurns.isEmpty()) {
        const auto lastTurn = completeTurns.last().toObject();
        const int turnMode = lastTurn.value(QLatin1String("mode")).toInt(0);
        if (turnMode > 0)
            effectiveMode = turnMode;
    }
    if (effectiveMode >= 0 && effectiveMode <= 2) {
        m_currentMode = effectiveMode;
        m_sessionModel->setMode(effectiveMode);
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->setMode(effectiveMode);
#else
        m_inputWidget->setCurrentMode(effectiveMode);
#endif
    }
    if (!modelId.isEmpty())
        m_sessionModel->setSelectedModel(modelId);
    if (!providerId.isEmpty())
        m_sessionModel->setProviderId(providerId);

    for (const auto &v : completeTurns) {
        const ChatTurnModel turn = ChatTurnModel::fromJson(v.toObject());

        m_sessionModel->beginTurn(
            turn.userMessage, turn.attachmentNames, turn.mode,
            turn.modelId, turn.providerId, turn.slashCommand);

        // Add content parts to model and transcript
        int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->addTurn(idx, m_sessionModel->turn(idx).toJson());
#endif

        for (const auto &part : turn.parts) {
            m_sessionModel->appendPart(part);
#ifdef EXORCIST_HAS_ULTRALIGHT
            m_jsBridge->addContentPart(idx, part.toJson());
#else
            m_transcript->addContentPart(idx, part);
#endif
        }

        // Complete the turn in the model (emits turnCompleted → finishTurn)
        m_sessionModel->completeTurn();

        // Restore original metadata AFTER completeTurn (which forces State::Complete)
        auto &currentTurn = m_sessionModel->currentTurn();
        currentTurn.state = turn.state;
        currentTurn.feedback = turn.feedback;
        currentTurn.id = turn.id;
        currentTurn.timestamp = turn.timestamp;

        // Apply the real restored state to the widget
#ifdef EXORCIST_HAS_ULTRALIGHT
        m_jsBridge->finishTurn(idx, static_cast<int>(turn.state));
#else
        if (auto *w = m_transcript->turnWidget(idx))
            w->finishTurn(turn.state);
#endif

        // Rebuild conversation history with tool calls for full context
        m_conversationHistory.append({AgentMessage::Role::User, turn.userMessage});

        // Include tool call context so the model remembers what tools were used
        for (const auto &part : turn.parts) {
            if (part.type == ChatContentPart::Type::ToolInvocation
                && !part.toolName.isEmpty()) {
                // Assistant message requesting the tool call
                AgentMessage toolReq;
                toolReq.role = AgentMessage::Role::Assistant;
                ToolCall tc;
                tc.id   = part.toolCallId;
                tc.name = part.toolName;
                // toolInput is a formatted string; store as-is for context
                tc.arguments = part.toolInput;
                toolReq.toolCalls.append(tc);
                m_conversationHistory.append(toolReq);

                // Tool result message
                AgentMessage toolRes;
                toolRes.role       = AgentMessage::Role::Tool;
                toolRes.toolCallId = part.toolCallId;
                toolRes.content    = part.toolOutput;
                m_conversationHistory.append(toolRes);
            }
        }

        m_conversationHistory.append({AgentMessage::Role::Assistant, turn.fullMarkdownText()});
    }

    // Pre-seed the AgentController session so agent mode has full context
    if (m_agentController && !m_conversationHistory.isEmpty()) {
        if (!m_agentSessionActive) {
            m_agentController->newSession(true);
            m_agentSessionActive = true;
        }
        if (auto *session = m_agentController->session())
            session->setMessages(m_conversationHistory);
    }

    // Switch UI from welcome to transcript
    showWelcomeOrTranscript();
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

            // Prefer rich turn data if available
            if (!session.completeTurns.isEmpty()) {
                restoreSession(session.sessionId, session.completeTurns,
                               session.mode, session.title,
                               session.modelId, session.providerId);
            } else {
                onNewSession();
                // Fall back to simple message pairs
                for (const auto &msg : session.messages) {
                    if (msg.first == QLatin1String("user")) {
                        m_sessionModel->beginTurn(msg.second);
#ifdef EXORCIST_HAS_ULTRALIGHT
                        {
                            int idx = m_sessionModel->turnCount() - 1;
                            m_jsBridge->addTurn(idx, m_sessionModel->turn(idx).toJson());
                        }
#endif
                        m_conversationHistory.append({AgentMessage::Role::User, msg.second});
                    } else if (msg.first == QLatin1String("assistant")) {
                        if (m_sessionModel->isEmpty())
                            continue;
                        m_sessionModel->appendMarkdownDelta(msg.second);
                        m_sessionModel->completeTurn();
                        int idx = m_sessionModel->turnCount() - 1;
#ifdef EXORCIST_HAS_ULTRALIGHT
                        m_jsBridge->appendMarkdownDelta(idx, msg.second);
                        m_jsBridge->finishTurn(idx, static_cast<int>(ChatTurnModel::State::Complete));
#else
                        m_transcript->appendMarkdownDelta(idx, msg.second);
                        if (auto *w = m_transcript->turnWidget(idx))
                            w->finishTurn(ChatTurnModel::State::Complete);
#endif
                        m_conversationHistory.append({AgentMessage::Role::Assistant, msg.second});
                    } else if (msg.first == QLatin1String("tool")) {
                        // Tool call context — add to history so model has context
                        m_conversationHistory.append({AgentMessage::Role::Assistant, msg.second});
                    }
                }

                // Pre-seed AgentController with restored context
                if (m_agentController && !m_conversationHistory.isEmpty()) {
                    if (!m_agentSessionActive) {
                        m_agentController->newSession(true);
                        m_agentSessionActive = true;
                    }
                    if (auto *session = m_agentController->session())
                        session->setMessages(m_conversationHistory);
                }
            }
#ifdef EXORCIST_HAS_ULTRALIGHT
            m_jsBridge->showTranscript();
#else
            m_stack->setCurrentWidget(m_transcript);
#endif
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

    // Position above the bottom of the panel
    const QPoint pos = mapToGlobal(QPoint(0, height() - 400));
    m_historyPopup->showAt(pos);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Changes bar
// ═══════════════════════════════════════════════════════════════════════════════

void ChatPanelWidget::showChangesBar(int editCount)
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->showChangesBar(editCount);
#else
    m_changesLabel->setText(
        tr("%1 file%2 changed")
            .arg(editCount)
            .arg(editCount == 1 ? "" : "s"));
    m_changesBar->show();
#endif
}

void ChatPanelWidget::hideChangesBar()
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_jsBridge->hideChangesBar();
#else
    m_changesBar->hide();
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Slash commands
// ═══════════════════════════════════════════════════════════════════════════════

QString ChatPanelWidget::resolveSlashCommand(const QString &text,
                                             AgentIntent &intent) const
{
    // Longer prefixes first to avoid ambiguous partial matches.
    if (text.startsWith(QLatin1String("/explain"))) {
        intent = AgentIntent::ExplainCode;
        return text.mid(8).trimmed();
    }
    if (text.startsWith(QLatin1String("/refactor"))) {
        intent = AgentIntent::RefactorSelection;
        return text.mid(9).trimmed();
    }
    if (text.startsWith(QLatin1String("/review"))) {
        intent = AgentIntent::CodeReview;
        return text.mid(7).trimmed();
    }
    if (text.startsWith(QLatin1String("/generate"))) {
        intent = AgentIntent::GenerateCode;
        return text.mid(9).trimmed();
    }
    if (text.startsWith(QLatin1String("/search"))) {
        intent = AgentIntent::Chat; // no dedicated search intent yet
        return text.mid(7).trimmed();
    }
    if (text.startsWith(QLatin1String("/tests"))) {
        intent = AgentIntent::GenerateTests;
        return text.mid(6).trimmed();
    }
    if (text.startsWith(QLatin1String("/edit"))) {
        intent = AgentIntent::CreateFile;
        return text.mid(5).trimmed();
    }
    if (text.startsWith(QLatin1String("/fix"))) {
        intent = AgentIntent::FixDiagnostic;
        return text.mid(4).trimmed();
    }
    if (text.startsWith(QLatin1String("/doc"))) {
        intent = AgentIntent::Chat; // no dedicated docs intent yet
        return text.mid(4).trimmed();
    }
    if (text.startsWith(QLatin1String("/new"))) {
        // /new is handled upstream (triggers new session)
        return text;
    }
    if (text.startsWith(QLatin1String("/compact"))) {
        // /compact is agent-mode history compression; no text rewriting needed
        intent = AgentIntent::Chat;
        return text.mid(8).trimmed();
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
