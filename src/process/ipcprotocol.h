#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

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

QString bridgePipeName();

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
                           const QJsonObject &params = {});
    static Message notification(const QString &method,
                                const QJsonObject &params = {});
    static Message response(int id, const QJsonObject &result);
    static Message errorResponse(int id, int code, const QString &message);

    QByteArray serialize() const;
    static Message deserialize(const QByteArray &data);
};

// ── Wire framing helpers ──────────────────────────────────────────────────
// 4-byte big-endian length prefix + JSON payload

QByteArray frame(const QByteArray &payload);
bool tryUnframe(QByteArray &buffer, QByteArray &outPayload);

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
