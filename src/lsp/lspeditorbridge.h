#pragma once

#include <QJsonArray>
#include <QObject>
#include <QString>

class QTimer;
class EditorView;
class LspClient;
class CompletionPopup;

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

private slots:
    void onDocumentChanged();
    void onCursorPositionChanged();
    void onCompletionResult(const QString &uri, int line, int character,
                            const QJsonArray &items);
    void onHoverResult(const QString &uri, int line, int character,
                       const QString &markdown);
    void onSignatureHelpResult(const QString &uri, int line, int character,
                               const QJsonObject &result);
    void onDiagnosticsPublished(const QString &uri, const QJsonArray &diags);
    void onFormattingResult(const QString &uri, const QJsonArray &edits);
    void onCompletionAccepted(const QString &insertText,
                              const QString &filterText);

private:
    void sendDidChange();
    void triggerCompletion();
    bool isCompletionTrigger(QChar ch) const;

    EditorView      *m_editor;
    LspClient       *m_client;
    QString          m_filePath;
    QString          m_uri;
    QString          m_languageId;
    int              m_version = 1;
    QTimer          *m_changeTimer;
    CompletionPopup *m_completion;
    QString          m_completionPrefix; // word typed since last completion
};
