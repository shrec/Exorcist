#include "socketlsptransport.h"

#include <QJsonDocument>
#include <QTcpSocket>

SocketLspTransport::SocketLspTransport(QObject *parent)
    : LspTransport(parent),
      m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::readyRead,
            this, &SocketLspTransport::handleReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, [this]() {
                emit transportError(m_socket->errorString());
            });
}

void SocketLspTransport::connectToServer(const QString &host, int port)
{
    m_socket->connectToHost(host, static_cast<quint16>(port));
}

void SocketLspTransport::start()
{
    // Nothing extra — socket connect happens via connectToServer()
}

void SocketLspTransport::stop()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->disconnectFromHost();
}

void SocketLspTransport::send(const LspMessage &message)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        emit transportError(tr("Socket not connected"));
        return;
    }

    const QJsonDocument doc(message.payload);
    const QByteArray json = doc.toJson(QJsonDocument::Compact);
    const QByteArray header = "Content-Length: " + QByteArray::number(json.size()) + "\r\n\r\n";
    m_socket->write(header);
    m_socket->write(json);
    m_socket->flush();
}

bool SocketLspTransport::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void SocketLspTransport::handleReadyRead()
{
    m_buffer.append(m_socket->readAll());
    while (tryReadMessage()) {
    }
}

bool SocketLspTransport::tryReadMessage()
{
    const QByteArray separator = "\r\n\r\n";
    const int headerEnd = m_buffer.indexOf(separator);
    if (headerEnd < 0)
        return false;

    const QByteArray header = m_buffer.left(headerEnd);
    const QList<QByteArray> lines = header.split('\n');
    int contentLength = -1;
    for (const QByteArray &lineRaw : lines) {
        const QByteArray line = lineRaw.trimmed();
        if (line.startsWith("Content-Length:")) {
            contentLength = line.mid(static_cast<int>(strlen("Content-Length:"))).trimmed().toInt();
            break;
        }
    }

    if (contentLength < 0) {
        emit transportError(tr("Invalid LSP header"));
        m_buffer.remove(0, headerEnd + separator.size());
        return false;
    }

    const int totalSize = headerEnd + separator.size() + contentLength;
    if (m_buffer.size() < totalSize)
        return false;

    const QByteArray payload = m_buffer.mid(headerEnd + separator.size(), contentLength);
    m_buffer.remove(0, totalSize);

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        emit transportError(tr("Invalid JSON payload"));
        return true;
    }

    LspMessage message;
    message.payload = doc.object();
    emit messageReceived(message);
    return true;
}
