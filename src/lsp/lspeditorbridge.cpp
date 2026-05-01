#include "lspeditorbridge.h"

#include <QAction>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QWidget>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonObject>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QScrollBar>
#include <QSettings>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QToolTip>
#include <QUrl>

#include "../editor/editorview.h"
#include "completionpopup.h"
#include "doxygengenerator.h"
#include "hovertooltipwidget.h"
#include "lspclient.h"

#include <QMainWindow>
#include <QStatusBar>

// ── CodeLensOverlay ──────────────────────────────────────────────────────────
//
// A transparent child widget that sits on top of the editor's viewport and
// paints code-lens labels just above function lines. Click forwarding falls
// back to the editor for empty regions; clicks on a lens hit-rect execute the
// associated LSP command.

class CodeLensOverlay : public QWidget
{
public:
    explicit CodeLensOverlay(EditorView *editor, LspClient *client)
        : QWidget(editor->viewport()),
          m_editor(editor),
          m_client(client)
    {
        // Mouse events fall through to the editor by default; clicks on
        // a lens hit-rect are handled by the bridge's viewport eventFilter
        // which forwards to executeCommand.
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(editor->viewport()->rect());
        editor->viewport()->installEventFilter(this);
        show();
        raise();
    }

    const QVector<LspCodeLens> &lenses() const { return m_lenses; }

    void setLenses(const QVector<LspCodeLens> &lenses)
    {
        m_lenses = lenses;
        update();
    }

    void clear()
    {
        m_lenses.clear();
        update();
    }

protected:
    bool eventFilter(QObject *obj, QEvent *event) override
    {
        if (obj == m_editor->viewport()
            && event->type() == QEvent::Resize) {
            setGeometry(m_editor->viewport()->rect());
            raise();
        }
        return QWidget::eventFilter(obj, event);
    }

    void paintEvent(QPaintEvent * /*event*/) override
    {
        if (m_lenses.isEmpty()) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        QFont f = m_editor->font();
        f.setItalic(true);
        int sz = f.pointSize();
        if (sz <= 0) sz = QFontMetrics(m_editor->font()).height() * 3 / 4;
        f.setPointSize(qMax(7, sz - 1));
        p.setFont(f);
        p.setPen(QPen(QColor(0x7e, 0x7e, 0x7e)));

        const QFontMetrics fm(f);
        const int viewportH = height();

        for (LspCodeLens &lens : m_lenses) {
            const QTextBlock block =
                m_editor->document()->findBlockByNumber(lens.line);
            if (!block.isValid() || !block.isVisible()) {
                lens.hitRect = QRect();
                continue;
            }
            // QPlainTextEdit::cursorRect gives viewport-relative coords for a
            // QTextCursor anchored at the block; this avoids touching protected
            // QPlainTextEdit geometry helpers.
            QTextCursor anchor(block);
            const QRect cr = m_editor->cursorRect(anchor);
            if (cr.bottom() < 0 || cr.top() > viewportH) {
                lens.hitRect = QRect();
                continue;
            }

            const int textW = fm.horizontalAdvance(lens.title);
            const int indentX = lens.character > 0
                ? fm.horizontalAdvance(QString(lens.character, ' '))
                : 0;
            const int x = cr.left() + indentX;
            const int baselineY = cr.top() - 2;
            if (baselineY - fm.ascent() < 0) {
                // No room above this line in the viewport; skip.
                lens.hitRect = QRect();
                continue;
            }
            p.drawText(QPoint(x, baselineY), lens.title);
            lens.hitRect = QRect(x, baselineY - fm.ascent(),
                                 textW, fm.height());
        }
    }

private:
    EditorView          *m_editor;
    LspClient           *m_client;
    QVector<LspCodeLens> m_lenses;
};

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
      m_codeLensTimer(new QTimer(this)),
      m_completion(new CompletionPopup(editor)),
      m_hoverTooltip(new HoverTooltipWidget(editor))
{
    // Read code lens enabled flag from settings (default true)
    {
        QSettings s;
        m_codeLensEnabled = s.value(QStringLiteral("editor/codeLens"), true).toBool();
    }

    // Code-lens overlay: only created when enabled (otherwise zero overhead).
    if (m_codeLensEnabled) {
        m_codeLensOverlay = new CodeLensOverlay(m_editor, m_client);
    }

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

    // Code lens: re-request 1500ms after the last change
    m_codeLensTimer->setSingleShot(true);
    m_codeLensTimer->setInterval(1500);
    connect(m_codeLensTimer, &QTimer::timeout,
            this, &LspEditorBridge::requestCodeLens);

    // Install event filter on editor viewport for mouse tracking hover
    m_editor->viewport()->setMouseTracking(true);
    m_editor->viewport()->installEventFilter(this);
    // Also filter editor key events so we can dismiss signature popup on Esc
    m_editor->installEventFilter(this);

    // ── Signature help popup (small floating frame, rich-text label) ──────
    m_signaturePopup = new QFrame(m_editor);
    m_signaturePopup->setObjectName("LspSignaturePopup");
    m_signaturePopup->setFrameShape(QFrame::StyledPanel);
    m_signaturePopup->setFrameShadow(QFrame::Plain);
    m_signaturePopup->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint
                                     | Qt::NoDropShadowWindowHint);
    m_signaturePopup->setAttribute(Qt::WA_ShowWithoutActivating);
    m_signaturePopup->setFocusPolicy(Qt::NoFocus);
    m_signaturePopup->setStyleSheet(
        "QFrame#LspSignaturePopup {"
        "  background: #252526;"
        "  border: 1px solid #454545;"
        "  border-radius: 3px;"
        "}"
        "QLabel {"
        "  color: #d4d4d4;"
        "  background: transparent;"
        "  font-family: Consolas, 'Courier New', monospace;"
        "  font-size: 9pt;"
        "  padding: 4px 8px;"
        "}");
    auto *sigLayout = new QHBoxLayout(m_signaturePopup);
    sigLayout->setContentsMargins(0, 0, 0, 0);
    sigLayout->setSpacing(0);
    m_signatureLabel = new QLabel(m_signaturePopup);
    m_signatureLabel->setTextFormat(Qt::RichText);
    m_signatureLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    sigLayout->addWidget(m_signatureLabel);
    m_signaturePopup->hide();

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
    connect(m_client, &LspClient::codeLensResult,
            this, &LspEditorBridge::onCodeLensResult);

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

    // Ctrl+Alt+D → generate Doxygen comment above the function under cursor.
    // Per UX principles: keyboard-first, tooltip with shortcut, status feedback.
    auto *doxygenAction = new QAction(tr("Generate Documentation Comment"),
                                       editor);
    doxygenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D));
    doxygenAction->setShortcutContext(Qt::WidgetShortcut);
    doxygenAction->setToolTip(tr("Generate Doxygen Comment (Ctrl+Alt+D)"));
    doxygenAction->setObjectName(QStringLiteral("lsp.generateDoxygenComment"));
    connect(doxygenAction, &QAction::triggered,
            this, &LspEditorBridge::generateDoxygenComment);
    editor->addAction(doxygenAction);

    // Re-request inlay hints on scroll; also keep signature popup glued
    connect(m_editor->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() {
        m_inlayHintTimer->start();
        if (m_signaturePopup && m_signaturePopup->isVisible())
            positionSignaturePopup();
        if (m_codeLensOverlay)
            m_codeLensOverlay->update();
    });

    // Send initial open notification, then request symbols
    if (m_client->isInitialized()) {
        m_client->didOpen(m_uri, m_languageId,
                          m_editor->toPlainText(), m_version);
        m_opened = true;
        QTimer::singleShot(500, this, &LspEditorBridge::sendDocumentSymbols);
        QTimer::singleShot(800, this, &LspEditorBridge::requestInlayHints);
        QTimer::singleShot(900, this, &LspEditorBridge::requestCodeLens);
    } else {
        connect(m_client, &LspClient::initialized, this, [this]() {
            m_client->didOpen(m_uri, m_languageId,
                              m_editor->toPlainText(), m_version);
            m_opened = true;
            QTimer::singleShot(500, this, &LspEditorBridge::sendDocumentSymbols);
            QTimer::singleShot(800, this, &LspEditorBridge::requestInlayHints);
            QTimer::singleShot(900, this, &LspEditorBridge::requestCodeLens);
        }, Qt::SingleShotConnection);
    }
}

LspEditorBridge::~LspEditorBridge()
{
    m_completion->dismiss();
    m_hoverTooltip->hideTooltip();
    if (m_signaturePopup) m_signaturePopup->hide();
    if (m_codeLensOverlay) {
        m_codeLensOverlay->hide();
        m_codeLensOverlay->deleteLater();
        m_codeLensOverlay = nullptr;
    }
    m_client->didClose(m_uri);
}

// ── Event filter (mouse hover on viewport) ────────────────────────────────────

bool LspEditorBridge::eventFilter(QObject *obj, QEvent *event)
{
    // Esc dismisses the signature popup before any other handling
    if (obj == m_editor && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_Escape && m_signaturePopup
            && m_signaturePopup->isVisible()) {
            hideSignaturePopup();
            return true;  // swallow Esc so it doesn't cancel selection too
        }
    }
    // Hide popup when editor loses focus or scrolls noticeably
    if (obj == m_editor && (event->type() == QEvent::FocusOut
                             || event->type() == QEvent::Hide)) {
        hideSignaturePopup();
    }

    if (obj == m_editor->viewport()) {
        // Click on a code-lens label → execute its command. We use the
        // overlay's last-painted hit-rects rather than the bridge's copy
        // since the overlay is what actually drew them in viewport coords.
        if (event->type() == QEvent::MouseButtonPress
            && m_codeLensEnabled && m_codeLensOverlay) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                for (const LspCodeLens &lens : m_codeLensOverlay->lenses()) {
                    if (!lens.command.isEmpty()
                        && lens.hitRect.isValid()
                        && lens.hitRect.contains(me->pos())) {
                        m_client->executeCommand(lens.command, lens.arguments);
                        return true;  // swallow click
                    }
                }
            }
        }

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
    if (m_codeLensEnabled) m_codeLensTimer->start(); // refresh code lens after edit
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
        // Signature help: open on '(' and refresh on ',' (next active param)
        if (prevChar == '(') {
            m_changeTimer->stop();
            sendDidChange();
            const int line = cur.blockNumber();
            const int col  = cur.positionInBlock();
            // Anchor at the '(' so we can detect when cursor leaves the call
            m_signatureAnchorLine = line;
            m_signatureAnchorCol  = col - 1;
            m_client->requestSignatureHelp(m_uri, line, col);
        } else if (prevChar == ',' && m_signaturePopup
                   && m_signaturePopup->isVisible()) {
            m_changeTimer->stop();
            sendDidChange();
            const int line = cur.blockNumber();
            const int col  = cur.positionInBlock();
            m_client->requestSignatureHelp(m_uri, line, col);
        } else if (prevChar == ')') {
            // End of argument list — dismiss popup
            hideSignaturePopup();
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

    // Signature help: dismiss if cursor moved off the anchor line, or
    // before the '(' position; otherwise keep the popup glued near cursor.
    if (m_signaturePopup && m_signaturePopup->isVisible()) {
        const QTextCursor cur = m_editor->textCursor();
        const int line = cur.blockNumber();
        const int col  = cur.positionInBlock();
        if (line != m_signatureAnchorLine || col <= m_signatureAnchorCol) {
            hideSignaturePopup();
        } else {
            positionSignaturePopup();
        }
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
    if (sigs.isEmpty()) {
        hideSignaturePopup();
        return;
    }

    // Per LSP spec: activeSignature picks which signature, activeParameter
    // applies to that signature unless the signature has its own override.
    const int activeSig    = result["activeSignature"].toInt(0);
    const int sigIdx       = (activeSig >= 0 && activeSig < sigs.size())
                                 ? activeSig : 0;
    const QJsonObject sig  = sigs[sigIdx].toObject();
    const int activeParam  = sig.contains("activeParameter")
                                 ? sig["activeParameter"].toInt(0)
                                 : result["activeParameter"].toInt(0);
    const QJsonArray params = sig["parameters"].toArray();
    const QString label     = sig["label"].toString();
    if (label.isEmpty()) {
        hideSignaturePopup();
        return;
    }

    // Highlight the active parameter. parameter.label is either a string
    // (substring of the signature label) or a [startUtf16, endUtf16] pair.
    QString rich;
    auto htmlEscape = [](const QString &s) {
        QString out;
        out.reserve(s.size());
        for (QChar c : s) {
            if      (c == '<') out += "&lt;";
            else if (c == '>') out += "&gt;";
            else if (c == '&') out += "&amp;";
            else               out += c;
        }
        return out;
    };

    int hlStart = -1;
    int hlEnd   = -1;
    if (activeParam >= 0 && activeParam < params.size()) {
        const QJsonValue pLabel = params[activeParam].toObject()["label"];
        if (pLabel.isArray()) {
            const QJsonArray range = pLabel.toArray();
            if (range.size() == 2) {
                hlStart = range[0].toInt(-1);
                hlEnd   = range[1].toInt(-1);
            }
        } else if (pLabel.isString()) {
            const QString p = pLabel.toString();
            if (!p.isEmpty()) {
                hlStart = label.indexOf(p);
                if (hlStart >= 0) hlEnd = hlStart + p.size();
            }
        }
    }

    if (hlStart >= 0 && hlEnd > hlStart && hlEnd <= label.size()) {
        rich  = htmlEscape(label.left(hlStart));
        rich += "<b style=\"color:#569cd6\">"
              + htmlEscape(label.mid(hlStart, hlEnd - hlStart))
              + "</b>";
        rich += htmlEscape(label.mid(hlEnd));
    } else {
        rich = htmlEscape(label);
    }

    // Append doc/overload hint if multiple signatures exist
    if (sigs.size() > 1) {
        rich += QString("<span style=\"color:#808080\"> &nbsp;(%1/%2)</span>")
                    .arg(sigIdx + 1).arg(sigs.size());
    }

    showSignaturePopup(rich);
}

// ── Signature help popup helpers ──────────────────────────────────────────────

void LspEditorBridge::showSignaturePopup(const QString &richLabel)
{
    if (!m_signaturePopup || !m_signatureLabel) return;
    m_signatureLabel->setText(richLabel);
    m_signaturePopup->adjustSize();
    positionSignaturePopup();
    m_signaturePopup->show();
    m_signaturePopup->raise();
}

void LspEditorBridge::hideSignaturePopup()
{
    if (m_signaturePopup && m_signaturePopup->isVisible())
        m_signaturePopup->hide();
    m_signatureAnchorLine = -1;
    m_signatureAnchorCol  = -1;
}

void LspEditorBridge::positionSignaturePopup()
{
    if (!m_signaturePopup || !m_editor) return;
    // Anchor above the current line so the popup never covers what's typed
    const QRect cr = m_editor->cursorRect();
    QPoint anchor = m_editor->viewport()->mapToGlobal(cr.topLeft());
    const int popupH = m_signaturePopup->sizeHint().height();
    anchor.ry() -= popupH + 2;
    if (anchor.y() < 0) {
        // Not enough space above; place below the line instead
        anchor = m_editor->viewport()->mapToGlobal(cr.bottomLeft());
        anchor.ry() += 2;
    }
    m_signaturePopup->move(anchor);
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

void LspEditorBridge::generateDoxygenComment()
{
    if (!m_editor) return;

    // ── 1. Locate the function line ──────────────────────────────────────────
    // The cursor may be sitting on a continuation line (e.g. one of several
    // lines a parameter list is wrapped across) or on the line preceded by a
    // template<...> declaration. We accept either:
    //   - the cursor's current block, OR
    //   - the next non-empty block (skip blank lines / template prefix)
    QTextCursor cur = m_editor->textCursor();
    QTextBlock  block = cur.block();

    // If the current line is a `template<...>` line, slide down to the
    // declaration line that follows.
    auto isTemplateOnly = [](const QString &t) {
        const QString s = t.trimmed();
        return s.startsWith(QLatin1String("template"))
               && (s.endsWith(QLatin1Char('>'))
                   || s.contains(QLatin1Char('<')));
    };

    QTextBlock funcBlock = block;
    if (isTemplateOnly(funcBlock.text()) && funcBlock.next().isValid())
        funcBlock = funcBlock.next();

    // Join continuation lines until we see a `;`, `{`, or end-of-block balance
    // on parens. Cap at 8 lines to avoid runaway scans.
    QString joined;
    int parenDepth = 0;
    QTextBlock walker = funcBlock;
    for (int i = 0; i < 8 && walker.isValid(); ++i) {
        const QString ltext = walker.text();
        if (!joined.isEmpty()) joined += QLatin1Char(' ');
        joined += ltext;
        for (const QChar c : ltext) {
            if (c == QLatin1Char('(')) ++parenDepth;
            else if (c == QLatin1Char(')') && parenDepth > 0) --parenDepth;
        }
        const QString trimmed = ltext.trimmed();
        if (parenDepth == 0
            && (trimmed.endsWith(QLatin1Char(';'))
                || trimmed.endsWith(QLatin1Char('{'))
                || trimmed.endsWith(QLatin1Char(')')))) {
            break;
        }
        walker = walker.next();
    }

    const DoxygenGenerator::ParsedSignature sig =
        DoxygenGenerator::parseSignature(joined);

    // ── 2. Determine indent from the function block's leading whitespace ─────
    const QString funcLineText = funcBlock.text();
    int indent = 0;
    while (indent < funcLineText.size() && funcLineText.at(indent).isSpace())
        ++indent;
    // Tabs count as 1 space each for our indent string — we re-emit spaces so
    // the comment block is consistent regardless of the source indentation
    // style of that particular line.
    const QString indentStr(indent, QLatin1Char(' '));

    // ── 3. Build comment text ────────────────────────────────────────────────
    QString commentText;
    QString statusMsg;
    if (!sig.valid) {
        // Non-function line → single `///` marker on a new line above.
        commentText = indentStr + QStringLiteral("/// \n");
        statusMsg   = tr("No function detected on this line");
    } else {
        commentText = DoxygenGenerator::buildComment(sig, indent);
        statusMsg   = tr("Doxygen comment inserted");
    }

    // ── 4. Insert above the function line, atomically ────────────────────────
    QTextCursor insertCur(funcBlock);
    insertCur.movePosition(QTextCursor::StartOfBlock);
    insertCur.beginEditBlock();
    insertCur.insertText(commentText);
    insertCur.endEditBlock();

    // ── 5. Move cursor to the @brief line so the user can immediately type ───
    // The first inserted line is the `/// @brief ` line (or the lone `///` for
    // non-functions). Position cursor at end of that line.
    QTextBlock briefBlock = funcBlock.previous();
    // Walk back to the first inserted line (commentText may be multi-line).
    const int newlineCount = commentText.count(QLatin1Char('\n'));
    for (int i = 1; i < newlineCount && briefBlock.previous().isValid(); ++i)
        briefBlock = briefBlock.previous();
    if (briefBlock.isValid()) {
        QTextCursor briefCur(briefBlock);
        briefCur.movePosition(QTextCursor::EndOfBlock);
        m_editor->setTextCursor(briefCur);
    }

    // ── 6. Status feedback (3s auto-dismiss per UX principle 9) ──────────────
    if (auto *mw = qobject_cast<QMainWindow *>(m_editor->window())) {
        if (QStatusBar *sb = mw->statusBar())
            sb->showMessage(statusMsg, 3000);
    }
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
    if (uri != m_uri) return;

    // Build a Quick Fix popup styled like VS Code's dark theme.
    auto *menu = new QMenu(m_editor);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setObjectName("LspQuickFixMenu");
    menu->setStyleSheet(
        "QMenu#LspQuickFixMenu {"
        "  background: #252526;"
        "  color: #d4d4d4;"
        "  border: 1px solid #454545;"
        "  padding: 4px 0;"
        "  font-family: 'Segoe UI', Arial, sans-serif;"
        "  font-size: 9pt;"
        "}"
        "QMenu#LspQuickFixMenu::item {"
        "  padding: 4px 22px 4px 22px;"
        "  background: transparent;"
        "}"
        "QMenu#LspQuickFixMenu::item:selected {"
        "  background: #094771;"
        "  color: #ffffff;"
        "}"
        "QMenu#LspQuickFixMenu::item:disabled {"
        "  color: #808080;"
        "}"
        "QMenu#LspQuickFixMenu::separator {"
        "  height: 1px;"
        "  background: #3c3c3c;"
        "  margin: 4px 0;"
        "}");

    for (const QJsonValue &v : actions) {
        if (!v.isObject()) continue;
        const QJsonObject action = v.toObject();
        const QString title = action["title"].toString();
        if (title.isEmpty()) continue;

        // Prefix the menu entry with the action kind for context (e.g.
        // "quickfix", "refactor", "source"). Some servers (clangd) return
        // a Command directly rather than a CodeAction — those have no kind.
        const QString kind = action["kind"].toString();
        QString display = title;
        if (!kind.isEmpty()) {
            QString shortKind = kind;
            const int dot = shortKind.indexOf('.');
            if (dot > 0) shortKind = shortKind.left(dot);
            display = QStringLiteral("[%1] %2").arg(shortKind, title);
        }

        QAction *item = menu->addAction(display);

        // CodeAction:  { title, kind, edit?, command?, diagnostics? }
        // Command:     { title, command, arguments? }   (legacy form)
        const QJsonObject edit    = action["edit"].toObject();

        // `command` may be either an object (CodeAction.command) or the
        // outer object itself (legacy Command form: action["command"] is
        // the command id string).
        QJsonObject command;
        QString commandId;
        QJsonArray commandArgs;
        if (action["command"].isObject()) {
            command     = action["command"].toObject();
            commandId   = command["command"].toString();
            commandArgs = command["arguments"].toArray();
        } else if (action["command"].isString()) {
            commandId   = action["command"].toString();
            commandArgs = action["arguments"].toArray();
        }

        connect(item, &QAction::triggered, this,
                [this, edit, commandId, commandArgs]() {
            // Per LSP spec: if both edit and command exist, apply edit first,
            // then send the command (servers occasionally return both, e.g.
            // "apply this textEdit AND notify the server").
            if (!edit.isEmpty())
                emit workspaceEditReady(edit);
            if (!commandId.isEmpty())
                m_client->executeCommand(commandId, commandArgs);
        });
    }

    if (menu->isEmpty()) {
        // Show a disabled "no quick fixes" hint so the user gets feedback.
        QAction *empty = menu->addAction(tr("No quick fixes available"));
        empty->setEnabled(false);
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

// ── Code Lens ────────────────────────────────────────────────────────────────

void LspEditorBridge::requestCodeLens()
{
    if (!m_codeLensEnabled) return;
    if (!m_client->isInitialized()) return;
    if (!m_opened) return;
    m_client->requestCodeLens(m_uri);
}

void LspEditorBridge::onCodeLensResult(const QString &uri,
                                       const QJsonArray &lenses)
{
    if (uri != m_uri) return;
    if (!m_codeLensEnabled) return;

    // Group lenses by line — clangd often returns several lenses per
    // function (references, implementations) on the same line, which we
    // concatenate using " | " for a compact display.
    struct Bucket {
        int        line = 0;
        int        character = 0;
        QStringList titles;
        QString    command;
        QJsonArray arguments;
    };
    QMap<int, Bucket> byLine;

    constexpr int kMaxLenses = 200;
    int count = 0;

    for (const QJsonValue &v : lenses) {
        if (count >= kMaxLenses) break;
        if (!v.isObject()) continue;
        const QJsonObject lens = v.toObject();
        const QJsonObject range = lens[QLatin1String("range")].toObject();
        const QJsonObject startPos = range[QLatin1String("start")].toObject();
        const int line = startPos[QLatin1String("line")].toInt(-1);
        const int ch   = startPos[QLatin1String("character")].toInt(0);
        if (line < 0) continue;

        const QJsonObject cmd = lens[QLatin1String("command")].toObject();
        const QString title = cmd[QLatin1String("title")].toString();
        if (title.isEmpty()) continue;  // unresolved lens (no command yet)

        Bucket &b = byLine[line];
        b.line      = line;
        if (b.titles.isEmpty()) {
            b.character = ch;
            b.command   = cmd[QLatin1String("command")].toString();
            b.arguments = cmd[QLatin1String("arguments")].toArray();
        }
        b.titles.append(title);
        ++count;
    }

    m_codeLenses.clear();
    m_codeLenses.reserve(byLine.size());
    for (const Bucket &b : std::as_const(byLine)) {
        CodeLens cl;
        cl.line      = b.line;
        cl.character = b.character;
        cl.title     = b.titles.join(QStringLiteral(" | "));
        cl.command   = b.command;
        cl.arguments = b.arguments;
        m_codeLenses.append(cl);
    }

    if (m_codeLensOverlay)
        m_codeLensOverlay->setLenses(m_codeLenses);
}
