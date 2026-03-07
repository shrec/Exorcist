#pragma once

#include <QDialog>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

class AgentOrchestrator;
struct AgentResponse;
struct AgentError;

// ─────────────────────────────────────────────────────────────────────────────
// QuickChatDialog — lightweight floating chat window (Ctrl+Shift+Alt+L)
//
// A minimal dialog that lets the user ask a quick question without
// switching to the full AI panel. Shows a text input + compact response.
// ─────────────────────────────────────────────────────────────────────────────

class QuickChatDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickChatDialog(AgentOrchestrator *orchestrator, QWidget *parent = nullptr);

    void setEditorContext(const QString &filePath, const QString &selectedText,
                          const QString &languageId);

signals:
    void insertAtCursorRequested(const QString &text);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void onSend();
    void onResponseDelta(const QString &requestId, const QString &chunk);
    void onResponseFinished(const QString &requestId, const AgentResponse &resp);
    void onResponseError(const QString &requestId, const AgentError &error);

    AgentOrchestrator *m_orchestrator;
    QPlainTextEdit *m_input;
    QPlainTextEdit *m_output;
    QPushButton    *m_sendBtn;
    QString         m_activeRequestId;
    QString         m_accum;

    QString m_filePath;
    QString m_selectedText;
    QString m_languageId;
};
