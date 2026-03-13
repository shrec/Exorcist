#pragma once

#include "../itool.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

// ── HttpRequestTool ──────────────────────────────────────────────────────────
// Makes HTTP requests (GET/POST/PUT/DELETE). Like curl for the AI agent.
// Returns raw response body, status code, headers, and content type.

class HttpRequestTool : public ITool
{
public:
    HttpRequestTool() = default;

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("http_request");
        s.description = QStringLiteral(
            "Make an HTTP request to a URL. Supports GET, POST, PUT, DELETE. "
            "Use this to fetch API documentation, query REST APIs, download "
            "configuration files, or interact with web services. "
            "Returns status code, headers, and response body (max 1MB).");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 60000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("url"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("The URL to request (must start with http:// or https://).")}
                }},
                {QStringLiteral("method"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("HTTP method. Default: GET.")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("GET"), QStringLiteral("POST"),
                        QStringLiteral("PUT"), QStringLiteral("DELETE"),
                        QStringLiteral("HEAD"), QStringLiteral("PATCH")
                    }}
                }},
                {QStringLiteral("headers"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("object")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional HTTP headers as key-value pairs "
                                    "(e.g. {\"Authorization\": \"Bearer ...\"})")}
                }},
                {QStringLiteral("body"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"),
                     QStringLiteral("Optional request body (for POST/PUT/PATCH).")}
                }},
                {QStringLiteral("timeout"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"),
                     QStringLiteral("Request timeout in seconds (default: 15, max: 60).")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("url")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString urlStr = args[QLatin1String("url")].toString();
        if (urlStr.isEmpty())
            return {false, {}, {}, QStringLiteral("'url' parameter is required.")};

        const QUrl url(urlStr);
        if (!url.isValid() || url.scheme().isEmpty())
            return {false, {}, {}, QStringLiteral("Invalid URL: %1").arg(urlStr)};

        // Security: only allow http(s)
        if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
            return {false, {}, {}, QStringLiteral("Only http:// and https:// URLs are allowed.")};

        const QString method = args[QLatin1String("method")].toString(QStringLiteral("GET")).toUpper();
        const QString body   = args[QLatin1String("body")].toString();
        const int timeoutSec = qBound(1, args[QLatin1String("timeout")].toInt(15), 60);

        // Build request
        QNetworkRequest req(url);
        req.setTransferTimeout(timeoutSec * 1000);

        // Default headers
        req.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("Exorcist-Agent/1.0"));

        // Custom headers
        const QJsonObject headers = args[QLatin1String("headers")].toObject();
        for (auto it = headers.begin(); it != headers.end(); ++it)
            req.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());

        // Disable HTTP/2 — Qt 6.10 GOAWAY crash bug
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        // Execute request synchronously with event loop
        QNetworkAccessManager nam;
        QNetworkReply *reply = nullptr;

        const QByteArray bodyBytes = body.toUtf8();
        if (method == QLatin1String("GET"))
            reply = nam.get(req);
        else if (method == QLatin1String("POST"))
            reply = nam.post(req, bodyBytes);
        else if (method == QLatin1String("PUT"))
            reply = nam.put(req, bodyBytes);
        else if (method == QLatin1String("DELETE"))
            reply = nam.deleteResource(req);
        else if (method == QLatin1String("HEAD"))
            reply = nam.head(req);
        else if (method == QLatin1String("PATCH"))
            reply = nam.sendCustomRequest(req, "PATCH", bodyBytes);
        else
            return {false, {}, {}, QStringLiteral("Unsupported method: %1").arg(method)};

        // Wait for reply
        QEventLoop loop;
        QTimer::singleShot(timeoutSec * 1000 + 500, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (!reply->isFinished()) {
            reply->abort();
            reply->deleteLater();
            return {false, {}, {}, QStringLiteral("Request timed out after %1s.").arg(timeoutSec)};
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

        // Read body with 1MB limit
        constexpr qint64 kMaxBody = 1024 * 1024;
        QByteArray responseBody = reply->read(kMaxBody);
        const bool truncated = reply->bytesAvailable() > 0;

        // Collect response headers
        QJsonObject respHeaders;
        for (const auto &pair : reply->rawHeaderPairs())
            respHeaders[QString::fromUtf8(pair.first)] = QString::fromUtf8(pair.second);

        reply->deleteLater();

        // Check for network errors
        if (reply->error() != QNetworkReply::NoError && status == 0)
            return {false, {}, {},
                    QStringLiteral("Network error: %1").arg(reply->errorString())};

        // Build result
        const QString bodyText = QString::fromUtf8(responseBody);
        const int bodyLen = responseBody.size();

        QString text = QStringLiteral("HTTP %1 %2\nContent-Type: %3\nBody: %4 bytes")
                           .arg(status)
                           .arg(method)
                           .arg(contentType)
                           .arg(bodyLen);
        if (truncated)
            text += QStringLiteral(" (truncated to 1MB)");
        text += QStringLiteral("\n\n");

        // For text content, include body directly; for binary, note the type
        if (contentType.contains(QLatin1String("text")) ||
            contentType.contains(QLatin1String("json")) ||
            contentType.contains(QLatin1String("xml")) ||
            contentType.contains(QLatin1String("javascript"))) {
            // Limit text output to 50KB for model context
            constexpr int kMaxText = 50 * 1024;
            if (bodyText.size() > kMaxText)
                text += bodyText.left(kMaxText) + QStringLiteral("\n... (truncated)");
            else
                text += bodyText;
        } else {
            text += QStringLiteral("[Binary content: %1, %2 bytes]").arg(contentType).arg(bodyLen);
        }

        QJsonObject data;
        data[QLatin1String("status")]      = status;
        data[QLatin1String("contentType")] = contentType;
        data[QLatin1String("bodySize")]    = bodyLen;
        data[QLatin1String("headers")]     = respHeaders;
        data[QLatin1String("truncated")]   = truncated;

        return {true, data, text, {}};
    }
};
