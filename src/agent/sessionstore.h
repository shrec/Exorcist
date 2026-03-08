#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

class SessionStore : public QObject
{
    Q_OBJECT

public:
    explicit SessionStore(const QString &dir, QObject *parent = nullptr);
    explicit SessionStore(QObject *parent = nullptr);

    struct ChatSession {
        QString sessionId;
        QDateTime createdAt;
        QVector<QPair<QString, QString>> messages; // {role, text}
        QJsonArray completeTurns;  // full turn JSON (from turn.complete events)
        bool isEmpty() const { return messages.isEmpty() && completeTurns.isEmpty(); }
    };

    void recordSessionStart(const QString &id, const QString &model,
                            const QString &mode);
    void recordUserMessage(const QString &id, const QString &content);
    void recordAssistantMessage(const QString &id, const QString &text);
    void recordToolCall(const QString &id, const QString &name,
                        const QJsonObject &args, bool ok, const QString &result);
    void recordSessionEnd(const QString &id);

    void recordCompleteTurn(const QString &id, const QJsonObject &turnJson);

    struct Summary {
        QString   sessionId;
        QString   title;
        QDateTime created;
        int       turnCount = 0;
    };
    QVector<Summary> recentSessions(int max = 20) const;

    static QString defaultDir();

    // Legacy API used by AgentChatPanel
    void startSession(const QString &id, const QString &providerId,
                      const QString &model, bool agentMode);
    void appendUserMessage(const QString &id, const QString &content);
    void appendAssistantMessage(const QString &id, const QString &text);
    void appendToolCall(const QString &id, const QString &name,
                        const QJsonObject &args, const QString &result, bool ok);
    void appendError(const QString &id, const QString &message);

    QStringList listSessionFiles(int max = 20) const;
    ChatSession loadSession(const QString &path) const;
    ChatSession loadLastSession() const;
    QString renameSession(const QString &path, const QString &newName);
    void deleteSession(const QString &path);
    QStringList searchSessions(const QString &query, int max = 20) const;
    void setSessionTitle(const QString &id, const QString &title);

private:
    void appendLine(const QString &id, const QJsonObject &entry);
    void pruneOldSessions(int keep = 50);
    QString sessionFilePath(const QString &id) const;
    static QString sanitizeName(const QString &name);
    QString m_dir;
};
