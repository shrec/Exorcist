#include "sessionstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStandardPaths>

SessionStore::SessionStore(QObject *parent)
    : QObject(parent)
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    setRootDirectory(base + QLatin1String("/sessions"));
}

void SessionStore::setRootDirectory(const QString &rootDir)
{
    m_rootDir = rootDir;
    QDir().mkpath(m_rootDir);
}

QString SessionStore::rootDirectory() const
{
    return m_rootDir;
}

void SessionStore::startSession(const QString &sessionId,
                                const QString &providerId,
                                const QString &model,
                                bool agentMode)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("provider"), providerId);
    payload.insert(QStringLiteral("model"), model);
    payload.insert(QStringLiteral("agentMode"), agentMode);
    appendEvent(sessionId, QStringLiteral("session_start"), payload);
}

void SessionStore::appendUserMessage(const QString &sessionId, const QString &text)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("text"), text);
    appendEvent(sessionId, QStringLiteral("user"), payload);
}

void SessionStore::appendAssistantMessage(const QString &sessionId, const QString &text)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("text"), text);
    appendEvent(sessionId, QStringLiteral("assistant"), payload);
}

void SessionStore::appendToolCall(const QString &sessionId,
                                  const QString &toolName,
                                  const QJsonObject &args,
                                  const QString &result,
                                  bool ok)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("tool"), toolName);
    payload.insert(QStringLiteral("ok"), ok);
    payload.insert(QStringLiteral("args"), args);
    payload.insert(QStringLiteral("result"), result);
    appendEvent(sessionId, QStringLiteral("tool"), payload);
}

void SessionStore::appendError(const QString &sessionId, const QString &message)
{
    QJsonObject payload;
    payload.insert(QStringLiteral("message"), message);
    appendEvent(sessionId, QStringLiteral("error"), payload);
}

QString SessionStore::ensureSessionFile(const QString &sessionId)
{
    const auto found = m_sessionFiles.constFind(sessionId);
    if (found != m_sessionFiles.constEnd())
        return found.value();

    QDir().mkpath(m_rootDir);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString path = m_rootDir + QLatin1Char('/') + stamp + QLatin1Char('-') + sessionId + QStringLiteral(".jsonl");
    m_sessionFiles.insert(sessionId, path);
    return path;
}

void SessionStore::appendEvent(const QString &sessionId,
                               const QString &eventType,
                               const QJsonObject &payload)
{
    if (sessionId.isEmpty())
        return;

    const QString path = ensureSessionFile(sessionId);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    QJsonObject line;
    line.insert(QStringLiteral("ts"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    line.insert(QStringLiteral("sessionId"), sessionId);
    line.insert(QStringLiteral("event"), eventType);
    line.insert(QStringLiteral("data"), payload);

    const QByteArray json = QJsonDocument(line).toJson(QJsonDocument::Compact);
    file.write(json);
    file.write("\n");
}

// ── Session reading ───────────────────────────────────────────────────────────

QStringList SessionStore::listSessionFiles(int maxCount) const
{
    QDir dir(m_rootDir);
    if (!dir.exists())
        return {};

    const QFileInfoList infos = dir.entryInfoList(
        QStringList() << QStringLiteral("*.jsonl"),
        QDir::Files, QDir::Time);  // newest first

    QStringList result;
    for (const QFileInfo &fi : infos) {
        result << fi.absoluteFilePath();
        if (result.size() >= maxCount)
            break;
    }
    return result;
}

ChatSession SessionStore::loadLastSession() const
{
    const QStringList files = listSessionFiles(10);
    for (const QString &path : files) {
        ChatSession s = parseSessionFile(path);
        if (!s.isEmpty())
            return s;
    }
    return {};
}

ChatSession SessionStore::loadSession(const QString &filePath) const
{
    return parseSessionFile(filePath);
}

bool SessionStore::deleteSession(const QString &filePath)
{
    return QFile::remove(filePath);
}

QString SessionStore::renameSession(const QString &filePath, const QString &newName)
{
    const QFileInfo fi(filePath);
    if (!fi.exists())
        return {};

    // Sanitize: keep only safe characters for the filename
    QString safe = newName.simplified();
    safe.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    if (safe.isEmpty())
        return {};

    // Preserve the timestamp prefix (yyyyMMdd-HHmmss), replace the rest
    const QString baseName = fi.baseName();
    QString prefix;
    if (baseName.size() >= 15)
        prefix = baseName.left(15); // "yyyyMMdd-HHmmss"
    else
        prefix = baseName;

    const QString newPath = fi.absolutePath() + QLatin1Char('/')
                          + prefix + QLatin1Char('-') + safe + QStringLiteral(".jsonl");

    if (QFile::rename(filePath, newPath)) {
        // Update the in-memory cache if this session is tracked
        for (auto it = m_sessionFiles.begin(); it != m_sessionFiles.end(); ++it) {
            if (it.value() == filePath) {
                it.value() = newPath;
                break;
            }
        }
        return newPath;
    }
    return {};
}

QStringList SessionStore::searchSessions(const QString &query, int maxCount) const
{
    if (query.isEmpty())
        return listSessionFiles(maxCount);

    const QStringList allFiles = listSessionFiles(200);
    QStringList results;

    for (const QString &path : allFiles) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        const QByteArray content = file.readAll();
        if (content.contains(query.toUtf8())) {
            results.append(path);
            if (results.size() >= maxCount)
                break;
        }
    }
    return results;
}

ChatSession SessionStore::parseSessionFile(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    ChatSession session;
    session.sessionId = QFileInfo(filePath).baseName()
                            .section(QLatin1Char('-'), 2);  // strip yyyyMMdd-HHmmss-

    while (!file.atEnd()) {
        const QByteArray raw = file.readLine().trimmed();
        if (raw.isEmpty())
            continue;

        QJsonParseError err;
        const QJsonObject obj = QJsonDocument::fromJson(raw, &err).object();
        if (err.error != QJsonParseError::NoError || obj.isEmpty())
            continue;

        const QString event = obj.value(QLatin1String("event")).toString();
        const QJsonObject data = obj.value(QLatin1String("data")).toObject();

        if (event == QLatin1String("session_start")) {
            session.sessionId  = obj.value(QLatin1String("sessionId")).toString();
            session.providerId = data.value(QLatin1String("provider")).toString();
            session.model      = data.value(QLatin1String("model")).toString();
            session.agentMode  = data.value(QLatin1String("agentMode")).toBool();
            const QString ts   = obj.value(QLatin1String("ts")).toString();
            session.createdAt  = QDateTime::fromString(ts, Qt::ISODateWithMs);
        } else if (event == QLatin1String("user")) {
            const QString text = data.value(QLatin1String("text")).toString();
            if (!text.isEmpty())
                session.messages.append({QStringLiteral("user"), text});
        } else if (event == QLatin1String("assistant")) {
            const QString text = data.value(QLatin1String("text")).toString();
            if (!text.isEmpty())
                session.messages.append({QStringLiteral("assistant"), text});
        }
    }
    return session;
}
