#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class EditorView;
class IAgentProvider;

// Represents a predicted next edit location and suggestion.
struct NextEditSuggestion {
    int     line     = -1;       // 0-based line number in current file
    int     column   = 0;        // 0-based column
    QString filePath;            // target file (empty = same file)
    QString preview;             // short preview of what the edit would be
    QString fullSuggestion;      // full replacement text for ghost text
};

// After the user accepts an inline completion, this engine predicts the next
// logical edit location in the same (or another) file and shows a gutter
// arrow indicator.  Pressing Tab at the indicator jumps there and displays
// ghost text for the predicted edit.
class NextEditEngine : public QObject
{
    Q_OBJECT

public:
    explicit NextEditEngine(QObject *parent = nullptr);

    void setProvider(IAgentProvider *provider);
    void attachEditor(EditorView *editor, const QString &filePath);
    void detachEditor();

    bool hasSuggestion() const { return m_suggestion.line >= 0; }
    const NextEditSuggestion &currentSuggestion() const { return m_suggestion; }

    // Called when the user accepts a ghost text completion — triggers prediction
    void onEditAccepted(const QString &acceptedText, int atLine, int atColumn);

    // User presses Tab at the arrow indicator — jump and show ghost text
    void acceptSuggestion();

    // Dismiss the current NES arrow
    void dismissSuggestion();

signals:
    // Emitted when a new NES suggestion is ready (gutter arrow should appear)
    void suggestionReady(int line);
    // Emitted when suggestion is cleared
    void suggestionDismissed();
    // Emitted when the user should navigate to another file for a multi-file edit
    void navigateToFile(const QString &filePath, int line, int column);

private:
    void requestPrediction(const QString &editContext);
    void onPredictionResponse(const QString &requestId, const QString &text);

    EditorView     *m_editor   = nullptr;
    IAgentProvider *m_provider = nullptr;
    QString         m_filePath;
    QString         m_pendingRequestId;

    NextEditSuggestion m_suggestion;
    QTimer             m_cooldown;       // brief delay before requesting NES

    // Context from the last accepted edit
    QString m_lastAcceptedText;
    int     m_lastAcceptedLine   = -1;
    int     m_lastAcceptedColumn = -1;

    QMetaObject::Connection m_deltaConn;
    QMetaObject::Connection m_finishConn;
    QMetaObject::Connection m_errorConn;
    QString m_streamBuffer;
};
