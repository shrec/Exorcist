#pragma once

#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include "chatcontentpart.h"
#include "chatthemetokens.h"

// ── ChainLineWidget ──────────────────────────────────────────────────────────
//
// Paints a vertical chain-of-thought line with a gap around an icon dot,
// matching VS Code's thinking section.

class ChainLineWidget : public QWidget
{
public:
    explicit ChainLineWidget(const QString &icon, QWidget *parent = nullptr)
        : QWidget(parent), m_icon(icon)
    {
        setFixedWidth(22);
    }

    void setShowLine(bool top, bool bottom)
    {
        m_topLine    = top;
        m_bottomLine = bottom;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int cx   = 10;
        const int cy   = 12;
        const int dotR = 5;
        const QColor lineColor(ChatTheme::pick(ChatTheme::Border, ChatTheme::L_Border));
        const QColor iconColor(ChatTheme::pick(ChatTheme::FgSecondary, ChatTheme::L_FgSecondary));

        p.setPen(Qt::NoPen);
        p.setBrush(lineColor);
        if (m_topLine)
            p.drawRect(cx, 0, 1, cy - dotR - 2);
        if (m_bottomLine)
            p.drawRect(cx, cy + dotR + 2, 1, height() - cy - dotR - 2);

        QFont f = font();
        f.setPixelSize(11);
        p.setFont(f);
        p.setPen(iconColor);
        p.drawText(QRect(cx - dotR, cy - dotR, dotR * 2, dotR * 2),
                   Qt::AlignCenter, m_icon);
    }

private:
    QString m_icon;
    bool    m_topLine    = false;
    bool    m_bottomLine = false;
};

// ── ChatThinkingWidget ────────────────────────────────────────────────────────
//
// VS Code-style "Thinking" collapsible box.
//
// Architecture (matches VS Code chatThinkingContentPart):
//   ┌─────────────────────────────────────────────┐
//   │  ▼ Thinking…  (shimmer when streaming)      │  header
//   ├─────────────────────────────────────────────┤
//   │  ● reasoning text paragraph…                │  thinking text
//   │  ●─ Searched for "foo"              ✓       │  tool item
//   │  ●─ Read file bar.cpp               ✓       │  tool item
//   │  ◎ Working…                                 │  spinner
//   └─────────────────────────────────────────────┘
//
// When streaming ends the box auto-collapses.
// Tool invocations are embedded inside (not separate cards).

class ChatThinkingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChatThinkingWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(0, 4, 0, 4);
        root->setSpacing(0);

        // Outer container with border
        m_box = new QWidget(this);
        m_box->setObjectName(QStringLiteral("thinkingBox"));
        updateBoxStyle(false);

        auto *boxLayout = new QVBoxLayout(m_box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(0);

        // Header bar (toggle button)
        m_headerBtn = new QToolButton(m_box);
        m_headerBtn->setCheckable(true);
        m_headerBtn->setChecked(true);
        m_headerBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_headerBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_headerBtn->setCursor(Qt::PointingHandCursor);
        updateHeaderStyle(true);
        updateHeaderText(true, false);

        connect(m_headerBtn, &QToolButton::toggled, this, [this](bool expanded) {
            m_contentArea->setVisible(expanded);
            updateHeaderStyle(expanded);
            updateGeometry();
        });
        boxLayout->addWidget(m_headerBtn);

        // Collapsible content area
        m_contentArea = new QWidget(m_box);
        auto *contentLayout = new QVBoxLayout(m_contentArea);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(0);

        // Thinking text with chain line
        m_thinkingTextContainer = new QWidget(m_contentArea);
        auto *thinkingRow = new QHBoxLayout(m_thinkingTextContainer);
        thinkingRow->setContentsMargins(0, 0, 0, 0);
        thinkingRow->setSpacing(0);

        m_thinkingChainLine = new ChainLineWidget(
            QStringLiteral("\U0001F4A1"), m_thinkingTextContainer);
        m_thinkingChainLine->setShowLine(false, true);

        auto *textScroll = new QScrollArea(m_thinkingTextContainer);
        textScroll->setWidgetResizable(true);
        textScroll->setFrameShape(QFrame::NoFrame);
        textScroll->setMaximumHeight(180);
        textScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        textScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        textScroll->setStyleSheet(
            QStringLiteral("QScrollArea { background:transparent; border:none; }"));

        m_contentLabel = new QLabel;
        m_contentLabel->setWordWrap(true);
        m_contentLabel->setTextFormat(Qt::RichText);
        m_contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        m_contentLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px; padding:6px 8px 6px 2px;")
                .arg(ChatTheme::pick(ChatTheme::ThinkingFg, ChatTheme::L_ThinkingFg)));
        textScroll->setWidget(m_contentLabel);

        thinkingRow->addWidget(m_thinkingChainLine);
        thinkingRow->addWidget(textScroll, 1);
        contentLayout->addWidget(m_thinkingTextContainer);

        // Tool items container
        m_toolsContainer = new QWidget(m_contentArea);
        m_toolsLayout = new QVBoxLayout(m_toolsContainer);
        m_toolsLayout->setContentsMargins(0, 0, 0, 0);
        m_toolsLayout->setSpacing(0);
        contentLayout->addWidget(m_toolsContainer);

        // Spinner row
        m_spinnerRow = new QWidget(m_contentArea);
        auto *spinnerRowLayout = new QHBoxLayout(m_spinnerRow);
        spinnerRowLayout->setContentsMargins(0, 0, 0, 4);
        spinnerRowLayout->setSpacing(0);

        m_spinnerChainLine = new ChainLineWidget(
            QStringLiteral("\u25CB"), m_spinnerRow);
        m_spinnerChainLine->setShowLine(true, false);

        m_spinnerLabel = new QLabel(m_spinnerRow);
        m_spinnerLabel->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px; padding:4px 0;")
                .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed)));
        m_spinnerLabel->setText(tr("Working\u2026"));

        spinnerRowLayout->addWidget(m_spinnerChainLine);
        spinnerRowLayout->addWidget(m_spinnerLabel, 1);
        contentLayout->addWidget(m_spinnerRow);
        m_spinnerRow->hide();

        boxLayout->addWidget(m_contentArea);
        root->addWidget(m_box);
    }

    // ── Thinking text streaming ───────────────────────────────────────

    void setThinkingText(const QString &text, bool streaming = false)
    {
        m_rawText = text;
        m_streaming = streaming;

        if (text.isEmpty()) {
            m_thinkingTextContainer->hide();
        } else {
            m_thinkingTextContainer->show();
            const QString safe = text.toHtmlEscaped()
                .replace(QStringLiteral("\n"), QStringLiteral("<br>"));
            m_contentLabel->setText(safe);
        }

        updateHeaderText(streaming, false);
        m_spinnerRow->setVisible(streaming);
        updateChainLines();
    }

    void appendDelta(const QString &delta)
    {
        m_rawText += delta;
        m_streaming = true;
        setThinkingText(m_rawText, true);
    }

    void finalize()
    {
        m_streaming = false;
        setThinkingText(m_rawText, false);
        m_spinnerRow->hide();
        updateHeaderText(false, m_toolCount > 0);
        updateChainLines();
    }

    void setCollapsed(bool collapsed)
    {
        m_headerBtn->setChecked(!collapsed);
    }

    bool isCollapsed() const { return !m_headerBtn->isChecked(); }
    QString rawText() const { return m_rawText; }
    bool isStreaming() const { return m_streaming; }

    // ── Embedded tool items ───────────────────────────────────────────

    struct ToolItem {
        QWidget         *row         = nullptr;
        ChainLineWidget *chain       = nullptr;
        QLabel          *title       = nullptr;
        QLabel          *state       = nullptr;
        QToolButton     *ioToggle    = nullptr;
        QWidget         *ioPanel     = nullptr;
        QLabel          *inputLabel  = nullptr;
        QLabel          *outputLabel = nullptr;
        QWidget         *confirmRow  = nullptr;
        QToolButton     *allowBtn    = nullptr;
        QToolButton     *denyBtn     = nullptr;
        QString          callId;
        QString          toolName;
    };

    ToolItem *addToolItem(const ChatContentPart &part)
    {
        auto *item = new ToolItem;
        item->callId = part.toolCallId;
        item->toolName = part.toolName;

        item->row = new QWidget(m_toolsContainer);
        auto *rowMain = new QVBoxLayout(item->row);
        rowMain->setContentsMargins(0, 0, 0, 0);
        rowMain->setSpacing(0);

        // Tool header with chain line
        auto *headerWidget = new QWidget(item->row);
        auto *headerLayout = new QHBoxLayout(headerWidget);
        headerLayout->setContentsMargins(0, 0, 8, 0);
        headerLayout->setSpacing(6);

        item->chain = new ChainLineWidget(toolIcon(part.toolName), headerWidget);
        item->chain->setShowLine(true, true);

        item->title = new QLabel(headerWidget);
        item->title->setText(part.toolFriendlyTitle.isEmpty()
            ? part.toolName : part.toolFriendlyTitle);
        item->title->setStyleSheet(
            QStringLiteral("color:%1; font-size:12px; padding:4px 0;")
                .arg(ChatTheme::pick(ChatTheme::FgPrimary, ChatTheme::L_FgPrimary)));

        item->state = new QLabel(headerWidget);
        item->state->setStyleSheet(
            QStringLiteral("color:%1; font-size:11px;")
                .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed)));

        item->ioToggle = new QToolButton(headerWidget);
        item->ioToggle->setText(QStringLiteral("\u25B6"));
        item->ioToggle->setStyleSheet(
            QStringLiteral("QToolButton { color:%1; background:transparent;"
                          " border:none; font-size:10px; padding:2px; }")
                .arg(ChatTheme::pick(ChatTheme::FgDimmed, ChatTheme::L_FgDimmed)));
        item->ioToggle->setCheckable(true);

        headerLayout->addWidget(item->chain);
        headerLayout->addWidget(item->title, 1);
        headerLayout->addWidget(item->state);
        headerLayout->addWidget(item->ioToggle);
        rowMain->addWidget(headerWidget);

        // IO panel (collapsible)
        item->ioPanel = new QWidget(item->row);
        item->ioPanel->hide();
        auto *ioLayout = new QVBoxLayout(item->ioPanel);
        ioLayout->setContentsMargins(22, 0, 8, 4);
        ioLayout->setSpacing(4);

        const QString monoStyle = QStringLiteral(
            "color:%1; font-size:11px; font-family:%2;"
            " background:%3; padding:6px 8px; border-radius:3px;")
                .arg(ChatTheme::pick(ChatTheme::FgSecondary, ChatTheme::L_FgSecondary),
                     ChatTheme::MonoFamily,
                     ChatTheme::pick(ChatTheme::CodeBg, ChatTheme::L_CodeBg));

        item->inputLabel = new QLabel(item->ioPanel);
        item->inputLabel->setWordWrap(true);
        item->inputLabel->setTextFormat(Qt::RichText);
        item->inputLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        item->inputLabel->setStyleSheet(monoStyle);
        item->inputLabel->hide();

        item->outputLabel = new QLabel(item->ioPanel);
        item->outputLabel->setWordWrap(true);
        item->outputLabel->setTextFormat(Qt::RichText);
        item->outputLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        item->outputLabel->setStyleSheet(monoStyle);
        item->outputLabel->hide();

        ioLayout->addWidget(item->inputLabel);
        ioLayout->addWidget(item->outputLabel);
        rowMain->addWidget(item->ioPanel);

        connect(item->ioToggle, &QToolButton::toggled, this, [item](bool on) {
            item->ioPanel->setVisible(on);
            item->ioToggle->setText(
                on ? QStringLiteral("\u25BC") : QStringLiteral("\u25B6"));
        });

        // Confirmation row
        item->confirmRow = new QWidget(item->row);
        item->confirmRow->hide();
        auto *confirmLayout = new QHBoxLayout(item->confirmRow);
        confirmLayout->setContentsMargins(22, 2, 8, 2);
        confirmLayout->setSpacing(6);

        const QString primaryBtnStyle = QStringLiteral(
            "QToolButton { background:%1; color:%2; padding:3px 10px;"
            " border-radius:3px; font-size:11px; border:none; }"
            "QToolButton:hover { background:%3; }")
                .arg(ChatTheme::ButtonBg, ChatTheme::ButtonFg, ChatTheme::ButtonHover);

        item->allowBtn = new QToolButton(item->confirmRow);
        item->allowBtn->setText(tr("Allow"));
        item->allowBtn->setStyleSheet(primaryBtnStyle);
        connect(item->allowBtn, &QToolButton::clicked, this, [this, item]() {
            item->allowBtn->setEnabled(false);
            item->denyBtn->setEnabled(false);
            emit toolConfirmed(item->callId, 1);
        });

        item->denyBtn = new QToolButton(item->confirmRow);
        item->denyBtn->setText(tr("Deny"));
        item->denyBtn->setStyleSheet(
            QStringLiteral("QToolButton { background:%1; color:%2; padding:3px 10px;"
                          " border-radius:3px; font-size:11px; border:none; }"
                          "QToolButton:hover { background:%3; }")
                .arg(ChatTheme::SecondaryBtnBg, ChatTheme::SecondaryBtnFg,
                     ChatTheme::SecondaryBtnHover));
        connect(item->denyBtn, &QToolButton::clicked, this, [this, item]() {
            item->allowBtn->setEnabled(false);
            item->denyBtn->setEnabled(false);
            emit toolConfirmed(item->callId, 0);
        });

        confirmLayout->addWidget(item->allowBtn);
        confirmLayout->addWidget(item->denyBtn);
        confirmLayout->addStretch();
        rowMain->addWidget(item->confirmRow);

        m_toolsLayout->addWidget(item->row);
        m_toolItems.append(item);
        m_toolCount++;
        updateChainLines();

        // Apply initial state (important for session restore — tools arrive
        // with their final state already set, not just Queued)
        if (part.toolState != ChatContentPart::ToolState::Queued)
            updateToolState(part.toolCallId, part);

        // Populate input if available (session restore)
        if (!part.toolInput.isEmpty()) {
            item->inputLabel->setText(
                QStringLiteral("<span style='color:%1;font-weight:600;'>"
                              "Input</span><br>%2")
                    .arg(ChatTheme::pick(ChatTheme::FgPrimary,
                                         ChatTheme::L_FgPrimary),
                         part.toolInput.toHtmlEscaped()
                             .replace(QStringLiteral("\n"),
                                      QStringLiteral("<br>"))
                             .left(1000)));
            item->inputLabel->show();
        }

        return item;
    }

    void updateToolState(const QString &callId, const ChatContentPart &part)
    {
        for (auto *item : m_toolItems) {
            if (item->callId != callId)
                continue;

            item->confirmRow->hide();

            QString stateText;
            switch (part.toolState) {
            case ChatContentPart::ToolState::Queued:
                stateText = QStringLiteral("\u25CB");
                break;
            case ChatContentPart::ToolState::Streaming:
                stateText = QStringLiteral("\u25CF \u2026");
                break;
            case ChatContentPart::ToolState::ConfirmationNeeded:
                stateText = QStringLiteral("\u26A0");
                item->confirmRow->show();
                item->allowBtn->setEnabled(true);
                item->denyBtn->setEnabled(true);
                break;
            case ChatContentPart::ToolState::CompleteSuccess:
                stateText = QStringLiteral("\u2713");
                item->state->setStyleSheet(
                    QStringLiteral("color:%1; font-size:11px;")
                        .arg(ChatTheme::ToolBorderOk));
                break;
            case ChatContentPart::ToolState::CompleteError:
                stateText = QStringLiteral("\u2717");
                item->state->setStyleSheet(
                    QStringLiteral("color:%1; font-size:11px;")
                        .arg(ChatTheme::ToolBorderFail));
                break;
            }
            item->state->setText(stateText);

            if (!part.toolPastTenseMsg.isEmpty()
                && (part.toolState == ChatContentPart::ToolState::CompleteSuccess
                    || part.toolState == ChatContentPart::ToolState::CompleteError)) {
                item->title->setText(part.toolPastTenseMsg);
            } else if (!part.toolFriendlyTitle.isEmpty()) {
                item->title->setText(part.toolFriendlyTitle);
            }

            if (!part.toolOutput.isEmpty()) {
                item->outputLabel->setText(
                    QStringLiteral("<span style='color:%1;font-weight:600;'>"
                                  "Output</span><br>%2")
                        .arg(ChatTheme::pick(ChatTheme::FgPrimary,
                                             ChatTheme::L_FgPrimary),
                             part.toolOutput.toHtmlEscaped()
                                 .replace(QStringLiteral("\n"),
                                          QStringLiteral("<br>"))
                                 .left(1000)));
                item->outputLabel->show();
            }
            break;
        }
    }

    ToolItem *findToolItem(const QString &callId) const
    {
        for (auto *item : m_toolItems)
            if (item->callId == callId)
                return item;
        return nullptr;
    }

    int toolCount() const { return m_toolCount; }

signals:
    void toolConfirmed(const QString &callId, int approval);

private:
    void updateBoxStyle(bool streaming)
    {
        const char *borderColor = streaming
            ? ChatTheme::AccentBlue
            : ChatTheme::pick(ChatTheme::Border, ChatTheme::L_Border);
        m_box->setStyleSheet(
            QStringLiteral("QWidget#thinkingBox {"
                          " border:1px solid %1; border-radius:6px;"
                          " background:transparent; }")
                .arg(QLatin1String(borderColor)));
    }

    void updateHeaderStyle(bool expanded)
    {
        Q_UNUSED(expanded)
        const char *fg = ChatTheme::pick(ChatTheme::FgSecondary,
                                         ChatTheme::L_FgSecondary);
        const char *hover = ChatTheme::pick(ChatTheme::HoverBg,
                                            ChatTheme::L_HoverBg);
        m_headerBtn->setStyleSheet(
            QStringLiteral("QToolButton { color:%1; background:transparent;"
                          " border:none; font-size:12px; padding:6px 10px;"
                          " text-align:left; }"
                          "QToolButton:hover { background:%2; }")
                .arg(QLatin1String(fg), QLatin1String(hover)));
    }

    void updateHeaderText(bool streaming, bool hasToolResults)
    {
        if (streaming)
            m_headerBtn->setText(QStringLiteral("\u25BC  Thinking\u2026"));
        else if (hasToolResults || !m_rawText.isEmpty())
            m_headerBtn->setText(QStringLiteral("\u2713  Thought"));
        else
            m_headerBtn->setText(QStringLiteral("\u25B6  Thinking"));
    }

    void updateChainLines()
    {
        const bool hasThinking = !m_rawText.isEmpty()
                               && m_thinkingTextContainer->isVisible();
        const bool hasTools   = m_toolCount > 0;
        const bool hasSpinner = m_spinnerRow->isVisible();

        if (hasThinking)
            m_thinkingChainLine->setShowLine(false, hasTools || hasSpinner);

        for (int i = 0; i < m_toolItems.size(); ++i) {
            bool top    = (i > 0) || hasThinking;
            bool bottom = (i < m_toolItems.size() - 1) || hasSpinner;
            m_toolItems[i]->chain->setShowLine(top, bottom);
        }

        if (hasSpinner)
            m_spinnerChainLine->setShowLine(hasThinking || hasTools, false);
    }

    static QString toolIcon(const QString &name)
    {
        if (name.contains(QLatin1String("search"))
            || name.contains(QLatin1String("grep")))
            return QStringLiteral("\U0001F50D");
        if (name.contains(QLatin1String("read_file"))
            || name.contains(QLatin1String("file")))
            return QStringLiteral("\U0001F4C4");
        if (name.contains(QLatin1String("terminal"))
            || name.contains(QLatin1String("run")))
            return QStringLiteral("\u25B6");
        if (name.contains(QLatin1String("edit"))
            || name.contains(QLatin1String("replace"))
            || name.contains(QLatin1String("create")))
            return QStringLiteral("\u270E");
        if (name.contains(QLatin1String("list_dir")))
            return QStringLiteral("\U0001F4C1");
        return QStringLiteral("\u2699");
    }

    QWidget         *m_box                   = nullptr;
    QToolButton     *m_headerBtn             = nullptr;
    QWidget         *m_contentArea           = nullptr;
    QWidget         *m_thinkingTextContainer = nullptr;
    ChainLineWidget *m_thinkingChainLine     = nullptr;
    QLabel          *m_contentLabel           = nullptr;
    QWidget         *m_toolsContainer        = nullptr;
    QVBoxLayout     *m_toolsLayout           = nullptr;
    QWidget         *m_spinnerRow            = nullptr;
    ChainLineWidget *m_spinnerChainLine      = nullptr;
    QLabel          *m_spinnerLabel           = nullptr;

    QList<ToolItem *> m_toolItems;
    QString           m_rawText;
    bool              m_streaming  = false;
    int               m_toolCount  = 0;
};
