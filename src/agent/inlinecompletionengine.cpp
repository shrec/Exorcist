#include "inlinecompletionengine.h"
#include "../editor/editorview.h"
#include "../aiinterface.h"

#include <QTextBlock>
#include <QTextCursor>

// Forward-declared in the header; we need the concrete signal here.
// The provider exposes: void completionReady(const QString &suggestion);
// and: void requestCompletion(filePath, languageId, textBefore, textAfter);
// We communicate via QMetaObject::invokeMethod since we only hold IAgentProvider*.

InlineCompletionEngine::InlineCompletionEngine(QObject *parent)
    : QObject(parent)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(500);   // 500 ms after last keystroke
    connect(&m_debounce, &QTimer::timeout, this, &InlineCompletionEngine::requestCompletion);
}

void InlineCompletionEngine::attachEditor(EditorView *editor,
                                          const QString &filePath,
                                          const QString &languageId)
{
    detachEditor();

    m_editor     = editor;
    m_filePath   = filePath;
    m_languageId = languageId;

    m_textConn = connect(m_editor, &QPlainTextEdit::textChanged,
                         this, &InlineCompletionEngine::onTextChanged);
    m_cursorConn = connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
                           this, &InlineCompletionEngine::onCursorChanged);
    m_acceptConn = connect(m_editor, &EditorView::ghostTextAccepted,
                           this, [this] { m_waitingReply = false; });
    m_dismissConn = connect(m_editor, &EditorView::ghostTextDismissed,
                            this, [this] { m_waitingReply = false; });
}

void InlineCompletionEngine::detachEditor()
{
    if (!m_editor)
        return;

    m_debounce.stop();
    disconnect(m_textConn);
    disconnect(m_cursorConn);
    disconnect(m_acceptConn);
    disconnect(m_dismissConn);
    m_editor->clearGhostText();
    m_editor = nullptr;
}

void InlineCompletionEngine::setProvider(IAgentProvider *provider)
{
    // Disconnect previous provider's completionReady signal
    disconnect(m_completionConn);
    m_provider = provider;

    if (m_provider) {
        // Connect the completionReady signal dynamically
        m_completionConn = connect(m_provider, SIGNAL(completionReady(QString)),
                                   this, SLOT(onCompletionReady(QString)));
    }
}

void InlineCompletionEngine::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        m_debounce.stop();
        if (m_editor)
            m_editor->clearGhostText();
    }
}

void InlineCompletionEngine::onTextChanged()
{
    if (!m_enabled || !m_provider || !isLanguageEnabled(m_languageId))
        return;

    // Dismiss current ghost text on edit and restart the debounce timer
    if (m_editor->hasGhostText())
        m_editor->clearGhostText();

    m_debounce.start();
}

void InlineCompletionEngine::onCursorChanged()
{
    // If the cursor moves (arrow keys, mouse click), dismiss ghost text
    if (m_editor && m_editor->hasGhostText())
        m_editor->clearGhostText();
}

void InlineCompletionEngine::requestCompletion()
{
    if (!m_editor || !m_provider || !m_enabled || !isLanguageEnabled(m_languageId))
        return;

    // Don't fire another request if one is in flight
    if (m_waitingReply)
        return;

    const QTextCursor cur = m_editor->textCursor();

    // Gather context: text before and after cursor
    const int pos = cur.position();
    const QString all = m_editor->toPlainText();

    // Limit context to ~4000 chars before/after for speed
    constexpr int kMaxContext = 4000;
    const QString textBefore = all.mid(qMax(0, pos - kMaxContext), qMin(pos, kMaxContext));
    const QString textAfter  = all.mid(pos, kMaxContext);

    // Skip if there's no meaningful context
    if (textBefore.trimmed().isEmpty())
        return;

    m_waitingReply = true;

    // Temporarily switch to the completion-specific model if configured
    QString savedModel;
    if (!m_completionModel.isEmpty() && m_completionModel != m_provider->currentModel()) {
        savedModel = m_provider->currentModel();
        m_provider->setModel(m_completionModel);
    }

    QMetaObject::invokeMethod(m_provider, "requestCompletion",
                              Q_ARG(QString, m_filePath),
                              Q_ARG(QString, m_languageId),
                              Q_ARG(QString, textBefore),
                              Q_ARG(QString, textAfter));

    // Restore original model after request dispatch
    if (!savedModel.isEmpty())
        m_provider->setModel(savedModel);
}

void InlineCompletionEngine::onCompletionReady(const QString &suggestion)
{
    m_waitingReply = false;

    if (!m_editor || suggestion.isEmpty() || !m_enabled)
        return;

    m_editor->setGhostText(suggestion);
}

void InlineCompletionEngine::triggerCompletion()
{
    m_debounce.stop();
    requestCompletion();
}
