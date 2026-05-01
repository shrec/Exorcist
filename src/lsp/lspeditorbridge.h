#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

class QTimer;
class QFrame;
class QLabel;
class EditorView;
class LspClient;
class CompletionPopup;
class HoverTooltipWidget;
class CodeLensOverlay;

// Code lens datum used by both the bridge and the overlay widget.
struct LspCodeLens {
    int        line = 0;       // 0-based
    int        character = 0;  // 0-based
    QString    title;          // command title shown above the line
    QString    command;        // command id (for executeCommand)
    QJsonArray arguments;      // command arguments
    QRect      hitRect;        // last painted rect in viewport coords
};

// ── LspEditorBridge ───────────────────────────────────────────────────────────
//
// Connects one EditorView to an LspClient for a specific file.
//
// Responsibilities:
//  - Sends textDocument/didOpen on construction
//  - Sends textDocument/didChange (debounced 300ms) on every edit
//  - Sends textDocument/didClose on destruction
//  - Triggers completion on Ctrl+Space or '.' / '->' / '::'
//  - Triggers signatureHelp on '('
//  - Triggers hover on mouse hover (via editor signal)
//  - Applies incoming diagnostics as wavy underlines + gutter dots
//  - Applies formatting edits from textDocument/formatting
//  - Ctrl+Shift+F → format document

class LspEditorBridge : public QObject
{
    Q_OBJECT

public:
    LspEditorBridge(EditorView *editor, LspClient *client,
                    const QString &filePath, QObject *parent = nullptr);
    ~LspEditorBridge() override;

    // Trigger format document programmatically (also bound to Ctrl+Shift+F)
    void formatDocument();

    // Trigger a document-symbol request (called by MainWindow on tab switch)
    void requestSymbols();

    // Generate a Doxygen `///`-style comment block above the function under
    // the cursor (also bound to Ctrl+Alt+D). If the cursor is not on a
    // parseable function, a single `///` line is inserted instead and the
    // status bar shows "No function detected on this line".
    void generateDoxygenComment();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onDocumentChanged();
    void onCursorPositionChanged();
    void onCompletionResult(const QString &uri, int line, int character,
                            const QJsonArray &items, bool isIncomplete);
    void onHoverResult(const QString &uri, int line, int character,
                       const QString &markdown);
    void onSignatureHelpResult(const QString &uri, int line, int character,
                               const QJsonObject &result);
    void onDiagnosticsPublished(const QString &uri, const QJsonArray &diags);
    void onFormattingResult(const QString &uri, const QJsonArray &edits);
    void onDefinitionResult(const QString &uri, const QJsonArray &locations);
    void onDeclarationResult(const QString &uri, const QJsonArray &locations);
    void onReferencesResult(const QString &uri, const QJsonArray &locations);
    void onRenameResult(const QString &uri, const QJsonObject &workspaceEdit);
    void onDocumentSymbolsResult(const QString &uri, const QJsonArray &symbols);
    void onCodeActionResult(const QString &uri, int line, int character,
                            const QJsonArray &actions);
    void onInlayHintsResult(const QString &uri, const QJsonArray &hints);
    void onTypeDefinitionResult(const QString &uri, const QJsonArray &locations);
    void onCodeLensResult(const QString &uri, const QJsonArray &lenses);
    void requestCodeLens();
    void onCompletionAccepted(const QString &insertText,
                              const QString &filterText);
    void sendDocumentSymbols();
    void requestInlayHints();

private:
    void sendDidChange();
    void triggerCompletion();
    void requestCodeActions();
    bool isCompletionTrigger(QChar ch) const;
    bool isFormatOnTypeTrigger(QChar ch) const;
    void requestSignatureHelpAtCursor();
    void showSignaturePopup(const QString &richLabel);
    void hideSignaturePopup();
    void positionSignaturePopup();

    EditorView      *m_editor;
    LspClient       *m_client;
    QString          m_filePath;
    QString          m_uri;
    QString          m_languageId;
    int              m_version            = 1;
    int              m_formatReqVersion   = 0;
    QTimer          *m_changeTimer;
    QTimer          *m_symbolTimer;       // debounced 1s for documentSymbol
    QTimer          *m_hoverTimer;        // debounced 500ms for hover
    QTimer          *m_inlayHintTimer;    // debounced 500ms for inlay hints
    QTimer          *m_codeLensTimer;     // debounced 1500ms for code lens

    // Code-lens data (cached per editor)
    using CodeLens = LspCodeLens;
    QVector<CodeLens> m_codeLenses;
    bool             m_codeLensEnabled = true;
    CodeLensOverlay *m_codeLensOverlay = nullptr;
    CompletionPopup *m_completion;
    HoverTooltipWidget *m_hoverTooltip;
    QFrame          *m_signaturePopup = nullptr;  // floating signature help popup
    QLabel          *m_signatureLabel = nullptr;  // rich-text label inside popup
    int              m_signatureAnchorLine = -1;  // line where '(' was typed
    int              m_signatureAnchorCol  = -1;  // column of the '('
    QPoint           m_hoverGlobalPos;    // mouse position when hover was requested
    int              m_hoverLine = -1;
    int              m_hoverCol  = -1;
    bool             m_opened     = false;
    QString          m_completionPrefix;
    QJsonArray       m_currentDiagnostics;  // latest diagnostics for m_uri

signals:
    void navigateToLocation(const QString &filePath, int line, int character);
    void referencesFound(const QString &symbol, const QJsonArray &locations);
    void workspaceEditReady(const QJsonObject &workspaceEdit);
    void symbolsUpdated(const QString &uri, const QJsonArray &symbols);
};
