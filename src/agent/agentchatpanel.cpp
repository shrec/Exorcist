#include "agentchatpanel.h"
#include "agentorchestrator.h"
#include "agentcontroller.h"
#include "agentmodes.h"
#include "agentsession.h"
#include "contextbuilder.h"
#include "itool.h"
#include "markdownrenderer.h"
#include "promptfileloader.h"
#include "promptvariables.h"
#include "sessionstore.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <utility>

AgentChatPanel::AgentChatPanel(AgentOrchestrator *orchestrator, QWidget *parent)
    : QWidget(parent),
      m_orchestrator(orchestrator),
      m_providerTabBar(new QWidget(this)),
      m_providerTabLayout(new QHBoxLayout(m_providerTabBar)),
      m_history(new QTextBrowser(this)),
      m_slashMenu(new QListWidget(this)),
      m_mentionMenu(new QListWidget(this)),
      m_changesBar(new QWidget(this)),
      m_changesLabel(new QLabel(this)),
      m_keepBtn(new QToolButton(this)),
      m_undoBtn(new QToolButton(this)),
      m_sendBtn(new QToolButton(this)),
      m_cancelBtn(new QToolButton(this)),
      m_attachBtn(new QToolButton(this)),
      m_askBtn(new QToolButton(this)),
      m_editBtn(new QToolButton(this)),
      m_agentBtn(new QToolButton(this)),
      m_newSessionBtn(new QToolButton(this)),
      m_historyBtn(new QToolButton(this)),
      m_modelCombo(new QComboBox(this)),
      m_ctxBtn(new QToolButton(this)),
      m_contextStrip(new QLabel(this)),
      m_input(new QPlainTextEdit(this))
{
    // ── Panel-wide dark theme (VS Code–aligned) ──────────────────────────
    setStyleSheet(
        "AgentChatPanel { background: #1f1f1f; }"
        "QTextBrowser   { background: #1f1f1f; border: none; color: #cccccc;"
        "                 selection-background-color: #264f78; }"
        "QLineEdit {"
        "  background: #2b2b2b; border: 1px solid #3e3e42; border-radius: 6px;"
        "  color: #cccccc; padding: 4px 8px; font-size: 13px; }"
        "QLineEdit:focus { border-color: #0078d4; }"
        "QToolButton {"
        "  background: transparent; border: none; color: #9d9d9d;"
        "  padding: 4px 6px; border-radius: 4px; font-size: 13px; }"
        "QToolButton:hover { background: #2a2d2e; color: #cccccc; }"
        "QToolButton#sendBtn {"
        "  background: #0078d4; color: white; padding: 4px 10px; border-radius: 4px;"
        "  font-size: 14px; }"
        "QToolButton#sendBtn:hover  { background: #026ec1; }"
        "QToolButton#sendBtn:disabled { background: #2a2a2a; color: #4a4a4a; }"
        "QToolButton#cancelBtn { color: #9d9d9d; font-size: 14px; }"
        "QToolButton#newSessionBtn {"
        "  color: #858585; font-size: 16px; font-weight: 300; padding: 2px 6px; }"
        "QToolButton#historyBtn {"
        "  color: #858585; font-size: 13px; padding: 2px 6px; }"
        "QToolButton#keepBtn {"
        "  background: #0078d4; color: white; padding: 3px 10px; border-radius: 4px;"
        "  font-size: 12px; }"
        "QToolButton#keepBtn:hover { background: #026ec1; }"
        "QToolButton#undoBtn {"
        "  background: #3a3d41; color: #cccccc; padding: 3px 10px; border-radius: 4px;"
        "  font-size: 12px; }"
        "QToolButton#undoBtn:hover { background: #4a4d51; }"
        "QComboBox {"
        "  background: transparent; border: none; color: #858585;"
        "  font-size: 12px; padding: 2px 4px; }"
        "QComboBox:hover { color: #cccccc; }"
        "QComboBox::drop-down { border: none; width: 12px; }"
        "QComboBox::down-arrow { border: none; }"
        "QComboBox QAbstractItemView {"
        "  background: #2b2b2b; color: #cccccc; border: 1px solid #3e3e42;"
        "  selection-background-color: #094771; outline: none; }"
        "QListWidget {"
        "  background: #2b2b2b; border: 1px solid #3e3e42; color: #cccccc; }"
        "QListWidget::item { padding: 5px 10px; font-size: 12px; }"
        "QListWidget::item:hover, QListWidget::item:selected { background: #094771; }"
        "QScrollBar:vertical { background: transparent; width: 8px; border: none; }"
        "QScrollBar::handle:vertical {"
        "  background: #424242; border-radius: 4px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: #4f4f4f; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar:horizontal { height: 0; }"
        "QPlainTextEdit {"
        "  background: #2b2b2b; border: 1px solid #3e3e42; border-radius: 6px;"
        "  color: #cccccc; font-size: 13px; padding: 4px 4px 4px 8px; }"
        "QPlainTextEdit:focus { border-color: #0078d4; }");

    // ── Provider tab bar (top) ────────────────────────────────────────────
    m_providerTabBar->setFixedHeight(35);
    m_providerTabBar->setStyleSheet("background:#1f1f1f;");
    m_providerTabLayout->setContentsMargins(0, 0, 0, 0);
    m_providerTabLayout->setSpacing(0);

    // ── Chat history ───────────────────────────────────────────────────
    m_history->setOpenExternalLinks(false);
    m_history->setReadOnly(true);
    m_history->setFrameShape(QFrame::NoFrame);
    m_history->document()->setDefaultStyleSheet(
        /* ── Base ────────────────────────────────────────────────────── */
        "body  { color:#cccccc; font-family:'Segoe WPC','Segoe UI',Arial,sans-serif;"
        "        font-size:13px; line-height:1.4; margin:0; padding:0; }"

        /* ── Message row (table-based: avatar | content) ─────────── */
        ".msg-table { margin:0; padding:0; border:none; border-spacing:0; width:100%; }"
        ".avatar-cell { width:28px; vertical-align:top; padding:8px 0 4px 10px; }"
        ".avatar { width:24px; height:24px; border-radius:12px; color:#ffffff;"
        "          font-size:13px; font-weight:600; text-align:center;"
        "          line-height:24px; }"
        ".avatar-user { background:#0078d4; }"
        ".avatar-ai   { background:#5a4fcf; }"
        ".content-cell { vertical-align:top; padding:4px 20px 2px 6px; }"

        /* ── Name / timestamp row ────────────────────────────────── */
        ".who  { font-size:12px; font-weight:600; margin-bottom:2px; }"
        ".you  { color:#4daafc; }"
        ".ai   { color:#b5bec9; }"
        ".body { color:#d4d4d4; }"
        ".timestamp { color:#4a4a4a; font-size:10px; margin-left:6px; font-weight:normal; }"

        /* ── System / error messages ─────────────────────────────── */
        ".sys  { color:#858585; font-size:12px; font-style:italic; padding:2px 0 2px 42px;"
        "        margin:1px 0; }"
        ".err  { color:#f14c4c; font-size:12px; padding:6px 10px 6px 42px;"
        "        border-left:2px solid #f14c4c; margin:4px 10px 4px 38px;"
        "        background:rgba(241,76,76,0.06); }"

        /* ── Code blocks ─────────────────────────────────────────── */
        "pre   { background:#1a1a1a; padding:10px 12px; border-radius:0 0 4px 4px;"
        "        font-family:'Cascadia Code','Fira Code',Consolas,monospace; font-size:13px;"
        "        margin:0 0 6px 0; white-space:pre-wrap; }"
        "code  { color:#ce9178; font-family:'Cascadia Code','Fira Code',Consolas,monospace;"
        "        font-size:12px; background:#383838; padding:1px 4px; border-radius:3px; }"
        ".code-header { background:#2d2d30; border-radius:4px 4px 0 0; padding:4px 10px;"
        "              margin:6px 0 0 0; font-size:11px; color:#858585; }"
        ".code-lang { color:#569cd6; font-weight:600; }"
        ".code-header-actions { float:right; }"
        ".code-header-actions a { color:#569cd6; font-size:11px; margin-left:8px; }"
        ".code-actions { color:#6a6a6a; font-size:11px; margin:0 0 8px 0; }"
        ".code-actions a { color:#569cd6; margin-right:10px; }"

        /* ── Links ───────────────────────────────────────────────── */
        "a     { color:#4daafc; text-decoration:none; }"

        /* ── Horizontal rule ─────────────────────────────────────── */
        "hr    { border:none; border-top:1px solid #2d2d2d; margin:8px 10px; }"

        /* ── Blockquotes ─────────────────────────────────────────── */
        "blockquote { border-left:3px solid #007acc; padding:4px 12px; margin:6px 0;"
        "             color:#b0b0b0; background:#1a1a1a; }"

        /* ── Tables ──────────────────────────────────────────────── */
        "table.data { border-collapse:collapse; margin:6px 0; width:100%; }"
        "th    { background:#252526; color:#569cd6; padding:5px 10px; text-align:left;"
        "        font-size:12px; border-bottom:1px solid #3e3e42; }"
        "td    { padding:4px 10px; font-size:12px; border-bottom:1px solid #2d2d2d; }"

        /* ── Lists ───────────────────────────────────────────────── */
        "ol    { padding-left:24px; margin:4px 0; }"
        "ul    { padding-left:20px; margin:4px 0; }"
        "li    { margin:2px 0; }"
        "s     { color:#6a6a6a; }"

        /* ── Message actions (Retry / Edit / Copy) ───────────────── */
        ".msg-actions { margin:4px 0 0 0; font-size:11px; }"
        ".msg-actions a { color:#6a6a6a; text-decoration:none; padding:1px 4px;"
        "                  border-radius:2px; }"

        /* ── Feedback row (thumbs up/down, copy) ─────────────────── */
        ".feedback { margin:4px 0 2px 0; font-size:11px; }"
        ".feedback a { color:#6a6a6a; text-decoration:none; padding:1px 5px;"
        "              border-radius:2px; }"

        /* ── Reference pills ─────────────────────────────────────── */
        ".ref-pill { background:#2d2d30; color:#569cd6; padding:1px 6px; border-radius:2px;"
        "            font-size:12px; text-decoration:none; }"

        /* ── Tool call blocks ────────────────────────────────────── */
        ".tool-call { background:#1e1e1e; border-radius:2px; padding:5px 10px;"
        "             margin:3px 0; font-size:12px; border-left:3px solid #3e3e42; }"
        ".tool-run  { border-left-color:#007acc; color:#9d9d9d; }"
        ".tool-ok   { border-left-color:#4ec9b0; color:#9d9d9d; }"
        ".tool-fail { border-left-color:#f14c4c; color:#f14c4c; }"
        ".tool-name { color:#cccccc; font-weight:600; }"
        ".tool-detail { color:#6a6a6a; font-size:11px; margin-top:2px; }"

        /* ── Thinking blocks ─────────────────────────────────────── */
        ".thinking-block { background:#1a1a2e; border-left:3px solid #5a4fcf;"
        "                   padding:6px 10px; margin:4px 0; border-radius:2px;"
        "                   font-size:12px; color:#9d9dbd; }"
        ".thinking-summary { color:#7a7a9e; font-size:11px; }");

    // ── Changes bar ───────────────────────────────────────────────────────
    m_changesLabel->setStyleSheet("color:#cccccc; font-size:12px;");
    m_keepBtn->setText(tr("Keep"));
    m_keepBtn->setObjectName("keepBtn");
    m_undoBtn->setText(tr("Undo"));
    m_undoBtn->setObjectName("undoBtn");

    auto *changesLayout = new QHBoxLayout(m_changesBar);
    changesLayout->setContentsMargins(10, 4, 10, 4);
    changesLayout->setSpacing(6);
    changesLayout->addWidget(m_changesLabel, 1);
    changesLayout->addWidget(m_keepBtn);
    changesLayout->addWidget(m_undoBtn);
    m_changesBar->setStyleSheet("background:#1a2433; border-top:1px solid #2d2d2d;"
                                "border-radius:0;");
    m_changesBar->hide();

    // ── Working indicator bar ─────────────────────────────────────────────
    m_workingBar   = new QWidget(this);
    m_workingLabel = new QLabel(m_workingBar);
    m_workingTimer = new QTimer(this);
    m_workingTimer->setInterval(400);
    connect(m_workingTimer, &QTimer::timeout, this, [this]() {
        m_workingDots = (m_workingDots + 1) % 4;
        m_workingLabel->setText(tr("Working") + QString(m_workingDots, '.'));
    });
    m_workingLabel->setStyleSheet("color:#858585; font-size:11px; font-style:italic;");

    auto *progressTrack = new QWidget(m_workingBar);
    progressTrack->setFixedHeight(2);
    progressTrack->setStyleSheet("background:#2d2d2d;");

    auto *progressThumb = new QWidget(progressTrack);
    progressThumb->setFixedHeight(2);
    progressThumb->setStyleSheet("background:#007acc; border-radius:1px;");
    progressThumb->setGeometry(0, 0, 80, 2);

    // Shimmer animation for progress bar
    auto *shimmerTimer = new QTimer(m_workingBar);
    shimmerTimer->setObjectName(QStringLiteral("shimmer"));
    shimmerTimer->setInterval(30);
    connect(shimmerTimer, &QTimer::timeout, this, [progressThumb, progressTrack]() {
        const int trackW = progressTrack->width();
        if (trackW <= 0) return;
        int x = progressThumb->x() + 3;
        if (x > trackW) x = -80;
        progressThumb->move(x, 0);
        progressThumb->setFixedWidth(qMin(80, trackW));
    });

    auto *wbl = new QVBoxLayout(m_workingBar);
    wbl->setContentsMargins(0, 0, 0, 0);
    wbl->setSpacing(0);
    wbl->addWidget(progressTrack);
    auto *labelRow = new QHBoxLayout;
    labelRow->setContentsMargins(42, 3, 10, 3);
    labelRow->addWidget(m_workingLabel);
    labelRow->addStretch();
    wbl->addLayout(labelRow);
    m_workingBar->setStyleSheet("background:#1e1e1e;");
    m_workingBar->hide();

    // ── Attachment chips bar ──────────────────────────────────────────────
    m_attachBar    = new QWidget(this);
    m_attachLayout = new QHBoxLayout(m_attachBar);
    m_attachLayout->setContentsMargins(8, 4, 8, 0);
    m_attachLayout->setSpacing(4);
    m_attachLayout->addStretch();
    m_attachBar->setStyleSheet("background:#1e1e1e;");
    m_attachBar->hide();

    // ── Input row ─────────────────────────────────────────────────────────
    m_input->setPlaceholderText(tr("Ask Copilot or type / for commands"));
    m_input->setMinimumHeight(60);
    m_input->setMaximumHeight(180);
    m_input->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_input->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_input->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_input->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    // Auto-resize as text grows
    connect(m_input->document(), &QTextDocument::contentsChanged, this, [this]() {
        const int docH = int(m_input->document()->size().height()) + 12;
        m_input->setFixedHeight(qBound(60, docH, 180));
    });
    m_input->installEventFilter(this);  // intercept Enter key
    setAcceptDrops(true);               // enable file/image drop onto panel

    m_sendBtn->setText(tr("↑"));
    m_sendBtn->setObjectName("sendBtn");
    m_sendBtn->setToolTip(tr("Send  (Enter)"));
    m_cancelBtn->setText(tr("✕"));
    m_cancelBtn->setObjectName("cancelBtn");
    m_cancelBtn->setToolTip(tr("Cancel request"));
    m_cancelBtn->setEnabled(false);

    m_attachBtn->setText(QStringLiteral("⊕"));
    m_attachBtn->setObjectName("attachBtn");
    m_attachBtn->setToolTip(tr("Attach file or image"));
    m_attachBtn->setStyleSheet(
        "QToolButton#attachBtn { color:#858585; font-size:14px; padding:2px 5px;"
        "  background:transparent; border:none; border-radius:3px; }"
        "QToolButton#attachBtn:hover { background:#2d2d2d; color:#cccccc; }");
    connect(m_attachBtn, &QToolButton::clicked, this, [this]() {
        const QStringList paths = QFileDialog::getOpenFileNames(
            this, tr("Attach files"),  {},
            tr("All files (*);;Images (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"));
        for (const QString &p : paths)
            addAttachment(p);
    });

    auto *inputBtns = new QVBoxLayout;
    inputBtns->setSpacing(2);
    inputBtns->setContentsMargins(0,0,0,0);
    inputBtns->addWidget(m_sendBtn);
    inputBtns->addWidget(m_cancelBtn);

    auto *inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(4, 6, 4, 2);
    inputRow->setSpacing(4);
    inputRow->addWidget(m_attachBtn);
    inputRow->addWidget(m_input, 1);
    inputRow->addLayout(inputBtns);

    // ── Mode pill buttons (Ask / Edit / Agent) ────────────────────────────
    m_askBtn->setText(tr("Ask"));
    m_editBtn->setText(tr("Edit"));
    m_agentBtn->setText(tr("Agent"));
    m_askBtn->setToolTip(tr("Ask — chat about code"));
    m_editBtn->setToolTip(tr("Edit — safe targeted changes"));
    m_agentBtn->setToolTip(tr("Agent — full tool autonomy"));

    auto *modeContainer = new QWidget(this);
    modeContainer->setObjectName("modeContainer");
    modeContainer->setStyleSheet(
        "QWidget#modeContainer { background:#252526; border:1px solid #3e3e42;"
        "  border-radius:4px; }");
    auto *modeLayout = new QHBoxLayout(modeContainer);
    modeLayout->setContentsMargins(1, 1, 1, 1);
    modeLayout->setSpacing(0);
    modeLayout->addWidget(m_askBtn);
    modeLayout->addWidget(m_editBtn);
    modeLayout->addWidget(m_agentBtn);

    // ── Bottom bar — mode / model / new session ───────────────────────────
    m_newSessionBtn->setText(QStringLiteral("+"));
    m_newSessionBtn->setObjectName("newSessionBtn");
    m_newSessionBtn->setToolTip(tr("New session (clears conversation)"));
    connect(m_newSessionBtn, &QToolButton::clicked, this, [this]() {
        m_conversationHistory.clear();
        m_pendingRequestId.clear();
        m_pendingAccum.clear();
        m_pendingPatches.clear();
        m_msgQueue.clear();
        m_streamStarted   = false;
        m_streamAnchorPos = 0;
        m_history->clear();
        m_askSessionId.clear();
        m_userMessages.clear();
        m_aiMessages.clear();
        m_userMsgCount = 0;
        m_messageCount = 0;
        hideChangesBar();
        updateContextInfo();
        showEmptyState();
    });

    m_historyBtn->setText(QStringLiteral("\u2630"));
    m_historyBtn->setObjectName("historyBtn");
    m_historyBtn->setToolTip(tr("Session history"));
    connect(m_historyBtn, &QToolButton::clicked, this, &AgentChatPanel::showSessionHistory);

    // ── Keep / Undo for proposed patches ─────────────────────────────────
    connect(m_keepBtn, &QToolButton::clicked, this, [this]() {
        // Accept current state — patches already rendered, nothing to undo
        m_pendingPatches.clear();
        hideChangesBar();
        appendMessage("system", tr("Changes accepted."));
    });
    connect(m_undoBtn, &QToolButton::clicked, this, [this]() {
        if (m_pendingPatches.isEmpty()) {
            hideChangesBar();
            return;
        }
        // Restore file snapshots via session (if available)
        if (m_agentController) {
            const auto *session = m_agentController->session();
            if (session) {
                const auto &snaps = session->fileSnapshots();
                bool anyRestored = false;
                for (auto it = snaps.begin(); it != snaps.end(); ++it) {
                    QFile f(it.key());
                    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                        f.write(it.value().toUtf8());
                        anyRestored = true;
                    }
                }
                if (anyRestored)
                    appendMessage("system", tr("Changes reverted to state before agent run."));
                else
                    appendMessage("system", tr("No file snapshots available for undo."));
            }
        }
        m_pendingPatches.clear();
        hideChangesBar();
    });

    m_modelCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_modelCombo->setToolTip(tr("Select model"));

    m_ctxBtn->setText(QStringLiteral("{ }"));
    m_ctxBtn->setObjectName("ctxBtn");
    m_ctxBtn->setToolTip(tr("Context window usage"));
    m_ctxBtn->setStyleSheet(
        "QToolButton#ctxBtn { color:#858585; font-size:11px; font-family:monospace;"
        "  padding:2px 5px; background:transparent; border:none; border-radius:2px; }"
        "QToolButton#ctxBtn:hover { background:#2a2d2e; color:#cccccc; }"
        "QToolButton#ctxBtn:checked { background:#2a2d2e; color:#cccccc; }");
    m_ctxBtn->setCheckable(true);
    connect(m_ctxBtn, &QToolButton::toggled, this, [this](bool on) {
        if (on) showContextPopup(); else hideContextPopup();
    });

    auto *bottomBar = new QHBoxLayout;
    bottomBar->setContentsMargins(6, 2, 6, 4);
    bottomBar->setSpacing(4);
    bottomBar->addWidget(modeContainer);
    bottomBar->addWidget(m_modelCombo, 1);
    bottomBar->addWidget(m_ctxBtn);
    bottomBar->addWidget(m_historyBtn);
    bottomBar->addWidget(m_newSessionBtn);

    // ── Unified input container (VS Code style) ──────────────────────────
    auto *inputBlock = new QWidget(this);
    inputBlock->setObjectName("inputBlock");
    inputBlock->setStyleSheet(
        "QWidget#inputBlock { background:#2b2b2b; border:1px solid #3e3e42;"
        "  border-radius:8px; margin:4px 8px 8px 8px; }"
        "QWidget#inputBlock QPlainTextEdit {"
        "  background:transparent; border:none; border-radius:0; }");
    auto *inputBlockLayout = new QVBoxLayout(inputBlock);
    inputBlockLayout->setContentsMargins(0, 0, 0, 0);
    inputBlockLayout->setSpacing(0);
    inputBlockLayout->addLayout(inputRow);
    inputBlockLayout->addLayout(bottomBar);

    // ── Context info strip ────────────────────────────────────────────────
    m_contextStrip->setStyleSheet(
        "color:#858585; font-size:11px; padding:1px 10px; background:#1f1f1f;");
    m_contextStrip->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_contextStrip->setFixedHeight(16);
    m_contextStrip->hide();

    // ── Separators ────────────────────────────────────────────────────────
    auto makeSep = [this]() {
        auto *s = new QFrame(this);
        s->setFrameShape(QFrame::HLine);
        s->setFixedHeight(1);
        s->setStyleSheet("color:#2d2d2d; background:#2d2d2d; border:none;");
        return s;
    };

    // ── Root layout ───────────────────────────────────────────────────────
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_providerTabBar);
    root->addWidget(makeSep());
    root->addWidget(m_history, 1);
    root->addWidget(m_workingBar);
    root->addWidget(m_changesBar);
    root->addWidget(makeSep());
    root->addWidget(m_contextStrip);
    root->addWidget(m_attachBar);
    root->addWidget(inputBlock);

    // ── Slash-command autocomplete popup ──────────────────────────────────
    m_slashMenu->setWindowFlags(Qt::ToolTip);
    m_slashMenu->setFocusPolicy(Qt::NoFocus);
    m_slashMenu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_slashMenu->setFixedWidth(300);
    m_slashMenu->addItem(tr("/explain  — Explain selected code"));
    m_slashMenu->addItem(tr("/fix      — Find and fix bugs"));
    m_slashMenu->addItem(tr("/fixtest  — Fix failing tests"));
    m_slashMenu->addItem(tr("/test     — Generate unit tests"));
    m_slashMenu->addItem(tr("/doc      — Add documentation comments"));
    m_slashMenu->addItem(tr("/review   — Code review and suggestions"));
    m_slashMenu->addItem(tr("/commit   — Suggest a commit message"));
    m_slashMenu->addItem(tr("/new      — Start a new chat session"));
    m_slashMenu->addItem(tr("/compact  — Summarize and compact context"));
    m_slashMenu->addItem(tr("/resolve  — Resolve merge conflicts with AI"));
    m_slashMenu->addItem(tr("/changes  — Review git changes"));
    m_staticSlashCount = m_slashMenu->count();
    m_slashMenu->hide();

    // ── @-mention / #-file autocomplete popup ─────────────────────────────
    m_mentionMenu->setWindowFlags(Qt::ToolTip);
    m_mentionMenu->setFocusPolicy(Qt::NoFocus);
    m_mentionMenu->setSelectionMode(QAbstractItemView::SingleSelection);
    m_mentionMenu->setFixedWidth(340);
    m_mentionMenu->hide();
    connect(m_mentionMenu, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        // Replace the @-mention / #-reference with the selected file path
        const QString filePath = item->data(Qt::UserRole).toString();
        QTextCursor tc = m_input->textCursor();
        const QString text = m_input->toPlainText();
        // Find the trigger character position (search backwards from cursor)
        int triggerPos = tc.position() - 1;
        while (triggerPos >= 0 && text[triggerPos] != QLatin1Char('@') &&
               text[triggerPos] != QLatin1Char('#'))
            --triggerPos;
        if (triggerPos >= 0) {
            tc.setPosition(triggerPos);
            tc.setPosition(tc.position() + (tc.position() - triggerPos) + text.mid(triggerPos).indexOf(' '),
                           QTextCursor::MoveAnchor);
            // Select from trigger to current cursor
            tc.setPosition(triggerPos);
            tc.movePosition(QTextCursor::End, QTextCursor::KeepAnchor); // select to end for simple case
            const QString after = text.mid(m_input->textCursor().position());
            tc.setPosition(triggerPos);
            tc.setPosition(m_input->textCursor().position(), QTextCursor::KeepAnchor);
            if (m_mentionTrigger == QLatin1String("#")) {
                tc.insertText(QStringLiteral("#%1 ").arg(filePath));
                // Also auto-attach the file
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

    // ── Connections — UI ──────────────────────────────────────────────────
    connect(m_sendBtn,   &QToolButton::clicked, this, &AgentChatPanel::onSend);
    connect(m_cancelBtn, &QToolButton::clicked, this, &AgentChatPanel::onCancel);
    // Enter key handled in eventFilter (Shift+Enter = newline, Enter = send)
    connect(m_modelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentChatPanel::onModelSelected);
    connect(m_askBtn,   &QToolButton::clicked, this, [this]() { onModeSelected(0); });
    connect(m_editBtn,  &QToolButton::clicked, this, [this]() { onModeSelected(1); });
    connect(m_agentBtn, &QToolButton::clicked, this, [this]() { onModeSelected(2); });

    connect(m_input->document(), &QTextDocument::contentsChanged, this, [this]() {
        const QString text = m_input->toPlainText();
        if (text.startsWith('/') && !text.contains(' ') && !text.contains('\n'))
            showSlashMenu();
        else
            hideSlashMenu();

        // Detect @-mention trigger: find @ before cursor
        const int cursorPos = m_input->textCursor().position();
        int atPos = text.lastIndexOf('@', cursorPos - 1);
        if (atPos >= 0 && (atPos == 0 || text[atPos - 1].isSpace())) {
            const QString filter = text.mid(atPos + 1, cursorPos - atPos - 1);
            if (!filter.contains(' ') && !filter.contains('\n'))
                showMentionMenu(QStringLiteral("@"), filter);
            else
                hideMentionMenu();
        } else {
            // Detect #-file trigger: find # before cursor
            int hashPos = text.lastIndexOf('#', cursorPos - 1);
            if (hashPos >= 0 && (hashPos == 0 || text[hashPos - 1].isSpace())) {
                const QString filter = text.mid(hashPos + 1, cursorPos - hashPos - 1);
                if (!filter.contains(' ') && !filter.contains('\n'))
                    showMentionMenu(QStringLiteral("#"), filter);
                else
                    hideMentionMenu();
            } else {
                hideMentionMenu();
            }
        }
    });
    connect(m_slashMenu, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString cmd = item->text().section(' ', 0, 0); // e.g. "/explain"
        m_input->setPlainText(cmd + ' ');
        m_input->moveCursor(QTextCursor::End);
        hideSlashMenu();
        m_input->setFocus();
    });

    // ── Connections — orchestrator ────────────────────────────────────────
    connect(m_orchestrator, &AgentOrchestrator::providerRegistered,
            this, &AgentChatPanel::onProviderRegistered);
    connect(m_orchestrator, &AgentOrchestrator::providerRemoved,
            this, &AgentChatPanel::onProviderRemoved);
    connect(m_orchestrator, &AgentOrchestrator::activeProviderChanged,
            this, &AgentChatPanel::onActiveProviderChanged);
    connect(m_orchestrator, &AgentOrchestrator::responseDelta,
            this, &AgentChatPanel::onResponseDelta);
    connect(m_orchestrator, &AgentOrchestrator::thinkingDelta,
            this, &AgentChatPanel::onThinkingDelta);
    connect(m_orchestrator, &AgentOrchestrator::responseFinished,
            this, &AgentChatPanel::onResponseFinished);
    connect(m_orchestrator, &AgentOrchestrator::responseError,
            this, &AgentChatPanel::onResponseError);
    connect(m_orchestrator, &AgentOrchestrator::modelsChanged,
            this, &AgentChatPanel::refreshModelList);
        connect(m_history, &QTextBrowser::anchorClicked,
            this, &AgentChatPanel::onAnchorClicked);

    refreshProviderList();
    refreshModelList();
    setModeButtonActive(0);
    onModeSelected(0);
}

// ── Agent controller ──────────────────────────────────────────────────────────

void AgentChatPanel::setAgentController(AgentController *controller)
{
    m_agentController = controller;
    if (!m_agentController)
        return;

    // Wire agent controller signals for tool call feedback
    connect(m_agentController, &AgentController::toolCallStarted,
            this, [this](const QString &toolName, const QJsonObject &args) {
        // Lock in any in-progress streaming bubble
        if (m_streamStarted) {
            const auto rendered = MarkdownRenderer::toHtmlWithActions(m_pendingAccum);
            finalizeStreamingBubble(m_pendingAccum, rendered.html);
            m_pendingAccum.clear();
            m_streamStarted   = false;
            m_streamAnchorPos = 0;
        }

        // Lock in any live thinking bubble
        if (m_thinkingStreamStarted)
            finalizeThinkingBubble();

        // Start working section if not already active
        if (!m_workingSectionActive) {
            m_workingSectionActive = true;
            QTextCursor temp(m_history->document());
            temp.movePosition(QTextCursor::End);
            m_workingSectionAnchor = temp.position();
        }

        // Build human-readable description
        const QString path = args[QLatin1String("filePath")].toString();
        const QString fn   = path.isEmpty() ? QString() : QFileInfo(path).fileName();
        QString desc;
        QString icon = QStringLiteral("&#9654;"); // default: right triangle
        if (toolName.contains(QLatin1String("grep")) || toolName.contains(QLatin1String("search"))) {
            icon = QStringLiteral("Q");
            const QString q = args[QLatin1String("query")].toString();
            desc = q.isEmpty() ? QStringLiteral("Searched workspace")
                : QStringLiteral("Searched for text <span style='background:#2d2d30;padding:0 4px;border-radius:2px;color:#ce9178;'>%1</span>").arg(q.left(50).toHtmlEscaped());
        } else if (toolName.contains(QLatin1String("read"))) {
            icon = QStringLiteral("&#9776;"); // trigram
            const int s = args[QLatin1String("startLine")].toInt();
            const int e = args[QLatin1String("endLine")].toInt();
            if (!fn.isEmpty() && s > 0 && e > 0)
                desc = QStringLiteral("Read <span style='background:#2d2d30;padding:0 4px;border-radius:2px;color:#4ec9b0;'>%1</span>, lines %2 to %3").arg(fn.toHtmlEscaped()).arg(s).arg(e);
            else if (!fn.isEmpty())
                desc = QStringLiteral("Read <span style='background:#2d2d30;padding:0 4px;border-radius:2px;color:#4ec9b0;'>%1</span>").arg(fn.toHtmlEscaped());
            else
                desc = QStringLiteral("Read file");
        } else if (toolName.contains(QLatin1String("list"))) {
            icon = QStringLiteral("&#9776;");
            const QString d = args[QLatin1String("path")].toString();
            desc = d.isEmpty() ? QStringLiteral("Listed directory")
                : QStringLiteral("Listed <span style='background:#2d2d30;padding:0 4px;border-radius:2px;color:#4ec9b0;'>%1</span>").arg(QFileInfo(d).fileName().toHtmlEscaped());
        } else if (toolName.contains(QLatin1String("create"))) {
            icon = QStringLiteral("+");
            desc = fn.isEmpty() ? QStringLiteral("Created file")
                : QStringLiteral("Created <span style='background:#2d2d30;padding:0 4px;border-radius:2px;color:#4ec9b0;'>%1</span>").arg(fn.toHtmlEscaped());
        } else if (toolName.contains(QLatin1String("replace")) || toolName.contains(QLatin1String("edit"))) {
            icon = QStringLiteral("&#9998;"); // pencil
            desc = fn.isEmpty() ? QStringLiteral("Edited file")
                : QStringLiteral("Edited <span style='background:#2d2d30;padding:0 4px;border-radius:2px;color:#4ec9b0;'>%1</span>").arg(fn.toHtmlEscaped());
        } else if (toolName.contains(QLatin1String("terminal")) || toolName.contains(QLatin1String("run"))) {
            icon = QStringLiteral("&gt;");
            const QString cmd = args[QLatin1String("command")].toString().left(50);
            desc = cmd.isEmpty() ? QStringLiteral("Ran command")
                : QStringLiteral("Ran <span style='background:#2d2d30;padding:0 4px;border-radius:2px;color:#ce9178;'>%1</span>").arg(cmd.toHtmlEscaped());
        } else {
            // Fallback: tool name + first meaningful arg
            for (const auto &k : {QLatin1String("path"), QLatin1String("query"), QLatin1String("command")}) {
                const QString v = args[k].toString();
                if (!v.isEmpty()) {
                    desc = QStringLiteral("%1: %2").arg(toolName.toHtmlEscaped(), v.left(50).toHtmlEscaped());
                    break;
                }
            }
            if (desc.isEmpty()) desc = toolName.toHtmlEscaped();
        }

        WorkingStep step;
        step.toolName = toolName;
        step.summary  = icon + QStringLiteral(" ") + desc;
        m_workingSteps.append(step);
        updateWorkingSection();
        showWorkingIndicator(tr("Running %1\u2026").arg(toolName));
    });

    connect(m_agentController, &AgentController::toolCallFinished,
            this, [this](const QString &toolName, const ToolExecResult &result) {
        // Find the last unfinished step for this tool and mark it done
        for (int i = m_workingSteps.size() - 1; i >= 0; --i) {
            auto &step = m_workingSteps[i];
            if (step.toolName == toolName && !step.finished) {
                step.finished = true;
                step.success  = result.ok;
                if (!result.ok) {
                    step.result = result.error.left(60);
                } else if (toolName.contains(QLatin1String("grep"))
                        || toolName.contains(QLatin1String("search"))) {
                    const int count = result.textContent.count(QLatin1Char('\n'));
                    if (count > 0) step.result = tr("%1 results").arg(count);
                }
                break;
            }
        }
        updateWorkingSection();
        showWorkingIndicator(tr("Analyzing\u2026"));
    });

    connect(m_agentController, &AgentController::patchProposed,
            this, [this](const PatchProposal &patch) {
        m_pendingPatches.append(patch);
        showChangesBar(m_pendingPatches.size());
        appendMessage("system",
            tr("Patch proposed: %1 — %2")
                .arg(QFileInfo(patch.filePath).fileName(), patch.description));
    });

    // Tool confirmation dialog for dangerous operations
    m_agentController->setToolConfirmationCallback(
        [this](const QString &toolName, const QJsonObject &args,
               const QString &desc) -> AgentController::ToolApproval {
            const QString argSumm = [&]() -> QString {
                for (const QString &k : {"command", "path", "url"}) {
                    const QString v = args[k].toString();
                    if (!v.isEmpty()) return v;
                }
                return QJsonDocument(args).toJson(QJsonDocument::Compact);
            }();
            QMessageBox box(this);
            box.setWindowTitle(tr("Tool Confirmation"));
            box.setTextFormat(Qt::RichText);
            box.setText(QStringLiteral("<b>%1</b><br><br>%2<br><br><code>%3</code>")
                .arg(toolName.toHtmlEscaped(), desc.toHtmlEscaped(),
                     argSumm.left(200).toHtmlEscaped()));
            auto *allowBtn  = box.addButton(tr("Allow"), QMessageBox::AcceptRole);
            auto *alwaysBtn = box.addButton(tr("Always Allow"), QMessageBox::YesRole);
            box.addButton(tr("Deny"), QMessageBox::RejectRole);
            box.setDefaultButton(allowBtn);
            box.exec();
            if (box.clickedButton() == alwaysBtn)
                return AgentController::ToolApproval::AllowAlways;
            if (box.clickedButton() == allowBtn)
                return AgentController::ToolApproval::AllowOnce;
            return AgentController::ToolApproval::Deny;
        });

    onModeSelected(m_currentMode);
}

void AgentChatPanel::setVariableResolver(PromptVariableResolver *resolver)
{
    m_varResolver = resolver;
}

void AgentChatPanel::setInsertAtCursorCallback(std::function<void(const QString &)> cb)
{
    m_insertAtCursorFn = std::move(cb);
}

void AgentChatPanel::setRunInTerminalCallback(std::function<void(const QString &)> cb)
{
    m_runInTerminalFn = std::move(cb);
}

void AgentChatPanel::setSessionStore(SessionStore *store)
{
    m_sessionStore = store;
    if (!m_sessionStore)
        return;

    // Clear any startup messages (e.g. "No AI providers") before restoring
    m_history->clear();

    // Restore the last saved session into the chat view
    const SessionStore::ChatSession lastSession = m_sessionStore->loadLastSession();
    if (lastSession.isEmpty()) {
        showEmptyState();
        return;
    }

    // Header showing when the session was created
    const QString when = lastSession.createdAt.isValid()
        ? lastSession.createdAt.toLocalTime().toString(QStringLiteral("MMM d, hh:mm"))
        : tr("previous session");

    appendMessage("system",
        tr("─────  Restored session from %1  ─────").arg(when));

    for (const auto &[role, text] : lastSession.messages) {
        if (role == QLatin1String("user")) {
            appendMessage("user", text.toHtmlEscaped());
            m_conversationHistory.append({AgentMessage::Role::User, text});
        } else if (role == QLatin1String("assistant")) {
            const auto rendered = MarkdownRenderer::toHtmlWithActions(text);
            m_lastBlocks = rendered.codeBlocks;
            appendMessage("ai", rendered.html);
            m_conversationHistory.append({AgentMessage::Role::Assistant, text});
        }
    }

    // Reuse the restored session ID for continued Ask-mode saving
    m_askSessionId = lastSession.sessionId;

    appendMessage("system", tr("─────  New messages below  ─────"));
}

// ── Provider list ─────────────────────────────────────────────────────────────

void AgentChatPanel::refreshProviderList()
{
    // Remove all old tab buttons
    while (QLayoutItem *item = m_providerTabLayout->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    m_providerTabs.clear();

    const auto list = m_orchestrator->providers();
    if (list.isEmpty()) {
        auto *tab = new QToolButton(m_providerTabBar);
        tab->setText(tr("No providers"));
        tab->setEnabled(false);
        tab->setStyleSheet(
            "QToolButton { color:#555555; border:none; border-bottom:2px solid transparent;"
            " background:transparent; padding:0 14px; height:35px; font-size:12px; border-radius:0; }");
        m_providerTabLayout->addWidget(tab);
        m_providerTabs.append(tab);
        appendMessage("system", tr("No AI providers registered. "
                                   "Install an agent plugin to get started."));
    } else {
        for (const IAgentProvider *p : list) {
            auto *tab = new QToolButton(m_providerTabBar);
            tab->setText(p->displayName());
            const QString pid = p->id();
            connect(tab, &QToolButton::clicked, this, [this, pid]() {
                m_orchestrator->setActiveProvider(pid);
                refreshModelList();
            });
            m_providerTabLayout->addWidget(tab);
            m_providerTabs.append(tab);
        }
    }
    m_providerTabLayout->addStretch();

    const IAgentProvider *active = m_orchestrator->activeProvider();
    if (active)
        setProviderTabActive(active->id());
}

void AgentChatPanel::refreshModelList()
{
    QSignalBlocker blocker(m_modelCombo);
    m_modelCombo->clear();

    IAgentProvider *active = m_orchestrator->activeProvider();
    if (!active) {
        m_modelCombo->addItem(tr("(no models)"));
        m_modelCombo->setEnabled(false);
        return;
    }

    m_modelCombo->setEnabled(true);
    const QStringList models = active->availableModels();
    for (const QString &model : models)
        m_modelCombo->addItem(model);

    // Select the current model
    const int idx = m_modelCombo->findText(active->currentModel());
    if (idx >= 0)
        m_modelCombo->setCurrentIndex(idx);
}

void AgentChatPanel::setProviderTabActive(const QString &id)
{
    const auto list = m_orchestrator->providers();
    for (int i = 0; i < m_providerTabs.size() && i < list.size(); ++i) {
        const bool active = (list[i]->id() == id);
        if (active) {
            m_providerTabs[i]->setStyleSheet(
                "QToolButton { color:#e8e8e8; border:none; border-bottom:2px solid #007acc;"
                " background:transparent; padding:0 14px; height:35px; font-size:12px; border-radius:0; }");
        } else {
            m_providerTabs[i]->setStyleSheet(
                "QToolButton { color:#858585; border:none; border-bottom:2px solid transparent;"
                " background:transparent; padding:0 14px; height:35px; font-size:12px; border-radius:0; }"
                "QToolButton:hover { color:#cccccc; background:#2a2d2e; }");
        }
    }
}

void AgentChatPanel::setModeButtonActive(int mode)
{
    m_currentMode = mode;
    const auto style = [](bool active) -> QString {
        if (active)
            return QStringLiteral(
                "QToolButton { background:#007acc; color:#ffffff; border:none;"
                " padding:2px 10px; font-size:12px; border-radius:3px; }");
        return QStringLiteral(
            "QToolButton { background:transparent; color:#858585; border:none;"
            " padding:2px 10px; font-size:12px; border-radius:3px; }"
            "QToolButton:hover { color:#cccccc; background:#2a2d2e; }");
    };
    m_askBtn->setStyleSheet(style(mode == 0));
    m_editBtn->setStyleSheet(style(mode == 1));
    m_agentBtn->setStyleSheet(style(mode == 2));
}

// ── Slot implementations ──────────────────────────────────────────────────────

void AgentChatPanel::onProviderRegistered(const QString &)
{
    refreshProviderList();
}

void AgentChatPanel::onProviderRemoved(const QString &)
{
    refreshProviderList();
}

void AgentChatPanel::onActiveProviderChanged(const QString &id)
{
    refreshModelList();
    setProviderTabActive(id);
}

void AgentChatPanel::onModelSelected(int index)
{
    IAgentProvider *active = m_orchestrator->activeProvider();
    if (!active || index < 0)
        return;
    const QString modelId = m_modelCombo->itemText(index);
    active->setModel(modelId);
    m_perModeModel[m_currentMode] = modelId;
}

void AgentChatPanel::onModeSelected(int mode)
{
    // Save current model preference for the old mode
    if (IAgentProvider *active = m_orchestrator->activeProvider()) {
        m_perModeModel[m_currentMode] = active->currentModel();
    }

    setModeButtonActive(mode);
    // When switching away from Ask mode, close the current ask session
    // so the next Ask-mode send starts a fresh one.
    if (mode != 0)
        m_askSessionId.clear();

    // Restore per-mode model preference
    if (IAgentProvider *active = m_orchestrator->activeProvider()) {
        const QString preferred = m_perModeModel.value(mode);
        if (!preferred.isEmpty() && preferred != active->currentModel()) {
            active->setModel(preferred);
            refreshModelList();
        }
    }

    if (!m_agentController)
        return;

    m_agentController->setSystemPrompt(AgentModes::systemPromptForMode(mode));
    m_agentController->setMaxToolPermission(AgentModes::maxPermissionForMode(mode));
}

void AgentChatPanel::onSend()
{
    const QString raw = m_input->toPlainText().trimmed();
    if (raw.isEmpty() && m_attachments.isEmpty())
        return;

    // Push to input history for Up/Down navigation
    if (!raw.isEmpty()) {
        m_inputHistory.append(raw);
        m_historyIndex = -1;
    }

    hideSlashMenu();
    hideMentionMenu();
// Handle /new as a special command (not sent to model)
    if (raw.startsWith(QLatin1String("/new"))) {
        m_input->clear();
        m_newSessionBtn->click(); // trigger new session
        return;
    }

    
    const IAgentProvider *active = m_orchestrator->activeProvider();
    if (!active || !active->isAvailable()) {
        appendMessage("error", tr("No available provider. "
                                  "Select or configure a provider first."));
        return;
    }

    // If a request is already in flight, queue this message
    if (!m_pendingRequestId.isEmpty()) {
        m_msgQueue.enqueue(raw);
        m_input->clear();
        const int queued = m_msgQueue.size();
        appendMessage("system",
            tr("Message queued (%1 pending). Cancel the current request to skip.")
                .arg(queued));
        return;
    }

    QString expandedPrompt = raw;
    if (m_varResolver) {
        QStringList unresolved;
        expandedPrompt = m_varResolver->resolveInPrompt(raw, &unresolved);
        if (!unresolved.isEmpty()) {
            appendMessage("system",
                          tr("Unknown variables: %1")
                            .arg(unresolved.join(QStringLiteral(", "))));
        }
    }

    // Resolve slash commands → intent + effective prompt
    AgentIntent intent = AgentIntent::Chat;
    const QString text = resolveSlashCommand(expandedPrompt, intent);

    // Show attachments in the user bubble
    QString attachHtml;
    for (const auto &att : std::as_const(m_attachments)) {
        attachHtml += QStringLiteral(
            "<div style='display:inline-block;background:#252526;border:1px solid #3e3e42;"
            "border-radius:3px;padding:2px 7px;margin:2px 2px 0 0;font-size:11px;"
            "color:#cccccc;'>&#128206; %1</div>").arg(att.name.toHtmlEscaped());
    }
    appendMessage("user", raw.toHtmlEscaped()
                  + (attachHtml.isEmpty() ? QString() : QStringLiteral("<br>") + attachHtml));

    // Store raw text for retry/edit
    m_userMessages.append(raw);

    // Build attachment context to inject into the model request
    QString attachContext;
    for (const auto &att : std::as_const(m_attachments)) {
        if (!att.isImage && !att.path.isEmpty()) {
            QFile f(att.path);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString content = QString::fromUtf8(f.readAll());
                attachContext += QStringLiteral("\n\n--- Attached file: %1 ---\n%2\n--- End of %1 ---\n")
                    .arg(att.name, content);
            }
        }
    }

    m_attachments.clear();
    // Clear chips
    while (QLayoutItem *item = m_attachLayout->takeAt(0)) {
        delete item->widget(); delete item;
    }
    m_attachLayout->addStretch();
    m_attachBar->hide();

    m_input->clear();
    setInputEnabled(false);
    showWorkingIndicator(tr("Sending request\u2026"));

    // Effective prompt = user text + any attached file contents
    const QString effectiveText = attachContext.isEmpty() ? text : (text + attachContext);

    // Record user message in conversation history
    m_conversationHistory.append({AgentMessage::Role::User, effectiveText});

    m_pendingRequestId  = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingAccum.clear();
    m_pendingIntent     = intent;
    m_streamStarted   = false;  // streaming hasn't started yet

    const bool agentMode = AgentModes::usesAgentLoop(m_currentMode);

    // Save user message in Ask mode (agent mode saves via AgentController)
    if (!agentMode && m_sessionStore) {
        if (m_askSessionId.isEmpty()) {
            m_askSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const IAgentProvider *prov = m_orchestrator->activeProvider();
            m_sessionStore->startSession(m_askSessionId,
                prov ? prov->id()           : QString{},
                prov ? prov->currentModel() : QString{},
                false);
        }
        m_sessionStore->appendUserMessage(m_askSessionId, effectiveText);
    }

    // In agent mode, delegate to the AgentController for the full loop
    if (agentMode && m_agentController) {
        m_agentController->newSession(true);

        // Wire streaming for this turn
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

            const QString responseText = m_pendingAccum.isEmpty()
                ? (turn.steps.isEmpty() ? QString() : turn.steps.last().finalText)
                : m_pendingAccum;

            AgentResponse resp;
            resp.requestId = m_pendingRequestId;
            resp.text = responseText;
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

        m_agentController->sendMessage(effectiveText, m_activeFilePath,
                                       m_selectedText, m_languageId, intent);
        return;
    }

    // Standard ask mode — direct orchestrator request
    AgentRequest req;
    req.requestId           = m_pendingRequestId;
    req.intent              = intent;
    req.agentMode           = agentMode;
    req.userPrompt          = effectiveText;
    req.activeFilePath      = m_activeFilePath;
    req.selectedText        = m_selectedText;
    req.languageId          = m_languageId;
    req.conversationHistory = m_conversationHistory;

    m_orchestrator->sendRequest(req);
}

void AgentChatPanel::onCancel()
{
    if (!m_pendingRequestId.isEmpty()) {
        m_orchestrator->cancelRequest(m_pendingRequestId);
        m_pendingRequestId.clear();
        hideWorkingIndicator();
        setInputEnabled(true);
        appendMessage("system", tr("Request cancelled."));
    }
}

void AgentChatPanel::onResponseDelta(const QString &requestId, const QString &chunk)
{
    if (requestId != m_pendingRequestId)
        return;

    m_pendingAccum += chunk;

    // Finalize any live thinking bubble before starting the response stream
    if (m_thinkingStreamStarted)
        finalizeThinkingBubble();

    // Finalize working section before response stream
    if (m_workingSectionActive)
        finalizeWorkingSection();

    // Lazily record where streaming content should start BEFORE hiding the
    // working bar (which triggers a layout resize that could affect positions).
    if (!m_streamStarted) {
        m_streamStarted = true;
        QTextCursor temp(m_history->document());
        temp.movePosition(QTextCursor::End);
        m_streamAnchorPos = temp.position();
    }

    // Now safe to hide working indicator (layout shift happens after anchor is set)
    hideWorkingIndicator();

    updateStreamingContent();
}

void AgentChatPanel::onThinkingDelta(const QString &requestId, const QString &chunk)
{
    if (requestId != m_pendingRequestId)
        return;

    m_thinkingAccum += chunk;

    // Finalize working section if it was active
    if (m_workingSectionActive)
        finalizeWorkingSection();

    // On first thinking chunk, set up the live thinking anchor
    if (!m_thinkingStreamStarted) {
        m_thinkingStreamStarted = true;
        hideWorkingIndicator();
        QTextCursor temp(m_history->document());
        temp.movePosition(QTextCursor::End);
        m_thinkingAnchorPos = temp.position();
    }

    updateThinkingContent();
}

void AgentChatPanel::onResponseFinished(const QString &requestId,
                                        const AgentResponse &response)
{
    if (requestId != m_pendingRequestId)
        return;

    const QString responseText = m_pendingAccum.isEmpty() ? response.text : m_pendingAccum;

    // Finalize any live thinking that's still streaming
    if (m_thinkingStreamStarted)
        finalizeThinkingBubble();

    // Finalize working section
    if (m_workingSectionActive)
        finalizeWorkingSection();

    // Render thinking block only if we have content that WASN'T already streamed live
    const QString thinking = m_thinkingAccum.isEmpty()
                                 ? response.thinkingContent : m_thinkingAccum;
    if (!thinking.isEmpty() && !m_thinkingStreamStarted) {
        const QString escapedThinking = thinking.toHtmlEscaped()
            .replace(QStringLiteral("\n"), QStringLiteral("<br>"));
        m_history->append(
            QStringLiteral("<div class='thinking-block'>"
                           "<div class='thinking-summary'>&#128161; Thinking</div>"
                           "<div style='margin-top:4px;white-space:pre-wrap;'>%1</div>"
                           "</div>").arg(escapedThinking));
    }

    const auto rendered = MarkdownRenderer::toHtmlWithActions(responseText);
    m_lastBlocks = rendered.codeBlocks;

    // Store raw response text for copy button
    m_aiMessages.append(responseText);

    if (!responseText.isEmpty())
        m_conversationHistory.append({AgentMessage::Role::Assistant, responseText});

    // Save assistant message in Ask mode (agent mode saves via AgentController)
    if (!AgentModes::usesAgentLoop(m_currentMode) && m_sessionStore
            && !m_askSessionId.isEmpty() && !responseText.isEmpty()) {
        m_sessionStore->appendAssistantMessage(m_askSessionId, responseText);
    }

    // Replace streaming content (or just append) with fully rendered HTML
    finalizeStreamingBubble(responseText, rendered.html);

    if (!response.proposedEdits.isEmpty()) {
        showChangesBar(response.proposedEdits.size());
    }

    // Parse review annotations from response (for /review intent)
    if (m_pendingIntent == AgentIntent::CodeReview && !m_activeFilePath.isEmpty()) {
        // Look for patterns like "Line 42:" or "L42:" or "line 42 -" in the response
        static const QRegularExpression lineRx(
            QStringLiteral("(?:^|\\n)\\s*(?:Line|L)\\s*(\\d+)\\s*[:\\-]\\s*(.+?)(?=\\n\\s*(?:Line|L)\\s*\\d+|\\n\\n|$)"),
            QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        auto it = lineRx.globalMatch(responseText);
        QList<QPair<int, QString>> annotations;
        while (it.hasNext()) {
            auto match = it.next();
            const int lineNum = match.captured(1).toInt();
            const QString comment = match.captured(2).trimmed();
            if (lineNum > 0 && !comment.isEmpty())
                annotations.append({lineNum, comment});
        }
        if (!annotations.isEmpty())
            emit reviewAnnotationsReady(m_activeFilePath, annotations);
    }

    // Test scaffold: extract code block and create test file
    if (m_pendingIntent == AgentIntent::GenerateTests && !m_activeFilePath.isEmpty()) {
        // Extract suggested file path from response (pattern: "file path: <path>")
        static const QRegularExpression pathRx(
            QStringLiteral("(?:file\\s*path|save\\s+(?:as|to)|suggested\\s+path)[:\\s]+([^\\n]+)"),
            QRegularExpression::CaseInsensitiveOption);
        const auto pathMatch = pathRx.match(responseText);

        // Extract the first code block as the test content
        static const QRegularExpression codeRx(
            QStringLiteral("```[^\\n]*\\n([\\s\\S]*?)```"));
        const auto codeMatch = codeRx.match(responseText);

        if (codeMatch.hasMatch()) {
            const QString testCode = codeMatch.captured(1);

            // Determine test file path
            QString testFilePath;
            if (pathMatch.hasMatch()) {
                testFilePath = pathMatch.captured(1).trimmed();
            } else {
                // Infer from source file
                QFileInfo fi(m_activeFilePath);
                const QString lang = m_languageId.toLower();
                const QString base = fi.completeBaseName();
                QString testName;
                if (lang == QLatin1String("cpp") || lang == QLatin1String("c"))
                    testName = QStringLiteral("test_%1.cpp").arg(base);
                else if (lang == QLatin1String("python"))
                    testName = QStringLiteral("test_%1.py").arg(base);
                else if (lang == QLatin1String("javascript") || lang == QLatin1String("typescript"))
                    testName = QStringLiteral("%1.test.%2").arg(base, fi.suffix());
                else if (lang == QLatin1String("java"))
                    testName = QStringLiteral("%1Test.java").arg(base);
                else
                    testName = QStringLiteral("test_%1.%2").arg(base, fi.suffix());

                QString testDir = fi.absolutePath() + QStringLiteral("/tests");
                if (!QDir(testDir).exists())
                    testDir = fi.absolutePath() + QStringLiteral("/test");
                if (!QDir(testDir).exists())
                    testDir = fi.absolutePath();
                testFilePath = testDir + QStringLiteral("/") + testName;
            }

            // Show "Create Test File" action link
            const QString escapedPath = testFilePath.toHtmlEscaped();
            const QString scaffoldHtml = QStringLiteral(
                "<div style='margin:8px 0;padding:8px 12px;background:#1a3a2a;"
                "border-left:3px solid #4ec97a;border-radius:4px;'>"
                "&#128196; <b>Test file:</b> %1<br>"
                "<a href='action:createTestFile:%2' style='color:#4ec97a;'>"
                "Click to create test file</a>"
                "</div>").arg(escapedPath, QString::fromUtf8(testFilePath.toUtf8().toBase64()));
            m_history->append(scaffoldHtml);

            // Store for action handler
            m_pendingTestFilePath = testFilePath;
            m_pendingTestCode = testCode;
        }
    }

    m_pendingRequestId.clear();
    m_pendingAccum.clear();
    m_thinkingAccum.clear();
    m_streamStarted           = false;
    m_streamAnchorPos         = 0;
    m_thinkingStreamStarted   = false;
    m_thinkingAnchorPos       = 0;
    m_workingSectionActive    = false;
    m_workingSectionAnchor    = 0;
    m_workingSteps.clear();
    hideWorkingIndicator();
    setInputEnabled(true);
    checkQueue();
}

void AgentChatPanel::onResponseError(const QString &requestId,
                                     const AgentError &error)
{
    if (requestId != m_pendingRequestId)
        return;

    // Build error-specific HTML with recovery suggestions
    QString errHtml;
    switch (error.code) {
    case AgentError::Code::AuthError:
        errHtml = QStringLiteral(
            "<div class='err'>"
            "<b>&#128274; Authentication Failed</b><br>"
            "%1<br><br>"
            "<i>Recovery:</i><br>"
            "&#8226; Check your API key in Settings (Ctrl+,)<br>"
            "&#8226; Your token may have expired — re-authenticate<br>"
            "&#8226; Verify your provider configuration"
            "</div>").arg(error.message.toHtmlEscaped());
        break;

    case AgentError::Code::RateLimited: {
        // Extract retry-after seconds from message if available (e.g., "retry after 30s")
        int retrySec = 60; // default fallback
        static const QRegularExpression retryRe(QStringLiteral("(\\d+)\\s*s"));
        const auto retryMatch = retryRe.match(error.message);
        if (retryMatch.hasMatch())
            retrySec = retryMatch.captured(1).toInt();

        errHtml = QStringLiteral(
            "<div class='err'>"
            "<b>&#9202; Rate Limited</b><br>"
            "%1<br><br>"
            "<i>Your request will be automatically retried in ~%2 seconds.</i><br>"
            "&#8226; Consider switching to a different model (Ctrl+Shift+M)<br>"
            "&#8226; Reduce context size in Settings"
            "</div>").arg(error.message.toHtmlEscaped()).arg(retrySec);

        // Auto-retry after the rate limit window
        if (!m_pendingAccum.isEmpty() || !m_msgQueue.isEmpty()) {
            QTimer::singleShot(retrySec * 1000, this, [this]() {
                if (m_pendingRequestId.isEmpty())
                    checkQueue();
            });
        }
        break;
    }

    case AgentError::Code::NetworkError:
        errHtml = QStringLiteral(
            "<div class='err'>"
            "<b>&#127760; Network Error</b><br>"
            "%1<br><br>"
            "<i>Recovery:</i><br>"
            "&#8226; Check your internet connection<br>"
            "&#8226; Verify proxy settings if behind a firewall<br>"
            "&#8226; The API server may be temporarily unavailable"
            "</div>").arg(error.message.toHtmlEscaped());
        break;

    case AgentError::Code::Cancelled:
        errHtml = QStringLiteral(
            "<div class='err'><i>Request cancelled.</i></div>");
        break;

    default:
        errHtml = QStringLiteral(
            "<div class='err'>"
            "<b>&#9888; Error</b><br>"
            "%1<br><br>"
            "&#8226; Try again or check the request log for details"
            "</div>").arg(error.message.toHtmlEscaped());
        break;
    }

    // If streaming was in progress, replace the accumulated text with the error
    if (m_streamStarted) {
        QTextCursor c(m_history->document());
        c.setPosition(m_streamAnchorPos, QTextCursor::MoveAnchor);
        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        c.insertHtml(errHtml);
    } else {
        m_history->append(errHtml);
    }

    m_pendingRequestId.clear();
    m_pendingAccum.clear();
    m_thinkingAccum.clear();
    m_streamStarted           = false;
    m_streamAnchorPos         = 0;
    m_thinkingStreamStarted   = false;
    m_thinkingAnchorPos       = 0;
    m_workingSectionActive    = false;
    m_workingSectionAnchor    = 0;
    m_workingSteps.clear();
    hideWorkingIndicator();
    setInputEnabled(true);
    checkQueue();
}

void AgentChatPanel::onAnchorClicked(const QUrl &url)
{
    // Suggestion links from empty state
    if (url.scheme() == QLatin1String("suggest")) {
        const QString cmd = QLatin1Char('/') + url.host();
        m_input->setPlainText(cmd + QLatin1Char(' '));
        m_input->moveCursor(QTextCursor::End);
        m_input->setFocus();
        return;
    }

    // Retry user message
    if (url.scheme() == QLatin1String("retry")) {
        bool ok = false;
        const int idx = url.path().mid(1).toInt(&ok);
        if (idx < 0) ok = false;
        if (!ok) { const int idx2 = url.host().toInt(&ok); Q_UNUSED(idx2); }
        const int realIdx = ok ? url.host().toInt() : url.path().mid(1).toInt();
        if (realIdx >= 0 && realIdx < m_userMessages.size()) {
            m_input->setPlainText(m_userMessages[realIdx]);
            m_input->moveCursor(QTextCursor::End);
            onSend();
        }
        return;
    }

    // Edit user message — put it in input for editing
    if (url.scheme() == QLatin1String("edit")) {
        const int idx = url.host().toInt();
        if (idx >= 0 && idx < m_userMessages.size()) {
            m_input->setPlainText(m_userMessages[idx]);
            m_input->moveCursor(QTextCursor::End);
            m_input->setFocus();
        }
        return;
    }

    // Copy AI response
    if (url.scheme() == QLatin1String("copyresp")) {
        const int idx = url.host().toInt();
        if (idx >= 0 && idx < m_aiMessages.size()) {
            if (auto *cb = QApplication::clipboard())
                cb->setText(m_aiMessages[idx]);
            appendMessage("system", tr("Response copied to clipboard."));
        }
        return;
    }

    // Feedback links
    if (url.scheme() == QLatin1String("feedback")) {
        const QString action = url.path().mid(1).toLower();
        if (action == QLatin1String("up"))
            appendMessage("system", tr("Thanks for the feedback!"));
        else if (action == QLatin1String("down"))
            appendMessage("system", tr("Thanks — we'll try to improve."));
        return;
    }

    // Create test file action
    if (url.scheme() == QLatin1String("action")) {
        const QString full = url.host() + url.path();
        if (full.startsWith(QLatin1String("createTestFile:"))
            || full.startsWith(QLatin1String("createtestfile:"))) {
            if (!m_pendingTestFilePath.isEmpty() && !m_pendingTestCode.isEmpty()) {
                QFileInfo fi(m_pendingTestFilePath);
                QDir().mkpath(fi.absolutePath());
                QFile f(m_pendingTestFilePath);
                if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    f.write(m_pendingTestCode.toUtf8());
                    f.close();
                    appendMessage("system",
                        tr("&#10004; Test file created: <b>%1</b>")
                            .arg(m_pendingTestFilePath.toHtmlEscaped()));
                    emit openFileRequested(m_pendingTestFilePath);
                } else {
                    appendMessage("error",
                        tr("Failed to create test file: %1").arg(f.errorString()));
                }
                m_pendingTestFilePath.clear();
                m_pendingTestCode.clear();
            }
            return;
        }
    }

    if (url.scheme() != QLatin1String("codeblock"))
        return;

    bool ok = false;
    const int idx = url.host().toInt(&ok);
    if (!ok || idx < 0 || idx >= m_lastBlocks.size())
        return;

    const QString action = url.path().mid(1).toLower();
    const auto &block = m_lastBlocks[idx];

    if (action == QLatin1String("copy")) {
        if (auto *cb = QApplication::clipboard())
            cb->setText(block.code);
        appendMessage("system", tr("Code block copied to clipboard."));
        return;
    }

    if (action == QLatin1String("apply")) {
        const QString target = !block.filePath.isEmpty()
            ? block.filePath
            : m_activeFilePath;
        if (target.isEmpty()) {
            appendMessage("error", tr("No target file to apply the code block."));
            return;
        }
        QFile f(target);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            appendMessage("error",
                tr("Failed to write file: %1").arg(f.errorString()));
            return;
        }
        f.write(block.code.toUtf8());
        f.close();
        appendMessage("system", tr("Code block applied to %1.").arg(target.toHtmlEscaped()));
        emit openFileRequested(target);
        return;
    }

    if (action == QLatin1String("insert")) {
        if (m_insertAtCursorFn) {
            m_insertAtCursorFn(block.code);
            appendMessage("system", tr("Code block inserted into editor."));
        }
        return;
    }

    if (action == QLatin1String("run")) {
        if (m_runInTerminalFn) {
            m_runInTerminalFn(block.code);
            appendMessage("system", tr("Code block sent to terminal."));
        }
    }
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void AgentChatPanel::appendMessage(const QString &role, const QString &html)
{
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("hh:mm"));

    if (role == QLatin1String("user")) {
        const int userIdx = m_userMsgCount++;
        m_history->append(
            QStringLiteral(
                "<table class='msg-table'><tr>"
                "<td class='avatar-cell'>"
                "<div class='avatar avatar-user'>U</div></td>"
                "<td class='content-cell'>"
                "<div class='who you'>You <span class='timestamp'>%3</span></div>"
                "<div class='body'>%1</div>"
                "<div class='msg-actions'>"
                "<a href='retry://%2' title='Retry'>&#8635; Retry</a> &nbsp;"
                "<a href='edit://%2' title='Edit'>&#9998; Edit</a>"
                "</div>"
                "</td></tr></table>").arg(html).arg(userIdx).arg(ts));
    } else if (role == QLatin1String("ai")) {
        const int msgIdx = m_messageCount++;
        m_history->append(
            QStringLiteral(
                "<table class='msg-table'><tr>"
                "<td class='avatar-cell'>"
                "<div class='avatar avatar-ai'>&#10022;</div></td>"
                "<td class='content-cell'>"
                "<div class='who ai'>Copilot <span class='timestamp'>%3</span></div>"
                "<div class='body'>%1</div>"
                "<div class='feedback'>"
                "<a href='feedback://%2/up' title='Helpful'>&#128077;</a> "
                "<a href='feedback://%2/down' title='Not helpful'>&#128078;</a>"
                " &nbsp;<a href='copyresp://%2' title='Copy response'>&#128203; Copy</a>"
                "</div>"
                "</td></tr></table>").arg(html).arg(msgIdx).arg(ts));
    } else if (role == QLatin1String("error")) {
        m_history->append(
            QStringLiteral("<div class='err'>%1</div>").arg(html));
    } else {
        // system / tool call feedback — compact italic line
        m_history->append(
            QStringLiteral("<div class='sys'>%1</div>").arg(html));
    }
}

// ── Live streaming helpers ────────────────────────────────────────────────────

void AgentChatPanel::updateStreamingContent()
{
    // Build streaming HTML: plain text with a blinking cursor indicator
    const QString safeText = m_pendingAccum.toHtmlEscaped()
        .replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    const QString streamHtml = QStringLiteral(
        "<table class='msg-table'><tr>"
        "<td class='avatar-cell'>"
        "<div class='avatar avatar-ai'>&#10022;</div></td>"
        "<td class='content-cell'>"
        "<div class='who ai'>Copilot</div>"
        "<div class='body'>"
        "<span style='color:#d4d4d4;white-space:pre-wrap;'>%1</span>"
        "<span style='color:#007acc;'>&#9607;</span>"
        "</div></td></tr></table>").arg(safeText);

    // Guard: if anchor exceeds document length (shouldn't happen but be safe)
    const int docLen = m_history->document()->characterCount();
    if (m_streamAnchorPos > docLen)
        m_streamAnchorPos = docLen > 0 ? docLen - 1 : 0;

    QTextCursor c(m_history->document());
    c.setPosition(m_streamAnchorPos, QTextCursor::MoveAnchor);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    c.insertHtml(streamHtml);

    // Deferred scroll so layout is settled before we query the maximum
    QTimer::singleShot(0, this, [this]() {
        m_history->verticalScrollBar()->setValue(
            m_history->verticalScrollBar()->maximum());
    });
}

void AgentChatPanel::finalizeStreamingBubble(const QString &/*markdownText*/,
                                             const QString &renderedHtml)
{
    if (m_streamStarted) {
        // Replace the accumulated streaming placeholder with final rendered HTML
        const QString finalHtml = QStringLiteral(
            "<table class='msg-table'><tr>"
            "<td class='avatar-cell'>"
            "<div class='avatar avatar-ai'>&#10022;</div></td>"
            "<td class='content-cell'>"
            "<div class='who ai'>Copilot</div>"
            "<div class='body'>%1</div>"
            "</td></tr></table>").arg(renderedHtml);

        QTextCursor c(m_history->document());
        c.setPosition(m_streamAnchorPos, QTextCursor::MoveAnchor);
        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        c.insertHtml(finalHtml);
    } else {
        // No streaming occurred (e.g. no delta events) — just append
        appendMessage("ai", renderedHtml);
    }

    // Scroll to bottom
    QTimer::singleShot(0, this, [this]() {
        m_history->verticalScrollBar()->setValue(
            m_history->verticalScrollBar()->maximum());
    });
}

// ── Live thinking helpers ─────────────────────────────────────────────────────

void AgentChatPanel::updateThinkingContent()
{
    const QString safeText = m_thinkingAccum.toHtmlEscaped()
        .replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    const QString thinkHtml = QStringLiteral(
        "<div class='thinking-block'>"
        "<div class='thinking-summary'>&#128161; Thinking\u2026</div>"
        "<div style='margin-top:4px;white-space:pre-wrap;'>%1"
        "<span style='color:#5a4fcf;'>\u2588</span>"
        "</div></div>").arg(safeText);

    const int docLen = m_history->document()->characterCount();
    if (m_thinkingAnchorPos > docLen)
        m_thinkingAnchorPos = docLen > 0 ? docLen - 1 : 0;

    QTextCursor c(m_history->document());
    c.setPosition(m_thinkingAnchorPos, QTextCursor::MoveAnchor);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    c.insertHtml(thinkHtml);

    QTimer::singleShot(0, this, [this]() {
        m_history->verticalScrollBar()->setValue(
            m_history->verticalScrollBar()->maximum());
    });
}

void AgentChatPanel::finalizeThinkingBubble()
{
    if (!m_thinkingStreamStarted)
        return;

    const QString safeText = m_thinkingAccum.toHtmlEscaped()
        .replace(QStringLiteral("\n"), QStringLiteral("<br>"));

    const QString finalHtml = QStringLiteral(
        "<div class='thinking-block'>"
        "<div class='thinking-summary'>&#128161; Thinking</div>"
        "<div style='margin-top:4px;white-space:pre-wrap;'>%1</div>"
        "</div>").arg(safeText);

    const int docLen = m_history->document()->characterCount();
    if (m_thinkingAnchorPos > docLen)
        m_thinkingAnchorPos = docLen > 0 ? docLen - 1 : 0;

    QTextCursor c(m_history->document());
    c.setPosition(m_thinkingAnchorPos, QTextCursor::MoveAnchor);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    c.insertHtml(finalHtml);

    m_thinkingStreamStarted = false;
    m_thinkingAnchorPos     = 0;
}

// ── Working section (VS Code-style tool activity display) ───────────────────

void AgentChatPanel::updateWorkingSection()
{
    QString stepsHtml;
    for (const auto &step : m_workingSteps) {
        if (step.finished) {
            const QString mark = step.success
                ? QStringLiteral("<span style='color:#4ec9b0;'> &#10003;</span>")
                : QStringLiteral("<span style='color:#f14c4c;'> &#10007;</span>");
            const QString res  = step.result.isEmpty() ? QString()
                : QStringLiteral(", %1").arg(step.result.toHtmlEscaped());
            stepsHtml += QStringLiteral(
                "<div style='color:#858585;font-size:12px;padding:1px 0 1px 12px;'>"
                "%1%2%3</div>").arg(step.summary, res, mark);
        } else {
            stepsHtml += QStringLiteral(
                "<div style='color:#cccccc;font-size:12px;padding:1px 0 1px 12px;'>"
                "%1&#8230;</div>").arg(step.summary);
        }
    }

    stepsHtml += QStringLiteral(
        "<div style='color:#858585;font-size:11px;padding:3px 0 1px 12px;"
        "font-style:italic;'>&#9679; Loading</div>");

    const QString html = QStringLiteral(
        "<div style='margin:6px 0;'>"
        "<div style='color:#858585;font-size:12px;font-weight:600;margin-bottom:2px;'>Working</div>"
        "<div style='border-left:2px solid #3e3e42;padding-left:8px;margin-left:4px;'>"
        "%1</div></div>").arg(stepsHtml);

    const int docLen = m_history->document()->characterCount();
    if (m_workingSectionAnchor > docLen)
        m_workingSectionAnchor = docLen > 0 ? docLen - 1 : 0;

    QTextCursor c(m_history->document());
    c.setPosition(m_workingSectionAnchor, QTextCursor::MoveAnchor);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    c.insertHtml(html);

    QTimer::singleShot(0, this, [this]() {
        m_history->verticalScrollBar()->setValue(
            m_history->verticalScrollBar()->maximum());
    });
}

void AgentChatPanel::finalizeWorkingSection()
{
    if (!m_workingSectionActive)
        return;

    QString stepsHtml;
    for (const auto &step : m_workingSteps) {
        const QString mark = step.finished
            ? (step.success
                ? QStringLiteral("<span style='color:#4ec9b0;'> &#10003;</span>")
                : QStringLiteral("<span style='color:#f14c4c;'> &#10007;</span>"))
            : QString();
        const QString res  = step.result.isEmpty() ? QString()
            : QStringLiteral(", %1").arg(step.result.toHtmlEscaped());
        stepsHtml += QStringLiteral(
            "<div style='color:#858585;font-size:12px;padding:1px 0 1px 12px;'>"
            "%1%2%3</div>").arg(step.summary, res, mark);
    }

    const QString html = QStringLiteral(
        "<div style='margin:6px 0;'>"
        "<div style='color:#858585;font-size:12px;font-weight:600;margin-bottom:2px;'>Working</div>"
        "<div style='border-left:2px solid #3e3e42;padding-left:8px;margin-left:4px;'>"
        "%1</div></div>").arg(stepsHtml);

    const int docLen = m_history->document()->characterCount();
    if (m_workingSectionAnchor > docLen)
        m_workingSectionAnchor = docLen > 0 ? docLen - 1 : 0;

    QTextCursor c(m_history->document());
    c.setPosition(m_workingSectionAnchor, QTextCursor::MoveAnchor);
    c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    c.insertHtml(html);

    m_workingSectionActive = false;
    m_workingSectionAnchor = 0;
    m_workingSteps.clear();
}

// ── Empty state ───────────────────────────────────────────────────────────────

void AgentChatPanel::showEmptyState()
{
    m_history->setHtml(
        QStringLiteral(
            "<div style='text-align:center; padding-top:48px;'>"

            /* Copilot sparkle icon — large */
            "<div style='font-size:36px; color:#5a4fcf; margin-bottom:12px;'>&#10022;</div>"

            /* Title */
            "<div style='font-size:15px; font-weight:600; color:#cccccc;'>Copilot</div>"
            "<div style='font-size:12px; color:#858585; margin:6px 0 28px 0;'>"
            "Ask anything or pick a suggestion to get started</div>"

            /* Suggestion pills — stacked, link-colored, VS Code style */
            "<div style='margin:0 auto; text-align:center;'>"
            "<div style='margin:5px 0;'>"
            "<a href='suggest://explain' style='color:#4daafc; font-size:12px;"
            "   text-decoration:none; background:#2b2b2b; padding:5px 14px;"
            "   border:1px solid #3e3e42; border-radius:14px;'>"
            "/explain &mdash; Explain selected code</a></div>"

            "<div style='margin:5px 0;'>"
            "<a href='suggest://fix' style='color:#4daafc; font-size:12px;"
            "   text-decoration:none; background:#2b2b2b; padding:5px 14px;"
            "   border:1px solid #3e3e42; border-radius:14px;'>"
            "/fix &mdash; Find and fix bugs</a></div>"

            "<div style='margin:5px 0;'>"
            "<a href='suggest://test' style='color:#4daafc; font-size:12px;"
            "   text-decoration:none; background:#2b2b2b; padding:5px 14px;"
            "   border:1px solid #3e3e42; border-radius:14px;'>"
            "/test &mdash; Generate unit tests</a></div>"

            "<div style='margin:5px 0;'>"
            "<a href='suggest://review' style='color:#4daafc; font-size:12px;"
            "   text-decoration:none; background:#2b2b2b; padding:5px 14px;"
            "   border:1px solid #3e3e42; border-radius:14px;'>"
            "/review &mdash; Code review</a></div>"

            "<div style='margin:5px 0;'>"
            "<a href='suggest://doc' style='color:#4daafc; font-size:12px;"
            "   text-decoration:none; background:#2b2b2b; padding:5px 14px;"
            "   border:1px solid #3e3e42; border-radius:14px;'>"
            "/doc &mdash; Add documentation</a></div>"
            "</div>"

            "</div>"));
}
void AgentChatPanel::showSessionHistory()
{
    if (!m_sessionStore)
        return;

    const QStringList files = m_sessionStore->listSessionFiles(20);
    if (files.isEmpty()) {
        appendMessage("system", tr("No saved sessions."));
        return;
    }

    auto *menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu { background:#252526; color:#cccccc; border:1px solid #3e3e42; }"
        "QMenu::item { padding:6px 16px; font-size:12px; }"
        "QMenu::item:selected { background:#094771; }");

    // ── Search bar at the top ─────────────────────────────────────────────
    auto *searchWidget = new QWidget;
    auto *searchEdit = new QLineEdit(searchWidget);
    searchEdit->setPlaceholderText(tr("Search sessions…"));
    searchEdit->setStyleSheet(
        "QLineEdit { background:#1e1e1e; color:#dcdcdc; border:1px solid #3e3e42;"
        "  border-radius:2px; padding:4px 6px; font-size:12px; }");
    auto *searchLayout = new QHBoxLayout(searchWidget);
    searchLayout->setContentsMargins(8, 4, 8, 4);
    searchLayout->addWidget(searchEdit);

    auto *searchAction = new QWidgetAction(menu);
    searchAction->setDefaultWidget(searchWidget);
    menu->addAction(searchAction);
    menu->addSeparator();

    // Helper lambda to build session items
    auto buildSessionItems = [this, menu](const QStringList &sessionFiles) {
        for (const QString &path : sessionFiles) {
            const QFileInfo fi(path);
            const QString name = fi.baseName();
            QString displayName = name;
            if (name.size() >= 15) {
                const QString dateStr = name.left(8);
                const QString timeStr = name.mid(9, 6);
                const QDate d = QDate::fromString(dateStr, QStringLiteral("yyyyMMdd"));
                const QTime t = QTime::fromString(timeStr, QStringLiteral("HHmmss"));
                if (d.isValid() && t.isValid())
                    displayName = QLocale().toString(QDateTime(d, t), QStringLiteral("MMM d, hh:mm"));
            }

            // Session load action
            QAction *act = menu->addAction(displayName);
            connect(act, &QAction::triggered, this, [this, path]() {
                const SessionStore::ChatSession sess = m_sessionStore->loadSession(path);
                if (sess.isEmpty()) {
                    appendMessage("system", tr("Session is empty."));
                    return;
                }
                m_conversationHistory.clear();
                m_history->clear();
                m_askSessionId = sess.sessionId;
                m_userMessages.clear();
                m_aiMessages.clear();
                m_userMsgCount = 0;
                m_messageCount = 0;
                for (const auto &pair : sess.messages) {
                    if (pair.first == QLatin1String("user")) {
                        m_userMessages.append(pair.second);
                        m_conversationHistory.append({AgentMessage::Role::User, pair.second});
                        appendMessage("user", pair.second.toHtmlEscaped());
                    } else {
                        const auto rendered = MarkdownRenderer::toHtmlWithActions(pair.second);
                        m_aiMessages.append(pair.second);
                        m_conversationHistory.append({AgentMessage::Role::Assistant, pair.second});
                        appendMessage("ai", rendered.html);
                        m_lastBlocks = rendered.codeBlocks;
                    }
                }
            });

            // Rename action
            QAction *renameAct = menu->addAction(QStringLiteral("    \u270F ") + tr("Rename: %1").arg(displayName));
            connect(renameAct, &QAction::triggered, this, [this, path, displayName]() {
                bool ok = false;
                const QString newName = QInputDialog::getText(
                    this, tr("Rename Session"),
                    tr("New name for \"%1\":").arg(displayName),
                    QLineEdit::Normal, displayName, &ok);
                if (ok && !newName.trimmed().isEmpty()) {
                    const QString result = m_sessionStore->renameSession(path, newName.trimmed());
                    if (!result.isEmpty())
                        appendMessage("system", tr("Session renamed."));
                    else
                        appendMessage("system", tr("Failed to rename session."));
                }
            });

            // Delete action
            QAction *delAct = menu->addAction(QStringLiteral("    \xF0\x9F\x97\x91 ") + tr("Delete: %1").arg(displayName));
            delAct->setData(path);
            connect(delAct, &QAction::triggered, this, [this, path, displayName]() {
                if (QMessageBox::question(this, tr("Delete Session"),
                        tr("Delete session \"%1\"?").arg(displayName),
                        QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
                    m_sessionStore->deleteSession(path);
                    appendMessage("system", tr("Session deleted."));
                }
            });
        }
    };

    // Build initial items
    buildSessionItems(files);

    // Connect search — rebuild menu on search
    connect(searchEdit, &QLineEdit::textChanged, this, [this, menu, searchAction, buildSessionItems](const QString &text) {
        // Remove all actions except the search widget and first separator
        const QList<QAction*> acts = menu->actions();
        for (int i = acts.size() - 1; i >= 0; --i) {
            if (acts[i] != searchAction && !acts[i]->isSeparator())
                menu->removeAction(acts[i]);
            else if (acts[i]->isSeparator() && i > 1)
                menu->removeAction(acts[i]);
        }

        const QStringList filtered = m_sessionStore->searchSessions(text.trimmed(), 20);
        if (filtered.isEmpty()) {
            QAction *empty = menu->addAction(tr("No matching sessions."));
            empty->setEnabled(false);
        } else {
            buildSessionItems(filtered);
        }
    });

    menu->addSeparator();

    // Position above the history button
    const QPoint pos = m_historyBtn->mapToGlobal(QPoint(0, 0));
    menu->popup(QPoint(pos.x(), pos.y() - menu->sizeHint().height()));

    // Focus the search input after popup
    QTimer::singleShot(100, searchEdit, [searchEdit]() { searchEdit->setFocus(); });
}


// ── Message queue ─────────────────────────────────────────────────────────────

void AgentChatPanel::checkQueue()
{
    if (m_msgQueue.isEmpty())
        return;

    const QString nextMsg = m_msgQueue.dequeue();

    // Slight delay so the current response fully renders first
    QTimer::singleShot(50, this, [this, nextMsg]() {
        m_input->setPlainText(nextMsg);
        m_input->moveCursor(QTextCursor::End);
        onSend();
    });
}

// ── Context info ──────────────────────────────────────────────────────────────

void AgentChatPanel::updateContextInfo()
{
    QStringList parts;

    if (!m_activeFilePath.isEmpty()) {
        const QString name = QFileInfo(m_activeFilePath).fileName();
        parts << QStringLiteral("@ %1").arg(name);
    }

    if (!m_selectedText.isEmpty()) {
        const int lines = m_selectedText.count(QLatin1Char('\n')) + 1;
        parts << tr("%1 line(s) selected").arg(lines);
    }

    // Rough estimate: average 4 chars per token
    int estTokens = 0;
    for (const auto &msg : m_conversationHistory)
        estTokens += msg.content.size() / 4;
    if (!m_activeFilePath.isEmpty())
        estTokens += 1500;   // rough file context budget

    if (estTokens > 100)
        parts << tr("~%1 tokens").arg(estTokens);

    if (parts.isEmpty()) {
        m_contextStrip->hide();
    } else {
        m_contextStrip->setText(parts.join(QStringLiteral("  ·  ")));
        m_contextStrip->show();
    }
}

void AgentChatPanel::hideContextPopup()
{
    qApp->removeEventFilter(this);
    if (m_ctxPopup) {
        m_ctxPopup->hide();
        m_ctxPopup->deleteLater();
        m_ctxPopup = nullptr;
    }
    m_ctxBtn->setChecked(false);
}

void AgentChatPanel::showContextPopup()
{
    hideContextPopup();

    // ── Token estimation ──────────────────────────────────────────────────
    const IAgentProvider *prov = m_orchestrator->activeProvider();
    const ModelInfo mi  = prov ? prov->currentModelInfo() : ModelInfo{};
    const int ctxMax    = mi.capabilities.maxContextWindowTokens > 0
                          ? mi.capabilities.maxContextWindowTokens : 128000;
    const int outMax    = mi.capabilities.maxOutputTokens > 0
                          ? mi.capabilities.maxOutputTokens : 16384;

    // System instructions
    const QString sysPr = AgentModes::systemPromptForMode(m_currentMode);
    const int tokSystem = sysPr.size() / 4;

    // Tool definitions (~350 tokens per tool)
    int toolCount = 0;
    if (m_agentController)
        if (auto *reg = m_agentController->toolRegistry())
            toolCount = reg->toolNames().size();
    const int tokTools = toolCount * 350;

    // Reserved output budget
    const int tokReserved = outMax;

    // Conversation messages (user + assistant)
    int tokMessages = 0;
    for (const auto &msg : m_conversationHistory)
        tokMessages += msg.content.size() / 4;

    // Tool results (from agent session)
    int tokToolResults = 0;
    if (m_agentController && m_agentController->session()) {
        for (const auto &msg : m_agentController->session()->messages()) {
            if (msg.role == AgentMessage::Role::Tool)
                tokToolResults += msg.content.size() / 4;
        }
    }

    // File context
    const int tokFiles = m_activeFilePath.isEmpty() ? 0 : 1500;

    const int tokTotal = tokSystem + tokTools + tokReserved
                       + tokMessages + tokToolResults + tokFiles;

    const int pct = qBound(0, tokTotal * 100 / ctxMax, 100);

    // Update button label
    m_ctxBtn->setText(QStringLiteral("%1K").arg(tokTotal / 1000));

    // ── Build popup ───────────────────────────────────────────────────────
    m_ctxPopup = new QFrame(this);
    m_ctxPopup->setObjectName("ctxPopup");
    m_ctxPopup->setFrameShape(QFrame::StyledPanel);
    m_ctxPopup->setStyleSheet(
        "QFrame#ctxPopup {"
        "  background:#252526; border:1px solid #3e3e42; border-radius:4px; }"
        "QLabel { color:#cccccc; background:transparent; }"
        "QLabel.section { color:#cccccc; font-weight:600; font-size:12px; padding-top:6px; }"
        "QLabel.row-key  { color:#9d9d9d; font-size:12px; padding-left:8px; }"
        "QLabel.row-val  { color:#858585; font-size:12px; }"
        "QProgressBar {"
        "  background:#2d2d2d; border:none; border-radius:2px; height:4px; }"
        "QProgressBar::chunk { background:#007acc; border-radius:2px; }"
        "QPushButton {"
        "  background:#0e639c; color:white; border:none; border-radius:2px;"
        "  padding:4px 12px; font-size:12px; }"
        "QPushButton:hover { background:#1177bb; }");

    auto *vl = new QVBoxLayout(m_ctxPopup);
    vl->setContentsMargins(12, 10, 12, 12);
    vl->setSpacing(3);

    // Header
    auto *header = new QLabel(
        QStringLiteral("Context Window"), m_ctxPopup);
    header->setStyleSheet("font-weight:600; font-size:13px; color:#cccccc;");
    vl->addWidget(header);

    // Token count + percent
    auto *subhdr = new QLabel(
        QStringLiteral("%1K / %2K tokens  •  %3%")
            .arg(tokTotal / 1000).arg(ctxMax / 1000).arg(pct),
        m_ctxPopup);
    subhdr->setStyleSheet("color:#858585; font-size:11px;");
    vl->addWidget(subhdr);

    // Progress bar
    auto *bar = new QProgressBar(m_ctxPopup);
    bar->setRange(0, 100);
    bar->setValue(pct);
    bar->setTextVisible(false);
    bar->setFixedHeight(4);
    bar->setStyleSheet(pct >= 80
        ? "QProgressBar{background:#333337;border:none;border-radius:2px;height:4px;}"
          "QProgressBar::chunk{background:#f14c4c;border-radius:2px;}"
        : "QProgressBar{background:#333337;border:none;border-radius:2px;height:4px;}"
          "QProgressBar::chunk{background:#007acc;border-radius:2px;}");
    vl->addWidget(bar);
    vl->addSpacing(4);

    // Separator line helper
    auto addSep = [&]() {
        auto *s = new QFrame(m_ctxPopup);
        s->setFrameShape(QFrame::HLine);
        s->setStyleSheet("color:#3c3c3c; background:#3c3c3c; border:none;");
        s->setFixedHeight(1);
        vl->addWidget(s);
    };

    // Row helper: key + value aligned
    auto addRow = [&](const QString &key, int tokens) {
        auto *row = new QWidget(m_ctxPopup);
        auto *hl  = new QHBoxLayout(row);
        hl->setContentsMargins(0,0,0,0);
        hl->setSpacing(4);
        auto *kl = new QLabel(key, row);
        kl->setStyleSheet("color:#9d9d9d; font-size:12px; padding-left:8px;");
        auto *vLabel = new QLabel(
            tokens > 0 ? QStringLiteral("%1%")
                .arg(tokens * 100 / ctxMax) : QStringLiteral("—"),
            row);
        vLabel->setStyleSheet("color:#6a6a6a; font-size:12px;");
        vLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        hl->addWidget(kl, 1);
        hl->addWidget(vLabel);
        vl->addWidget(row);
    };

    // Section: System
    auto *secSystem = new QLabel(tr("System"), m_ctxPopup);
    secSystem->setStyleSheet("color:#cccccc; font-weight:600; font-size:12px; padding-top:4px;");
    vl->addWidget(secSystem);
    addRow(tr("System Instructions"), tokSystem);
    addRow(tr("Tool Definitions"),    tokTools);
    addRow(tr("Reserved Output"),     tokReserved);

    addSep();

    // Section: User Context
    auto *secUser = new QLabel(tr("User Context"), m_ctxPopup);
    secUser->setStyleSheet("color:#cccccc; font-weight:600; font-size:12px; padding-top:4px;");
    vl->addWidget(secUser);
    addRow(tr("Messages"),    tokMessages);
    addRow(tr("Tool Results"),tokToolResults);
    addRow(tr("Files"),       tokFiles);

    addSep();

    // Context type toggles (if ContextBuilder is available)
    if (m_contextBuilder) {
        auto *secToggle = new QLabel(tr("Context Sources"), m_ctxPopup);
        secToggle->setStyleSheet("color:#cccccc; font-weight:600; font-size:12px; padding-top:4px;");
        vl->addWidget(secToggle);

        auto addToggle = [&](const QString &label, ContextItem::Type type) {
            auto *row = new QWidget(m_ctxPopup);
            auto *hl  = new QHBoxLayout(row);
            hl->setContentsMargins(0,0,0,0);
            hl->setSpacing(4);

            auto *cb = new QCheckBox(label, row);
            cb->setChecked(m_contextBuilder->isContextTypeEnabled(type));
            cb->setStyleSheet("color:#9d9d9d; font-size:12px; padding-left:8px;");
            connect(cb, &QCheckBox::toggled, this, [this, type](bool on) {
                if (on) m_contextBuilder->enableContextType(type);
                else    m_contextBuilder->disableContextType(type);
            });
            hl->addWidget(cb, 1);

            auto *pinBtn = new QPushButton(
                m_contextBuilder->isContextTypePinned(type)
                    ? QStringLiteral("\U0001F4CC") : QStringLiteral("\U0001F4CC"), row);
            pinBtn->setFixedSize(22, 22);
            pinBtn->setCheckable(true);
            pinBtn->setChecked(m_contextBuilder->isContextTypePinned(type));
            pinBtn->setToolTip(tr("Pin — keep this context during pruning"));
            pinBtn->setStyleSheet(
                m_contextBuilder->isContextTypePinned(type)
                    ? "background:#1e6e1e; border:none; border-radius:3px; font-size:10px;"
                    : "background:transparent; border:none; border-radius:3px; font-size:10px; color:#6a6a6a;");
            connect(pinBtn, &QPushButton::toggled, this, [this, type, pinBtn](bool on) {
                if (on) m_contextBuilder->pinContextType(type);
                else    m_contextBuilder->unpinContextType(type);
                pinBtn->setStyleSheet(
                    on ? "background:#1e6e1e; border:none; border-radius:3px; font-size:10px;"
                       : "background:transparent; border:none; border-radius:3px; font-size:10px; color:#6a6a6a;");
            });
            hl->addWidget(pinBtn);

            vl->addWidget(row);
        };

        addToggle(tr("Active File"),   ContextItem::Type::ActiveFile);
        addToggle(tr("Open Files"),    ContextItem::Type::OpenFiles);
        addToggle(tr("Diagnostics"),   ContextItem::Type::Diagnostics);
        addToggle(tr("Git Diff"),      ContextItem::Type::GitDiff);
        addToggle(tr("Terminal"),      ContextItem::Type::WorkspaceInfo);

        addSep();
    }

    // Actions section
    auto *secActions = new QLabel(tr("Actions"), m_ctxPopup);
    secActions->setStyleSheet("color:#cccccc; font-weight:600; font-size:12px; padding-top:4px;");
    vl->addWidget(secActions);

    auto *compactBtn = new QPushButton(tr("Compact Conversation"), m_ctxPopup);
    connect(compactBtn, &QPushButton::clicked, this, [this]() {
        // Keep only last 2 exchange pairs (4 messages) to save context
        while (m_conversationHistory.size() > 4)
            m_conversationHistory.removeFirst();
        hideContextPopup();
        appendMessage("system", tr("Conversation compacted — older messages removed."));
    });
    vl->addWidget(compactBtn);

    // Position popup above the ctx button
    m_ctxPopup->setFixedWidth(280);
    m_ctxPopup->adjustSize();

    const QPoint btnGlobal = m_ctxBtn->mapToGlobal(QPoint(0, 0));
    const QPoint panelLocal = mapFromGlobal(btnGlobal);
    const int popupH = m_ctxPopup->sizeHint().height();
    m_ctxPopup->move(panelLocal.x() - 280 + m_ctxBtn->width(),
                     panelLocal.y() - popupH - 4);
    m_ctxPopup->show();
    m_ctxPopup->raise();

    // Install on parent so any click outside the popup closes it
    qApp->installEventFilter(this);
}

bool AgentChatPanel::eventFilter(QObject *obj, QEvent *ev)
{
    // Enter key in input; Shift+Enter = newline; Up/Down = history nav
    if (obj == m_input && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);

        // Rich paste: Ctrl+V with image in clipboard → attach as image
        if (ke->key() == Qt::Key_V && (ke->modifiers() & Qt::ControlModifier)) {
            const QMimeData *mime = QApplication::clipboard()->mimeData();
            if (mime && mime->hasImage()) {
                const QImage img = qvariant_cast<QImage>(mime->imageData());
                if (!img.isNull()) {
                    // Save to temp file and attach
                    const QString tmpPath = QDir::tempPath()
                        + QStringLiteral("/exorcist_paste_%1.png")
                              .arg(QDateTime::currentMSecsSinceEpoch());
                    if (img.save(tmpPath, "PNG"))
                        addAttachment(tmpPath);
                    return true;
                }
            }
        }

        // Escape dismisses mention/slash menus
        if (ke->key() == Qt::Key_Escape) {
            if (m_mentionMenu->isVisible()) { hideMentionMenu(); return true; }
            if (m_slashMenu->isVisible())   { hideSlashMenu();   return true; }
        }

        // Tab/Enter accepts selected mention menu item
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

        // Up/Down navigate mention menu when visible
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

        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier)
                return false;  // let QPlainTextEdit insert newline
            onSend();
            return true;
        }
        // Up/Down arrow for input history (only when cursor is on first/last line)
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

    // Close context popup on click outside
    if (m_ctxPopup && ev->type() == QEvent::MouseButtonPress) {
        const auto *me = static_cast<QMouseEvent *>(ev);
        const QPoint local = m_ctxPopup->mapFromGlobal(me->globalPosition().toPoint());
        if (!m_ctxPopup->rect().contains(local))
            hideContextPopup();
    }
    return QWidget::eventFilter(obj, ev);
}

void AgentChatPanel::showChangesBar(int editCount)
{
    m_changesLabel->setText(
        tr("%n edit(s) proposed", nullptr, editCount));
    m_changesBar->show();
}

void AgentChatPanel::hideChangesBar()
{
    m_changesBar->hide();
}

// ── Working indicator ─────────────────────────────────────────────────────────

void AgentChatPanel::showWorkingIndicator(const QString &label)
{
    m_workingDots = 0;
    m_workingLabel->setText(label.isEmpty() ? tr("Working") : label);
    m_workingBar->show();
    m_workingTimer->start();
    // Start shimmer animation
    if (auto *shimmer = m_workingBar->findChild<QTimer *>(QStringLiteral("shimmer")))
        shimmer->start();
}

void AgentChatPanel::hideWorkingIndicator()
{
    m_workingTimer->stop();
    // Stop shimmer animation
    if (auto *shimmer = m_workingBar->findChild<QTimer *>(QStringLiteral("shimmer")))
        shimmer->stop();
    m_workingBar->hide();
}

// ── File / image attachment ───────────────────────────────────────────────────

void AgentChatPanel::addAttachment(const QString &path)
{
    const QFileInfo fi(path);
    if (!fi.exists()) return;

    Attachment att;
    att.path    = path;
    att.name    = fi.fileName();
    const QString ext = fi.suffix().toLower();
    att.isImage = (ext == QLatin1String("png")  || ext == QLatin1String("jpg")
                || ext == QLatin1String("jpeg") || ext == QLatin1String("gif")
                || ext == QLatin1String("webp") || ext == QLatin1String("bmp"));
    if (att.isImage) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly))
            att.data = f.readAll();
    }
    m_attachments.append(att);

    // Add chip to attach bar
    auto *chip = new QWidget(m_attachBar);

    // Preview tooltip — first 8 lines for text files, dimensions for images
    if (att.isImage) {
        const QImage img(path);
        if (!img.isNull())
            chip->setToolTip(tr("Image: %1×%2 px").arg(img.width()).arg(img.height()));
    } else {
        QFile preview(path);
        if (preview.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString tip;
            int lines = 0;
            while (!preview.atEnd() && lines < 8) {
                tip += QString::fromUtf8(preview.readLine());
                ++lines;
            }
            if (!preview.atEnd()) tip += QStringLiteral("…");
            chip->setToolTip(tip);
        }
    }

    chip->setStyleSheet(
        "background:#252526; border:1px solid #3e3e42; border-radius:3px; padding:0;");
    auto *hl = new QHBoxLayout(chip);
    hl->setContentsMargins(6, 2, 4, 2);
    hl->setSpacing(4);
    auto *lbl = new QLabel(QStringLiteral("&#128206; %1").arg(att.name.toHtmlEscaped()), chip);
    lbl->setStyleSheet("color:#cccccc; font-size:11px; background:transparent;");
    lbl->setTextFormat(Qt::RichText);
    auto *rm = new QToolButton(chip);
    rm->setText(QStringLiteral("×"));
    rm->setStyleSheet(
        "QToolButton{background:transparent;border:none;color:#858585;font-size:12px;padding:0 2px;}"
        "QToolButton:hover{color:#f14c4c;}");
    const int idx = m_attachments.size() - 1;
    connect(rm, &QToolButton::clicked, this, [this, idx, chip]() {
        m_attachments.removeAt(idx);
        chip->deleteLater();
        if (m_attachments.isEmpty()) m_attachBar->hide();
    });
    hl->addWidget(lbl);
    hl->addWidget(rm);
    m_attachLayout->insertWidget(m_attachLayout->count() - 1, chip);
    m_attachBar->show();
}

void AgentChatPanel::dragEnterEvent(QDragEnterEvent *ev)
{
    if (ev->mimeData()->hasUrls() || ev->mimeData()->hasImage())
        ev->acceptProposedAction();
}

void AgentChatPanel::dropEvent(QDropEvent *ev)
{
    for (const QUrl &url : ev->mimeData()->urls()) {
        if (url.isLocalFile())
            addAttachment(url.toLocalFile());
    }
    ev->acceptProposedAction();
}

void AgentChatPanel::setInputEnabled(bool enabled)
{
    // In the new design we keep the input always enabled so users can type
    // ahead (queued messages). Only the send button is disabled while streaming.
    m_sendBtn->setEnabled(enabled);
    m_cancelBtn->setEnabled(!enabled);
    if (enabled)
        m_input->setFocus();
}

void AgentChatPanel::setWorkspaceRoot(const QString &root)
{
    m_workspaceRoot = root;

    // Remove old dynamic prompt file items
    while (m_slashMenu->count() > m_staticSlashCount)
        delete m_slashMenu->takeItem(m_slashMenu->count() - 1);

    // Load prompt files from workspace
    const auto promptFiles = PromptFileLoader::loadPromptFiles(root);
    for (const auto &pf : promptFiles) {
        auto *item = new QListWidgetItem(
            QStringLiteral("/%1  — %2").arg(pf.name, tr("Prompt file")));
        item->setData(Qt::UserRole, pf.body);
        item->setData(Qt::UserRole + 1, pf.filePath);
        m_slashMenu->addItem(item);
    }
}

void AgentChatPanel::focusInput()
{
    if (auto *dock = qobject_cast<QDockWidget *>(parent())) {
        dock->show();
        dock->raise();
    }
    m_input->setFocus();
}

void AgentChatPanel::setInputText(const QString &text)
{
    m_input->setPlainText(text);
    m_input->moveCursor(QTextCursor::End);
}

void AgentChatPanel::setEditorContext(const QString &filePath,
                                      const QString &selectedText,
                                      const QString &languageId)
{
    m_activeFilePath = filePath;
    m_selectedText   = selectedText;
    m_languageId     = languageId;
    updateContextInfo();
}

// ── Slash-command helpers ──────────────────────────────────────────────────────

void AgentChatPanel::showSlashMenu()
{
    // Filter items based on current input
    const QString typed = m_input->toPlainText().toLower();
    bool anyVisible = false;
    for (int i = 0; i < m_slashMenu->count(); ++i) {
        QListWidgetItem *item = m_slashMenu->item(i);
        const bool matches = item->text().toLower().startsWith(typed);
        item->setHidden(!matches);
        if (matches) anyVisible = true;
    }

    if (!anyVisible) {
        hideSlashMenu();
        return;
    }

    // Position just above the input field
    const QPoint inputGlobalPos = m_input->mapToGlobal(QPoint(0, 0));
    m_slashMenu->adjustSize();
    const int menuH = m_slashMenu->sizeHintForRow(0) *
                      std::min(m_slashMenu->count(), 6) + 4;
    m_slashMenu->setFixedHeight(menuH);
    m_slashMenu->move(inputGlobalPos.x(),
                      inputGlobalPos.y() - menuH - 2);
    m_slashMenu->show();
    m_slashMenu->raise();
}

void AgentChatPanel::hideSlashMenu()
{
    m_slashMenu->hide();
}

// ── @-mention / #-file autocomplete ───────────────────────────────────────────

void AgentChatPanel::showMentionMenu(const QString &trigger, const QString &filter)
{
    if (!m_workspaceFileFn) {
        hideMentionMenu();
        return;
    }

    m_mentionTrigger = trigger;
    m_mentionMenu->clear();

    const QStringList files = m_workspaceFileFn();
    const QString lowerFilter = filter.toLower();
    int shown = 0;

    for (const QString &absPath : files) {
        // Show relative path for display
        const QFileInfo fi(absPath);
        const QString display = fi.fileName();
        const QString relPath = absPath; // provider gives relative paths
        if (!lowerFilter.isEmpty()
            && !display.toLower().contains(lowerFilter)
            && !relPath.toLower().contains(lowerFilter))
            continue;

        auto *item = new QListWidgetItem(
            trigger == QLatin1String("#")
                ? QStringLiteral("📄 %1").arg(relPath)
                : QStringLiteral("@file:%1").arg(relPath));
        item->setData(Qt::UserRole, relPath);     // relative path
        item->setData(Qt::UserRole + 1, absPath); // absolute path
        m_mentionMenu->addItem(item);
        if (++shown >= 15) break;
    }

    if (shown == 0) {
        hideMentionMenu();
        return;
    }

    // Position just above the input field
    const QPoint inputGlobalPos = m_input->mapToGlobal(QPoint(0, 0));
    m_mentionMenu->adjustSize();
    const int menuH = m_mentionMenu->sizeHintForRow(0) *
                      std::min(shown, 8) + 4;
    m_mentionMenu->setFixedHeight(menuH);
    m_mentionMenu->move(inputGlobalPos.x(),
                        inputGlobalPos.y() - menuH - 2);
    m_mentionMenu->show();
    m_mentionMenu->raise();
}

void AgentChatPanel::hideMentionMenu()
{
    m_mentionMenu->hide();
}

void AgentChatPanel::setWorkspaceFileProvider(std::function<QStringList()> fn)
{
    m_workspaceFileFn = std::move(fn);
}

void AgentChatPanel::attachSelection(const QString &text, const QString &filePath,
                                     int startLine)
{
    if (text.isEmpty()) return;

    const QFileInfo fi(filePath);
    const QString label = fi.fileName().isEmpty()
        ? tr("Selection")
        : QStringLiteral("%1:%2").arg(fi.fileName()).arg(startLine);

    // Add as a special snippet attachment
    Attachment att;
    att.path = filePath;
    att.name = label;
    att.data = text.toUtf8();
    att.isImage = false;
    m_attachments.append(att);

    // Create visual chip
    auto *chip = new QWidget(m_attachBar);
    auto *hl = new QHBoxLayout(chip);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(2);
    chip->setStyleSheet(
        "background:#3a3d41; border-radius:3px; padding:2px 6px;");
    auto *lbl = new QLabel(QStringLiteral("\xe2\x9c\x82 %1").arg(label.toHtmlEscaped()), chip);
    lbl->setStyleSheet("color:#cccccc; font-size:11px;");
    auto *rm = new QToolButton(chip);
    rm->setText(QStringLiteral("\xc3\x97"));
    rm->setStyleSheet(
        "QToolButton{background:transparent;border:none;color:#858585;font-size:12px;padding:0 2px;}"
        "QToolButton:hover{color:#f14c4c;}");
    const int selIdx = m_attachments.size() - 1;
    connect(rm, &QToolButton::clicked, this, [this, selIdx, chip]() {
        if (selIdx < m_attachments.size()) {
            m_attachments.removeAt(selIdx);
            chip->deleteLater();
            if (m_attachments.isEmpty()) m_attachBar->hide();
        }
    });
    hl->addWidget(lbl);
    hl->addWidget(rm);
    m_attachLayout->insertWidget(m_attachLayout->count() - 1, chip);
    m_attachBar->show();
}

void AgentChatPanel::attachDiagnostics(const QList<AgentDiagnostic> &diagnostics)
{
    if (diagnostics.isEmpty()) return;

    // Build text representation of diagnostics
    QString text;
    for (const auto &d : diagnostics) {
        const QChar sev = (d.severity == AgentDiagnostic::Severity::Error)   ? QLatin1Char('E')
                        : (d.severity == AgentDiagnostic::Severity::Warning) ? QLatin1Char('W')
                                                                             : QLatin1Char('I');
        text += QStringLiteral("[%1] %2:%3 — %4\n")
                    .arg(sev)
                    .arg(QFileInfo(d.filePath).fileName())
                    .arg(d.line)
                    .arg(d.message);
    }

    Attachment att;
    att.name = tr("%n diagnostic(s)", nullptr, diagnostics.size());
    att.data = text.toUtf8();
    att.isImage = false;
    m_attachments.append(att);

    // Visual chip
    auto *chip = new QWidget(m_attachBar);
    auto *hl2 = new QHBoxLayout(chip);
    hl2->setContentsMargins(6, 2, 4, 2);
    hl2->setSpacing(4);
    chip->setStyleSheet("background:#3a3d41; border-radius:3px; padding:0;");
    auto *lbl2 = new QLabel(QStringLiteral("⚠ %1").arg(att.name.toHtmlEscaped()), chip);
    lbl2->setStyleSheet("color:#f14c4c; font-size:11px; background:transparent;");
    chip->setToolTip(text);
    auto *rm2 = new QToolButton(chip);
    rm2->setText(QStringLiteral("×"));
    rm2->setStyleSheet(
        "QToolButton{background:transparent;border:none;color:#858585;font-size:12px;padding:0 2px;}"
        "QToolButton:hover{color:#f14c4c;}");
    const int idx = m_attachments.size() - 1;
    connect(rm2, &QToolButton::clicked, this, [this, idx, chip]() {
        if (idx < m_attachments.size()) {
            m_attachments.removeAt(idx);
            chip->deleteLater();
            if (m_attachments.isEmpty()) m_attachBar->hide();
        }
    });
    hl2->addWidget(lbl2);
    hl2->addWidget(rm2);
    m_attachLayout->insertWidget(m_attachLayout->count() - 1, chip);
    m_attachBar->show();
}

QString AgentChatPanel::resolveSlashCommand(const QString &text,
                                            AgentIntent &intent) const
{
    // Returns the user-facing prompt suffix; sets intent.
    if (text.startsWith(QLatin1String("/explain"))) {
        intent = AgentIntent::ExplainCode;
        const QString rest = text.mid(8).trimmed();
        return rest.isEmpty()
            ? tr("Explain what this code does, step by step.")
            : rest;
    }
    if (text.startsWith(QLatin1String("/fix"))) {
        intent = AgentIntent::FixDiagnostic;
        const QString rest = text.mid(4).trimmed();

        // Auto-include current diagnostics/errors for context
        QString diagContext;
        if (m_contextBuilder) {
            const auto items = m_contextBuilder->buildContext().items;
            for (const auto &item : items) {
                if (item.type == ContextItem::Type::Diagnostics && !item.content.isEmpty()) {
                    diagContext = tr("\n\nCurrent diagnostics/errors:\n%1").arg(item.content);
                    break;
                }
            }
        }

        if (!rest.isEmpty())
            return rest + diagContext;
        return tr("Find and fix all bugs, errors, and issues in this code. "
                  "Analyze the diagnostics and provide corrected code.") + diagContext;
    }
    if (text.startsWith(QLatin1String("/fixtest"))) {
        intent = AgentIntent::FixDiagnostic;
        const QString rest = text.mid(8).trimmed();

        // Gather terminal output which may contain test failure info
        QString terminalContext;
        if (m_contextBuilder) {
            const auto items = m_contextBuilder->buildContext().items;
            for (const auto &item : items) {
                if (item.label.contains(QLatin1String("terminal"), Qt::CaseInsensitive)
                    && !item.content.isEmpty()) {
                    terminalContext = tr("\n\nRecent terminal output (may contain test failures):\n%1")
                        .arg(item.content);
                    break;
                }
            }
        }

        if (!rest.isEmpty())
            return rest + terminalContext;
        return tr("Analyze failing test output and fix the code to make the tests pass. "
                  "Look at assertion failures, error messages, and stack traces.") + terminalContext;
    }
    if (text.startsWith(QLatin1String("/test"))) {
        intent = AgentIntent::GenerateTests;
        const QString rest = text.mid(5).trimmed();
        QString frameworkHint;
        if (!m_workspaceRoot.isEmpty()) {
            // Auto-detect test framework from workspace files
            QStringList detected;
            if (QFile::exists(m_workspaceRoot + QStringLiteral("/CMakeLists.txt"))) {
                QFile cm(m_workspaceRoot + QStringLiteral("/CMakeLists.txt"));
                if (cm.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString cmContent = QString::fromUtf8(cm.readAll()).toLower();
                    if (cmContent.contains(QLatin1String("gtest")) || cmContent.contains(QLatin1String("googletest")))
                        detected << QStringLiteral("Google Test (gtest)");
                    if (cmContent.contains(QLatin1String("catch2")) || cmContent.contains(QLatin1String("catch_discover")))
                        detected << QStringLiteral("Catch2");
                    if (cmContent.contains(QLatin1String("doctest")))
                        detected << QStringLiteral("doctest");
                    if (cmContent.contains(QLatin1String("qt_add_test")) || cmContent.contains(QLatin1String("qtest")))
                        detected << QStringLiteral("Qt Test (QTest)");
                    if (cmContent.contains(QLatin1String("ctest")))
                        detected << QStringLiteral("CTest");
                }
            }
            if (QFile::exists(m_workspaceRoot + QStringLiteral("/package.json"))) {
                QFile pj(m_workspaceRoot + QStringLiteral("/package.json"));
                if (pj.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString pjContent = QString::fromUtf8(pj.readAll()).toLower();
                    if (pjContent.contains(QLatin1String("jest")))
                        detected << QStringLiteral("Jest");
                    if (pjContent.contains(QLatin1String("mocha")))
                        detected << QStringLiteral("Mocha");
                    if (pjContent.contains(QLatin1String("vitest")))
                        detected << QStringLiteral("Vitest");
                    if (pjContent.contains(QLatin1String("@testing-library")))
                        detected << QStringLiteral("Testing Library");
                }
            }
            if (QFile::exists(m_workspaceRoot + QStringLiteral("/pytest.ini")) ||
                QFile::exists(m_workspaceRoot + QStringLiteral("/pyproject.toml")) ||
                QFile::exists(m_workspaceRoot + QStringLiteral("/setup.cfg"))) {
                detected << QStringLiteral("pytest");
            }
            if (!detected.isEmpty())
                frameworkHint = tr("\nDetected test framework(s): %1. "
                                  "Use the project's existing test framework and conventions.")
                                   .arg(detected.join(QStringLiteral(", ")));
        }

        // Infer test file path from source file
        QString scaffoldHint;
        if (!m_activeFilePath.isEmpty()) {
            QFileInfo fi(m_activeFilePath);
            QString testFileName;
            const QString lang = m_languageId.toLower();
            const QString base = fi.completeBaseName();

            if (lang == QLatin1String("cpp") || lang == QLatin1String("c"))
                testFileName = QStringLiteral("test_%1.cpp").arg(base);
            else if (lang == QLatin1String("python"))
                testFileName = QStringLiteral("test_%1.py").arg(base);
            else if (lang == QLatin1String("javascript") || lang == QLatin1String("typescript"))
                testFileName = QStringLiteral("%1.test.%2").arg(base, fi.suffix());
            else if (lang == QLatin1String("java"))
                testFileName = QStringLiteral("%1Test.java").arg(base);
            else
                testFileName = QStringLiteral("test_%1.%2").arg(base, fi.suffix());

            // Check for tests/ directory
            QString testDir = fi.absolutePath() + QStringLiteral("/tests");
            if (!QDir(testDir).exists())
                testDir = fi.absolutePath() + QStringLiteral("/test");
            if (!QDir(testDir).exists())
                testDir = fi.absolutePath();

            scaffoldHint = tr("\nOutput the complete test file. Suggest the file path: %1/%2")
                .arg(testDir, testFileName);
        }
        if (!rest.isEmpty())
            return rest + frameworkHint + scaffoldHint;
        return tr("Generate comprehensive unit tests for this code.") + frameworkHint + scaffoldHint;
    }
    if (text.startsWith(QLatin1String("/doc"))) {
        intent = AgentIntent::Chat;
        const QString rest = text.mid(4).trimmed();
        return rest.isEmpty()
            ? tr("Add clear documentation comments to this code.")
            : rest;
    }
    if (text.startsWith(QLatin1String("/review"))) {
        intent = AgentIntent::CodeReview;
        const QString rest = text.mid(7).trimmed();
        return rest.isEmpty()
            ? tr("Review this code and suggest improvements for correctness, "
                 "readability, and performance.")
            : rest;
    }
    if (text.startsWith(QLatin1String("/commit"))) {
        intent = AgentIntent::SuggestCommitMessage;
        const QString rest = text.mid(7).trimmed();
        return rest.isEmpty()
            ? tr("Suggest a concise, descriptive git commit message "
                 "for the current staged changes.")
            : rest;
    }
    if (text.startsWith(QLatin1String("/compact"))) {
        intent = AgentIntent::Chat;
        return tr("Summarize the conversation so far into a compact context. "
                  "Keep only the essential decisions, code changes, and "
                  "current state. Drop any resolved back-and-forth.");
    }
    if (text.startsWith(QLatin1String("/resolve"))) {
        intent = AgentIntent::Chat;
        const QString rest = text.mid(8).trimmed();
        return rest.isEmpty()
            ? tr("Resolve all merge conflicts in the conflicted files. "
                 "For each conflict, choose the best resolution that "
                 "preserves the intent of both sides. Show me the resolved code.")
            : rest;
    }
    if (text.startsWith(QLatin1String("/changes"))) {
        intent = AgentIntent::CodeReview;
        const QString rest = text.mid(8).trimmed();
        return rest.isEmpty()
            ? tr("Review the current git changes (diff). Identify bugs, "
                 "issues, and suggest improvements.")
            : rest;
    }

    // Check if it matches a prompt file command
    if (!m_workspaceRoot.isEmpty() && text.startsWith(QLatin1Char('/'))) {
        const QString cmdName = text.section(QLatin1Char(' '), 0, 0).mid(1);
        const auto promptFiles = PromptFileLoader::loadPromptFiles(m_workspaceRoot);
        for (const auto &pf : promptFiles) {
            if (pf.name == cmdName) {
                intent = AgentIntent::Chat;
                const QString rest = text.mid(1 + cmdName.length()).trimmed();
                QString body = PromptFileLoader::interpolatePromptBody(
                    pf.body, m_workspaceRoot, m_activeFilePath, m_selectedText);
                if (!rest.isEmpty())
                    body += QStringLiteral("\n\n") + rest;
                return body;
            }
        }
    }

    // Not a slash command
    intent = AgentIntent::Chat;
    return text;
}
