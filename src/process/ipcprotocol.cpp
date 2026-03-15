#include "ipcprotocol.h"

#include <QJsonDocument>

namespace Ipc {

QString bridgePipeName()
{
    const QString user = qEnvironmentVariable("USER");  // Unix
    const QString winUser = qEnvironmentVariable("USERNAME"); // Windows
    const QString name = user.isEmpty() ? winUser : user;
    return QStringLiteral("exobridge-%1").arg(
        name.isEmpty() ? QStringLiteral("default") : name);
}

// ── Message factory methods ───────────────────────────────────────────────

Message Message::request(int id, const QString &method,
                         const QJsonObject &params)
{
    return {MessageKind::Request, id, method, params, {}, {}, false};
}

Message Message::notification(const QString &method,
                              const QJsonObject &params)
{
    return {MessageKind::Notification, -1, method, params, {}, {}, false};
}

Message Message::response(int id, const QJsonObject &result)
{
    return {MessageKind::Response, id, {}, {}, result, {}, false};
}

Message Message::errorResponse(int id, int code, const QString &message)
{
    QJsonObject err;
    err[QLatin1String("code")]    = code;
    err[QLatin1String("message")] = message;
    return {MessageKind::Response, id, {}, {}, {}, err, true};
}

QByteArray Message::serialize() const
{
    QJsonObject obj;
    obj[QLatin1String("jsonrpc")] = QStringLiteral("2.0");

    switch (kind) {
    case MessageKind::Request:
        obj[QLatin1String("id")]     = id;
        obj[QLatin1String("method")] = method;
        if (!params.isEmpty())
            obj[QLatin1String("params")] = params;
        break;
    case MessageKind::Notification:
        obj[QLatin1String("method")] = method;
        if (!params.isEmpty())
            obj[QLatin1String("params")] = params;
        break;
    case MessageKind::Response:
        obj[QLatin1String("id")] = id;
        if (isError)
            obj[QLatin1String("error")] = error;
        else
            obj[QLatin1String("result")] = result;
        break;
    }

    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

Message Message::deserialize(const QByteArray &data)
{
    const auto doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return {};

    const auto obj = doc.object();
    Message msg;

    const bool hasMethod = obj.contains(QLatin1String("method"));
    const bool hasId     = obj.contains(QLatin1String("id"));

    if (hasMethod && hasId) {
        msg.kind   = MessageKind::Request;
        msg.id     = obj[QLatin1String("id")].toInt(-1);
        msg.method = obj[QLatin1String("method")].toString();
        msg.params = obj[QLatin1String("params")].toObject();
    } else if (hasMethod && !hasId) {
        msg.kind   = MessageKind::Notification;
        msg.method = obj[QLatin1String("method")].toString();
        msg.params = obj[QLatin1String("params")].toObject();
    } else if (hasId) {
        msg.kind    = MessageKind::Response;
        msg.id      = obj[QLatin1String("id")].toInt(-1);
        msg.isError = obj.contains(QLatin1String("error"));
        if (msg.isError)
            msg.error = obj[QLatin1String("error")].toObject();
        else
            msg.result = obj[QLatin1String("result")].toObject();
    }

    return msg;
}

// ── Wire framing helpers ──────────────────────────────────────────────────

QByteArray frame(const QByteArray &payload)
{
    const quint32 len = static_cast<quint32>(payload.size());
    QByteArray framed;
    framed.reserve(4 + payload.size());
    framed.append(static_cast<char>((len >> 24) & 0xFF));
    framed.append(static_cast<char>((len >> 16) & 0xFF));
    framed.append(static_cast<char>((len >> 8) & 0xFF));
    framed.append(static_cast<char>(len & 0xFF));
    framed.append(payload);
    return framed;
}

bool tryUnframe(QByteArray &buffer, QByteArray &outPayload)
{
    if (buffer.size() < 4)
        return false;

    const auto *ptr = reinterpret_cast<const unsigned char *>(buffer.constData());
    const quint32 len = (static_cast<quint32>(ptr[0]) << 24)
                      | (static_cast<quint32>(ptr[1]) << 16)
                      | (static_cast<quint32>(ptr[2]) << 8)
                      | static_cast<quint32>(ptr[3]);

    if (len > 16 * 1024 * 1024) {  // sanity: max 16 MB per message
        // Drain the oversized header to prevent the buffer from being
        // stuck forever with unparseable data.
        buffer.remove(0, 4);
        return false;
    }

    if (static_cast<quint32>(buffer.size()) < 4 + len)
        return false;

    outPayload = buffer.mid(4, static_cast<int>(len));
    buffer.remove(0, 4 + static_cast<int>(len));
    return true;
}

} // namespace Ipc
