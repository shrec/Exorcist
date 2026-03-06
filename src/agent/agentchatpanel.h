#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QScrollArea;
class QTextBrowser;
class QToolButton;
class QVBoxLayout;
class AgentOrchestrator;
struct AgentResponse;
struct AgentError;

// ── AgentChatPanel ────────────────────────────────────────────────────────────
//
// The AI dock panel. Shows:
//   - Provider selector (combo)
//   - Status indicator (dot: green/grey)
//   - Chat history (QTextBrowser — read-only Markdown-ish display)
//   - Input field + Send / Cancel buttons
//
// When no providers are registered it shows a placeholder message.

class AgentChatPanel : public QWidget
{
    Q_OBJECT

public:
    explicit AgentChatPanel(AgentOrchestrator *orchestrator,
                            QWidget *parent = nullptr);

    // Called by MainWindow to inject editor context into the next request.
    void setEditorContext(const QString &filePath,
                          const QString &selectedText,
                          const QString &languageId);

private slots:
    void onSend();
    void onCancel();
    void onProviderSelected(int index);
    void onProviderRegistered(const QString &id);
    void onProviderRemoved(const QString &id);
    void onActiveProviderChanged(const QString &id);
    void onResponseDelta(const QString &requestId, const QString &chunk);
    void onResponseFinished(const QString &requestId, const AgentResponse &response);
    void onResponseError(const QString &requestId, const AgentError &error);

private:
    void appendMessage(const QString &role, const QString &html);
    void setInputEnabled(bool enabled);
    void refreshProviderList();
    void updateStatusDot();

    AgentOrchestrator *m_orchestrator;

    QComboBox    *m_providerCombo;
    QLabel       *m_statusDot;
    QTextBrowser *m_history;
    QLineEdit    *m_input;
    QToolButton  *m_sendBtn;
    QToolButton  *m_cancelBtn;

    // Editor context for next request
    QString m_activeFilePath;
    QString m_selectedText;
    QString m_languageId;

    // Tracks the in-flight request so we can stream into the right bubble.
    QString m_pendingRequestId;
    QString m_pendingAccum;
};
