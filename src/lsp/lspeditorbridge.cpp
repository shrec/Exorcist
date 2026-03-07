#include "lspeditorbridge.h"

#include <QAction>
#include <QInputDialog>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineEdit>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QToolTip>
#include <QUrl>

#include "../editor/editorview.h"
#include "completionpopup.h"
#include "lspclient.h"

LspEditorBridge::LspEditorBridge(EditorView *editor, LspClient *client,
                                 const QString &filePath, QObject *parent)
    : QObject(parent),
      m_editor(editor),
      m_client(client),
      m_filePath(filePath),
      m_uri(LspClient::pathToUri(filePath)),
      m_languageId(LspClient::languageIdForPath(filePath)),
      m_version(1),
      m_changeTimer(new QTimer(this)),
      m_symbolTimer(new QTimer(this)),
      m_completion(new CompletionPopup(editor))
{
    // Debounce: fire didChange 300 ms after the last keystroke
    m_changeTimer->setSingleShot(true);
    m_changeTimer->setInterval(300);
    connect(m_changeTimer, &QTimer::timeout, this, &LspEditorBridge::sendDidChange);

    // Symbol outline: re-request 1 s after the last change
    m_symbolTimer->setSingleShot(true);
    m_symbolTimer->setInterval(1000);
    connect(m_symbolTimer, &QTimer::timeout, this, &LspEditorBridge::sendDocumentSymbols);

    // Editor signals
    connect(m_editor->document(), &QTextDocument::contentsChanged,
            this, &LspEditorBridge::onDocumentChanged);
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged,
            this, &LspEditorBridge::onCursorPositionChanged);

    // LSP client signals (filter by URI in slots)
    connect(m_client, &LspClient::completionResult,
            this, &LspEditorBridge::onCompletionResult);
    connect(m_client, &LspClient::hoverResult,
            this, &LspEditorBridge::onHoverResult);
    connect(m_client, &LspClient::signatureHelpResult,
            this, &LspEditorBridge::onSignatureHelpResult);
    connect(m_client, &LspClient::diagnosticsPublished,
            this, &LspEditorBridge::onDiagnosticsPublished);
    connect(m_client, &LspClient::formattingResult,
            this, &LspEditorBridge::onFormattingResult);
    connect(m_client, &LspClient::definitionResult,
            this, &LspEditorBridge::onDefinitionResult);
    connect(m_client, &LspClient::referencesResult,
            this, &LspEditorBridge::onReferencesResult);
    connect(m_client, &LspClient::renameResult,
            this, &LspEditorBridge::onRenameResult);
    connect(m_client, &LspClient::documentSymbolsResult,
            this, &LspEditorBridge::onDocumentSymbolsResult);

    // Completion popup
    connect(m_completion, &CompletionPopup::itemAccepted,
            this, &LspEditorBridge::onCompletionAccepted);

    // Ctrl+Space → trigger completion manually
    auto *completeAction = new QAction(editor);
    completeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Space));
    completeAction->setShortcutContext(Qt::WidgetShortcut);
    connect(completeAction, &QAction::triggered,
            this, &LspEditorBridge::triggerCompletion);
    editor->addAction(completeAction);

    // F12 → go to definition
    auto *gotoDefAction = new QAction(editor);
    gotoDefAction->setShortcut(QKeySequence(Qt::Key_F12));
    gotoDefAction->setShortcutContext(Qt::WidgetShortcut);
    connect(gotoDefAction, &QAction::triggered, this, [this]() {
        const QTextCursor cur = m_editor->textCursor();
        m_client->requestDefinition(m_uri, cur.blockNumber(),
                                    cur.positionInBlock());
    });
    editor->addAction(gotoDefAction);

    // Shift+F12 → find all references
    auto *refsAction = new QAction(editor);
    refsAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F12));
    refsAction->setShortcutContext(Qt::WidgetShortcut);
    connect(refsAction, &QAction::triggered, this, [this]() {
        const QTextCursor cur = m_editor->textCursor();
        m_client->requestReferences(m_uri, cur.blockNumber(),
                                    cur.positionInBlock());
    });
    editor->addAction(refsAction);

    // F2 → rename symbol
    auto *renameAction = new QAction(editor);
    renameAction->setShortcut(QKeySequence(Qt::Key_F2));
    renameAction->setShortcutContext(Qt::WidgetShortcut);
    connect(renameAction, &QAction::triggered, this, [this]() {
        QTextCursor word = m_editor->textCursor();
        word.select(QTextCursor::WordUnderCursor);
        const QString current = word.selectedText();
        if (current.isEmpty()) return;

        bool ok = false;
        const QString newName = QInputDialog::getText(
            m_editor, tr("Rename Symbol"),
            tr("New name for \"%1\":").arg(current),
            QLineEdit::Normal, current, &ok);
        if (!ok || newName.isEmpty() || newName == current) return;

        const QTextCursor cur = m_editor->textCursor();
        m_client->requestRename(m_uri, cur.blockNumber(),
                                cur.positionInBlock(), newName);
    });
    editor->addAction(renameAction);

    // Context menu signals from EditorView
    connect(m_editor, &EditorView::goToDefinitionRequested, this, [this]() {
        const QTextCursor cur = m_editor->textCursor();
        m_client->requestDefinition(m_uri, cur.blockNumber(),
                                    cur.positionInBlock());
    });
    connect(m_editor, &EditorView::findReferencesRequested, this, [this]() {
        const QTextCursor cur = m_editor->textCursor();
        m_client->requestReferences(m_uri, cur.blockNumber(),
                                    cur.positionInBlock());
    });
    connect(m_editor, &EditorView::renameSymbolRequested, renameAction,
            &QAction::trigger);
    connect(m_editor, &EditorView::formatDocumentRequested,
            this, &LspEditorBridge::formatDocument);

    // Ctrl+Shift+F → format document
    auto *formatAction = new QAction(editor);
    formatAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    formatAction->setShortcutContext(Qt::WidgetShortcut);
    connect(formatAction, &QAction::triggered,
            this, &LspEditorBridge::formatDocument);
    editor->addAction(formatAction);

    // Send initial open notification, then request symbols
    if (m_client->isInitialized()) {
        m_client->didOpen(m_uri, m_languageId,
                          m_editor->toPlainText(), m_version);
        QTimer::singleShot(500, this, &LspEditorBridge::sendDocumentSymbols);
    } else {
        connect(m_client, &LspClient::initialized, this, [this]() {
            m_client->didOpen(m_uri, m_languageId,
                              m_editor->toPlainText(), m_version);
            QTimer::singleShot(500, this, &LspEditorBridge::sendDocumentSymbols);
        }, Qt::SingleShotConnection);
    }
}

LspEditorBridge::~LspEditorBridge()
{
    m_completion->dismiss();
    m_client->didClose(m_uri);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void LspEditorBridge::onDocumentChanged()
{
    m_changeTimer->start();  // restart debounce
    m_symbolTimer->start();  // restart symbol outline refresh

    // Check last typed character to decide on triggers
    if (m_completion->isVisible()) {
        // Re-filter by the word at cursor
        const QTextCursor cur = m_editor->textCursor();
        QTextCursor word = cur;
        word.select(QTextCursor::WordUnderCursor);
        m_completion->filterByPrefix(word.selectedText());
    }

    // Check trigger characters for auto-completion
    const QTextCursor cur = m_editor->textCursor();
    if (cur.position() > 0) {
        const QChar prevChar = m_editor->document()
            ->characterAt(cur.position() - 1);
        if (isCompletionTrigger(prevChar)) {
            m_changeTimer->stop();
            sendDidChange();   // flush immediately before requesting
            triggerCompletion();
        }
        // Signature help on '('
        if (prevChar == '(') {
            m_changeTimer->stop();
            sendDidChange();
            const int line = cur.blockNumber();
            const int col  = cur.positionInBlock();
            m_client->requestSignatureHelp(m_uri, line, col);
        }
    }
}

void LspEditorBridge::onCursorPositionChanged()
{
    if (m_completion->isVisible()) {
        // Re-filter; dismiss if cursor moved to different line
        const QTextCursor cur = m_editor->textCursor();
        QTextCursor word = cur;
        word.select(QTextCursor::WordUnderCursor);
        m_completion->filterByPrefix(word.selectedText());
    }
}

void LspEditorBridge::sendDidChange()
{
    m_client->didChange(m_uri, m_editor->toPlainText(), ++m_version);
}

void LspEditorBridge::triggerCompletion()
{
    sendDidChange();
    const QTextCursor cur = m_editor->textCursor();
    m_client->requestCompletion(m_uri, cur.blockNumber(),
                                cur.positionInBlock());
}

bool LspEditorBridge::isCompletionTrigger(QChar ch) const
{
    // C++ triggers: . -> ::
    if (m_languageId == "cpp" || m_languageId == "c")
        return ch == '.' || ch == '>';
    // Python
    if (m_languageId == "python")
        return ch == '.';
    // JS/TS/Rust/Go etc.
    return ch == '.';
}

// ── LSP result slots ──────────────────────────────────────────────────────────

void LspEditorBridge::onCompletionResult(const QString &uri, int /*line*/,
                                         int /*character*/,
                                         const QJsonArray &items)
{
    if (uri != m_uri || items.isEmpty()) return;
    m_completion->showForEditor(m_editor, items);
}

void LspEditorBridge::onHoverResult(const QString &uri, int /*line*/,
                                    int /*character*/,
                                    const QString &markdown)
{
    if (uri != m_uri || markdown.isEmpty()) return;
    // Show as a tooltip at the current cursor position
    const QPoint globalPos = m_editor->viewport()->mapToGlobal(
        m_editor->cursorRect().topRight());
    QToolTip::showText(globalPos, markdown, m_editor);
}

void LspEditorBridge::onSignatureHelpResult(const QString &uri, int /*line*/,
                                            int /*character*/,
                                            const QJsonObject &result)
{
    if (uri != m_uri) return;
    const QJsonArray sigs = result["signatures"].toArray();
    if (sigs.isEmpty()) return;

    const QJsonObject sig    = sigs[0].toObject();
    const int activeParam    = result["activeParameter"].toInt(
                                   sig["activeParameter"].toInt(0));
    const QJsonArray params  = sig["parameters"].toArray();
    QString label            = sig["label"].toString();

    // Bold the active parameter if possible
    if (activeParam >= 0 && activeParam < params.size()) {
        const QJsonValue pLabel = params[activeParam].toObject()["label"];
        if (pLabel.isString()) {
            const QString p = pLabel.toString();
            label.replace(p, "<b>" + p + "</b>");
        }
    }

    const QPoint globalPos = m_editor->viewport()->mapToGlobal(
        m_editor->cursorRect().topLeft());
    QToolTip::showText(globalPos, label, m_editor);
}

void LspEditorBridge::onDiagnosticsPublished(const QString &uri,
                                             const QJsonArray &diags)
{
    if (uri != m_uri) return;
    m_editor->setDiagnostics(diags);
}

void LspEditorBridge::onFormattingResult(const QString &uri,
                                         const QJsonArray &edits)
{
    if (uri != m_uri || edits.isEmpty()) return;
    // Discard stale result: user typed more since we sent the request.
    if (m_version != m_formatReqVersion) return;
    m_editor->applyTextEdits(edits);
}

void LspEditorBridge::onDefinitionResult(const QString &uri,
                                         const QJsonArray &locations)
{
    if (uri != m_uri || locations.isEmpty()) return;

    const QJsonObject loc = locations[0].toObject();

    // LSP allows both Location {uri, range} and LocationLink {targetUri, targetSelectionRange}
    const QString targetUri = loc.contains("targetUri")
        ? loc["targetUri"].toString()
        : loc["uri"].toString();
    const QJsonObject range = loc.contains("targetSelectionRange")
        ? loc["targetSelectionRange"].toObject()
        : loc["range"].toObject();

    const QString filePath = QUrl(targetUri).toLocalFile();
    if (filePath.isEmpty()) return;

    const int line = range["start"].toObject()["line"].toInt();
    const int col  = range["start"].toObject()["character"].toInt();
    emit navigateToLocation(filePath, line, col);
}

void LspEditorBridge::onCompletionAccepted(const QString &insertText,
                                           const QString &filterText)
{
    // Remove the typed prefix (word under cursor) and insert the full text
    QTextCursor cur = m_editor->textCursor();
    cur.select(QTextCursor::WordUnderCursor);
    const QString selected = cur.selectedText();

    // Only replace if the selected word is a prefix of filterText
    if (filterText.startsWith(selected, Qt::CaseInsensitive)) {
        cur.removeSelectedText();
    }
    cur.insertText(insertText);
    m_editor->setTextCursor(cur);
}

void LspEditorBridge::onReferencesResult(const QString &uri,
                                         const QJsonArray &locations)
{
    if (uri != m_uri) return;
    QTextCursor word = m_editor->textCursor();
    word.select(QTextCursor::WordUnderCursor);
    emit referencesFound(word.selectedText(), locations);
}

void LspEditorBridge::onRenameResult(const QString &uri,
                                     const QJsonObject &workspaceEdit)
{
    if (uri != m_uri) return;
    emit workspaceEditReady(workspaceEdit);
}

void LspEditorBridge::onDocumentSymbolsResult(const QString &uri,
                                              const QJsonArray &symbols)
{
    if (uri != m_uri) return;
    emit symbolsUpdated(m_uri, symbols);
}

// ── Public ────────────────────────────────────────────────────────────────────

void LspEditorBridge::formatDocument()
{
    sendDidChange();
    m_formatReqVersion = m_version;
    m_client->requestFormatting(m_uri);
}

void LspEditorBridge::requestSymbols()
{
    if (m_client->isInitialized())
        m_client->requestDocumentSymbols(m_uri);
}

void LspEditorBridge::sendDocumentSymbols()
{
    m_client->requestDocumentSymbols(m_uri);
}
