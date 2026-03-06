#include "agentchatpanel.h"
#include "agentorchestrator.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextBrowser>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

AgentChatPanel::AgentChatPanel(AgentOrchestrator *orchestrator, QWidget *parent)
    : QWidget(parent),
      m_orchestrator(orchestrator),
      m_providerCombo(new QComboBox(this)),
      m_statusDot(new QLabel(this)),
      m_history(new QTextBrowser(this)),
      m_input(new QLineEdit(this)),
      m_sendBtn(new QToolButton(this)),
      m_cancelBtn(new QToolButton(this))
{
    // ── Provider bar ──────────────────────────────────────────────────────
    m_statusDot->setFixedSize(10, 10);
    m_statusDot->setToolTip(tr("Provider status"));

    m_providerCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *providerBar = new QHBoxLayout;
    providerBar->setContentsMargins(0, 0, 0, 0);
    providerBar->addWidget(m_statusDot);
    providerBar->addWidget(m_providerCombo, 1);

    // ── Chat history ──────────────────────────────────────────────────────
    m_history->setOpenExternalLinks(false);
    m_history->setReadOnly(true);
    m_history->document()->setDefaultStyleSheet(
        "body { color: #dcdcdc; font-family: monospace; }"
        ".user    { color: #9cdcfe; font-weight: bold; }"
        ".ai      { color: #dcdcdc; }"
        ".system  { color: #6a9955; font-style: italic; }"
        ".error   { color: #f44747; }"
        "pre      { background: #1e1e1e; padding: 4px; border-radius: 3px; }");

    // ── Input bar ─────────────────────────────────────────────────────────
    m_input->setPlaceholderText(tr("Ask AI…  (Enter to send)"));
    m_sendBtn->setText(tr("Send"));
    m_cancelBtn->setText(tr("✕"));
    m_cancelBtn->setToolTip(tr("Cancel request"));
    m_cancelBtn->setEnabled(false);

    auto *inputBar = new QHBoxLayout;
    inputBar->setContentsMargins(0, 0, 0, 0);
    inputBar->addWidget(m_input, 1);
    inputBar->addWidget(m_sendBtn);
    inputBar->addWidget(m_cancelBtn);

    // ── Root layout ───────────────────────────────────────────────────────
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(4);
    root->addLayout(providerBar);
    root->addWidget(m_history, 1);
    root->addLayout(inputBar);

    // ── Connections — UI ──────────────────────────────────────────────────
    connect(m_sendBtn,      &QToolButton::clicked,     this, &AgentChatPanel::onSend);
    connect(m_cancelBtn,    &QToolButton::clicked,     this, &AgentChatPanel::onCancel);
    connect(m_input,        &QLineEdit::returnPressed,  this, &AgentChatPanel::onSend);
    connect(m_providerCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgentChatPanel::onProviderSelected);

    // ── Connections — orchestrator ────────────────────────────────────────
    connect(m_orchestrator, &AgentOrchestrator::providerRegistered,
            this, &AgentChatPanel::onProviderRegistered);
    connect(m_orchestrator, &AgentOrchestrator::providerRemoved,
            this, &AgentChatPanel::onProviderRemoved);
    connect(m_orchestrator, &AgentOrchestrator::activeProviderChanged,
            this, &AgentChatPanel::onActiveProviderChanged);
    connect(m_orchestrator, &AgentOrchestrator::responseDelta,
            this, &AgentChatPanel::onResponseDelta);
    connect(m_orchestrator, &AgentOrchestrator::responseFinished,
            this, &AgentChatPanel::onResponseFinished);
    connect(m_orchestrator, &AgentOrchestrator::responseError,
            this, &AgentChatPanel::onResponseError);

    refreshProviderList();
    updateStatusDot();
}

// ── Provider list ─────────────────────────────────────────────────────────────

void AgentChatPanel::refreshProviderList()
{
    QSignalBlocker blocker(m_providerCombo);
    m_providerCombo->clear();

    const auto list = m_orchestrator->providers();
    if (list.isEmpty()) {
        m_providerCombo->addItem(tr("(no providers)"));
        m_providerCombo->setEnabled(false);
        appendMessage("system", tr("No AI providers registered. "
                                   "Install an agent plugin to get started."));
    } else {
        m_providerCombo->setEnabled(true);
        for (const IAgentProvider *p : list)
            m_providerCombo->addItem(p->displayName(), p->id());

        // Reflect active provider selection
        const IAgentProvider *active = m_orchestrator->activeProvider();
        if (active) {
            const int idx = m_providerCombo->findData(active->id());
            if (idx >= 0)
                m_providerCombo->setCurrentIndex(idx);
        }
    }
    updateStatusDot();
}

void AgentChatPanel::updateStatusDot()
{
    const IAgentProvider *p = m_orchestrator->activeProvider();
    const bool ok = p && p->isAvailable();
    const QString colour = ok ? "#4ec9b0" : "#555555";
    m_statusDot->setStyleSheet(
        QString("background:%1; border-radius:5px;").arg(colour));
    m_statusDot->setToolTip(ok ? tr("Provider available") : tr("Provider unavailable"));
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

void AgentChatPanel::onActiveProviderChanged(const QString &)
{
    updateStatusDot();
}

void AgentChatPanel::onProviderSelected(int index)
{
    const QString id = m_providerCombo->itemData(index).toString();
    if (!id.isEmpty())
        m_orchestrator->setActiveProvider(id);
    updateStatusDot();
}

void AgentChatPanel::onSend()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;

    const IAgentProvider *active = m_orchestrator->activeProvider();
    if (!active || !active->isAvailable()) {
        appendMessage("error", tr("No available provider. "
                                  "Select or configure a provider first."));
        return;
    }

    appendMessage("user", text.toHtmlEscaped());
    m_input->clear();
    setInputEnabled(false);

    m_pendingRequestId  = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_pendingAccum.clear();

    AgentRequest req;
    req.requestId      = m_pendingRequestId;
    req.intent         = AgentIntent::Chat;
    req.userPrompt     = text;
    req.activeFilePath = m_activeFilePath;
    req.selectedText   = m_selectedText;
    req.languageId     = m_languageId;

    m_orchestrator->sendRequest(req);
}

void AgentChatPanel::onCancel()
{
    if (!m_pendingRequestId.isEmpty()) {
        m_orchestrator->cancelRequest(m_pendingRequestId);
        m_pendingRequestId.clear();
        setInputEnabled(true);
        appendMessage("system", tr("Request cancelled."));
    }
}

void AgentChatPanel::onResponseDelta(const QString &requestId, const QString &chunk)
{
    if (requestId != m_pendingRequestId)
        return;
    m_pendingAccum += chunk;

    // Replace the last AI bubble with the updated accumulation
    QString html = m_history->toHtml();
    // Simple streaming: append a streaming indicator; onFinished replaces it
    Q_UNUSED(html)
    // For now just update the document cursor at the end
    m_history->moveCursor(QTextCursor::End);
    m_history->insertHtml(chunk.toHtmlEscaped());
}

void AgentChatPanel::onResponseFinished(const QString &requestId,
                                        const AgentResponse &response)
{
    if (requestId != m_pendingRequestId)
        return;

    // If we got non-streaming (no deltas), show the full response now.
    if (m_pendingAccum.isEmpty()) {
        appendMessage("ai", response.text.toHtmlEscaped());
    }
    // Streaming case: the history already has the text, just close the bubble.

    if (!response.proposedEdits.isEmpty()) {
        appendMessage("system",
                      tr("%1 edit(s) proposed. "
                         "Review and apply from the diff view.")
                          .arg(response.proposedEdits.size()));
    }

    m_pendingRequestId.clear();
    m_pendingAccum.clear();
    setInputEnabled(true);
}

void AgentChatPanel::onResponseError(const QString &requestId,
                                     const AgentError &error)
{
    if (requestId != m_pendingRequestId)
        return;

    appendMessage("error",
                  tr("Error: %1").arg(error.message.toHtmlEscaped()));
    m_pendingRequestId.clear();
    m_pendingAccum.clear();
    setInputEnabled(true);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void AgentChatPanel::appendMessage(const QString &role, const QString &html)
{
    const QString label = (role == "user")   ? tr("You")
                        : (role == "ai")     ? tr("AI")
                        : (role == "error")  ? tr("Error")
                                             : tr("System");

    m_history->append(
        QStringLiteral("<div><span class='%1'><b>%2</b></span>&nbsp;%3</div>")
            .arg(role, label, html));
}

void AgentChatPanel::setInputEnabled(bool enabled)
{
    m_input->setEnabled(enabled);
    m_sendBtn->setEnabled(enabled);
    m_cancelBtn->setEnabled(!enabled);
    if (enabled)
        m_input->setFocus();
}

void AgentChatPanel::setEditorContext(const QString &filePath,
                                      const QString &selectedText,
                                      const QString &languageId)
{
    m_activeFilePath = filePath;
    m_selectedText   = selectedText;
    m_languageId     = languageId;
}
