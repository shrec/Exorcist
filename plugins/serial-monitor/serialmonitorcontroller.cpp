#include "serialmonitorcontroller.h"

#ifdef EXORCIST_HAS_QT_SERIALPORT
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

SerialMonitorController::SerialMonitorController(QObject *parent)
    : QObject(parent)
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    m_port = std::make_unique<QSerialPort>();
    wirePortSignals();
#endif
}

SerialMonitorController::~SerialMonitorController() = default;

QStringList SerialMonitorController::availablePorts() const
{
    return m_ports;
}

bool SerialMonitorController::isConnected() const
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    return m_port && m_port->isOpen();
#else
    return false;
#endif
}

QString SerialMonitorController::currentPortName() const
{
    return m_currentPortName;
}

bool SerialMonitorController::backendAvailable() const
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    return true;
#else
    return false;
#endif
}

void SerialMonitorController::refreshPorts()
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    m_ports.clear();
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &port : ports)
        m_ports.append(port.portName());

    emit portsChanged(m_ports);
    if (m_ports.isEmpty())
        emit statusChanged(tr("No serial ports detected."));
    else
        emit statusChanged(tr("Detected %1 serial port(s).").arg(m_ports.size()));
#else
    m_ports.clear();
    emit portsChanged(m_ports);
    emit statusChanged(tr("Qt SerialPort is not available in this build."));
#endif
}

void SerialMonitorController::connectToPort(const QString &portName, int baudRate)
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    if (!m_port) {
        emit statusChanged(tr("Serial backend is not initialized."));
        return;
    }

    if (portName.isEmpty()) {
        emit statusChanged(tr("Select a serial port first."));
        return;
    }

    if (m_port->isOpen())
        m_port->close();

    m_port->setPortName(portName);
    m_port->setBaudRate(baudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        emit statusChanged(tr("Failed to open %1: %2")
                               .arg(portName, m_port->errorString()));
        emit connectionChanged(false);
        return;
    }

    m_currentPortName = portName;
    emit statusChanged(tr("Connected to %1 @ %2 baud.")
                           .arg(portName)
                           .arg(baudRate));
    emit connectionChanged(true);
#else
    Q_UNUSED(portName);
    Q_UNUSED(baudRate);
    emit statusChanged(tr("Qt SerialPort is not available in this build."));
    emit connectionChanged(false);
#endif
}

void SerialMonitorController::disconnectPort()
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    if (m_port && m_port->isOpen()) {
        const QString portName = m_port->portName();
        m_port->close();
        emit statusChanged(tr("Disconnected from %1.").arg(portName));
    }
    m_currentPortName.clear();
    emit connectionChanged(false);
#else
    emit statusChanged(tr("Qt SerialPort is not available in this build."));
    emit connectionChanged(false);
#endif
}

void SerialMonitorController::sendText(const QString &text, int newlineMode)
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    if (!m_port || !m_port->isOpen()) {
        emit statusChanged(tr("Connect to a serial port before sending data."));
        return;
    }

    QByteArray payload = text.toUtf8();
    const auto mode = static_cast<NewlineMode>(newlineMode);
    switch (mode) {
    case NewlineMode::Lf:
        payload.append('\n');
        break;
    case NewlineMode::CrLf:
        payload.append("\r\n");
        break;
    case NewlineMode::None:
        break;
    }

    m_port->write(payload);
    emit textReceived(tr("> %1").arg(text));
#else
    Q_UNUSED(text);
    Q_UNUSED(newlineMode);
    emit statusChanged(tr("Qt SerialPort is not available in this build."));
#endif
}

void SerialMonitorController::wirePortSignals()
{
#ifdef EXORCIST_HAS_QT_SERIALPORT
    if (!m_port)
        return;

    connect(m_port.get(), &QSerialPort::readyRead, this, [this]() {
        const QByteArray data = m_port->readAll();
        emit textReceived(QString::fromUtf8(data));
    });

    connect(m_port.get(), &QSerialPort::errorOccurred, this,
            [this](QSerialPort::SerialPortError error) {
        if (error == QSerialPort::NoError)
            return;
        emit statusChanged(tr("Serial port error: %1").arg(m_port->errorString()));
    });
#endif
}