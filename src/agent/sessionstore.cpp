#include "sessionstore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QStandardPaths>

SessionStore::SessionStore(const QString &dir, QObject *parent)
    : QObject(parent),
      m_dir(dir)
{
    QDir().mkpath(m_dir);
}

SessionStore::SessionStore(QObject *parent)
    : SessionStore(defaultDir(), parent)
{
}

QString SessionStore::defaultDir()
{
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    return base + QLatin1String("/sessions");
}

void SessionStore::recordSessionStart(const QString &id, const QString &model,
                                      const QString &mode)
{
    QJsonObject data;
    data["model"] = model;
    data["mode"] = mode;

    QJsonObject entry;
    entry["v"] = 1;
    entry["sid"] = id;
    entry["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["type"] = QStringLiteral("session.start");
    entry["data"] = data;
    appendLine(id, entry);
}

void SessionStore::recordUserMessage(const QString &id, const QString &content)
{
    QJsonObject data;
    data["content"] = content;

    QJsonObject entry;
    entry["v"] = 1;
    entry["sid"] = id;
    entry["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["type"] = QStringLiteral("user.message");
    entry["data"] = data;
    appendLine(id, entry);
}

void SessionStore::recordAssistantMessage(const QString &id, const QString &text)
{
    QJsonObject data;
    data["text"] = text;

    QJsonObject entry;
    entry["v"] = 1;
    entry["sid"] = id;
    entry["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["type"] = QStringLiteral("assistant.message");
    entry["data"] = data;
    appendLine(id, entry);
}

void SessionStore::recordToolCall(const QString &id, const QString &name,
                                  const QJsonObject &args, bool ok,
                                  const QString &result)
{
    QJsonObject data;
    data["name"] = name;
    data["args"] = args;
    data["ok"] = ok;
    data["result"] = result;

    QJsonObject entry;
    entry["v"] = 1;
    entry["sid"] = id;
    entry["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["type"] = QStringLiteral("tool.call");
    entry["data"] = data;
    appendLine(id, entry);
}

void SessionStore::recordSessionEnd(const QString &id)
{
    QJsonObject entry;
    entry["v"] = 1;
    entry["sid"] = id;
    entry["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["type"] = QStringLiteral("session.end");
    entry["data"] = QJsonObject{};
    appendLine(id, entry);
}

void SessionStore::recordCompleteTurn(const QString &id, const QJsonObject &turnJson)
{
    QJsonObject entry;
    entry[QLatin1String("v")] = 1;
    entry[QLatin1String("sid")] = id;
    entry[QLatin1String("ts")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry[QLatin1String("type")] = QStringLiteral("turn.complete");
    entry[QLatin1String("data")] = turnJson;
    appendLine(id, entry);
}

void SessionStore::appendLine(const QString &id, const QJsonObject &entry)
{
    QDir().mkpath(m_dir);
    const QString path = sessionFilePath(id);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;

    const QJsonDocument doc(entry);
    f.write(doc.toJson(QJsonDocument::Compact));
    f.write("\n");
    f.close();

    pruneOldSessions(50);
}

void SessionStore::pruneOldSessions(int keep)
{
    QDir dir(m_dir);
    const QFileInfoList files = dir.entryInfoList(
        {"*.jsonl"}, QDir::Files, QDir::Time);
    for (int i = keep; i < files.size(); ++i)
        QFile::remove(files[i].absoluteFilePath());
}

QVector<SessionStore::Summary> SessionStore::recentSessions(int max) const
{
    QVector<Summary> out;
    QDir dir(m_dir);
    const QFileInfoList files = dir.entryInfoList(
        {"*.jsonl"}, QDir::Files, QDir::Time);
    for (int i = 0; i < files.size() && out.size() < max; ++i) {
        Summary s;
        s.sessionId = files[i].completeBaseName();
        s.created = files[i].birthTime().isValid()
            ? files[i].birthTime()
            : files[i].lastModified();
        s.title = s.sessionId;
        s.turnCount = 0;

        // Read title and turn count from the JSONL file
        QFile f(files[i].absoluteFilePath());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!f.atEnd()) {
                const QByteArray line = f.readLine().trimmed();
                if (line.isEmpty())
                    continue;
                const QJsonDocument doc = QJsonDocument::fromJson(line);
                if (!doc.isObject())
                    continue;
                const QJsonObject obj = doc.object();
                const QString type = obj.value(QLatin1String("type")).toString();
                if (type == QLatin1String("title")) {
                    const QString t = obj.value(QLatin1String("title")).toString();
                    if (!t.isEmpty())
                        s.title = t;
                } else if (type == QLatin1String("user.message")
                           || type == QLatin1String("turn.complete")) {
                    ++s.turnCount;
                }
            }
        }

        out.append(s);
    }
    return out;
}

void SessionStore::startSession(const QString &id, const QString &providerId,
                                const QString &model, bool agentMode)
{
    QJsonObject data;
    data["provider"] = providerId;
    data["model"] = model;
    data["mode"] = agentMode ? QStringLiteral("Agent") : QStringLiteral("Ask");

    QJsonObject entry;
    entry["v"] = 1;
    entry["sid"] = id;
    entry["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["type"] = QStringLiteral("session.start");
    entry["data"] = data;
    appendLine(id, entry);
}

void SessionStore::appendUserMessage(const QString &id, const QString &content)
{
    recordUserMessage(id, content);
}

void SessionStore::appendAssistantMessage(const QString &id, const QString &text)
{
    recordAssistantMessage(id, text);
}

void SessionStore::appendToolCall(const QString &id, const QString &name,
                                  const QJsonObject &args, const QString &result, bool ok)
{
    recordToolCall(id, name, args, ok, result);
}

void SessionStore::appendError(const QString &id, const QString &message)
{
    QJsonObject data;
    data["message"] = message;

    QJsonObject entry;
    entry["v"] = 1;
    entry["sid"] = id;
    entry["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["type"] = QStringLiteral("error");
    entry["data"] = data;
    appendLine(id, entry);
}

QStringList SessionStore::listSessionFiles(int max) const
{
    QStringList out;
    QDir dir(m_dir);
    const QFileInfoList files = dir.entryInfoList(
        {"*.jsonl"}, QDir::Files, QDir::Time);
    for (int i = 0; i < files.size() && out.size() < max; ++i)
        out.append(files[i].absoluteFilePath());
    return out;
}

SessionStore::ChatSession SessionStore::loadSession(const QString &path) const
{
    ChatSession sess;
    QFileInfo fi(path);
    if (!fi.exists())
        return sess;

    sess.sessionId = fi.completeBaseName();
    sess.createdAt = fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return sess;

    while (!f.atEnd()) {
        const QByteArray line = f.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject())
            continue;
        const QJsonObject obj = doc.object();
        const QString type = obj.value("type").toString();
        const QJsonObject data = obj.value("data").toObject();
        if (type == QLatin1String("session.start")) {
            const QString ts = obj.value("ts").toString();
            const QDateTime dt = QDateTime::fromString(ts, Qt::ISODate);
            if (dt.isValid())
                sess.createdAt = dt;
        } else if (type == QLatin1String("user.message")) {
            sess.messages.append({QStringLiteral("user"),
                                  data.value("content").toString()});
        } else if (type == QLatin1String("assistant.message")) {
            sess.messages.append({QStringLiteral("assistant"),
                                  data.value("text").toString()});
        } else if (type == QLatin1String("turn.complete")) {
            sess.completeTurns.append(data);
        }
    }
    return sess;
}

SessionStore::ChatSession SessionStore::loadLastSession() const
{
    const QStringList files = listSessionFiles(1);
    if (files.isEmpty())
        return {};
    return loadSession(files.first());
}

QString SessionStore::renameSession(const QString &path, const QString &newName)
{
    QFileInfo fi(path);
    if (!fi.exists())
        return {};

    const QString safe = sanitizeName(newName);
    if (safe.isEmpty())
        return {};

    const QString newPath = fi.absoluteDir().absoluteFilePath(safe + ".jsonl");
    if (QFile::rename(path, newPath))
        return newPath;
    return {};
}

void SessionStore::deleteSession(const QString &path)
{
    QFile::remove(path);
}

void SessionStore::setSessionTitle(const QString &id, const QString &title)
{
    QJsonObject entry;
    entry[QLatin1String("type")] = QLatin1String("title");
    entry[QLatin1String("title")] = title;
    appendLine(id, entry);
}

QStringList SessionStore::searchSessions(const QString &query, int max) const
{
    if (query.trimmed().isEmpty())
        return listSessionFiles(max);

    QStringList out;
    QDir dir(m_dir);
    const QFileInfoList files = dir.entryInfoList(
        {"*.jsonl"}, QDir::Files, QDir::Time);
    const QString q = query.toLower();

    for (const QFileInfo &fi : files) {
        if (out.size() >= max)
            break;
        if (fi.completeBaseName().toLower().contains(q)) {
            out.append(fi.absoluteFilePath());
            continue;
        }

        QFile f(fi.absoluteFilePath());
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        bool match = false;
        while (!f.atEnd()) {
            const QByteArray line = f.readLine();
            if (QString::fromUtf8(line).toLower().contains(q)) {
                match = true;
                break;
            }
        }
        if (match)
            out.append(fi.absoluteFilePath());
    }
    return out;
}

QString SessionStore::sessionFilePath(const QString &id) const
{
    return m_dir + QLatin1Char('/') + id + QLatin1String(".jsonl");
}

QString SessionStore::sanitizeName(const QString &name)
{
    QString out = name;
    out.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]+")), QStringLiteral("_"));
    while (out.startsWith(QLatin1Char('_')))
        out.remove(0, 1);
    if (out.isEmpty())
        out = QStringLiteral("session");
    return out;
}

// ── Export ─────────────────────────────────────────────────────────────────────

bool SessionStore::exportToMarkdown(const QString &sessionPath, const QString &outputPath) const
{
    const ChatSession sess = loadSession(sessionPath);
    if (sess.isEmpty())
        return false;

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream ts(&out);
    ts << QStringLiteral("# Session: %1\n").arg(sess.sessionId);
    ts << QStringLiteral("Date: %1\n\n").arg(
              sess.createdAt.toString(Qt::ISODate));

    for (const auto &msg : sess.messages) {
        if (msg.first == QLatin1String("user"))
            ts << QStringLiteral("**User:** ");
        else
            ts << QStringLiteral("**Assistant:** ");
        ts << msg.second << QStringLiteral("\n\n");
    }

    return true;
}

bool SessionStore::exportToJson(const QString &sessionPath, const QString &outputPath) const
{
    const ChatSession sess = loadSession(sessionPath);
    if (sess.isEmpty())
        return false;

    QJsonObject root;
    root[QLatin1String("sessionId")] = sess.sessionId;
    root[QLatin1String("createdAt")] = sess.createdAt.toString(Qt::ISODate);

    QJsonArray msgs;
    for (const auto &msg : sess.messages) {
        QJsonObject m;
        m[QLatin1String("role")] = msg.first;
        m[QLatin1String("content")] = msg.second;
        msgs.append(m);
    }
    root[QLatin1String("messages")] = msgs;

    if (!sess.completeTurns.isEmpty())
        root[QLatin1String("turns")] = sess.completeTurns;

    QFile out(outputPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    out.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}
