#include "quickchatdialog.h"
#include "agentorchestrator.h"

#include <QLabel>
#include <QScrollBar>
#include <QUuid>

QuickChatDialog::QuickChatDialog(AgentOrchestrator *orchestrator, QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint)
    , m_orchestrator(orchestrator)
    , m_input(new QPlainTextEdit(this))
    , m_output(new QPlainTextEdit(this))
    , m_sendBtn(new QPushButton(tr("Ask"), this))
{
    setWindowTitle(tr("Quick Chat"));
    resize(500, 300);
    setAttribute(Qt::WA_DeleteOnClose, false);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    auto *title = new QLabel(tr("\u2728 Quick Chat"), this);
    title->setStyleSheet(QStringLiteral(
        "font-weight:bold;font-size:13px;color:#cccccc;padding:2px 0;"));
    layout->addWidget(title);

    m_output->setReadOnly(true);
    m_output->setPlaceholderText(tr("Response will appear here..."));
    m_output->setStyleSheet(QStringLiteral(
        "background:#1e1e1e;color:#d4d4d4;border:1px solid #3a3d41;"
        "border-radius:4px;padding:6px;font-family:monospace;font-size:12px;"));
    layout->addWidget(m_output, 1);

    m_input->setMaximumHeight(60);
    m_input->setPlaceholderText(tr("Ask a quick question..."));
    m_input->setStyleSheet(QStringLiteral(
        "background:#2d2d2d;color:#d4d4d4;border:1px solid #3a3d41;"
        "border-radius:4px;padding:6px;font-size:12px;"));
    layout->addWidget(m_input);

    m_sendBtn->setStyleSheet(QStringLiteral(
        "background:#0e639c;color:white;border:none;border-radius:4px;"
        "padding:6px 16px;font-weight:bold;"));
    layout->addWidget(m_sendBtn);

    setStyleSheet(QStringLiteral(
        "QuickChatDialog{background:#252526;border:1px solid #3a3d41;"
        "border-radius:8px;}"));

    connect(m_sendBtn, &QPushButton::clicked, this, &QuickChatDialog::onSend);

    connect(m_orchestrator, &AgentOrchestrator::responseDelta,
            this, &QuickChatDialog::onResponseDelta);
    connect(m_orchestrator, &AgentOrchestrator::responseFinished,
            this, &QuickChatDialog::onResponseFinished);
    connect(m_orchestrator, &AgentOrchestrator::responseError,
            this, &QuickChatDialog::onResponseError);
}

void QuickChatDialog::setEditorContext(const QString &filePath,
                                        const QString &selectedText,
                                        const QString &languageId)
{
    m_filePath     = filePath;
    m_selectedText = selectedText;
    m_languageId   = languageId;
}

void QuickChatDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    if (event->key() == Qt::Key_Return && !event->modifiers().testFlag(Qt::ShiftModifier)) {
        if (m_input->hasFocus()) {
            onSend();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

void QuickChatDialog::onSend()
{
    const QString text = m_input->toPlainText().trimmed();
    if (text.isEmpty()) return;

    if (!m_orchestrator->activeProvider()) {
        m_output->setPlainText(tr("No AI provider available."));
        return;
    }

    m_output->clear();
    m_accum.clear();
    m_sendBtn->setEnabled(false);
    m_input->setEnabled(false);

    m_activeRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    AgentRequest req;
    req.requestId      = m_activeRequestId;
    req.intent         = AgentIntent::Chat;
    req.agentMode      = false;
    req.userPrompt     = text;
    req.activeFilePath = m_filePath;
    req.selectedText   = m_selectedText;
    req.languageId     = m_languageId;

    m_orchestrator->sendRequest(req);
}

void QuickChatDialog::onResponseDelta(const QString &requestId, const QString &chunk)
{
    if (requestId != m_activeRequestId) return;
    m_accum += chunk;
    m_output->setPlainText(m_accum);
    m_output->verticalScrollBar()->setValue(m_output->verticalScrollBar()->maximum());
}

void QuickChatDialog::onResponseFinished(const QString &requestId,
                                          const AgentResponse &resp)
{
    if (requestId != m_activeRequestId) return;
    m_activeRequestId.clear();
    if (m_accum.isEmpty())
        m_output->setPlainText(resp.text);
    m_sendBtn->setEnabled(true);
    m_input->setEnabled(true);
    m_input->clear();
    m_input->setFocus();
}

void QuickChatDialog::onResponseError(const QString &requestId,
                                       const AgentError &error)
{
    if (requestId != m_activeRequestId) return;
    m_activeRequestId.clear();
    m_output->setPlainText(tr("Error: %1").arg(error.message));
    m_sendBtn->setEnabled(true);
    m_input->setEnabled(true);
}
