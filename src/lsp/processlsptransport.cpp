#include "processlsptransport.h"

#include <QJsonDocument>
#include <QProcess>

ProcessLspTransport::ProcessLspTransport(QProcess *process, QObject *parent)
    : LspTransport(parent),
      m_process(process)
{
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &ProcessLspTransport::handleReadyRead);
}

void ProcessLspTransport::start()
{
    if (!m_process) {
        emit transportError("Process not set");
    }
}

void ProcessLspTransport::stop()
{
}

void ProcessLspTransport::send(const LspMessage &message)
{
    const QJsonDocument doc(message.payload);
    const QByteArray json = doc.toJson(QJsonDocument::Compact);
    const QByteArray header = "Content-Length: " + QByteArray::number(json.size()) + "\r\n\r\n";
    m_process->write(header);
    m_process->write(json);
    m_process->waitForBytesWritten(0);
}

void ProcessLspTransport::handleReadyRead()
{
    m_buffer.append(m_process->readAllStandardOutput());
    while (tryReadMessage()) {
    }
}

bool ProcessLspTransport::tryReadMessage()
{
    const QByteArray separator = "\r\n\r\n";
    const int headerEnd = m_buffer.indexOf(separator);
    if (headerEnd < 0) {
        return false;
    }

    const QByteArray header = m_buffer.left(headerEnd);
    const QList<QByteArray> lines = header.split('\n');
    int contentLength = -1;
    for (const QByteArray &lineRaw : lines) {
        const QByteArray line = lineRaw.trimmed();
        if (line.startsWith("Content-Length:")) {
            const QByteArray value = line.mid(strlen("Content-Length:")).trimmed();
            contentLength = value.toInt();
            break;
        }
    }

    if (contentLength < 0) {
        emit transportError("Invalid LSP header");
        m_buffer.remove(0, headerEnd + separator.size());
        return false;
    }

    const int totalSize = headerEnd + separator.size() + contentLength;
    if (m_buffer.size() < totalSize) {
        return false;
    }

    const QByteArray payload = m_buffer.mid(headerEnd + separator.size(), contentLength);
    m_buffer.remove(0, totalSize);

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        emit transportError("Invalid JSON payload");
        return true;
    }

    LspMessage message;
    message.payload = doc.object();
    emit messageReceived(message);
    return true;
}
