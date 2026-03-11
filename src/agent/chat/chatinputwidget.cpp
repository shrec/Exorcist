#include "chatinputwidget.h"
#include "chatthemetokens.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QTextDocument>
#include <QToolButton>
#include <QVBoxLayout>

ChatInputWidget::ChatInputWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setAcceptDrops(true);
}

void ChatInputWidget::setupUi()
{
    // ── Style ─────────────────────────────────────────────────────────────
    setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background: transparent; border: none; color: %1;"
        "  padding: 4px 6px; border-radius: %2px; font-size: %3px; }"
        "QToolButton:hover { background: %4; color: %5; }"
        "QToolButton:focus { outline: 1px solid %6; }"
        "QToolButton#sendBtn {"
        "  background: %7; color: %8; padding: 0; border-radius: 13px;"
        "  font-size: 16px; font-weight: 700; qproperty-iconSize: 0px; }"
        "QToolButton#sendBtn:hover  { background: %9; }"
        "QToolButton#sendBtn:pressed { background: %10; }"
        "QToolButton#sendBtn:disabled { background: %11; color: %12; }"
        "QToolButton#cancelBtn { color: %1; font-size: 14px; }"
        "QToolButton#cancelBtn:hover { background: rgba(241,76,76,0.15); color: #f14c4c; }"
        "QToolButton#cancelBtn:pressed { background: rgba(241,76,76,0.25); }"
        "QToolButton#newSessionBtn {"
        "  color: %13; font-size: 16px; font-weight: 300; padding: 2px 6px; }"
        "QToolButton#historyBtn {"
        "  color: %13; font-size: %3px; padding: 2px 6px; }"
        "QPlainTextEdit {"
        "  background: transparent; border: none; border-radius: 0;"
        "  color: %5; font-size: %3px; padding: 2px 4px 2px 8px; }"
        "QListWidget {"
        "  background: %14; border: 1px solid %15; color: %5; }"
        "QListWidget::item { padding: 5px 10px; font-size: %16px; }"
        "QListWidget::item:hover, QListWidget::item:selected { background: %17; }")
        .arg(ChatTheme::FgSecondary)                 // %1
        .arg(ChatTheme::RadiusMedium)                // %2
        .arg(ChatTheme::FontSize)                    // %3
        .arg(ChatTheme::HoverBg)                     // %4
        .arg(ChatTheme::FgPrimary)                   // %5
        .arg(ChatTheme::FocusOutline)                // %6
        .arg(ChatTheme::ButtonBg)                    // %7
        .arg(ChatTheme::ButtonFg)                    // %8
        .arg(ChatTheme::ButtonHover)                 // %9
        .arg(ChatTheme::ButtonPressed)               // %10
        .arg(ChatTheme::DisabledBg)                  // %11
        .arg(ChatTheme::DisabledFg)                  // %12
        .arg(ChatTheme::FgDimmed)                    // %13
        .arg(ChatTheme::ScrollTrack)                 // %14
        .arg(ChatTheme::Border)                      // %15
        .arg(ChatTheme::SmallFont)                   // %16
        .arg(ChatTheme::ListSelection));              // %17

    // ── Input editor ──────────────────────────────────────────────────────
    m_input = new QPlainTextEdit(this);
    m_input->setPlaceholderText(tr("Ask anything, @ to mention, # to attach"));
    m_input->setAccessibleName(tr("Chat message input"));
    m_input->setMinimumHeight(32);
    m_input->setMaximumHeight(180);
    m_input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_input->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_input->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    connect(m_input->document(), &QTextDocument::contentsChanged, this, [this]() {
        const int docH = int(m_input->document()->size().height()) + 12;
        m_input->setFixedHeight(qBound(32, docH, 180));
    });
    m_input->installEventFilter(this);

    // ── Buttons ───────────────────────────────────────────────────────────
    m_sendBtn = new QToolButton(this);
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setToolTip(tr("Send  (Enter)"));
    m_sendBtn->setAccessibleName(tr("Send message"));
    m_sendBtn->setFixedSize(26, 26);
    // Draw a clean arrow-up icon instead of relying on a Unicode glyph
    {
        const int sz = 26;
        QPixmap pix(sz, sz);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor(ChatTheme::ButtonFg), 2.0,
                      Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Arrow shaft (center vertical line)
        p.drawLine(sz / 2, sz - 8, sz / 2, 8);
        // Arrow head (chevron)
        p.drawLine(sz / 2, 8, sz / 2 - 5, 13);
        p.drawLine(sz / 2, 8, sz / 2 + 5, 13);
        p.end();
        m_sendBtn->setIcon(QIcon(pix));
        m_sendBtn->setIconSize(QSize(sz, sz));
    }
    connect(m_sendBtn, &QToolButton::clicked, this, &ChatInputWidget::onSendClicked);

    m_cancelBtn = new QToolButton(this);
    m_cancelBtn->setText(tr("\u2715"));
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setToolTip(tr("Cancel request  (Escape)"));
    m_cancelBtn->setAccessibleName(tr("Cancel request"));
    m_cancelBtn->setFixedSize(24, 24);
    m_cancelBtn->setEnabled(false);
    connect(m_cancelBtn, &QToolButton::clicked, this, &ChatInputWidget::cancelRequested);

    m_attachBtn = new QToolButton(this);
    m_attachBtn->setText(QStringLiteral("+"));
    m_attachBtn->setObjectName("attachBtn");
    m_attachBtn->setToolTip(tr("Attach file or image"));
    m_attachBtn->setAccessibleName(tr("Attach file or image"));
    m_attachBtn->setFixedSize(22, 22);
    m_attachBtn->setStyleSheet(QStringLiteral(
        "QToolButton#attachBtn { color:%1; font-size:14px; padding:0;"
        "  background:transparent; border:none; border-radius:3px; }"
        "QToolButton#attachBtn:hover { background:%2; color:%3; }")
        .arg(ChatTheme::FgDimmed, ChatTheme::HoverBg, ChatTheme::FgPrimary));
    connect(m_attachBtn, &QToolButton::clicked, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, tr("Attach files"), {},
            tr("All files (*);;Images (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"));
        for (const QString &p : paths)
            addAttachment(p);
    });

    auto *inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(2, 2, 2, 0);
    inputRow->setSpacing(0);
    inputRow->addWidget(m_input, 1);

    // ── Mode dropdown (VS Code style) ─────────────────────────────────────
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Ask"));
    m_modeCombo->addItem(tr("Edit"));
    m_modeCombo->addItem(QStringLiteral("\u25C7 ") + tr("Agent"));
    m_modeCombo->setCurrentIndex(0);
    m_modeCombo->setToolTip(tr("Chat mode"));
    m_modeCombo->setAccessibleName(tr("Chat mode selector"));
    m_modeCombo->setMinimumWidth(55);
    m_modeCombo->setFixedHeight(22);
    m_modeCombo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_modeCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background:%1; border:1px solid %2;"
        "  border-radius:4px; color:%3; font-size:%4px; padding:2px 16px 2px 8px; }"
        "QComboBox:hover { color:%5; border-color:%6; }"
        "QComboBox::drop-down { subcontrol-origin:padding;"
        "  subcontrol-position:center right; border:none; width:14px; }"
        "QComboBox::down-arrow { image:none; }"
        "QComboBox QAbstractItemView { background:%7; color:%5;"
        "  border:1px solid %2; selection-background-color:%8; outline:none;"
        "  padding:2px 0; }")
        .arg(ChatTheme::HoverBg, ChatTheme::Border)
        .arg(ChatTheme::FgPrimary)
        .arg(ChatTheme::SmallFont)
        .arg(ChatTheme::FgPrimary, ChatTheme::FgDimmed,
             ChatTheme::ScrollTrack, ChatTheme::ListSelection));
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) { setCurrentMode(idx); });

    // Keep old buttons for API compat but hidden
    m_askBtn = new QToolButton(this);   m_askBtn->hide();
    m_editBtn = new QToolButton(this);  m_editBtn->hide();
    m_agentBtn = new QToolButton(this); m_agentBtn->hide();

    // ── Bottom bar ────────────────────────────────────────────────────────
    m_newSessionBtn = new QToolButton(this);
    m_newSessionBtn->setText(QStringLiteral("+"));
    m_newSessionBtn->setObjectName("newSessionBtn");
    m_newSessionBtn->setToolTip(tr("New session (clears conversation)"));
    m_newSessionBtn->setAccessibleName(tr("New chat session"));
    m_newSessionBtn->hide(); // managed by parent header bar
    connect(m_newSessionBtn, &QToolButton::clicked,
            this, &ChatInputWidget::newSessionRequested);

    m_historyBtn = new QToolButton(this);
    m_historyBtn->setText(QStringLiteral("\u2630"));
    m_historyBtn->setObjectName("historyBtn");
    m_historyBtn->setToolTip(tr("Session history"));
    m_historyBtn->setAccessibleName(tr("Session history"));
    m_historyBtn->hide(); // managed by parent header bar
    connect(m_historyBtn, &QToolButton::clicked,
            this, &ChatInputWidget::historyRequested);

    m_modelCombo = new QComboBox(this);
    m_modelCombo->setMinimumWidth(100);
    m_modelCombo->setFixedHeight(22);
    m_modelCombo->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_modelCombo->setToolTip(tr("Select model — hover items for rate info"));
    m_modelCombo->setAccessibleName(tr("Model selection"));
    m_modelCombo->setPlaceholderText(tr("Model"));
    m_modelCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background:transparent; border:none;"
        "  color:%1; font-size:%2px; padding:2px 16px 2px 6px; }"
        "QComboBox:hover { color:%3; }"
        "QComboBox:disabled { color:%4; }"
        "QComboBox::drop-down { subcontrol-origin:padding;"
        "  subcontrol-position:center right; border:none; width:14px; }"
        "QComboBox::down-arrow { image:none; }"
        "QComboBox QAbstractItemView { background:%5; color:%3;"
        "  border:1px solid %6; selection-background-color:%7; outline:none;"
        "  padding:2px 0; }"
        "QComboBox QAbstractItemView::item { padding:4px 10px; min-height:20px; }")
        .arg(ChatTheme::FgSecondary)
        .arg(ChatTheme::SmallFont)
        .arg(ChatTheme::FgPrimary, ChatTheme::DisabledFg,
             ChatTheme::ScrollTrack, ChatTheme::Border,
             ChatTheme::ListSelection));
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (idx >= 0) {
            const QString id = m_modelCombo->itemData(idx).toString();
            emit modelSelected(id.isEmpty() ? m_modelCombo->itemText(idx) : id);
        }
        // Show/hide thinking toggle based on model capability
        updateThinkingBtnVisibility();
    });

    // ── Thinking toggle ────────────────────────────────────────────────
    m_thinkingBtn = new QToolButton(this);
    m_thinkingBtn->setText(QStringLiteral("\U0001F4A1"));
    m_thinkingBtn->setObjectName("thinkingBtn");
    m_thinkingBtn->setToolTip(tr("Enable extended thinking"));
    m_thinkingBtn->setFixedHeight(22);
    m_thinkingBtn->setCheckable(true);
    m_thinkingBtn->setChecked(false);
    m_thinkingBtn->setStyleSheet(QStringLiteral(
        "QToolButton#thinkingBtn { color:%1; font-size:%2px;"
        "  padding:0 4px; background:transparent; border:none; border-radius:%3px; }"
        "QToolButton#thinkingBtn:hover { background:%4; color:%5; }"
        "QToolButton#thinkingBtn:checked { background:%6; color:%5; }")
        .arg(ChatTheme::FgDimmed)
        .arg(ChatTheme::SmallFont)
        .arg(ChatTheme::RadiusSmall)
        .arg(ChatTheme::HoverBg, ChatTheme::FgPrimary, ChatTheme::SelectionBg));
    m_thinkingBtn->hide(); // shown when a thinking-capable model is selected

    // ── Tools indicator ──────────────────────────────────────────────
    m_toolsBtn = new QToolButton(this);
    m_toolsBtn->setObjectName("toolsBtn");
    m_toolsBtn->setToolTip(tr("Available tools"));
    m_toolsBtn->setFixedHeight(22);
    m_toolsBtn->setStyleSheet(QStringLiteral(
        "QToolButton#toolsBtn { color:%1; font-size:%2px;"
        "  padding:0 5px; background:transparent; border:none; border-radius:%3px; }"
        "QToolButton#toolsBtn:hover { background:%4; color:%5; }")
        .arg(ChatTheme::FgDimmed)
        .arg(ChatTheme::TinyFont)
        .arg(ChatTheme::RadiusSmall)
        .arg(ChatTheme::HoverBg, ChatTheme::FgPrimary));
    m_toolsBtn->setText(QStringLiteral("\U0001F527"));

    m_ctxBtn = new QToolButton(this);
    m_ctxBtn->setText(QStringLiteral("{ }"));
    m_ctxBtn->setObjectName("ctxBtn");
    m_ctxBtn->setToolTip(tr("Context window usage"));
    m_ctxBtn->setFixedHeight(22);
    m_ctxBtn->setStyleSheet(QStringLiteral(
        "QToolButton#ctxBtn { color:%1; font-size:%2px; font-family:monospace;"
        "  padding:0 5px; background:transparent; border:none; border-radius:%3px; }"
        "QToolButton#ctxBtn:hover { background:%4; color:%5; }"
        "QToolButton#ctxBtn:checked { background:%4; color:%5; }")
        .arg(ChatTheme::FgDimmed)
        .arg(ChatTheme::TinyFont)
        .arg(ChatTheme::RadiusSmall)
        .arg(ChatTheme::HoverBg, ChatTheme::FgPrimary));
    m_ctxBtn->setCheckable(true);
    connect(m_ctxBtn, &QToolButton::toggled, this, [this](bool on) {
        if (on) emit contextPopupRequested();
    });

    auto *bottomBar = new QHBoxLayout;
    bottomBar->setContentsMargins(6, 0, 6, 4);
    bottomBar->setSpacing(2);
    bottomBar->addWidget(m_attachBtn);
    bottomBar->addSpacing(2);
    bottomBar->addWidget(m_modeCombo);
    bottomBar->addSpacing(4);
    bottomBar->addWidget(m_modelCombo);
    bottomBar->addWidget(m_thinkingBtn);
    bottomBar->addSpacing(2);
    bottomBar->addWidget(m_toolsBtn);
    bottomBar->addStretch();
    bottomBar->addWidget(m_ctxBtn);
    bottomBar->addSpacing(4);
    bottomBar->addWidget(m_sendBtn);
    bottomBar->addWidget(m_cancelBtn);

    // ── Attachment chips bar ──────────────────────────────────────────────
    m_attachBar = new QWidget(this);
    m_attachLayout = new QHBoxLayout(m_attachBar);
    m_attachLayout->setContentsMargins(8, 4, 8, 0);
    m_attachLayout->setSpacing(4);
    m_attachLayout->addStretch();
    m_attachBar->setStyleSheet(QStringLiteral("background:%1;").arg(ChatTheme::PanelBg));
    m_attachBar->hide();

    // ── Context strip ─────────────────────────────────────────────────────
    m_contextStrip = new QLabel(this);
    m_contextStrip->setStyleSheet(QStringLiteral(
        "color:%1; font-size:%2px; padding:1px 10px; background:%3;")
        .arg(ChatTheme::FgDimmed)
        .arg(ChatTheme::TinyFont)
        .arg(ChatTheme::PanelBg));
    m_contextStrip->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_contextStrip->setFixedHeight(16);
    m_contextStrip->hide();

    // ── Input block container ─────────────────────────────────────────────
    auto *inputBlock = new QWidget(this);
    m_inputBlock = inputBlock;
    inputBlock->setObjectName("inputBlock");
    inputBlock->setStyleSheet(QStringLiteral(
        "QWidget#inputBlock { background:%1; border:1px solid %2;"
        "  border-radius:8px; }"
        "QWidget#inputBlock QPlainTextEdit {"
        "  background:transparent; border:none; border-radius:0; }")
        .arg(ChatTheme::InputBg, ChatTheme::Border));
    auto *inputBlockLayout = new QVBoxLayout(inputBlock);
    inputBlockLayout->setContentsMargins(0, 2, 0, 0);
    inputBlockLayout->setSpacing(0);
    inputBlockLayout->addLayout(inputRow);
    inputBlockLayout->addLayout(bottomBar);

    // ── Autocomplete popups ───────────────────────────────────────────────
    m_slashMenu = new QListWidget(this);
    m_slashMenu->setWindowFlags(Qt::ToolTip);
    m_slashMenu->setFocusPolicy(Qt::NoFocus);
    m_slashMenu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_slashMenu->setFixedWidth(300);
    m_slashMenu->hide();
    connect(m_slashMenu, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString cmd = item->text().section(' ', 0, 0);
        m_input->setPlainText(cmd + ' ');
        m_input->moveCursor(QTextCursor::End);
        hideSlashMenu();
        m_input->setFocus();
    });

    m_mentionMenu = new QListWidget(this);
    m_mentionMenu->setWindowFlags(Qt::ToolTip);
    m_mentionMenu->setFocusPolicy(Qt::NoFocus);
    m_mentionMenu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_mentionMenu->setFixedWidth(340);
    m_mentionMenu->hide();
    connect(m_mentionMenu, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString filePath = item->data(Qt::UserRole).toString();
        QTextCursor tc = m_input->textCursor();
        const QString txt = m_input->toPlainText();
        int triggerPos = tc.position() - 1;
        while (triggerPos >= 0 && txt[triggerPos] != QLatin1Char('@') &&
               txt[triggerPos] != QLatin1Char('#'))
            --triggerPos;
        if (triggerPos >= 0) {
            tc.setPosition(triggerPos);
            tc.setPosition(m_input->textCursor().position(), QTextCursor::KeepAnchor);
            if (m_mentionTrigger == QLatin1String("#")) {
                tc.insertText(QStringLiteral("#%1 ").arg(filePath));
                const QString fullPath = item->data(Qt::UserRole + 1).toString();
                if (!fullPath.isEmpty())
                    addAttachment(fullPath);
            } else {
                tc.insertText(QStringLiteral("@file:%1 ").arg(filePath));
            }
        }
        hideMentionMenu();
        m_input->setFocus();
    });

    // Wire text changes for autocomplete
    connect(m_input->document(), &QTextDocument::contentsChanged,
            this, &ChatInputWidget::onTextChanged);

    // ── Streaming progress bar ───────────────────────────────────────
    m_streamingBar = new QProgressBar(this);
    m_streamingBar->setFixedHeight(2);
    m_streamingBar->setRange(0, 0);
    m_streamingBar->setTextVisible(false);
    m_streamingBar->setStyleSheet(QStringLiteral(
        "QProgressBar { background:transparent; border:none; max-height:2px; }"
        "QProgressBar::chunk { background:%1; }")
        .arg(ChatTheme::ProgressBlue));
    m_streamingBar->hide();

    // ── Root layout ───────────────────────────────────────────────────────
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 2, 8, 6);
    root->setSpacing(0);
    root->addWidget(m_contextStrip);
    root->addWidget(m_attachBar);
    root->addWidget(m_streamingBar);
    root->addWidget(inputBlock);

    // ── Tab order ─────────────────────────────────────────────────────────
    setTabOrder(m_input, m_modeCombo);
    setTabOrder(m_modeCombo, m_modelCombo);
    setTabOrder(m_modelCombo, m_sendBtn);

    m_modeCombo->setCurrentIndex(0);
}

// ── Public accessors ──────────────────────────────────────────────────────────

QString ChatInputWidget::text() const
{
    return m_input->toPlainText().trimmed();
}

void ChatInputWidget::setText(const QString &text)
{
    m_input->setPlainText(text);
    m_input->moveCursor(QTextCursor::End);
}

void ChatInputWidget::clearText()
{
    m_input->clear();
}

void ChatInputWidget::focusInput()
{
    m_input->setFocus();
}

void ChatInputWidget::setCurrentMode(int mode)
{
    if (m_currentMode == mode)
        return;
    m_currentMode = mode;
    setModeButtonActive(mode);
    updatePlaceholder();
    emit modeChanged(mode);
}

void ChatInputWidget::setModels(const QStringList &models, const QString &current)
{
    QSignalBlocker blocker(m_modelCombo);
    m_modelCombo->clear();
    for (const QString &m : models)
        m_modelCombo->addItem(m);
    const int idx = m_modelCombo->findText(current);
    if (idx >= 0)
        m_modelCombo->setCurrentIndex(idx);
}

void ChatInputWidget::clearModels()
{
    QSignalBlocker blocker(m_modelCombo);
    m_modelCombo->clear();
}

void ChatInputWidget::addModel(const QString &id, const QString &displayName,
                               bool isPremium, double multiplier,
                               bool thinking, bool vision, bool toolCalls)
{
    QSignalBlocker blocker(m_modelCombo);
    const QString name = displayName.isEmpty() ? id : displayName;

    // ── Build display label ──────────────────────────────────────────────
    // Format: "Claude Opus 4  5x  ★"  or  "GPT-4o mini  0.25x"
    QString label = name;

    // Rate multiplier — always show for cost awareness
    if (multiplier > 0.0) {
        if (multiplier < 1.0)
            label += QStringLiteral("  %1x").arg(multiplier, 0, 'f', 2);
        else if (multiplier > 1.0)
            label += QStringLiteral("  %1x").arg(multiplier, 0, 'g', 3);
        // 1.0x = standard rate, no label needed
    }

    // Premium badge
    if (isPremium)
        label += QStringLiteral("  \u2605");  // ★

    m_modelCombo->addItem(label, id);

    const int idx = m_modelCombo->count() - 1;

    // ── Rich tooltip ─────────────────────────────────────────────────────
    QString tip = name;
    tip += QStringLiteral("\n");

    // Capabilities line
    QStringList caps;
    if (thinking)  caps << tr("Thinking");
    if (vision)    caps << tr("Vision");
    if (toolCalls) caps << tr("Tools");
    if (!caps.isEmpty())
        tip += caps.join(QStringLiteral(" \u00B7 "));  // middle dot separator

    // Rate info
    tip += QStringLiteral("\n");
    if (multiplier < 1.0)
        tip += tr("Rate: %1x (cheaper)").arg(multiplier, 0, 'f', 2);
    else if (multiplier > 1.0)
        tip += tr("Rate: %1x (premium)").arg(multiplier, 0, 'g', 3);
    else
        tip += tr("Rate: 1x (standard)");

    if (isPremium)
        tip += QStringLiteral("\n\u2605 ") + tr("Premium model \u2014 uses premium requests");

    m_modelCombo->setItemData(idx, tip, Qt::ToolTipRole);

    // ── Color coding by tier ─────────────────────────────────────────────
    if (isPremium || multiplier > 1.0) {
        // Premium = purple tint
        m_modelCombo->setItemData(idx, QColor(QLatin1String(ChatTheme::PremiumFg)),
                                  Qt::ForegroundRole);
    } else if (multiplier < 1.0) {
        // Budget-friendly = green tint
        m_modelCombo->setItemData(idx, QColor(QLatin1String(ChatTheme::SuccessFg)),
                                  Qt::ForegroundRole);
    }

    // Store thinking capability for toggle visibility
    m_modelCombo->setItemData(idx, thinking, Qt::UserRole + 1);
}

void ChatInputWidget::setCurrentModel(const QString &id)
{
    QSignalBlocker blocker(m_modelCombo);
    for (int i = 0; i < m_modelCombo->count(); ++i) {
        if (m_modelCombo->itemData(i).toString() == id
            || m_modelCombo->itemText(i) == id) {
            m_modelCombo->setCurrentIndex(i);
            break;
        }
    }
}

QString ChatInputWidget::selectedModel() const
{
    const QString id = m_modelCombo->currentData().toString();
    return id.isEmpty() ? m_modelCombo->currentText() : id;
}

bool ChatInputWidget::isThinkingEnabled() const
{
    return m_thinkingBtn->isVisible() && m_thinkingBtn->isChecked();
}

void ChatInputWidget::updateThinkingBtnVisibility()
{
    const int idx = m_modelCombo->currentIndex();
    if (idx < 0) {
        m_thinkingBtn->hide();
        return;
    }
    const bool supportsThinking = m_modelCombo->itemData(idx, Qt::UserRole + 1).toBool();
    m_thinkingBtn->setVisible(supportsThinking);
}

void ChatInputWidget::setStreamingState(bool streaming)
{
    m_sendBtn->setEnabled(!streaming);
    m_cancelBtn->setEnabled(streaming);
    if (m_streamingBar) {
        if (streaming)
            m_streamingBar->show();
        else
            m_streamingBar->hide();
    }
}

void ChatInputWidget::setInputEnabled(bool enabled)
{
    m_sendBtn->setEnabled(enabled);
    m_cancelBtn->setEnabled(!enabled);
    if (enabled)
        m_input->setFocus();
}

void ChatInputWidget::setToolCount(int count)
{
    if (count > 0) {
        m_toolsBtn->setText(QStringLiteral("\U0001F527 %1").arg(count));
        m_toolsBtn->setToolTip(tr("%1 tools available").arg(count));
        m_toolsBtn->show();
    } else {
        m_toolsBtn->hide();
    }
}

void ChatInputWidget::updatePlaceholder()
{
    switch (m_currentMode) {
    case 0: // Ask
        m_input->setPlaceholderText(tr("Ask anything, @ to mention, # to attach"));
        break;
    case 1: // Edit
        m_input->setPlaceholderText(tr("Describe the edit you want to make..."));
        break;
    case 2: // Agent
        m_input->setPlaceholderText(tr("Describe what to build next"));
        break;
    default:
        m_input->setPlaceholderText(tr("Ask anything..."));
        break;
    }
}

void ChatInputWidget::setSlashCommands(const QStringList &commands)
{
    m_slashMenu->clear();
    for (const QString &cmd : commands)
        m_slashMenu->addItem(cmd);
    m_staticSlashCount = m_slashMenu->count();
}

void ChatInputWidget::addDynamicSlashCommands(const QStringList &commands)
{
    while (m_slashMenu->count() > m_staticSlashCount)
        delete m_slashMenu->takeItem(m_slashMenu->count() - 1);
    for (const QString &cmd : commands)
        m_slashMenu->addItem(cmd);
}

void ChatInputWidget::setWorkspaceFileProvider(std::function<QStringList()> fn)
{
    m_workspaceFileFn = std::move(fn);
}

void ChatInputWidget::setContextInfo(const QString &info)
{
    if (info.isEmpty()) {
        m_contextStrip->hide();
    } else {
        m_contextStrip->setText(info);
        m_contextStrip->show();
    }
}

void ChatInputWidget::setContextTokenCount(int tokens)
{
    m_ctxBtn->setText(tokens > 0
        ? QStringLiteral("%1K").arg(tokens / 1000)
        : QStringLiteral("{ }"));
}

// ── Attachments ───────────────────────────────────────────────────────────────

void ChatInputWidget::addAttachment(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists()) return;

    Attachment att;
    att.path = path;
    att.name = fi.fileName();
    const QString ext = fi.suffix().toLower();
    att.isImage = (ext == QLatin1String("png") || ext == QLatin1String("jpg")
                || ext == QLatin1String("jpeg") || ext == QLatin1String("gif")
                || ext == QLatin1String("webp") || ext == QLatin1String("bmp"));
    if (att.isImage) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            att.data = f.readAll();
    }
    m_attachments.append(att);
    rebuildAttachChips();
}

void ChatInputWidget::attachSelection(const QString &text, const QString &filePath,
                                       int startLine)
{
    if (text.isEmpty()) return;

    const QFileInfo fi(filePath);
    const QString label = fi.fileName().isEmpty()
        ? tr("Selection")
        : QStringLiteral("%1:%2").arg(fi.fileName()).arg(startLine);

    Attachment att;
    att.path = filePath;
    att.name = label;
    att.data = text.toUtf8();
    att.isImage = false;
    m_attachments.append(att);
    rebuildAttachChips();
}

void ChatInputWidget::attachDiagnostics(const QString &summary, int count)
{
    Attachment att;
    att.name = tr("%n diagnostic(s)", nullptr, count);
    att.data = summary.toUtf8();
    att.isImage = false;
    m_attachments.append(att);
    rebuildAttachChips();
}

void ChatInputWidget::clearAttachments()
{
    m_attachments.clear();
    rebuildAttachChips();
}

void ChatInputWidget::rebuildAttachChips()
{
    // Clear old chips
    while (QLayoutItem *item = m_attachLayout->takeAt(0)) {
        if (auto *w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_attachLayout->addStretch();

    if (m_attachments.isEmpty()) {
        m_attachBar->hide();
        return;
    }

    for (int i = 0; i < m_attachments.size(); ++i) {
        const auto &att = m_attachments[i];
        auto *chip = new QWidget(m_attachBar);
        chip->setStyleSheet(QStringLiteral(
            "background:%1; border:1px solid %2; border-radius:3px; padding:0;")
            .arg(ChatTheme::SideBarBg, ChatTheme::Border));
        auto *hl = new QHBoxLayout(chip);
        hl->setContentsMargins(6, 2, 4, 2);
        hl->setSpacing(4);
        auto *lbl = new QLabel(
            QStringLiteral("&#128206; %1").arg(att.name.toHtmlEscaped()), chip);
        lbl->setStyleSheet(QStringLiteral(
            "color:%1; font-size:%2px; background:transparent;")
            .arg(ChatTheme::FgPrimary).arg(ChatTheme::TinyFont));
        lbl->setTextFormat(Qt::RichText);
        auto *rm = new QToolButton(chip);
        rm->setText(QStringLiteral("\u00d7"));
        rm->setStyleSheet(QStringLiteral(
            "QToolButton{background:transparent;border:none;color:%1;font-size:12px;padding:0 2px;}"
            "QToolButton:hover{color:%2;}")
            .arg(ChatTheme::FgDimmed, ChatTheme::ErrorFg));
        connect(rm, &QToolButton::clicked, this, [this, i]() {
            if (i < m_attachments.size()) {
                m_attachments.removeAt(i);
                rebuildAttachChips();
            }
        });
        hl->addWidget(lbl);
        hl->addWidget(rm);
        m_attachLayout->insertWidget(m_attachLayout->count() - 1, chip);
    }
    m_attachBar->show();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ChatInputWidget::onSendClicked()
{
    const QString t = text();
    if (t.isEmpty() && m_attachments.isEmpty())
        return;

    if (!t.isEmpty()) {
        m_inputHistory.append(t);
        m_historyIndex = -1;
    }

    hideSlashMenu();
    hideMentionMenu();

    emit sendRequested(t, m_currentMode);
    m_attachments.clear();
    rebuildAttachChips();
    m_input->clear();
}

void ChatInputWidget::onTextChanged()
{
    const QString txt = m_input->toPlainText();
    if (txt.startsWith('/') && !txt.contains(' ') && !txt.contains('\n'))
        showSlashMenu();
    else
        hideSlashMenu();

    // @-mention trigger
    const int cursorPos = m_input->textCursor().position();
    int atPos = txt.lastIndexOf('@', cursorPos - 1);
    if (atPos >= 0 && (atPos == 0 || txt[atPos - 1].isSpace())) {
        const QString filter = txt.mid(atPos + 1, cursorPos - atPos - 1);
        if (!filter.contains(' ') && !filter.contains('\n'))
            showMentionMenu(QStringLiteral("@"), filter);
        else
            hideMentionMenu();
    } else {
        int hashPos = txt.lastIndexOf('#', cursorPos - 1);
        if (hashPos >= 0 && (hashPos == 0 || txt[hashPos - 1].isSpace())) {
            const QString filter = txt.mid(hashPos + 1, cursorPos - hashPos - 1);
            if (!filter.contains(' ') && !filter.contains('\n'))
                showMentionMenu(QStringLiteral("#"), filter);
            else
                hideMentionMenu();
        } else {
            hideMentionMenu();
        }
    }
}

// ── Mode button styling ───────────────────────────────────────────────────────

void ChatInputWidget::setModeButtonActive(int mode)
{
    QSignalBlocker blocker(m_modeCombo);
    m_modeCombo->setCurrentIndex(mode);
}

// ── Slash / mention menu ──────────────────────────────────────────────────────

void ChatInputWidget::showSlashMenu()
{
    const QString typed = m_input->toPlainText().toLower();
    bool anyVisible = false;
    for (int i = 0; i < m_slashMenu->count(); ++i) {
        QListWidgetItem *item = m_slashMenu->item(i);
        const bool matches = item->text().toLower().startsWith(typed);
        item->setHidden(!matches);
        if (matches) anyVisible = true;
    }
    if (!anyVisible) { hideSlashMenu(); return; }

    const QPoint pos = m_input->mapToGlobal(QPoint(0, 0));
    m_slashMenu->adjustSize();
    const int menuH = m_slashMenu->sizeHintForRow(0) *
                      std::min(m_slashMenu->count(), 6) + 4;
    m_slashMenu->setFixedHeight(menuH);
    m_slashMenu->move(pos.x(), pos.y() - menuH - 2);
    m_slashMenu->show();
    m_slashMenu->raise();
}

void ChatInputWidget::hideSlashMenu()
{
    m_slashMenu->hide();
}

void ChatInputWidget::showMentionMenu(const QString &trigger, const QString &filter)
{
    if (!m_workspaceFileFn) { hideMentionMenu(); return; }
    m_mentionTrigger = trigger;
    m_mentionMenu->clear();

    const QStringList files = m_workspaceFileFn();
    const QString lowerFilter = filter.toLower();
    int shown = 0;

    for (const QString &absPath : files) {
        const QFileInfo fi(absPath);
        const QString display = fi.fileName();
        if (!lowerFilter.isEmpty()
            && !display.toLower().contains(lowerFilter)
            && !absPath.toLower().contains(lowerFilter))
            continue;

        auto *item = new QListWidgetItem(
            trigger == QLatin1String("#")
                ? QStringLiteral("\U0001F4C4 %1").arg(absPath)
                : QStringLiteral("@file:%1").arg(absPath));
        item->setData(Qt::UserRole, absPath);
        item->setData(Qt::UserRole + 1, absPath);
        m_mentionMenu->addItem(item);
        if (++shown >= 15) break;
    }

    if (shown == 0) { hideMentionMenu(); return; }

    const QPoint pos = m_input->mapToGlobal(QPoint(0, 0));
    m_mentionMenu->adjustSize();
    const int menuH = m_mentionMenu->sizeHintForRow(0) * std::min(shown, 8) + 4;
    m_mentionMenu->setFixedHeight(menuH);
    m_mentionMenu->move(pos.x(), pos.y() - menuH - 2);
    m_mentionMenu->show();
    m_mentionMenu->raise();
}

void ChatInputWidget::hideMentionMenu()
{
    m_mentionMenu->hide();
}

// ── Event filter (Enter, Up/Down, Escape, Ctrl+V) ────────────────────────────

bool ChatInputWidget::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_input && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);

        // Rich paste: Ctrl+V with image
        if (ke->key() == Qt::Key_V && (ke->modifiers() & Qt::ControlModifier)) {
            const QMimeData *mime = QApplication::clipboard()->mimeData();
            if (mime && mime->hasImage()) {
                const QImage img = qvariant_cast<QImage>(mime->imageData());
                if (!img.isNull()) {
                    const QString tmpPath = QDir::tempPath()
                        + QStringLiteral("/exorcist_paste_%1.png")
                              .arg(QDateTime::currentMSecsSinceEpoch());
                    if (img.save(tmpPath, "PNG"))
                        addAttachment(tmpPath);
                    return true;
                }
            }
        }

        // Escape dismisses popups
        if (ke->key() == Qt::Key_Escape) {
            if (m_mentionMenu->isVisible()) { hideMentionMenu(); return true; }
            if (m_slashMenu->isVisible())   { hideSlashMenu();   return true; }
        }

        // Tab/Enter accepts popup item
        if ((ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Return
             || ke->key() == Qt::Key_Enter) && m_mentionMenu->isVisible()) {
            auto *cur = m_mentionMenu->currentItem();
            if (!cur && m_mentionMenu->count() > 0)
                cur = m_mentionMenu->item(0);
            if (cur) {
                emit m_mentionMenu->itemClicked(cur);
                return true;
            }
        }

        // Up/Down navigate popup
        if (ke->key() == Qt::Key_Up && m_mentionMenu->isVisible()) {
            int row = m_mentionMenu->currentRow();
            if (row > 0) m_mentionMenu->setCurrentRow(row - 1);
            return true;
        }
        if (ke->key() == Qt::Key_Down && m_mentionMenu->isVisible()) {
            int row = m_mentionMenu->currentRow();
            if (row < m_mentionMenu->count() - 1)
                m_mentionMenu->setCurrentRow(row + 1);
            return true;
        }

        // Enter = send, Shift+Enter = newline
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier)
                return false;
            onSendClicked();
            return true;
        }

        // Up/Down for input history
        if (ke->key() == Qt::Key_Up && !m_inputHistory.isEmpty()) {
            const QTextCursor tc = m_input->textCursor();
            if (tc.blockNumber() == 0) {
                if (m_historyIndex < 0)
                    m_historyIndex = m_inputHistory.size();
                if (m_historyIndex > 0) {
                    --m_historyIndex;
                    m_input->setPlainText(m_inputHistory[m_historyIndex]);
                    m_input->moveCursor(QTextCursor::End);
                }
                return true;
            }
        }
        if (ke->key() == Qt::Key_Down && !m_inputHistory.isEmpty()) {
            const QTextCursor tc = m_input->textCursor();
            if (tc.blockNumber() == m_input->document()->blockCount() - 1) {
                if (m_historyIndex >= 0 && m_historyIndex < m_inputHistory.size() - 1) {
                    ++m_historyIndex;
                    m_input->setPlainText(m_inputHistory[m_historyIndex]);
                    m_input->moveCursor(QTextCursor::End);
                } else {
                    m_historyIndex = -1;
                    m_input->clear();
                }
                return true;
            }
        }
    }
    if (obj == m_input) {
        if (ev->type() == QEvent::FocusIn) {
            if (m_inputBlock)
                m_inputBlock->setStyleSheet(QStringLiteral(
                    "QWidget#inputBlock { background:%1; border:1px solid %2;"
                    "  border-radius:8px; }"
                    "QWidget#inputBlock QPlainTextEdit {"
                    "  background:transparent; border:none; border-radius:0; }")
                    .arg(ChatTheme::InputBg, ChatTheme::InputFocusBorder));
        } else if (ev->type() == QEvent::FocusOut) {
            if (m_inputBlock)
                m_inputBlock->setStyleSheet(QStringLiteral(
                    "QWidget#inputBlock { background:%1; border:1px solid %2;"
                    "  border-radius:8px; }"
                    "QWidget#inputBlock QPlainTextEdit {"
                    "  background:transparent; border:none; border-radius:0; }")
                    .arg(ChatTheme::InputBg, ChatTheme::Border));
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void ChatInputWidget::dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasUrls() || ev->mimeData()->hasImage())
        ev->acceptProposedAction();
}

void ChatInputWidget::dropEvent(QDropEvent *ev)
{
    for (const QUrl &url : ev->mimeData()->urls()) {
        if (url.isLocalFile())
            addAttachment(url.toLocalFile());
    }
    ev->acceptProposedAction();
}
