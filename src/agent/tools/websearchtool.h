#pragma once

#include "agent/itool.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

// ── WebSearchTool ───────────────────────────────────────────────────────────
//
// Web search + URL fetch tool for the AI agent.
//
// Search backends (tried in order):
//   1. SearXNG  — configured via AI/searxngUrl setting (e.g. http://localhost:8080)
//   2. Direct URL fetch — if the query starts with http:// or https://
//
// SearXNG JSON API: GET {base}/search?q={query}&format=json&categories=general

class WebSearchTool : public ITool
{
public:
    ToolSpec spec() const override {
        QJsonObject props;
        props[QStringLiteral("query")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("string")},
            {QStringLiteral("description"),
             QStringLiteral("Search query text, or a URL to fetch directly.")}
        };
        props[QStringLiteral("max_results")] = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("integer")},
            {QStringLiteral("description"),
             QStringLiteral("Max number of search results to return (default: 5, max: 10).")}
        };
        QJsonArray req;
        req.append(QStringLiteral("query"));
        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        schema[QStringLiteral("properties")] = props;
        schema[QStringLiteral("required")] = req;

        return {QStringLiteral("web_search"),
                QStringLiteral(
                    "Search the web using SearXNG or fetch a URL directly. "
                    "For search queries, returns titles, URLs, and snippets. "
                    "For URLs, returns the page text content."),
                schema, {}, AgentToolPermission::ReadOnly, 20000, true};
    }

    ToolExecResult invoke(const QJsonObject &args) override {
        const QString query = args.value(QStringLiteral("query")).toString();
        if (query.isEmpty())
            return {false, {}, {}, QStringLiteral("Missing 'query' parameter")};

        // Direct URL fetch
        if (query.startsWith(QStringLiteral("http://")) ||
            query.startsWith(QStringLiteral("https://"))) {
            return fetchUrl(QUrl(query));
        }

        // Try SearXNG search
        const QString searxUrl = searxngBaseUrl();
        if (!searxUrl.isEmpty()) {
            const int maxResults = qBound(1,
                args.value(QStringLiteral("max_results")).toInt(5), 10);
            return searxngSearch(searxUrl, query, maxResults);
        }

        return {false, {}, {},
                QStringLiteral("No search backend configured. "
                               "Set a SearXNG URL in AI Settings, or pass a URL directly.\n"
                               "Example SearXNG: http://localhost:8080 (local) "
                               "or any public instance.")};
    }

private:
    // ── SearXNG search ──────────────────────────────────────────────────

    ToolExecResult searxngSearch(const QString &baseUrl, const QString &query,
                                 int maxResults)
    {
        QUrl url(baseUrl + QStringLiteral("/search"));
        QUrlQuery params;
        params.addQueryItem(QStringLiteral("q"), query);
        params.addQueryItem(QStringLiteral("format"), QStringLiteral("json"));
        params.addQueryItem(QStringLiteral("categories"), QStringLiteral("general"));
        url.setQuery(params);

        QNetworkAccessManager mgr;
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Exorcist-IDE/1.0"));
        req.setTransferTimeout(15000);

        auto *reply = mgr.get(req);
        QEventLoop loop;
        QTimer::singleShot(16000, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (!reply->isFinished()) {
            reply->abort();
            reply->deleteLater();
            return {false, {}, {},
                    QStringLiteral("SearXNG request timed out (%1)").arg(baseUrl)};
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            return {false, {}, {},
                    QStringLiteral("SearXNG error: %1 (%2)").arg(err, baseUrl)};
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject())
            return {false, {}, {},
                    QStringLiteral("SearXNG returned invalid JSON.")};

        const QJsonArray results = doc.object()
            .value(QStringLiteral("results")).toArray();

        if (results.isEmpty())
            return {true, QJsonObject{{QStringLiteral("count"), 0}},
                    QStringLiteral("No results found for: %1").arg(query), {}};

        // Format results
        QString text;
        QJsonArray resultArray;
        const int count = qMin(maxResults, results.size());

        for (int i = 0; i < count; ++i) {
            const QJsonObject r = results[i].toObject();
            const QString title   = r.value(QStringLiteral("title")).toString();
            const QString rUrl    = r.value(QStringLiteral("url")).toString();
            const QString content = r.value(QStringLiteral("content")).toString();
            const QString engine  = r.value(QStringLiteral("engine")).toString();

            text += QStringLiteral("[%1] %2\n").arg(i + 1).arg(title);
            text += QStringLiteral("    %1\n").arg(rUrl);
            if (!content.isEmpty())
                text += QStringLiteral("    %1\n").arg(content);
            text += QLatin1Char('\n');

            QJsonObject item;
            item[QStringLiteral("title")]   = title;
            item[QStringLiteral("url")]     = rUrl;
            item[QStringLiteral("snippet")] = content;
            if (!engine.isEmpty())
                item[QStringLiteral("engine")] = engine;
            resultArray.append(item);
        }

        QJsonObject meta;
        meta[QStringLiteral("query")]   = query;
        meta[QStringLiteral("count")]   = count;
        meta[QStringLiteral("results")] = resultArray;

        return {true, meta, text, {}};
    }

    // ── Direct URL fetch ────────────────────────────────────────────────

    ToolExecResult fetchUrl(const QUrl &url) {
        QNetworkAccessManager mgr;
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Exorcist-IDE/1.0"));
        req.setTransferTimeout(15000);

        auto *reply = mgr.get(req);
        QEventLoop loop;
        QTimer::singleShot(16000, &loop, &QEventLoop::quit);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (!reply->isFinished()) {
            reply->abort();
            reply->deleteLater();
            return {false, {}, {}, QStringLiteral("Request timed out.")};
        }

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            return {false, {}, {}, QStringLiteral("HTTP error: %1").arg(err)};
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();

        // Strip HTML tags for rough text extraction
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

    // ── Settings ────────────────────────────────────────────────────────

    static QString searxngBaseUrl()
    {
        QSettings s;
        s.beginGroup(QStringLiteral("AI"));
        QString url = s.value(QStringLiteral("searxngUrl")).toString().trimmed();
        s.endGroup();
        // Strip trailing slash
        while (url.endsWith(QLatin1Char('/')))
            url.chop(1);
        return url;
    }
};
