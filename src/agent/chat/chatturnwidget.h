#pragma once

#include <QClipboard>
#include <QDateTime>
#include <QEnterEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatfollowupswidget.h"
#include "chatmarkdownwidget.h"
#include "chatthemetokens.h"
#include "chatthinkingwidget.h"
#include "chattoolinvocationwidget.h"
#include "chatturnmodel.h"
#include "chatworkspaceeditwidget.h"
#include "aiinterface.h"

// ── ChatTurnWidget ────────────────────────────────────────────────────────────
//
// Renders one request/response pair in the chat transcript.
// Layout:
//   [Avatar]  [User Name]  [Timestamp]
//   User message text
//   ──────────────────────────────────
//   [Avatar]  [Assistant Name]
//   [Content parts laid out vertically...]
//   [Feedback row (thumbs up / thumbs down)]

class ChatTurnWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatTurnWidget(const ChatTurnModel &turn, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_turnId(turn.id)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(12, 8, 12, 8);
        root->setSpacing(0);

        // ── User Request ──────────────────────────────────────────────
        auto *userSection = new QWidget(this);
        auto *userLayout = new QVBoxLayout(userSection);
        userLayout->setContentsMargins(0, 0, 0, 4);
        userLayout->setSpacing(2);

        // Header: avatar + name + time
        auto *userHeader = new QHBoxLayout;
        userHeader->setContentsMargins(0, 0, 0, 0);
        userHeader->setSpacing(6);

        auto *userAvatar = new QLabel(QStringLiteral("U"), this);
        userAvatar->setFixedSize(ChatTheme::AvatarSize, ChatTheme::AvatarSize);
        userAvatar->setAlignment(Qt::AlignCenter);
        userAvatar->setStyleSheet(
            QStringLiteral("font-size:10px; font-weight:700; background:%1;"
                          " border-radius:%2px; color:white;")
                .arg(ChatTheme::AvatarUserBg)
                .arg(ChatTheme::AvatarRadius));

        auto *userName = new QLabel(tr("You"), this);
        userName->setStyleSheet(
            QStringLiteral("color:%1; font-weight:600; font-size:12px;")
                .arg(ChatTheme::FgPrimary));

        auto *userTime = new QLabel(turn.timestamp.toString(QStringLiteral("hh:mm")), this);
        userTime->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px;").arg(ChatTheme::FgDimmed));

        userHeader->addWidget(userAvatar);
        userHeader->addWidget(userName);
        userHeader->addWidget(userTime);
        userHeader->addStretch();
        userLayout->addLayout(userHeader);

        // User message body (with optional slash command pill)
        if (!turn.slashCommand.isEmpty()) {
            auto *msgRow = new QHBoxLayout;
            msgRow->setContentsMargins(22, 0, 0, 0);
            msgRow->setSpacing(6);

            auto *pill = new QLabel(this);
            pill->setText(turn.slashCommand);
            pill->setStyleSheet(
                QStringLiteral("background:%1; color:%2; font-size:12px;"
                              " padding:1px 6px; border-radius:3px;"
                              " font-weight:500;")
                    .arg(ChatTheme::SlashPillBg, ChatTheme::SlashPillFg));
            msgRow->addWidget(pill);

            auto *userMsg = new QLabel(this);
            userMsg->setWordWrap(true);
            userMsg->setTextFormat(Qt::PlainText);
            userMsg->setText(turn.userMessage);
            userMsg->setStyleSheet(
                QStringLiteral("color:%1; font-size:13px;")
                    .arg(ChatTheme::FgPrimary));
            msgRow->addWidget(userMsg, 1);
            userLayout->addLayout(msgRow);
        } else {
            auto *userMsg = new QLabel(this);
            userMsg->setWordWrap(true);
            userMsg->setTextFormat(Qt::PlainText);
            userMsg->setText(turn.userMessage);
            userMsg->setStyleSheet(
                QStringLiteral("color:%1; font-size:13px; padding-left:22px;")
                    .arg(ChatTheme::FgPrimary));
            userLayout->addWidget(userMsg);
        }

        // Attachments
        if (!turn.attachmentNames.isEmpty()) {
            auto *attRow = new QHBoxLayout;
            attRow->setContentsMargins(22, 2, 0, 0);
            attRow->setSpacing(4);
            for (const auto &name : turn.attachmentNames) {
                auto *chip = new QLabel(this);
                chip->setText(QStringLiteral("\U0001F4CE ") + name);
                chip->setStyleSheet(
                    QStringLiteral("color:%1; font-size:11px; background:%2;"
                                  " padding:1px 6px; border-radius:8px;")
                        .arg(ChatTheme::FgDimmed, ChatTheme::InputBg));
                attRow->addWidget(chip);
            }
            attRow->addStretch();
            userLayout->addLayout(attRow);
        }

        // Context references (badges)
        if (!turn.references.isEmpty()) {
            auto *refRow = new QHBoxLayout;
            refRow->setContentsMargins(22, 2, 0, 0);
            refRow->setSpacing(4);
            for (const auto &ref : turn.references) {
                auto *badge = new QLabel(this);
                badge->setText(ref.label);
                badge->setToolTip(ref.tooltip);
                badge->setCursor(ref.filePath.isEmpty()
                                     ? Qt::ArrowCursor
                                     : Qt::PointingHandCursor);
                badge->setStyleSheet(
                    QStringLiteral("color:%1; font-size:10px; background:%2;"
                                  " padding:2px 8px; border-radius:10px;"
                                  " border:1px solid %3;")
                        .arg(ChatTheme::AccentFg, ChatTheme::InputBg,
                             ChatTheme::Border));
                if (!ref.filePath.isEmpty()) {
                    const QString path = ref.filePath;
                    connect(badge, &QLabel::linkActivated, this,
                            [this, path](const QString &) {
                                emit fileClicked(path);
                            });
                    // QLabel linkActivated won't fire without a link,
                    // so use mouse press event via event filter
                    badge->installEventFilter(this);
                    badge->setProperty("_refPath", path);
                }
                refRow->addWidget(badge);
            }
            refRow->addStretch();
            userLayout->addLayout(refRow);
        }

        root->addWidget(userSection);
        root->addSpacing(10);

        // ── Assistant Response ────────────────────────────────────────
        m_responseSection = new QWidget(this);
        auto *respLayout = new QVBoxLayout(m_responseSection);
        respLayout->setContentsMargins(0, 4, 0, 0);
        respLayout->setSpacing(0);

        // Header: avatar + name + model info
        auto *assistHeader = new QHBoxLayout;
        assistHeader->setContentsMargins(0, 0, 0, 2);
        assistHeader->setSpacing(6);

        auto *assistAvatar = new QLabel(QStringLiteral("\u2726"), this);
        assistAvatar->setFixedSize(ChatTheme::AvatarSize, ChatTheme::AvatarSize);
        assistAvatar->setAlignment(Qt::AlignCenter);
        assistAvatar->setStyleSheet(
            QStringLiteral("font-size:10px; font-weight:700; background:%1;"
                          " border-radius:%2px; color:white;")
                .arg(ChatTheme::AvatarAiBg)
                .arg(ChatTheme::AvatarRadius));

        auto *assistName = new QLabel(tr("Copilot"), this);
        assistName->setStyleSheet(
            QStringLiteral("color:%1; font-weight:600; font-size:12px;")
                .arg(ChatTheme::FgPrimary));

        m_modelLabel = new QLabel(this);
        if (!turn.modelId.isEmpty())
            m_modelLabel->setText(turn.modelId);
        m_modelLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px;").arg(ChatTheme::FgDimmed));

        assistHeader->addWidget(assistAvatar);
        assistHeader->addWidget(assistName);
        assistHeader->addWidget(m_modelLabel);
        assistHeader->addStretch();
        respLayout->addLayout(assistHeader);

        // Content parts container
        m_partsContainer = new QWidget(this);
        m_partsLayout = new QVBoxLayout(m_partsContainer);
        m_partsLayout->setContentsMargins(22, 0, 0, 0);
        m_partsLayout->setSpacing(8);
        respLayout->addWidget(m_partsContainer);

        // Build initial content parts
        for (const auto &part : turn.parts)
            addPartWidget(part);

        // Feedback row (only visible when turn is complete)
        m_feedbackRow = new QWidget(this);
        m_feedbackRow->hide();
        auto *fbLayout = new QHBoxLayout(m_feedbackRow);
        fbLayout->setContentsMargins(22, 4, 0, 2);
        fbLayout->setSpacing(4);

        m_thumbUpBtn = new QToolButton(this);
        m_thumbUpBtn->setText(QStringLiteral("\u25B2"));
        m_thumbUpBtn->setToolTip(tr("Helpful"));
        m_thumbUpBtn->setAccessibleName(tr("Mark as helpful"));
        m_thumbUpBtn->setStyleSheet(feedbackBtnStyle());
        connect(m_thumbUpBtn, &QToolButton::clicked, this, [this]() {
            emit feedbackGiven(m_turnId, true);
        });

        m_thumbDownBtn = new QToolButton(this);
        m_thumbDownBtn->setText(QStringLiteral("\u25BC"));
        m_thumbDownBtn->setToolTip(tr("Not helpful"));
        m_thumbDownBtn->setAccessibleName(tr("Mark as not helpful"));
        m_thumbDownBtn->setStyleSheet(feedbackBtnStyle());
        connect(m_thumbDownBtn, &QToolButton::clicked, this, [this]() {
            emit feedbackGiven(m_turnId, false);
        });

        auto *copyResponseBtn = new QToolButton(this);
        copyResponseBtn->setText(QStringLiteral("\u2398"));
        copyResponseBtn->setToolTip(tr("Copy response"));
        copyResponseBtn->setStyleSheet(feedbackBtnStyle());
        connect(copyResponseBtn, &QToolButton::clicked, this, [this]() {
            // Collect raw markdown from all ChatMarkdownWidget children
            QString fullText;
            const auto mdWidgets = m_partsContainer->findChildren<ChatMarkdownWidget *>();
            for (const auto *md : mdWidgets) {
                if (!fullText.isEmpty())
                    fullText += QLatin1Char('\n');
                fullText += md->rawMarkdown();
            }
            if (!fullText.isEmpty())
                QGuiApplication::clipboard()->setText(fullText);
        });

        auto *retryBtn = new QToolButton(this);
        retryBtn->setText(QStringLiteral("\u21BB"));
        retryBtn->setToolTip(tr("Retry"));
        retryBtn->setStyleSheet(feedbackBtnStyle());
        connect(retryBtn, &QToolButton::clicked, this, [this]() {
            emit retryRequested(m_turnId);
        });

        fbLayout->addWidget(m_thumbUpBtn);
        fbLayout->addWidget(m_thumbDownBtn);
        fbLayout->addWidget(copyResponseBtn);
        fbLayout->addWidget(retryBtn);
        fbLayout->addStretch();

        m_usageLabel = new QLabel(this);
        m_usageLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:10px;").arg(ChatTheme::FgDimmed));
        m_usageLabel->hide();
        fbLayout->addWidget(m_usageLabel);

        respLayout->addWidget(m_feedbackRow);

        root->addWidget(m_responseSection);

        // Apply finished state if already complete
        if (turn.state == ChatTurnModel::State::Complete
            || turn.state == ChatTurnModel::State::Error)
            m_turnComplete = (turn.state == ChatTurnModel::State::Complete);
    }

    QString turnId() const { return m_turnId; }

    // ── Streaming updates ─────────────────────────────────────────────

    void appendMarkdownDelta(const QString &delta)
    {
        if (m_lastMarkdown) {
            m_lastMarkdown->appendDelta(delta);
        } else {
            auto *w = new ChatMarkdownWidget(this);
            w->setMarkdown({}, true);
            w->appendDelta(delta);
            connect(w, &ChatMarkdownWidget::anchorClicked,
                    this, &ChatTurnWidget::codeActionRequested);
            connect(w, &ChatMarkdownWidget::insertCodeRequested,
                    this, &ChatTurnWidget::insertCodeRequested);
            m_partsLayout->addWidget(w);
            m_lastMarkdown = w;
        }
        // Auto-collapse thinking when markdown starts streaming
        if (m_lastThinking) {
            m_lastThinking->finalize();
            m_lastThinking->setCollapsed(true);
            m_lastThinking = nullptr;
        }
    }

    void appendThinkingDelta(const QString &delta)
    {
        if (m_lastThinking) {
            m_lastThinking->appendDelta(delta);
        } else {
            auto *w = new ChatThinkingWidget(this);
            w->setThinkingText({}, true);
            w->appendDelta(delta);
            connect(w, &ChatThinkingWidget::toolConfirmed,
                    this, &ChatTurnWidget::toolConfirmed);
            m_partsLayout->addWidget(w);
            m_lastThinking = w;
            m_thinkingWidgets.append(w);
            m_lastMarkdown = nullptr;
        }
    }

    void addContentPart(const ChatContentPart &part)
    {
        addPartWidget(part);
    }

    void updateToolState(const QString &callId, const ChatContentPart &part)
    {
        // Check standalone tool widgets first
        auto it = m_toolWidgets.find(callId);
        if (it != m_toolWidgets.end()) {
            it.value()->updateState(part);
            return;
        }
        // Check tools embedded in thinking widgets
        for (auto *tw : m_thinkingWidgets) {
            if (tw->findToolItem(callId)) {
                tw->updateToolState(callId, part);
                return;
            }
        }
    }

    void finishTurn(ChatTurnModel::State state)
    {
        // Finalize any streaming markdown
        if (m_lastMarkdown)
            m_lastMarkdown->finalize();
        if (m_lastThinking) {
            m_lastThinking->finalize();
            m_lastThinking->setCollapsed(true);
        }

        // Mark turn as complete so hover can reveal feedback row
        m_turnComplete = (state == ChatTurnModel::State::Complete);
        // Don't show feedback row immediately — it's revealed on hover
        m_feedbackRow->hide();
    }

    void setTokenUsage(int promptTokens, int completionTokens, int totalTokens)
    {
        if (totalTokens <= 0) {
            m_usageLabel->hide();
            return;
        }
        m_usageLabel->setText(
            tr("%1 tokens (%2 in / %3 out)")
                .arg(totalTokens).arg(promptTokens).arg(completionTokens));
        m_usageLabel->show();
    }

    void showError(const QString &errorText,
                   AgentError::Code code = AgentError::Code::Unknown)
    {
        auto *row = new QWidget(this);
        auto *rowLay = new QVBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);
        rowLay->setSpacing(6);

        auto *err = new QLabel(row);
        err->setWordWrap(true);
        err->setTextFormat(Qt::PlainText);
        err->setText(errorText);
        err->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px; padding:4px 8px;"
                          " background:%2; border-radius:4px;")
                .arg(ChatTheme::ErrorFg, ChatTheme::ErrorBg));
        rowLay->addWidget(err);

        // Action button based on error type
        if (code == AgentError::Code::NetworkError
            || code == AgentError::Code::RateLimited) {
            auto *retryBtn = new QToolButton(row);
            retryBtn->setText(tr("Retry"));
            retryBtn->setCursor(Qt::PointingHandCursor);
            retryBtn->setStyleSheet(
                QStringLiteral("QToolButton { color:%1; background:%2;"
                              " border:1px solid %3; border-radius:4px;"
                              " padding:4px 12px; font-size:12px; }"
                              "QToolButton:hover { background:%4; }")
                    .arg(ChatTheme::FgPrimary, ChatTheme::InputBg,
                         ChatTheme::Border, ChatTheme::HoverBg));
            connect(retryBtn, &QToolButton::clicked, this, [this] {
                emit retryRequested(m_turnId);
            });
            rowLay->addWidget(retryBtn, 0, Qt::AlignLeft);
        } else if (code == AgentError::Code::AuthError) {
            auto *signInBtn = new QToolButton(row);
            signInBtn->setText(tr("Sign In"));
            signInBtn->setCursor(Qt::PointingHandCursor);
            signInBtn->setStyleSheet(
                QStringLiteral("QToolButton { color:%1; background:%2;"
                              " border:none; border-radius:4px;"
                              " padding:4px 12px; font-size:12px; }"
                              "QToolButton:hover { background:%3; }")
                    .arg(ChatTheme::ButtonFg, ChatTheme::ButtonBg,
                         ChatTheme::ButtonHover));
            connect(signInBtn, &QToolButton::clicked, this, [this] {
                emit signInRequested();
            });
            rowLay->addWidget(signInBtn, 0, Qt::AlignLeft);
        }

        m_partsLayout->addWidget(row);
    }

signals:
    void feedbackGiven(const QString &turnId, bool helpful);
    void followupClicked(const QString &message);
    void toolConfirmed(const QString &callId, int approval);
    void fileClicked(const QString &path);
    void codeActionRequested(const QUrl &url);
    void insertCodeRequested(const QString &code);
    void retryRequested(const QString &turnId);
    void signInRequested();
    void keepFileRequested(int fileIndex, const QString &path);
    void undoFileRequested(int fileIndex, const QString &path);
    void keepAllRequested();
    void undoAllRequested();

private:
    void addPartWidget(const ChatContentPart &part)
    {
        switch (part.type) {
        case ChatContentPart::Type::Markdown: {
            auto *w = new ChatMarkdownWidget(this);
            w->setMarkdown(part.markdownText);
            connect(w, &ChatMarkdownWidget::anchorClicked,
                    this, &ChatTurnWidget::codeActionRequested);
            connect(w, &ChatMarkdownWidget::insertCodeRequested,
                    this, &ChatTurnWidget::insertCodeRequested);
            m_partsLayout->addWidget(w);
            m_lastMarkdown = w;
            m_lastThinking = nullptr;
            break;
        }
        case ChatContentPart::Type::Thinking: {
            auto *w = new ChatThinkingWidget(this);
            w->setThinkingText(part.thinkingText);
            if (part.thinkingCollapsed)
                w->setCollapsed(true);
            connect(w, &ChatThinkingWidget::toolConfirmed,
                    this, &ChatTurnWidget::toolConfirmed);
            m_partsLayout->addWidget(w);
            m_lastThinking = w;
            m_thinkingWidgets.append(w);
            m_lastMarkdown = nullptr;
            break;
        }
        case ChatContentPart::Type::ToolInvocation: {
            // Embed inside the current thinking widget if one exists
            if (m_lastThinking) {
                m_lastThinking->addToolItem(part);
            } else {
                // Create a thinking widget to host the tool
                auto *tw = new ChatThinkingWidget(this);
                connect(tw, &ChatThinkingWidget::toolConfirmed,
                        this, &ChatTurnWidget::toolConfirmed);
                m_partsLayout->addWidget(tw);
                m_lastThinking = tw;
                m_thinkingWidgets.append(tw);
                tw->addToolItem(part);
            }
            m_lastMarkdown = nullptr;
            break;
        }
        case ChatContentPart::Type::WorkspaceEdit: {
            auto *w = new ChatWorkspaceEditWidget(this);
            w->setEditedFiles(part.editedFiles);
            connect(w, &ChatWorkspaceEditWidget::fileClicked,
                    this, [this](int, const QString &path) {
                emit fileClicked(path);
            });
            connect(w, &ChatWorkspaceEditWidget::keepFile,
                    this, &ChatTurnWidget::keepFileRequested);
            connect(w, &ChatWorkspaceEditWidget::undoFile,
                    this, &ChatTurnWidget::undoFileRequested);
            connect(w, &ChatWorkspaceEditWidget::keepAll,
                    this, &ChatTurnWidget::keepAllRequested);
            connect(w, &ChatWorkspaceEditWidget::undoAll,
                    this, &ChatTurnWidget::undoAllRequested);
            m_partsLayout->addWidget(w);
            m_lastMarkdown = nullptr;
            m_lastThinking = nullptr;
            break;
        }
        case ChatContentPart::Type::Followup: {
            auto *w = new ChatFollowupsWidget(this);
            w->setFollowups(part.followups);
            connect(w, &ChatFollowupsWidget::followupClicked,
                    this, &ChatTurnWidget::followupClicked);
            m_partsLayout->addWidget(w);
            m_lastMarkdown = nullptr;
            m_lastThinking = nullptr;
            break;
        }
        case ChatContentPart::Type::Warning: {
            auto *lbl = new QLabel(this);
            lbl->setWordWrap(true);
            lbl->setText(QStringLiteral("\u26A0 ") + part.warningText);
            lbl->setStyleSheet(
                QStringLiteral("color:%1; font-size:12px; padding:4px 8px;"
                              " background:%2; border-radius:4px;")
                    .arg(ChatTheme::WarningFg, ChatTheme::WarningBg));
            m_partsLayout->addWidget(lbl);
            break;
        }
        case ChatContentPart::Type::Error: {
            auto *lbl = new QLabel(this);
            lbl->setWordWrap(true);
            lbl->setText(QStringLiteral("\u2717 ") + part.errorText);
            lbl->setStyleSheet(
                QStringLiteral("color:%1; font-size:12px; padding:4px 8px;"
                              " background:%2; border-radius:4px;")
                    .arg(ChatTheme::ErrorFg, ChatTheme::ErrorBg));
            m_partsLayout->addWidget(lbl);
            break;
        }
        case ChatContentPart::Type::Progress: {
            auto *lbl = new QLabel(this);
            lbl->setText(QStringLiteral("\u25CF ") + part.progressLabel);
            lbl->setStyleSheet(
                QStringLiteral("color:%1; font-size:12px;")
                    .arg(ChatTheme::FgDimmed));
            m_partsLayout->addWidget(lbl);
            break;
        }
        case ChatContentPart::Type::Confirmation: {
            auto *w = new QWidget(this);
            auto *cl = new QVBoxLayout(w);
            cl->setContentsMargins(0, 2, 0, 2);
            auto *title = new QLabel(part.confirmationTitle, this);
            title->setStyleSheet(
                QStringLiteral("color:%1; font-weight:600; font-size:12px;")
                    .arg(ChatTheme::FgPrimary));
            auto *msg = new QLabel(part.confirmationMessage, this);
            msg->setWordWrap(true);
            msg->setStyleSheet(
                QStringLiteral("color:%1; font-size:12px;")
                    .arg(ChatTheme::FgSecondary));
            cl->addWidget(title);
            cl->addWidget(msg);
            m_partsLayout->addWidget(w);
            break;
        }
        case ChatContentPart::Type::FileTree: {
            auto *lbl = new QLabel(this);
            lbl->setWordWrap(true);
            lbl->setText(part.fileTreePaths.join(QLatin1Char('\n')));
            lbl->setStyleSheet(
                QStringLiteral("color:%1; font-size:11px; font-family:%2;"
                              " background:%3; padding:4px 8px; border-radius:4px;")
                    .arg(ChatTheme::FgSecondary, ChatTheme::MonoFamily, ChatTheme::CodeBg));
            m_partsLayout->addWidget(lbl);
            break;
        }
        case ChatContentPart::Type::CommandButton: {
            auto *btn = new QToolButton(this);
            btn->setText(part.buttonLabel);
            btn->setStyleSheet(
                QStringLiteral("QToolButton { background:%1; color:%2; padding:4px 12px;"
                              " border-radius:4px; font-size:12px; border:none; }"
                              "QToolButton:hover { background:%3; }"
                              "QToolButton:pressed { background:%4; }"
                              "QToolButton:focus { outline:1px solid %5; }")
                    .arg(ChatTheme::ButtonBg, ChatTheme::ButtonFg, ChatTheme::ButtonHover,
                         ChatTheme::ButtonPressed, ChatTheme::FocusOutline));
            // Command execution is handled by the parent panel
            m_partsLayout->addWidget(btn);
            break;
        }
        }
    }

    static QString feedbackBtnStyle()
    {
        return QStringLiteral(
            "QToolButton { background:transparent; border:none;"
            " font-size:12px; padding:2px 6px; color:%4; }")
            .arg(ChatTheme::FgDimmed)
          + QStringLiteral(
            "QToolButton:hover { background:%1; border-radius:4px; color:%4; }"
            "QToolButton:pressed { background:%3; border-radius:4px; }"
            "QToolButton:focus { outline:1px solid %2; border-radius:4px; }")
            .arg(ChatTheme::InputBg, ChatTheme::FocusOutline,
                 ChatTheme::SecondaryBtnHover, ChatTheme::FgPrimary);
    }

    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            const QString path = obj->property("_refPath").toString();
            if (!path.isEmpty()) {
                emit fileClicked(path);
                return true;
            }
        }
        return QWidget::eventFilter(obj, event);
    }

    void enterEvent(QEnterEvent *event) override
    {
        QWidget::enterEvent(event);
        if (m_turnComplete)
            m_feedbackRow->show();
    }

    void leaveEvent(QEvent *event) override
    {
        QWidget::leaveEvent(event);
        m_feedbackRow->hide();
    }

    QString m_turnId;
    bool    m_turnComplete = false;

    QWidget     *m_responseSection = nullptr;
    QWidget     *m_partsContainer  = nullptr;
    QVBoxLayout *m_partsLayout     = nullptr;
    QLabel      *m_modelLabel      = nullptr;
    QWidget     *m_feedbackRow     = nullptr;
    QToolButton *m_thumbUpBtn      = nullptr;
    QToolButton *m_thumbDownBtn    = nullptr;
    QLabel      *m_usageLabel      = nullptr;

    ChatMarkdownWidget  *m_lastMarkdown = nullptr;
    ChatThinkingWidget  *m_lastThinking = nullptr;
    QList<ChatThinkingWidget *> m_thinkingWidgets;
    QMap<QString, ChatToolInvocationWidget *> m_toolWidgets;
};
