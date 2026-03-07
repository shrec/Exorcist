#pragma once

#include <QDialog>
#include <QNetworkAccessManager>
#include <QString>
#include <QTimer>

class QLabel;
class QPushButton;

class CopilotAuthDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CopilotAuthDialog(QWidget *parent = nullptr);

    // Returns the GitHub token on success, empty on cancel/failure.
    QString githubToken()   const { return m_githubToken; }
    QString refreshToken()  const { return m_refreshToken; }

private slots:
    void startDeviceFlow();
    void pollForToken();

private:
    void setStatus(const QString &text, const QString &color = QStringLiteral("#dcdcdc"));

    QNetworkAccessManager m_nam;
    QTimer  m_pollTimer;

    QLabel      *m_codeLabel;
    QLabel      *m_statusLabel;
    QPushButton *m_copyBtn;
    QPushButton *m_openBtn;

    // Device flow state
    QString m_deviceCode;
    QString m_userCode;
    QString m_verificationUrl;
    int     m_interval   = 5;
    int     m_pollCount  = 0;
    QString m_githubToken;
    QString m_refreshToken;
};
