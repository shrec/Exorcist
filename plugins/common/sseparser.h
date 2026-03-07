#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class SseParser : public QObject
{
    Q_OBJECT

public:
    explicit SseParser(QObject *parent = nullptr);

    void feed(const QByteArray &chunk);
    void reset();

signals:
    void eventReceived(const QString &eventType, const QString &data);
    void done();

private:
    QByteArray m_buffer;
    QString m_eventType;
    QString m_data;
};
