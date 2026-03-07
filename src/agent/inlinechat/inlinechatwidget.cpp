#include "inlinechatwidget.h"
#include "../agentorchestrator.h"
#include "../markdownrenderer.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QTextBrowser>
#include <QUuid>
#include <QVBoxLayout>

InlineChatWidget::InlineChatWidget(AgentOrchestrator *orchestrator, QWidget *parent)
    : QFrame(parent),
      m_orchestrator(orchestrator),
      m_contextLabel(new QLabel(this)),
      m_input(new QLineEdit(this)),
      m_sendBtn(new QPushButton(tr("Send"), this)),
      m_cancelBtn(new QPushButton(tr("✕"), this)),
      m_response(new QTextBrowser(this)),
      m_applyBtn(new QPushButton(tr("Apply"), this)),
      m_discardBtn(new QPushButton(tr("Discard"), this))
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setMinimumWidth(480);
    setMaximumWidth(700);

    // Dark background to stand out from the editor
    setStyleSheet(QStringLiteral(
        "InlineChatWidget {"
        "  background: #252526;"
        "  border: 1px solid #454545;"
        "  border-radius: 6px;"
        "}"
        "QLineEdit {"
        "  background: #1e1e1e;"
        "  color: #dcdcdc;"
        "  border: 1px solid #3c3c3c;"
        "  border-radius: 3px;"
        "  padding: 4px 6px;"
        "}"
        "QTextBrowser {"
        "  background: #1e1e1e;"
        "  color: #dcdcdc;"
        "  border: 1px solid #3c3c3c;"
        "  border-radius: 3px;"
        "}"
        "QPushButton {"
        "  background: #0e639c;"
        "  color: white;"
        "  border: none;"
        "  border-radius: 3px;"
        "  padding: 4px 10px;"
        "}"
        "QPushButton:hover  { background: #1177bb; }"
        "QPushButton:disabled { background: #555; color: #888; }"));

    // Context label (shows what code is selected)
    m_contextLabel->setStyleSheet(QStringLiteral(
        "color: #888; font-size: 11px; font-style: italic; padding: 2px 4px;"));
    m_contextLabel->setWordWrap(true);

    // Input row
    m_input->setPlaceholderText(tr("Ask AI to edit or explain…  (Enter to send, Esc to dismiss)"));
    m_cancelBtn->setFixedWidth(28);
    m_cancelBtn->setToolTip(tr("Cancel"));
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#5a1d1d; } QPushButton:hover { background:#7a2020; }"));

    auto *inputRow = new QHBoxLayout;
    inputRow->setContentsMargins(0, 0, 0, 0);
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_sendBtn);
    inputRow->addWidget(m_cancelBtn);

    // Response area (hidden until we have a response)
    m_response->setReadOnly(true);
    m_response->setMaximumHeight(240);
    m_response->setMinimumHeight(60);
    m_response->setVisible(false);
    m_response->document()->setDefaultStyleSheet(
        "body { color:#dcdcdc; font-family:monospace; font-size:12px; }"
        "pre  { background:#1a1a1a; padding:4px; border-radius:3px; }"
        "code { color:#9cdcfe; }");

    // Action buttons (hidden until response ready)
    m_applyBtn->setEnabled(false);
    m_applyBtn->setVisible(false);
    m_discardBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background:#5a5a5a; } QPushButton:hover { background:#6a6a6a; }"));
    m_discardBtn->setVisible(false);

    auto *actionRow = new QHBoxLayout;
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->addStretch();
    actionRow->addWidget(m_applyBtn);
    actionRow->addWidget(m_discardBtn);

    // Root layout
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);
    root->addWidget(m_contextLabel);
    root->addLayout(inputRow);
    root->addWidget(m_response);
    root->addLayout(actionRow);

    // Connections
    connect(m_sendBtn,    &QPushButton::clicked, this, &InlineChatWidget::onSend);
    connect(m_cancelBtn,  &QPushButton::clicked, this, [this]() {
        if (!m_pendingReqId.isEmpty())
            m_orchestrator->cancelRequest(m_pendingReqId);
        m_pendingReqId.clear();
        setResponding(false);
    });
    connect(m_applyBtn,   &QPushButton::clicked, this, &InlineChatWidget::onApply);
    connect(m_discardBtn, &QPushButton::clicked, this, &InlineChatWidget::onDiscard);
    connect(m_input,      &QLineEdit::returnPressed, this, &InlineChatWidget::onSend);

    connect(m_orchestrator, &AgentOrchestrator::responseDelta,
            this, &InlineChatWidget::onDelta);
    connect(m_orchestrator, &AgentOrchestrator::responseFinished,
            this, &InlineChatWidget::onFinished);
    connect(m_orchestrator, &AgentOrchestrator::responseError,
            this, &InlineChatWidget::onError);
}

// ── Public API ────────────────────────────────────────────────────────────────

void InlineChatWidget::setContext(const QString &selectedText,
                                  const QString &filePath,
                                  const QString &languageId)
{
    m_selectedText = selectedText;
    m_filePath     = filePath;
    m_languageId   = languageId;

    if (selectedText.isEmpty()) {
        m_contextLabel->setText(tr("No selection — AI will edit multiple locations in the file."));
    } else {
        const QString preview = selectedText.left(80).simplified();
        m_contextLabel->setText(tr("Selection: %1%2")
            .arg(preview, selectedText.length() > 80 ? QStringLiteral("…") : QString()));
    }

    // Reset response area
    m_response->clear();
    m_response->setVisible(false);
    m_applyBtn->setVisible(false);
    m_applyBtn->setEnabled(false);
    m_discardBtn->setVisible(false);
    m_lastCode.clear();
    m_lastChunks.clear();
    m_accum.clear();
}

void InlineChatWidget::setFileContent(const QString &content)
{
    m_fileContent = content;
}

void InlineChatWidget::focusInput()
{
    m_input->setFocus();
    m_input->selectAll();
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void InlineChatWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        if (!m_pendingReqId.isEmpty()) {
            m_orchestrator->cancelRequest(m_pendingReqId);
            m_pendingReqId.clear();
            setResponding(false);
        } else {
            emit dismissed();
        }
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Return &&
        event->modifiers().testFlag(Qt::ControlModifier) &&
        m_applyBtn->isEnabled())
    {
        onApply();
        event->accept();
        return;
    }

    QFrame::keyPressEvent(event);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void InlineChatWidget::onSend()
{
    const QString prompt = m_input->text().trimmed();
    if (prompt.isEmpty()) return;

    const IAgentProvider *active = m_orchestrator->activeProvider();
    if (!active || !active->isAvailable()) return;

    m_input->clear();
    setResponding(true);

    m_response->clear();
    m_response->setVisible(true);
    m_accum.clear();
    m_lastCode.clear();
    m_applyBtn->setEnabled(false);
    m_applyBtn->setVisible(false);
    m_discardBtn->setVisible(false);

    // Build the request
    m_pendingReqId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    AgentRequest req;
    req.requestId      = m_pendingReqId;
    req.intent         = AgentIntent::RefactorSelection;
    req.agentMode      = false;
    req.activeFilePath = m_filePath;
    req.languageId     = m_languageId;
    req.selectedText   = m_selectedText;

    // Compose the user prompt
    if (m_selectedText.isEmpty()) {
        // Multi-chunk mode: send full file content + instructions for line-annotated blocks
        QString fileCtx;
        if (!m_fileContent.isEmpty()) {
            fileCtx = QStringLiteral(
                "\n\nFile (%1):\n```%2\n%3\n```\n\n"
                "If you need to edit multiple locations, use line-annotated code blocks:\n"
                "```%2:L<start>-L<end>\nnew code\n```\n"
                "Where start and end are 1-based line numbers of the region to replace.")
                .arg(m_filePath, m_languageId, m_fileContent);
        }
        req.userPrompt = prompt + fileCtx;
    } else {
        req.userPrompt = QStringLiteral(
            "%1\n\nCode:\n```%2\n%3\n```")
            .arg(prompt, m_languageId, m_selectedText);
    }

    m_orchestrator->sendRequest(req);
}

void InlineChatWidget::onApply()
{
    if (!m_lastChunks.isEmpty()) {
        emit applyMultiEditsRequested(m_lastChunks);
    } else if (!m_lastCode.isEmpty()) {
        emit applyRequested(m_lastCode);
    } else {
        emit applyRequested(m_accum);
    }
    emit dismissed();
}

void InlineChatWidget::onDiscard()
{
    emit dismissed();
}

void InlineChatWidget::onDelta(const QString &reqId, const QString &chunk)
{
    if (reqId != m_pendingReqId) return;
    m_accum += chunk;
    m_response->setPlainText(m_accum);

    // Scroll to bottom
    auto *sb = m_response->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

void InlineChatWidget::onFinished(const QString &reqId, const AgentResponse &resp)
{
    if (reqId != m_pendingReqId) return;

    const QString text = m_accum.isEmpty() ? resp.text : m_accum;
    m_accum = text;

    // Try multi-chunk extraction first
    m_lastChunks = extractMultiChunks(text);
    const bool isMultiChunk = !m_lastChunks.isEmpty();

    // Extract single code block as fallback
    m_lastCode = isMultiChunk ? QString() : extractCode(text);
    const bool hasCode = isMultiChunk || !m_lastCode.isEmpty();

    // Show appropriate diff view
    if (isMultiChunk) {
        m_response->setHtml(buildMultiDiffHtml(m_lastChunks));
    } else if (!m_lastCode.isEmpty() && !m_selectedText.isEmpty()) {
        m_response->setHtml(buildDiffHtml(m_selectedText, m_lastCode));
    } else {
        m_response->setHtml(MarkdownRenderer::toHtml(text));
    }

    m_applyBtn->setEnabled(hasCode);
    m_applyBtn->setVisible(true);
    if (isMultiChunk)
        m_applyBtn->setText(tr("Apply %1 chunks  (Ctrl+Enter)").arg(m_lastChunks.size()));
    else
        m_applyBtn->setText(hasCode
            ? tr("Apply  (Ctrl+Enter)")
            : tr("Apply (no code found)"));
    m_discardBtn->setVisible(true);

    m_pendingReqId.clear();
    setResponding(false);
    adjustSize();
}

void InlineChatWidget::onError(const QString &reqId, const AgentError &err)
{
    if (reqId != m_pendingReqId) return;
    m_response->setHtml(QStringLiteral("<span style='color:#f44747'>Error: %1</span>")
                        .arg(err.message.toHtmlEscaped()));
    m_response->setVisible(true);
    m_pendingReqId.clear();
    setResponding(false);
}

// ── Private ───────────────────────────────────────────────────────────────────

void InlineChatWidget::setResponding(bool responding)
{
    m_sendBtn->setEnabled(!responding);
    m_cancelBtn->setEnabled(responding);
    m_input->setEnabled(!responding);
    if (!responding) m_input->setFocus();
}

QString InlineChatWidget::extractCode(const QString &markdown)
{
    // Match fenced code blocks: ``` optional-lang \n code \n ```
    static const QRegularExpression re(
        QStringLiteral("```[^\\n]*\\n([\\s\\S]*?)```"),
        QRegularExpression::MultilineOption);

    const auto match = re.match(markdown);
    if (match.hasMatch())
        return match.captured(1).trimmed();

    return {};
}

QString InlineChatWidget::buildDiffHtml(const QString &original, const QString &modified) const
{
    const QStringList oldLines = original.split(QLatin1Char('\n'));
    const QStringList newLines = modified.split(QLatin1Char('\n'));

    // Simple LCS-based diff
    const int M = oldLines.size();
    const int N = newLines.size();

    // Build LCS table
    QVector<QVector<int>> dp(M + 1, QVector<int>(N + 1, 0));
    for (int i = 1; i <= M; ++i)
        for (int j = 1; j <= N; ++j)
            dp[i][j] = (oldLines[i-1] == newLines[j-1])
                ? dp[i-1][j-1] + 1
                : qMax(dp[i-1][j], dp[i][j-1]);

    // Backtrack to produce diff lines
    struct DiffLine { char type; QString text; };
    QList<DiffLine> diff;
    int i = M, j = N;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && oldLines[i-1] == newLines[j-1]) {
            diff.prepend({' ', oldLines[i-1]});
            --i; --j;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            diff.prepend({'+', newLines[j-1]});
            --j;
        } else {
            diff.prepend({'-', oldLines[i-1]});
            --i;
        }
    }

    // Render to HTML
    QString html = QStringLiteral(
        "<div style='font-family:monospace; font-size:12px; background:#1e1e1e; "
        "padding:6px; border-radius:4px; white-space:pre; overflow-x:auto;'>");

    for (const auto &dl : diff) {
        const QString escaped = dl.text.toHtmlEscaped();
        switch (dl.type) {
        case '+':
            html += QStringLiteral(
                "<div style='background:#1e3a1e; color:#4ec94e;'>"
                "+ %1</div>").arg(escaped);
            break;
        case '-':
            html += QStringLiteral(
                "<div style='background:#3a1e1e; color:#e05252;'>"
                "- %1</div>").arg(escaped);
            break;
        default:
            html += QStringLiteral(
                "<div style='color:#888;'>"
                "  %1</div>").arg(escaped);
            break;
        }
    }
    html += QStringLiteral("</div>");
    return html;
}

QList<EditChunk> InlineChatWidget::extractMultiChunks(const QString &markdown)
{
    // Match line-annotated code blocks: ```lang:L<start>-L<end>\ncode\n```
    static const QRegularExpression re(
        QStringLiteral("```[^:\\n]*:L(\\d+)-L(\\d+)\\s*\\n([\\s\\S]*?)```"),
        QRegularExpression::MultilineOption);

    QList<EditChunk> chunks;
    auto it = re.globalMatch(markdown);
    while (it.hasNext()) {
        const auto m = it.next();
        EditChunk chunk;
        chunk.startLine = m.captured(1).toInt();
        chunk.endLine   = m.captured(2).toInt();
        chunk.newCode   = m.captured(3);
        // Remove trailing newline if present
        if (chunk.newCode.endsWith(QLatin1Char('\n')))
            chunk.newCode.chop(1);
        if (chunk.startLine > 0 && chunk.endLine >= chunk.startLine)
            chunks.append(chunk);
    }

    // Only return if we found at least 2 chunks (otherwise fall back to single-chunk)
    if (chunks.size() < 2)
        return {};

    // Sort by startLine descending so we can apply bottom-up without shifting
    std::sort(chunks.begin(), chunks.end(),
              [](const EditChunk &a, const EditChunk &b) {
                  return a.startLine < b.startLine;
              });

    return chunks;
}

QString InlineChatWidget::buildMultiDiffHtml(const QList<EditChunk> &chunks) const
{
    const QStringList fileLines = m_fileContent.split(QLatin1Char('\n'));

    QString html = QStringLiteral(
        "<div style='font-family:monospace; font-size:12px; background:#1e1e1e; "
        "padding:6px; border-radius:4px; white-space:pre; overflow-x:auto;'>");

    // Title
    html += QStringLiteral(
        "<div style='color:#569cd6; margin-bottom:6px; font-weight:bold;'>"
        "── %1 edit(s) ──</div>").arg(chunks.size());

    for (const auto &chunk : chunks) {
        const int s = qMax(1, chunk.startLine) - 1;
        const int e = qMin(fileLines.size(), chunk.endLine);

        // Chunk header
        html += QStringLiteral(
            "<div style='color:#569cd6; margin-top:8px;'>"
            "@ Lines %1–%2 @</div>").arg(chunk.startLine).arg(chunk.endLine);

        // Show 2 context lines before
        for (int i = qMax(0, s - 2); i < s; ++i) {
            html += QStringLiteral(
                "<div style='color:#666;'>  %1</div>")
                .arg(fileLines[i].toHtmlEscaped());
        }

        // Removed lines (old code)
        for (int i = s; i < e; ++i) {
            html += QStringLiteral(
                "<div style='background:#3a1e1e; color:#e05252;'>"
                "- %1</div>").arg(fileLines[i].toHtmlEscaped());
        }

        // Added lines (new code)
        const QStringList newLines = chunk.newCode.split(QLatin1Char('\n'));
        for (const auto &nl : newLines) {
            html += QStringLiteral(
                "<div style='background:#1e3a1e; color:#4ec94e;'>"
                "+ %1</div>").arg(nl.toHtmlEscaped());
        }

        // Show 2 context lines after
        for (int i = e; i < qMin(fileLines.size(), e + 2); ++i) {
            html += QStringLiteral(
                "<div style='color:#666;'>  %1</div>")
                .arg(fileLines[i].toHtmlEscaped());
        }
    }

    html += QStringLiteral("</div>");
    return html;
}
