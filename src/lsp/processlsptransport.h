#pragma once

#include <QByteArray>

#include "lsptransport.h"

class QProcess;

class ProcessLspTransport : public LspTransport
{
    Q_OBJECT

public:
    explicit ProcessLspTransport(QProcess *process, QObject *parent = nullptr);

    void start() override;
    void stop() override;
    void send(const LspMessage &message) override;

private:
    void handleReadyRead();
    bool tryReadMessage();

    QProcess *m_process;
    QByteArray m_buffer;
};
