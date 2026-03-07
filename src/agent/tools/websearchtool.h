#pragma once

#include "agent/itool.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrl>

/// Tool that fetches a web page and returns its text content
class WebSearchTool : public ITool
{
public:
    ToolSpec spec() const override {
        QJsonObject props;
        props[QStringLiteral("query")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"), QStringLiteral("The search query or URL to fetch")}
        };
        QJsonArray req;
        req.append(QStringLiteral("query"));
        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        schema[QStringLiteral("properties")] = props;
        schema[QStringLiteral("required")] = req;

        return {QStringLiteral("web_search"),
                QStringLiteral("Search the web for information. Returns text content from URLs."),
                schema, {}, AgentToolPermission::ReadOnly, 20000, true};
    }

    ToolExecResult invoke(const QJsonObject &args) override {
        const QString query = args.value(QStringLiteral("query")).toString();
        if (query.isEmpty())
            return {false, {}, {}, QStringLiteral("Missing 'query' parameter")};

        // If it looks like a URL, fetch it directly
        if (query.startsWith(QStringLiteral("http://")) ||
            query.startsWith(QStringLiteral("https://"))) {
            return fetchUrl(QUrl(query));
        }

        // Otherwise return error (no search API configured)
        return {false, {}, {},
                QStringLiteral("Web search requires a search API key. "
                               "Use a URL directly instead (e.g., https://example.com).")};
    }

private:
    ToolExecResult fetchUrl(const QUrl &url) {
        QNetworkAccessManager mgr;
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Exorcist-IDE/1.0"));
        req.setTransferTimeout(15000);

        auto *reply = mgr.get(req);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            return {false, {}, {}, QStringLiteral("HTTP error: %1").arg(err)};
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();

        // Strip HTML tags for a rough text extraction
        QString text = QString::fromUtf8(data);
        text.remove(QRegularExpression(QStringLiteral("<script[^>]*>[\\s\\S]*?</script>")));
        text.remove(QRegularExpression(QStringLiteral("<style[^>]*>[\\s\\S]*?</style>")));
        text.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
        text = text.simplified();

        if (text.size() > 20000)
            text = text.left(20000) + QStringLiteral("\n...(truncated)");

        QJsonObject result;
        result[QStringLiteral("url")] = url.toString();
        result[QStringLiteral("length")] = text.size();
        return {true, result, text, {}};
    }
};
