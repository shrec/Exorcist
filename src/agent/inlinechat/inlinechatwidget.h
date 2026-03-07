#pragma once

#include <QFrame>
#include <QList>
#include <QString>

#include "../../aiinterface.h"

class QKeyEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;
class AgentOrchestrator;

// A single chunk in a multi-location edit. Lines are 1-based, inclusive.
struct EditChunk {
    int     startLine;   // first line to replace (1-based)
    int     endLine;     // last line to replace (1-based, inclusive)
    QString newCode;     // replacement text for this region
};

// ─────────────────────────────────────────────────────────────────────────────
// InlineChatWidget — floating AI chat overlay shown inside the editor.
//
// Triggered by Ctrl+I. Appears anchored near the cursor / selected text.
// Sends a single prompt to the AI and offers to apply the code block result
// back to the editor selection.  Supports multi-chunk edits — the AI can
// return multiple line-annotated code blocks to edit several non-contiguous
// regions in one operation.
//
// Keyboard:
//   Enter        — send prompt
//   Escape       — dismiss
//   Ctrl+Enter   — apply (when response is ready)
// ─────────────────────────────────────────────────────────────────────────────

class InlineChatWidget : public QFrame
{
    Q_OBJECT

public:
    explicit InlineChatWidget(AgentOrchestrator *orchestrator, QWidget *parent = nullptr);

    // Set the editor context for the next request.
    void setContext(const QString &selectedText,
                    const QString &filePath,
                    const QString &languageId);

    // Set full file content for multi-chunk edit context.
    void setFileContent(const QString &content);

    // Give focus to the input line.
    void focusInput();

signals:
    // Emitted when the user clicks "Apply" — caller should replace the
    // current selection (or insert at cursor) with this text.
    void applyRequested(const QString &codeToInsert);

    // Emitted for multi-chunk edits — caller should apply each chunk to the file.
    void applyMultiEditsRequested(const QList<EditChunk> &chunks);

    // Emitted when the user dismisses the widget (Escape / Discard).
    void dismissed();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSend();
    void onApply();
    void onDiscard();
    void onDelta(const QString &reqId, const QString &chunk);
    void onFinished(const QString &reqId, const AgentResponse &resp);
    void onError(const QString &reqId, const AgentError &err);

private:
    // Extract the first code block from markdown text.
    static QString extractCode(const QString &markdown);

    // Parse multiple line-annotated code blocks (```lang:L10-L15).
    static QList<EditChunk> extractMultiChunks(const QString &markdown);

    // Build a diff-like HTML comparing original and new code.
    QString buildDiffHtml(const QString &original, const QString &modified) const;

    // Build multi-chunk diff HTML showing changes at multiple file locations.
    QString buildMultiDiffHtml(const QList<EditChunk> &chunks) const;

    void setResponding(bool responding);

    AgentOrchestrator *m_orchestrator;

    QLabel       *m_contextLabel;   // shows first line of selection
    QLineEdit    *m_input;
    QPushButton  *m_sendBtn;
    QPushButton  *m_cancelBtn;
    QTextBrowser *m_response;
    QPushButton  *m_applyBtn;
    QPushButton  *m_discardBtn;

    QString m_selectedText;
    QString m_fileContent;    // full file content for multi-chunk mode
    QString m_filePath;
    QString m_languageId;

    QString m_pendingReqId;
    QString m_accum;
    QString m_lastCode;                // extracted from last response (single-chunk)
    QList<EditChunk> m_lastChunks;     // extracted multi-chunk edits
};
