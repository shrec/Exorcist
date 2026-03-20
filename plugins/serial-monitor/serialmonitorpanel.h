#pragma once

#include <QWidget>

#include <QStringList>

class QComboBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class SerialMonitorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SerialMonitorPanel(QWidget *parent = nullptr);

    enum class NewlineMode {
        None = 0,
        Lf,
        CrLf,
    };

    void setAvailablePorts(const QStringList &ports);
    void setStatusText(const QString &text);
    void appendMessage(const QString &text);
    void clearMessages();
    void setConnected(bool connected);
    QString selectedPort() const;
    int selectedBaudRate() const;
    void setSelectedPort(const QString &portName);
    void setSelectedBaudRate(int baudRate);
    NewlineMode selectedNewlineMode() const;
    void setSelectedNewlineMode(NewlineMode mode);
    bool timestampsEnabled() const;
    void setTimestampsEnabled(bool enabled);

signals:
    void refreshRequested();
    void connectRequested(const QString &portName, int baudRate);
    void disconnectRequested();
    void sendTextRequested(const QString &text, int newlineMode);
    void openExternalRequested();
    void settingsChanged();

private:
    QComboBox *m_portCombo = nullptr;
    QComboBox *m_baudCombo = nullptr;
    QComboBox *m_newlineCombo = nullptr;
    QCheckBox *m_timestampCheck = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLineEdit *m_input = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
};
