#pragma once

#include <QObject>

#include "lspmessage.h"

class LspTransport : public QObject
{
    Q_OBJECT

public:
    explicit LspTransport(QObject *parent = nullptr);
    virtual ~LspTransport() override = default;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void send(const LspMessage &message) = 0;

signals:
    void messageReceived(const LspMessage &message);
    void transportError(const QString &error);
};
