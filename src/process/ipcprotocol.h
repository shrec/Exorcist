#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringLiteral>

// ── ExoBridge IPC Protocol ─────────────────────────────────────────────────────
//
// JSON-RPC 2.0 messages over QLocalSocket (named pipe on Windows, Unix domain
// socket on Linux/macOS).  Framing: each message is preceded by a 4-byte
// big-endian length prefix (uint32), followed by the UTF-8 JSON payload.
//
// ExoBridge is the shared daemon process that manages cross-instance resources
// (workspace index, MCP servers, git file watchers, auth tokens).
// The protocol is intentionally LSP-like so our existing transport patterns
// (e.g. LspTransport) can be reused.

namespace Ipc {

// ── Bridge pipe name ──────────────────────────────────────────────────────
// Per-user unique to avoid collisions on multi-user machines.

inline QString bridgePipeName()
{
    const QString user = qEnvironmentVariable("USER");  // Unix
    const QString winUser = qEnvironmentVariable("USERNAME"); // Windows
    const QString name = user.isEmpty() ? winUser : user;
    return QStringLiteral("exobridge-%1").arg(
        name.isEmpty() ? QStringLiteral("default") : name);
}

// ── Message types ─────────────────────────────────────────────────────────

enum class MessageKind { Request, Response, Notification };

struct Message
{
    MessageKind kind  = MessageKind::Request;
    int         id    = -1;         // -1 for notifications
    QString     method;             // request/notification method
    QJsonObject params;             // request params
    QJsonObject result;             // response result
    QJsonObject error;              // response error
    bool        isError = false;

    static Message request(int id, const QString &method,
                           const QJsonObject &params = {})
    {
        return {MessageKind::Request, id, method, params, {}, {}, false};
    }

    static Message notification(const QString &method,
                                const QJsonObject &params = {})
    {
        return {MessageKind::Notification, -1, method, params, {}, {}, false};
    }

    static Message response(int id, const QJsonObject &result)
    {
        return {MessageKind::Response, id, {}, {}, result, {}, false};
    }

    static Message errorResponse(int id, int code, const QString &message)
    {
        QJsonObject err;
        err[QLatin1String("code")]    = code;
        err[QLatin1String("message")] = message;
        return {MessageKind::Response, id, {}, {}, {}, err, true};
    }

    QByteArray serialize() const
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

    static Message deserialize(const QByteArray &data)
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
};

// ── Wire framing helpers ──────────────────────────────────────────────────
// 4-byte big-endian length prefix + JSON payload

inline QByteArray frame(const QByteArray &payload)
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

inline bool tryUnframe(QByteArray &buffer, QByteArray &outPayload)
{
    if (buffer.size() < 4)
        return false;

    const auto *ptr = reinterpret_cast<const unsigned char *>(buffer.constData());
    const quint32 len = (static_cast<quint32>(ptr[0]) << 24)
                      | (static_cast<quint32>(ptr[1]) << 16)
                      | (static_cast<quint32>(ptr[2]) << 8)
                      | static_cast<quint32>(ptr[3]);

    if (len > 16 * 1024 * 1024)  // sanity: max 16 MB per message
        return false;

    if (static_cast<quint32>(buffer.size()) < 4 + len)
        return false;

    outPayload = buffer.mid(4, static_cast<int>(len));
    buffer.remove(0, 4 + static_cast<int>(len));
    return true;
}

// ── Standard method names ─────────────────────────────────────────────────

namespace Method {
    // Client → Server
    inline constexpr const char *Handshake       = "server/handshake";
    inline constexpr const char *Shutdown         = "server/shutdown";
    inline constexpr const char *RegisterService  = "service/register";
    inline constexpr const char *UnregisterService = "service/unregister";
    inline constexpr const char *CallService      = "service/call";
    inline constexpr const char *ListServices     = "service/list";
    inline constexpr const char *ProcessStart     = "process/start";
    inline constexpr const char *ProcessKill      = "process/kill";
    inline constexpr const char *ProcessList      = "process/list";

    // Server → Client (notifications)
    inline constexpr const char *ServiceEvent     = "service/event";
    inline constexpr const char *ProcessExited    = "process/exited";
    inline constexpr const char *ServerShutdown   = "server/shuttingDown";
}

// ── Error codes ───────────────────────────────────────────────────────────

namespace ErrorCode {
    inline constexpr int ParseError     = -32700;
    inline constexpr int InvalidRequest = -32600;
    inline constexpr int MethodNotFound = -32601;
    inline constexpr int InvalidParams  = -32602;
    inline constexpr int InternalError  = -32603;
    inline constexpr int ServiceNotFound = -32000;
    inline constexpr int ProcessNotFound = -32001;
}

} // namespace Ipc
