#include "websearchtool.h"

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

ToolSpec WebSearchTool::spec() const
{
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

ToolExecResult WebSearchTool::invoke(const QJsonObject &args)
{
    const QString query = args.value(QStringLiteral("query")).toString();
    if (query.isEmpty())
        return {false, {}, {}, QStringLiteral("Missing 'query' parameter")};

    // Direct URL fetch
    if (query.startsWith(QStringLiteral("http://")) ||
        query.startsWith(QStringLiteral("https://"))) {
        const QUrl url(query, QUrl::StrictMode);
        if (!url.isValid())
            return {false, {}, {}, QStringLiteral("Invalid URL: %1").arg(query)};
        return fetchUrl(url);
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

ToolExecResult WebSearchTool::searxngSearch(const QString &baseUrl,
                                            const QString &query,
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
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

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

ToolExecResult WebSearchTool::fetchUrl(const QUrl &url)
{
    QNetworkAccessManager mgr;
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Exorcist-IDE/1.0"));
    req.setTransferTimeout(15000);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

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

QString WebSearchTool::searxngBaseUrl()
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
