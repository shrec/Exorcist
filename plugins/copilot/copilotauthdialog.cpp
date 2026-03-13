#include "copilotauthdialog.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

// GitHub's OAuth App for Copilot — this is the same client ID used by
// VS Code's Copilot extension (public, non-secret).
static const char kClientId[] = "Iv1.b507a08c87ecfe98";
static const char kDeviceCodeUrl[] = "https://github.com/login/device/code";
static const char kTokenUrl[] = "https://github.com/login/oauth/access_token";
static const char kScope[] = "read:user";

CopilotAuthDialog::CopilotAuthDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Sign in to GitHub Copilot"));
    setFixedSize(420, 280);
    setModal(true);

    // ── UI ─────────────────────────────────────────────────────────────
    auto *titleLabel = new QLabel(tr("GitHub Copilot"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #dcdcdc;"));
    titleLabel->setAlignment(Qt::AlignCenter);

    auto *instrLabel = new QLabel(
        tr("To sign in, copy the code below and open GitHub in your browser:"), this);
    instrLabel->setWordWrap(true);
    instrLabel->setStyleSheet(QStringLiteral("color: #aaa;"));
    instrLabel->setAlignment(Qt::AlignCenter);

    m_codeLabel = new QLabel(tr("..."), this);
    m_codeLabel->setStyleSheet(QStringLiteral(
        "font-size: 28px; font-weight: bold; font-family: 'Cascadia Mono', 'Consolas', monospace;"
        "color: #4ec9b0; letter-spacing: 4px; padding: 8px;"));
    m_codeLabel->setAlignment(Qt::AlignCenter);
    m_codeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto *btnRow = new QHBoxLayout;
    m_copyBtn = new QPushButton(tr("Copy Code"), this);
    m_openBtn = new QPushButton(tr("Open GitHub"), this);
    m_openBtn->setDefault(true);
    btnRow->addStretch();
    btnRow->addWidget(m_copyBtn);
    btnRow->addWidget(m_openBtn);
    btnRow->addStretch();

    m_statusLabel = new QLabel(this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);
    root->addWidget(titleLabel);
    root->addWidget(instrLabel);
    root->addWidget(m_codeLabel);
    root->addLayout(btnRow);
    root->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────
    connect(m_copyBtn, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_userCode);
        setStatus(tr("Code copied!"), QStringLiteral("#4ec9b0"));
    });
    connect(m_openBtn, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(m_userCode);
        QDesktopServices::openUrl(QUrl(m_verificationUrl));
    });
    connect(&m_pollTimer, &QTimer::timeout, this, &CopilotAuthDialog::pollForToken);

    m_copyBtn->setEnabled(false);
    m_openBtn->setEnabled(false);

    // Start immediately
    startDeviceFlow();
}

// ── Device flow step 1: get device code ──────────────────────────────────────

void CopilotAuthDialog::startDeviceFlow()
{
    setStatus(tr("Contacting GitHub..."));

    QNetworkRequest req{QUrl{QLatin1String(kDeviceCodeUrl)}};
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("Accept", "application/json");

    const QByteArray body = QStringLiteral("client_id=%1&scope=%2")
                                .arg(QLatin1String(kClientId), QLatin1String(kScope))
                                .toUtf8();

    QNetworkReply *reply = m_nam.post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            setStatus(tr("Failed: %1").arg(reply->errorString()), QStringLiteral("#f44747"));
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_deviceCode      = obj[QLatin1String("device_code")].toString();
        m_userCode        = obj[QLatin1String("user_code")].toString();
        m_verificationUrl = obj[QLatin1String("verification_uri")].toString();
        m_interval        = obj[QLatin1String("interval")].toInt(5);

        if (m_deviceCode.isEmpty() || m_userCode.isEmpty()) {
            setStatus(tr("GitHub returned an unexpected response."), QStringLiteral("#f44747"));
            return;
        }

        m_codeLabel->setText(m_userCode);
        m_copyBtn->setEnabled(true);
        m_openBtn->setEnabled(true);
        setStatus(tr("Waiting for authorization..."));

        m_pollCount = 0;
        m_pollTimer.start(m_interval * 1000);
    });
}

// ── Device flow step 2: poll for token ───────────────────────────────────────

void CopilotAuthDialog::pollForToken()
{
    if (++m_pollCount > 60) { // 5 minute timeout
        m_pollTimer.stop();
        setStatus(tr("Timed out waiting for authorization."), QStringLiteral("#f44747"));
        return;
    }

    QNetworkRequest req{QUrl{QLatin1String(kTokenUrl)}};
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    req.setRawHeader("Accept", "application/json");

    const QByteArray body = QStringLiteral(
        "client_id=%1&device_code=%2&grant_type=urn:ietf:params:oauth:grant-type:device_code")
        .arg(QLatin1String(kClientId), m_deviceCode).toUtf8();

    QNetworkReply *reply = m_nam.post(req, body);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();

        if (obj.contains(QLatin1String("access_token"))) {
            m_pollTimer.stop();
            m_githubToken  = obj[QLatin1String("access_token")].toString();
            m_refreshToken = obj[QLatin1String("refresh_token")].toString();
            setStatus(tr("Signed in successfully!"), QStringLiteral("#4ec9b0"));
            accept();
            return;
        }

        const QString error = obj[QLatin1String("error")].toString();
        if (error == QLatin1String("authorization_pending")) {
            // Still waiting — keep polling
            return;
        }
        if (error == QLatin1String("slow_down")) {
            m_interval += 5;
            m_pollTimer.setInterval(m_interval * 1000);
            return;
        }

        // access_denied, expired_token, etc.
        m_pollTimer.stop();
        const QString desc = obj[QLatin1String("error_description")].toString();
        setStatus(tr("Authorization failed: %1").arg(desc.isEmpty() ? error : desc),
                  QStringLiteral("#f44747"));
    });
}

// ── Helper ───────────────────────────────────────────────────────────────────

void CopilotAuthDialog::setStatus(const QString &text, const QString &color)
{
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 12px;").arg(color));
}
