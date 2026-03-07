#pragma once

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

// ── ChatSession ───────────────────────────────────────────────────────────────
// Reconstructed view of a saved session (user/assistant messages only).

struct ChatSession {
    QString                       sessionId;
    QDateTime                     createdAt;
    QString                       providerId;
    QString                       model;
    bool                          agentMode = false;
    // Each pair: (role, text) where role is "user" or "assistant"
    QList<QPair<QString,QString>> messages;

    bool isEmpty() const { return messages.isEmpty(); }
};

class SessionStore : public QObject
{
    Q_OBJECT

public:
    explicit SessionStore(QObject *parent = nullptr);

    void setRootDirectory(const QString &rootDir);
    QString rootDirectory() const;

    void startSession(const QString &sessionId,
                      const QString &providerId,
                      const QString &model,
                      bool agentMode);

    void appendUserMessage(const QString &sessionId, const QString &text);
    void appendAssistantMessage(const QString &sessionId, const QString &text);
    void appendToolCall(const QString &sessionId,
                        const QString &toolName,
                        const QJsonObject &args,
                        const QString &result,
                        bool ok);
    void appendError(const QString &sessionId, const QString &message);

    // ── Session reading ───────────────────────────────────────────────────
    // Returns the most recent saved session (newest .jsonl file).
    // Returns an empty ChatSession if no sessions exist.
    ChatSession loadLastSession() const;

    // Load a session from a specific file path.
    ChatSession loadSession(const QString &filePath) const;

    // Delete a session file.
    bool deleteSession(const QString &filePath);

    // Rename a session file. Returns the new absolute path, or empty on failure.
    QString renameSession(const QString &filePath, const QString &newName);

    // Search sessions whose messages contain the query (case-insensitive).
    // Returns matching file paths (newest first).
    QStringList searchSessions(const QString &query, int maxCount = 20) const;

    // Lists saved session files (newest first). Each path is absolute.
    QStringList listSessionFiles(int maxCount = 50) const;

private:
    void appendEvent(const QString &sessionId,
                     const QString &eventType,
                     const QJsonObject &payload);
    QString ensureSessionFile(const QString &sessionId);
    ChatSession parseSessionFile(const QString &filePath) const;

    QString m_rootDir;
    QHash<QString, QString> m_sessionFiles;
};
