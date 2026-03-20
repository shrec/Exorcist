#pragma once

#include <QObject>
#include <QStringList>

#include <memory>

class QSerialPort;

class SerialMonitorController : public QObject
{
    Q_OBJECT

public:
    enum class NewlineMode {
        None = 0,
        Lf,
        CrLf,
    };

    explicit SerialMonitorController(QObject *parent = nullptr);
    ~SerialMonitorController() override;

    QStringList availablePorts() const;
    bool isConnected() const;
    QString currentPortName() const;
    bool backendAvailable() const;

public slots:
    void refreshPorts();
    void connectToPort(const QString &portName, int baudRate);
    void disconnectPort();
    void sendText(const QString &text, int newlineMode);

signals:
    void portsChanged(const QStringList &ports);
    void statusChanged(const QString &status);
    void connectionChanged(bool connected);
    void textReceived(const QString &text);

private:
    void wirePortSignals();

    QStringList m_ports;
    QString m_currentPortName;
    std::unique_ptr<QSerialPort> m_port;
};
