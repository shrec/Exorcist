#pragma once

#include <QObject>
#include <QHash>
#include <QString>

class QKeyEvent;
class QPlainTextEdit;

class VimHandler : public QObject
{
    Q_OBJECT
public:
    enum class Mode { Normal, Insert, Visual, VisualLine, Command };
    Q_ENUM(Mode)

    explicit VimHandler(QPlainTextEdit *editor, QObject *parent = nullptr);

    bool isEnabled() const;
    void setEnabled(bool enabled);
    Mode mode() const;
    QString modeString() const;

    // Returns true if the key event was consumed
    bool handleKeyPress(QKeyEvent *event);

    // Reset state (pending operator, count, etc.)
    void reset();

signals:
    void modeChanged(Mode mode);
    void statusMessage(const QString &msg);
    void saveRequested();
    void quitRequested();
    void openFileRequested(const QString &path);

private:
    enum class Operator { None, Delete, Change, Yank };

    void setMode(Mode m);

    // Key handlers per mode
    bool handleNormal(QKeyEvent *event);
    bool handleInsert(QKeyEvent *event);
    bool handleVisual(QKeyEvent *event);
    bool handleCommand(QKeyEvent *event);

    // Motion execution — returns true if motion was applied
    bool executeMotion(const QString &key, bool select = false);

    // Operator + motion composition
    void applyOperator(Operator op, const QString &motion);
    void applyLinewiseOperator(Operator op, int lines = 1);

    // Ex-command parsing
    void executeExCommand(const QString &cmd);

    // Helpers
    void moveCursor(int position, bool select = false);
    int findWordForward(int pos) const;
    int findWordBackward(int pos) const;
    int findWordEnd(int pos) const;
    int findCharOnLine(QChar ch, bool forward, int pos) const;
    int matchingBracket(int pos) const;

    QPlainTextEdit *m_editor;
    Mode m_mode = Mode::Normal;
    bool m_enabled = false;

    // Pending state for operator-motion composition
    Operator m_pendingOp = Operator::None;
    int m_count = 0;         // Numeric prefix (0 = not set)
    QString m_pendingKeys;   // Buffered keys (e.g. "g" waiting for "g")
    QString m_commandLine;   // :command text
    QString m_yankBuffer;    // Yank/delete register
    bool m_yankLinewise = false;
    QChar m_lastFindChar;    // For f/F repeat
    bool m_lastFindForward = true;
};
