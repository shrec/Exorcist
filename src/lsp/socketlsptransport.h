#pragma once

#include "lsptransport.h"

#include <QByteArray>

class QTcpSocket;

class SocketLspTransport : public LspTransport
{
    Q_OBJECT
public:
    explicit SocketLspTransport(QObject *parent = nullptr);

    void connectToServer(const QString &host, int port);

    void start() override;
    void stop() override;
    void send(const LspMessage &message) override;

    bool isConnected() const;

private:
    void handleReadyRead();
    bool tryReadMessage();

    QTcpSocket *m_socket;
    QByteArray m_buffer;
};
