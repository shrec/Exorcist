#pragma once
// ── ChatJSBridge ─────────────────────────────────────────────────────────────
//
// Bidirectional bridge between C++ chat backend and Ultralight JS frontend.
//
// C++ → JS: Methods that evaluate JavaScript to update the DOM.
// JS → C++: Qt signals emitted when the user interacts with the chat UI.
//
// Signals are designed to match ChatTranscriptView's signal signatures
// so ChatPanelWidget can rewire with minimal changes.

#ifdef EXORCIST_HAS_ULTRALIGHT

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

namespace exorcist {

class UltralightWidget;

class ChatJSBridge : public QObject
{
    Q_OBJECT
public:
    explicit ChatJSBridge(UltralightWidget *view, QObject *parent = nullptr);

    // ── C++ → JS (update UI) ────────────────────────────────────────────────

    /// Add a complete turn (from session restore or new request).
    void addTurn(int turnIndex, const QJsonObject &turnJson);

    /// Streaming: append markdown delta to the last markdown block.
    void appendMarkdownDelta(int turnIndex, const QString &delta);

    /// Streaming: append thinking delta.
    void appendThinkingDelta(int turnIndex, const QString &delta);

    /// Add a new content part to a turn.
    void addContentPart(int turnIndex, const QJsonObject &partJson);

    /// Update tool invocation state (queued → streaming → complete).
    void updateToolState(int turnIndex, const QString &callId,
                         const QJsonObject &stateJson);

    /// Mark a turn as finished.
    void finishTurn(int turnIndex, int state);

    /// Clear all turns (new session).
    void clearSession();

    // ── View state ──────────────────────────────────────────────────────────

    /// Show the welcome screen (hides transcript).
    void showWelcome(const QString &welcomeState = QStringLiteral("default"));

    /// Show the transcript (hides welcome).
    void showTranscript();

    /// Set streaming mode (enables auto-scroll).
    void setStreamingActive(bool active);

    /// Scroll to the bottom of the transcript.
    void scrollToBottom();

    // ── Theme ───────────────────────────────────────────────────────────────

    /// Update CSS custom properties from ChatThemeTokens.
    void setTheme(const QJsonObject &themeTokens);

    /// Set suggestion chips on the welcome screen.
    void setWelcomeSuggestions(const QJsonArray &suggestions);

    /// Set tool count badge.
    void setToolCount(int count);

signals:
    // ── JS → C++ (user actions) ─────────────────────────────────────────────

    void followupClicked(const QString &message);
    void feedbackGiven(const QString &turnId, bool helpful);
    void toolConfirmed(const QString &callId, int approval);
    void fileClicked(const QString &path);
    void insertCodeRequested(const QString &code);
    void retryRequested(const QString &turnId);
    void suggestionClicked(const QString &message);
    void signInRequested();

    // Workspace edit actions
    void keepFileRequested(int fileIndex, const QString &path);
    void undoFileRequested(int fileIndex, const QString &path);
    void keepAllRequested();
    void undoAllRequested();

    // Code block actions
    void copyCodeRequested(const QString &code);
    void applyCodeRequested(const QString &code, const QString &language);

private:
    /// Evaluate JS, escaping the string parameter safely.
    void callJS(const QString &functionCall);

    /// JSON-encode a string for safe embedding in JS.
    static QString jsonEscape(const QString &s);

    UltralightWidget *m_view;
};

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
