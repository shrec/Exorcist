#include "lspeditorbridge.h"

#include <QAction>
#include <QEvent>
#include <QInputDialog>
#include <QJsonObject>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QToolTip>
#include <QUrl>

#include "../editor/editorview.h"
#include "completionpopup.h"
#include "hovertooltipwidget.h"
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
      m_hoverTimer(new QTimer(this)),
      m_inlayHintTimer(new QTimer(this)),
      m_completion(new CompletionPopup(editor)),
      m_hoverTooltip(new HoverTooltipWidget(editor))
{
    // Debounce: fire didChange 300 ms after the last keystroke
    m_changeTimer->setSingleShot(true);
    m_changeTimer->setInterval(300);
    connect(m_changeTimer, &QTimer::timeout, this, &LspEditorBridge::sendDidChange);

    // Symbol outline: re-request 1 s after the last change
    m_symbolTimer->setSingleShot(true);
    m_symbolTimer->setInterval(1000);
    connect(m_symbolTimer, &QTimer::timeout, this, &LspEditorBridge::sendDocumentSymbols);

    // Hover timer: 500ms debounce
    m_hoverTimer->setSingleShot(true);
    m_hoverTimer->setInterval(500);
    connect(m_hoverTimer, &QTimer::timeout, this, [this]() {
        if (m_hoverLine >= 0)
            m_client->requestHover(m_uri, m_hoverLine, m_hoverCol);
    });

    // Inlay hints: re-request 500ms after last change or scroll
    m_inlayHintTimer->setSingleShot(true);
    m_inlayHintTimer->setInterval(500);
    connect(m_inlayHintTimer, &QTimer::timeout,
            this, &LspEditorBridge::requestInlayHints);

    // Install event filter on editor viewport for mouse tracking hover
    m_editor->viewport()->setMouseTracking(true);
    m_editor->viewport()->installEventFilter(this);

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
    connect(m_client, &LspClient::declarationResult,
            this, &LspEditorBridge::onDeclarationResult);
    connect(m_client, &LspClient::referencesResult,
            this, &LspEditorBridge::onReferencesResult);
    connect(m_client, &LspClient::renameResult,
            this, &LspEditorBridge::onRenameResult);
    connect(m_client, &LspClient::documentSymbolsResult,
            this, &LspEditorBridge::onDocumentSymbolsResult);
    connect(m_client, &LspClient::codeActionResult,
            this, &LspEditorBridge::onCodeActionResult);
    connect(m_client, &LspClient::inlayHintsResult,
            this, &LspEditorBridge::onInlayHintsResult);
    connect(m_client, &LspClient::typeDefinitionResult,
            this, &LspEditorBridge::onTypeDefinitionResult);

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
    connect(m_editor, &EditorView::goToDeclarationRequested, this, [this]() {
        const QTextCursor cur = m_editor->textCursor();
        m_client->requestDeclaration(m_uri, cur.blockNumber(),
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

    // Ctrl+. → code actions (quick-fixes, refactors)
    auto *codeActionAction = new QAction(editor);
    codeActionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Period));
    codeActionAction->setShortcutContext(Qt::WidgetShortcut);
    connect(codeActionAction, &QAction::triggered,
            this, &LspEditorBridge::requestCodeActions);
    editor->addAction(codeActionAction);

    // Ctrl+Shift+D → go to type definition
    auto *typeDefAction = new QAction(editor);
    typeDefAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    typeDefAction->setShortcutContext(Qt::WidgetShortcut);
    connect(typeDefAction, &QAction::triggered, this, [this]() {
        const QTextCursor cur = m_editor->textCursor();
        m_client->requestTypeDefinition(m_uri, cur.blockNumber(),
                                        cur.positionInBlock());
    });
    editor->addAction(typeDefAction);

    // Re-request inlay hints on scroll
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() { m_inlayHintTimer->start(); });

    // Send initial open notification, then request symbols
    if (m_client->isInitialized()) {
        m_client->didOpen(m_uri, m_languageId,
                          m_editor->toPlainText(), m_version);
        m_opened = true;
        QTimer::singleShot(500, this, &LspEditorBridge::sendDocumentSymbols);
        QTimer::singleShot(800, this, &LspEditorBridge::requestInlayHints);
    } else {
        connect(m_client, &LspClient::initialized, this, [this]() {
            m_client->didOpen(m_uri, m_languageId,
                              m_editor->toPlainText(), m_version);
            m_opened = true;
            QTimer::singleShot(500, this, &LspEditorBridge::sendDocumentSymbols);
            QTimer::singleShot(800, this, &LspEditorBridge::requestInlayHints);
        }, Qt::SingleShotConnection);
    }
}

LspEditorBridge::~LspEditorBridge()
{
    m_completion->dismiss();
    m_hoverTooltip->hideTooltip();
    m_client->didClose(m_uri);
}

// ── Event filter (mouse hover on viewport) ────────────────────────────────────

bool LspEditorBridge::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_editor->viewport()) {
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            const QTextCursor cur = m_editor->cursorForPosition(me->pos());
            const int line = cur.blockNumber();
            const int col  = cur.positionInBlock();

            // Only re-request if position changed
            if (line != m_hoverLine || col != m_hoverCol) {
                m_hoverLine = line;
                m_hoverCol  = col;
                m_hoverGlobalPos = me->globalPosition().toPoint();
                m_hoverTooltip->hideTooltip();
                m_hoverTimer->start();
            }
        } else if (event->type() == QEvent::Leave) {
            m_hoverTimer->stop();
            m_hoverTooltip->hideTooltip();
            m_hoverLine = -1;
            m_hoverCol  = -1;
        }
    }
    return QObject::eventFilter(obj, event);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void LspEditorBridge::onDocumentChanged()
{
    m_changeTimer->start();  // restart debounce
    m_symbolTimer->start();  // restart symbol outline refresh
    m_inlayHintTimer->start(); // refresh inlay hints after edit
    m_hoverTimer->stop();
    m_hoverTooltip->hideTooltip();

    // Check last typed character to decide on triggers
    if (m_completion->isVisible()) {
        // Re-filter by the word at cursor
        const QTextCursor cur = m_editor->textCursor();
        QTextCursor word = cur;
        word.select(QTextCursor::WordUnderCursor);
        const QString prefix = word.selectedText();

        if (m_completion->lastResponseIncomplete()) {
            // Server indicated more results exist — re-request instead of
            // filtering locally so clangd can return a more relevant set.
            m_changeTimer->stop();
            sendDidChange();
            const QTextCursor tc = m_editor->textCursor();
            m_client->requestCompletion(m_uri, tc.blockNumber(),
                                        tc.positionInBlock(),
                                        3); // TriggerForIncompleteCompletions
        } else {
            m_completion->filterByPrefix(prefix);
        }
    }

    // Check trigger characters for auto-completion
    const QTextCursor cur = m_editor->textCursor();
    if (cur.position() > 0) {
        const QChar prevChar = m_editor->document()
            ->characterAt(cur.position() - 1);
        if (isCompletionTrigger(prevChar)) {
            m_changeTimer->stop();
            sendDidChange();   // flush immediately before requesting
            // triggerKind 2 = TriggerCharacter, pass the actual trigger
            const QTextCursor tc = m_editor->textCursor();
            m_client->requestCompletion(m_uri, tc.blockNumber(),
                                        tc.positionInBlock(),
                                        2, QString(prevChar));
        }
        // Signature help on '('
        if (prevChar == '(') {
            m_changeTimer->stop();
            sendDidChange();
            const int line = cur.blockNumber();
            const int col  = cur.positionInBlock();
            m_client->requestSignatureHelp(m_uri, line, col);
        }

        // Format on type: \n, }, ;
        if (isFormatOnTypeTrigger(prevChar)) {
            m_changeTimer->stop();
            sendDidChange();
            m_formatReqVersion = m_version;
            const QTextCursor tc = m_editor->textCursor();
            m_client->requestOnTypeFormatting(
                m_uri, tc.blockNumber(), tc.positionInBlock(),
                QString(prevChar));
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
    if (!m_opened) return;
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
    // C++ triggers: . -> :: < " (for #include)
    if (m_languageId == "cpp" || m_languageId == "c")
        return ch == '.' || ch == '>' || ch == ':' || ch == '<' || ch == '"'
               || ch == '/';
    // Python
    if (m_languageId == "python")
        return ch == '.';
    // JS/TS/Rust/Go etc.
    return ch == '.';
}

bool LspEditorBridge::isFormatOnTypeTrigger(QChar ch) const
{
    if (m_languageId == "cpp" || m_languageId == "c")
        return ch == '\n' || ch == '}' || ch == ';';
    return ch == '\n';
}

// ── LSP result slots ──────────────────────────────────────────────────────────

void LspEditorBridge::onCompletionResult(const QString &uri, int /*line*/,
                                         int /*character*/,
                                         const QJsonArray &items,
                                         bool isIncomplete)
{
    if (uri != m_uri || items.isEmpty()) return;
    m_completion->showForEditor(m_editor, items, isIncomplete);
}

void LspEditorBridge::onHoverResult(const QString &uri, int /*line*/,
                                    int /*character*/,
                                    const QString &markdown)
{
    if (uri != m_uri || markdown.isEmpty()) return;
    m_hoverTooltip->showTooltip(m_hoverGlobalPos, markdown, m_editor);
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
    m_currentDiagnostics = diags;
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

void LspEditorBridge::onDeclarationResult(const QString &uri,
                                          const QJsonArray &locations)
{
    if (uri != m_uri || locations.isEmpty()) return;

    const QJsonObject loc = locations[0].toObject();

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

void LspEditorBridge::requestCodeActions()
{
    const QTextCursor cur = m_editor->textCursor();
    const int line = cur.blockNumber();
    const int col  = cur.positionInBlock();

    // Collect diagnostics whose range overlaps the cursor line
    QJsonArray relevantDiags;
    for (const QJsonValue &v : std::as_const(m_currentDiagnostics)) {
        const QJsonObject d    = v.toObject();
        const QJsonObject range = d["range"].toObject();
        const int startLine    = range["start"].toObject()["line"].toInt();
        const int endLine      = range["end"].toObject()["line"].toInt();
        if (line >= startLine && line <= endLine)
            relevantDiags.append(d);
    }

    m_client->requestCodeAction(m_uri, line, col, line, col, relevantDiags);
}

void LspEditorBridge::onCodeActionResult(const QString &uri, int /*line*/,
                                         int /*character*/,
                                         const QJsonArray &actions)
{
    if (uri != m_uri || actions.isEmpty()) return;

    auto *menu = new QMenu(m_editor);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    for (const QJsonValue &v : actions) {
        const QJsonObject action = v.isObject() ? v.toObject()
                                                : v.toObject(); // Command or CodeAction
        const QString title = action["title"].toString();
        if (title.isEmpty()) continue;

        QAction *item = menu->addAction(title);

        // Capture the edit or command for this action
        const QJsonObject edit    = action["edit"].toObject();
        const QJsonObject command = action["command"].toObject();

        connect(item, &QAction::triggered, this, [this, edit, command]() {
            if (!edit.isEmpty()) {
                emit workspaceEditReady(edit);
            } else if (!command.isEmpty()) {
                // Fallback: re-request with command execution not yet supported
                qWarning("LspEditorBridge: code action command execution not yet supported");
            }
        });
    }

    if (menu->isEmpty()) {
        menu->deleteLater();
        return;
    }

    // Show just below the cursor
    const QRect curRect = m_editor->cursorRect();
    const QPoint globalPos = m_editor->viewport()->mapToGlobal(
        curRect.bottomLeft());
    menu->popup(globalPos);
}

// ── Inlay Hints ──────────────────────────────────────────────────────────────

void LspEditorBridge::requestInlayHints()
{
    if (!m_client->isInitialized()) return;

    // Compute visible range
    const int startLine = m_editor->firstVisibleBlockNumber();
    // Estimate visible line count from viewport height
    const int lineH = m_editor->fontMetrics().height();
    const int visibleLines = lineH > 0 ? (m_editor->viewport()->height() / lineH) + 2 : 50;
    const int endLine = startLine + visibleLines;

    m_client->requestInlayHints(m_uri, startLine, 0, endLine, 0);
}

void LspEditorBridge::onInlayHintsResult(const QString &uri,
                                         const QJsonArray &hints)
{
    if (uri != m_uri) return;

    QList<EditorView::InlayHint> parsed;
    parsed.reserve(hints.size());

    for (const QJsonValue &v : hints) {
        const QJsonObject h = v.toObject();
        const QJsonObject pos = h[QLatin1String("position")].toObject();

        EditorView::InlayHint hint;
        hint.line      = pos[QLatin1String("line")].toInt();
        hint.character = pos[QLatin1String("character")].toInt();

        // Label can be a string or an array of InlayHintLabelPart
        const QJsonValue labelVal = h[QLatin1String("label")];
        if (labelVal.isString()) {
            hint.label = labelVal.toString();
        } else if (labelVal.isArray()) {
            QString combined;
            for (const QJsonValue &part : labelVal.toArray())
                combined += part.toObject()[QLatin1String("value")].toString();
            hint.label = combined;
        }

        hint.paddingLeft  = h[QLatin1String("paddingLeft")].toBool();
        hint.paddingRight = h[QLatin1String("paddingRight")].toBool();

        if (!hint.label.isEmpty())
            parsed.append(hint);
    }

    m_editor->setInlayHints(parsed);
}

// ── Type Definition ──────────────────────────────────────────────────────────

void LspEditorBridge::onTypeDefinitionResult(const QString &uri,
                                             const QJsonArray &locations)
{
    if (uri != m_uri || locations.isEmpty()) return;

    // Navigate to the first type definition location (same as definition handler)
    const QJsonObject loc = locations.first().toObject();
    const QString targetUri = loc[QLatin1String("uri")].toString();
    const QString filePath  = QUrl(targetUri).toLocalFile();
    const int line = loc[QLatin1String("range")].toObject()
                        [QLatin1String("start")].toObject()
                        [QLatin1String("line")].toInt();
    const int character = loc[QLatin1String("range")].toObject()
                             [QLatin1String("start")].toObject()
                             [QLatin1String("character")].toInt();

    if (!filePath.isEmpty())
        emit navigateToLocation(filePath, line, character);
}
