#include "serialmonitorpanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <memory>

SerialMonitorPanel::SerialMonitorPanel(QWidget *parent)
    : QWidget(parent)
{
    auto layout = std::make_unique<QVBoxLayout>(this);

    auto titleLabel = std::make_unique<QLabel>(tr("Serial monitor"), this);
    titleLabel->setObjectName(QStringLiteral("serialMonitorTitleLabel"));
    layout->addWidget(titleLabel.get());

    auto statusLabel = std::make_unique<QLabel>(this);
    m_statusLabel = statusLabel.get();
    m_statusLabel->setObjectName(QStringLiteral("serialMonitorStatusLabel"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto connectionRow = std::make_unique<QHBoxLayout>();
    auto portCombo = std::make_unique<QComboBox>(this);
    m_portCombo = portCombo.get();
    m_portCombo->setObjectName(QStringLiteral("serialMonitorPortCombo"));
    auto baudCombo = std::make_unique<QComboBox>(this);
    m_baudCombo = baudCombo.get();
    m_baudCombo->setObjectName(QStringLiteral("serialMonitorBaudCombo"));
    m_baudCombo->addItems({
        QStringLiteral("9600"),
        QStringLiteral("19200"),
        QStringLiteral("38400"),
        QStringLiteral("57600"),
        QStringLiteral("115200")
    });
    m_baudCombo->setCurrentText(QStringLiteral("115200"));

    auto refreshButton = std::make_unique<QPushButton>(tr("Refresh Ports"), this);
    m_refreshButton = refreshButton.get();
    m_refreshButton->setObjectName(QStringLiteral("serialMonitorRefreshButton"));
    auto connectButton = std::make_unique<QPushButton>(tr("Connect"), this);
    m_connectButton = connectButton.get();
    m_connectButton->setObjectName(QStringLiteral("serialMonitorConnectButton"));
    auto disconnectButton = std::make_unique<QPushButton>(tr("Disconnect"), this);
    m_disconnectButton = disconnectButton.get();
    m_disconnectButton->setObjectName(QStringLiteral("serialMonitorDisconnectButton"));
    auto newlineCombo = std::make_unique<QComboBox>(this);
    m_newlineCombo = newlineCombo.get();
    m_newlineCombo->setObjectName(QStringLiteral("serialMonitorNewlineCombo"));
    m_newlineCombo->addItem(tr("No newline"), static_cast<int>(NewlineMode::None));
    m_newlineCombo->addItem(tr("LF"), static_cast<int>(NewlineMode::Lf));
    m_newlineCombo->addItem(tr("CRLF"), static_cast<int>(NewlineMode::CrLf));
    auto timestampCheck = std::make_unique<QCheckBox>(tr("Timestamps"), this);
    m_timestampCheck = timestampCheck.get();
    m_timestampCheck->setObjectName(QStringLiteral("serialMonitorTimestampCheck"));

    connectionRow->addWidget(m_portCombo, 1);
    connectionRow->addWidget(m_baudCombo);
    connectionRow->addWidget(m_newlineCombo);
    connectionRow->addWidget(m_timestampCheck);
    connectionRow->addWidget(m_refreshButton);
    connectionRow->addWidget(m_connectButton);
    connectionRow->addWidget(m_disconnectButton);
    layout->addLayout(connectionRow.get());

    auto output = std::make_unique<QPlainTextEdit>(this);
    m_output = output.get();
    m_output->setObjectName(QStringLiteral("serialMonitorOutput"));
    m_output->setReadOnly(true);
    layout->addWidget(m_output, 1);

    auto buttonRow = std::make_unique<QHBoxLayout>();
    auto input = std::make_unique<QLineEdit>(this);
    m_input = input.get();
    m_input->setObjectName(QStringLiteral("serialMonitorInput"));
    m_input->setPlaceholderText(tr("Type text to send to the device"));
    auto openExternalButton = std::make_unique<QPushButton>(tr("Run External Monitor"), this);
    auto sendButton = std::make_unique<QPushButton>(tr("Send"), this);
    auto clearButton = std::make_unique<QPushButton>(tr("Clear"), this);
    buttonRow->addWidget(m_input, 1);
    buttonRow->addWidget(sendButton.get());
    buttonRow->addWidget(openExternalButton.get());
    buttonRow->addWidget(clearButton.get());
    buttonRow->addStretch();
    layout->addLayout(buttonRow.get());

    connect(m_refreshButton, &QPushButton::clicked,
            this, &SerialMonitorPanel::refreshRequested);
    connect(m_connectButton, &QPushButton::clicked,
            this, [this]() {
        emit connectRequested(selectedPort(), selectedBaudRate());
    });
    connect(m_disconnectButton, &QPushButton::clicked,
            this, &SerialMonitorPanel::disconnectRequested);
        connect(m_portCombo, &QComboBox::currentTextChanged,
            this, &SerialMonitorPanel::settingsChanged);
        connect(m_baudCombo, &QComboBox::currentTextChanged,
            this, &SerialMonitorPanel::settingsChanged);
        connect(m_newlineCombo, &QComboBox::currentIndexChanged,
            this, [this](int) { emit settingsChanged(); });
        connect(m_timestampCheck, &QCheckBox::toggled,
            this, &SerialMonitorPanel::settingsChanged);

    connect(openExternalButton.get(), &QPushButton::clicked,
            this, &SerialMonitorPanel::openExternalRequested);
    connect(sendButton.get(), &QPushButton::clicked,
            this, [this]() {
        emit sendTextRequested(m_input ? m_input->text() : QString(),
                       static_cast<int>(selectedNewlineMode()));
        if (m_input)
            m_input->clear();
    });
    connect(m_input, &QLineEdit::returnPressed,
            this, [this]() {
        emit sendTextRequested(m_input ? m_input->text() : QString(),
                       static_cast<int>(selectedNewlineMode()));
        if (m_input)
            m_input->clear();
    });
    connect(clearButton.get(), &QPushButton::clicked,
            this, &SerialMonitorPanel::clearMessages);

    titleLabel.release();
    statusLabel.release();
    portCombo.release();
    baudCombo.release();
    refreshButton.release();
    connectButton.release();
    disconnectButton.release();
    newlineCombo.release();
    timestampCheck.release();
    connectionRow.release();
    output.release();
    input.release();
    openExternalButton.release();
    sendButton.release();
    clearButton.release();
    buttonRow.release();
    layout.release();

    setAvailablePorts({});
    setSelectedNewlineMode(NewlineMode::Lf);
    setTimestampsEnabled(false);
    setConnected(false);
    setStatusText(tr("No serial backend is attached yet. Use an external monitor command or target-specific workflow."));
    appendMessage(tr("Serial monitor panel is ready. External monitor output can be routed through the terminal."));
}

void SerialMonitorPanel::setAvailablePorts(const QStringList &ports)
{
    if (!m_portCombo)
        return;

    const QString current = m_portCombo->currentText();
    m_portCombo->clear();
    m_portCombo->addItems(ports);
    const int index = m_portCombo->findText(current);
    if (index >= 0)
        m_portCombo->setCurrentIndex(index);
}

void SerialMonitorPanel::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

void SerialMonitorPanel::appendMessage(const QString &text)
{
    m_output->appendPlainText(text);
}

void SerialMonitorPanel::clearMessages()
{
    m_output->clear();
}

void SerialMonitorPanel::setConnected(bool connected)
{
    if (m_portCombo)
        m_portCombo->setEnabled(!connected);
    if (m_baudCombo)
        m_baudCombo->setEnabled(!connected);
    if (m_connectButton)
        m_connectButton->setEnabled(!connected);
    if (m_disconnectButton)
        m_disconnectButton->setEnabled(connected);
    if (m_input)
        m_input->setEnabled(connected);
}

QString SerialMonitorPanel::selectedPort() const
{
    return m_portCombo ? m_portCombo->currentText() : QString();
}

int SerialMonitorPanel::selectedBaudRate() const
{
    return m_baudCombo ? m_baudCombo->currentText().toInt() : 115200;
}

void SerialMonitorPanel::setSelectedPort(const QString &portName)
{
    if (!m_portCombo)
        return;

    const int index = m_portCombo->findText(portName);
    if (index >= 0)
        m_portCombo->setCurrentIndex(index);
}

void SerialMonitorPanel::setSelectedBaudRate(int baudRate)
{
    if (!m_baudCombo)
        return;

    const QString text = QString::number(baudRate);
    const int index = m_baudCombo->findText(text);
    if (index >= 0)
        m_baudCombo->setCurrentIndex(index);
    else
        m_baudCombo->setCurrentText(text);
}

SerialMonitorPanel::NewlineMode SerialMonitorPanel::selectedNewlineMode() const
{
    if (!m_newlineCombo)
        return NewlineMode::Lf;

    return static_cast<NewlineMode>(m_newlineCombo->currentData().toInt());
}

void SerialMonitorPanel::setSelectedNewlineMode(NewlineMode mode)
{
    if (!m_newlineCombo)
        return;

    const int index = m_newlineCombo->findData(static_cast<int>(mode));
    if (index >= 0)
        m_newlineCombo->setCurrentIndex(index);
}

bool SerialMonitorPanel::timestampsEnabled() const
{
    return m_timestampCheck && m_timestampCheck->isChecked();
}

void SerialMonitorPanel::setTimestampsEnabled(bool enabled)
{
    if (m_timestampCheck)
        m_timestampCheck->setChecked(enabled);
}