#pragma once

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#include <memory>

// ─────────────────────────────────────────────────────────────────────────────
// OAuthManager — handles OAuth login flow for GitHub Copilot.
//
// Flow:
//   1. Start local HTTP server on random port
//   2. Open browser to GitHub OAuth authorize URL with redirect_uri
//   3. Receive callback with authorization code
//   4. Exchange code for access token
//   5. Store token in SecureKeyStorage
//
// This is a stub that implements the OAuth protocol flow structure.
// The actual GitHub client_id and OAuth endpoints need configuration.
// ─────────────────────────────────────────────────────────────────────────────

class OAuthManager : public QObject
{
    Q_OBJECT

public:
    explicit OAuthManager(QObject *parent = nullptr)
        : QObject(parent)
        , m_callbackServer(std::make_unique<QTcpServer>(this))
        , m_network(std::make_unique<QNetworkAccessManager>(this))
    {
    }

    /// Set OAuth client ID (from GitHub App settings)
    void setClientId(const QString &clientId) { m_clientId = clientId; }

    /// Set OAuth client secret (optional for PKCE public clients)
    void setClientSecret(const QString &clientSecret) { m_clientSecret = clientSecret; }

    /// Set OAuth scopes (space-separated)
    void setScopes(const QString &scopes) { m_scopes = scopes; }

    /// Start the OAuth login flow — opens browser
    bool startLogin()
    {
        if (m_clientId.isEmpty()) {
            emit loginFailed(tr("Missing OAuth client ID"));
            return false;
        }

        // Start local callback server
        if (!m_callbackServer->listen(QHostAddress::LocalHost, 0)) {
            emit loginFailed(tr("Failed to start local callback server"));
            return false;
        }

        m_port = m_callbackServer->serverPort();
        m_state = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_codeVerifier = createCodeVerifier();
        m_codeChallenge = createCodeChallenge(m_codeVerifier);

        connect(m_callbackServer.get(), &QTcpServer::newConnection,
                this, &OAuthManager::handleCallback);

        // Build authorization URL
        const QString authUrl = QStringLiteral(
            "https://github.com/login/oauth/authorize"
            "?client_id=%1"
            "&redirect_uri=http://localhost:%2/callback"
            "&scope=%3"
            "&state=%4"
            "&code_challenge=%5"
            "&code_challenge_method=S256")
            .arg(m_clientId)
            .arg(m_port)
            .arg(m_scopes)
            .arg(m_state)
            .arg(m_codeChallenge);

        // Timeout after 2 minutes
        QTimer::singleShot(120000, this, [this] {
            if (m_callbackServer->isListening()) {
                m_callbackServer->close();
                emit loginFailed(tr("Login timed out"));
            }
        });

        // Open browser
        QDesktopServices::openUrl(QUrl(authUrl));
        emit loginStarted();
        return true;
    }

    /// Cancel an in-progress login
    void cancelLogin()
    {
        m_callbackServer->close();
        emit loginCancelled();
    }

    /// Check if we have a stored token
    bool hasToken() const { return !m_accessToken.isEmpty(); }

    /// Get the current access token
    QString accessToken() const { return m_accessToken; }

    /// Set token directly (e.g. loaded from SecureKeyStorage)
    void setAccessToken(const QString &token)
    {
        m_accessToken = token;
        if (token.isEmpty())
            emit loggedOut();
        else
            emit loginSucceeded(token);
    }

    /// Clear the token (sign out)
    void signOut()
    {
        m_accessToken.clear();
        emit loggedOut();
    }

signals:
    void loginStarted();
    void loginSucceeded(const QString &accessToken);
    void loginFailed(const QString &error);
    void loginCancelled();
    void loggedOut();
    void tokenRefreshed(const QString &newToken);

private:
    QString createCodeVerifier() const
    {
        const QByteArray bytes = QUuid::createUuid().toRfc4122()
                                     + QUuid::createUuid().toRfc4122();
        return QString::fromLatin1(bytes.toHex());
    }

    QString createCodeChallenge(const QString &verifier) const
    {
        const QByteArray hash =
            QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
        const QByteArray b64 = hash.toBase64(QByteArray::Base64UrlEncoding
                                             | QByteArray::OmitTrailingEquals);
        return QString::fromLatin1(b64);
    }

    void exchangeCodeForToken(const QString &code)
    {
        QUrl url(QStringLiteral("https://github.com/login/oauth/access_token"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
        req.setRawHeader("Accept", "application/json");
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QUrlQuery body;
        body.addQueryItem(QStringLiteral("client_id"), m_clientId);
        if (!m_clientSecret.isEmpty())
            body.addQueryItem(QStringLiteral("client_secret"), m_clientSecret);
        body.addQueryItem(QStringLiteral("code"), code);
        body.addQueryItem(QStringLiteral("redirect_uri"),
                          QStringLiteral("http://localhost:%1/callback").arg(m_port));
        body.addQueryItem(QStringLiteral("state"), m_state);
        body.addQueryItem(QStringLiteral("code_verifier"), m_codeVerifier);

        QNetworkReply *reply = m_network->post(req, body.toString(QUrl::FullyEncoded).toUtf8()); // Qt parent-owns reply
        connect(reply, &QNetworkReply::finished, this, [this, reply] {
            const QByteArray raw = reply->readAll();
            const QJsonDocument doc = QJsonDocument::fromJson(raw);
            const QJsonObject obj = doc.object();
            const QString token = obj.value(QStringLiteral("access_token")).toString();
            const QString error = obj.value(QStringLiteral("error_description")).toString();
            reply->deleteLater();

            if (!error.isEmpty()) {
                emit loginFailed(error);
                return;
            }

            if (token.isEmpty()) {
                emit loginFailed(tr("OAuth token exchange failed"));
                return;
            }

            m_accessToken = token;
            emit loginSucceeded(token);
        });
    }

    void handleCallback()
    {
        auto *socket = m_callbackServer->nextPendingConnection();
        if (!socket) return;

        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            const QByteArray request = socket->readAll();

            // Send a response to the browser
            const QByteArray html =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Connection: close\r\n\r\n"
                "<html><body><h2>Authentication successful!</h2>"
                "<p>You can close this tab and return to Exorcist IDE.</p>"
                "</body></html>";
            socket->write(html);
            socket->waitForBytesWritten(3000);
            socket->close();
            socket->deleteLater();

            // Parse the authorization code from the request
            const QString line = QString::fromUtf8(request.split('\n').first());
            // GET /callback?code=xxx&state=yyy HTTP/1.1
            const int qIdx = line.indexOf('?');
            const int spIdx = line.indexOf(' ', qIdx);
            if (qIdx < 0 || spIdx < 0) {
                emit loginFailed(tr("Invalid callback request"));
                m_callbackServer->close();
                return;
            }

            const QString query = line.mid(qIdx + 1, spIdx - qIdx - 1);
            QString code, state;
            const auto params = query.split('&');
            for (const QString &param : params) {
                const int eq = param.indexOf('=');
                if (eq < 0) continue;
                const QString key = param.left(eq);
                const QString val = param.mid(eq + 1);
                if (key == QStringLiteral("code"))
                    code = val;
                else if (key == QStringLiteral("state"))
                    state = val;
            }

            m_callbackServer->close();

            // Verify state to prevent CSRF
            if (state != m_state) {
                emit loginFailed(tr("OAuth state mismatch — possible CSRF attack"));
                return;
            }

            if (code.isEmpty()) {
                emit loginFailed(tr("No authorization code received"));
                return;
            }

            m_authCode = code;
            exchangeCodeForToken(code);
        });
    }

    std::unique_ptr<QTcpServer> m_callbackServer;
    std::unique_ptr<QNetworkAccessManager> m_network;
    QString m_clientId;
    QString m_clientSecret;
    QString m_scopes = QStringLiteral("read:user");
    QString m_state;
    QString m_accessToken;
    QString m_authCode;
    QString m_codeVerifier;
    QString m_codeChallenge;
    quint16 m_port = 0;
};
