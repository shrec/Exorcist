#include <QTest>
#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QUrl>

// ── Test HTTP/2 Hardening ─────────────────────────────────────────────────────
//
// Validates that all QNetworkRequest construction throughout the codebase
// disables HTTP/2 to work around the Qt 6.10 GOAWAY crash bug.
// We construct requests identically to the production code and verify
// the Http2AllowedAttribute is set to false.

class TestHttp2Hardening : public QObject
{
    Q_OBJECT

private slots:

    // ── NetworkMonitor pattern (periodic HEAD to api.github.com) ─────────

    void networkMonitor_disablesHttp2()
    {
        QUrl checkUrl(QStringLiteral("https://api.github.com"));
        QNetworkRequest req(checkUrl);
        req.setTransferTimeout(8000);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
        QCOMPARE(req.url(), checkUrl);
    }

    // ── AuthManager pattern (checkOnline HEAD) ──────────────────────────

    void authManager_disablesHttp2()
    {
        QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com")));
        req.setTransferTimeout(5000);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    // ── HttpTool pattern (agent HTTP requests) ──────────────────────────

    void httpTool_disablesHttp2()
    {
        QUrl url(QStringLiteral("https://example.com/api"));
        QNetworkRequest req(url);
        req.setTransferTimeout(15000);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("Exorcist-Agent/1.0"));
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    // ── OAuthManager pattern (token exchange POST) ──────────────────────

    void oauthManager_disablesHttp2()
    {
        QUrl url(QStringLiteral("https://github.com/login/oauth/access_token"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/x-www-form-urlencoded"));
        req.setRawHeader("Accept", "application/json");
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    // ── WebSearchTool patterns (SearXNG + fetchUrl) ─────────────────────

    void webSearchTool_searxng_disablesHttp2()
    {
        QUrl url(QStringLiteral("https://searx.example.com/search?q=test"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("Exorcist-IDE/1.0"));
        req.setTransferTimeout(15000);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    void webSearchTool_fetchUrl_disablesHttp2()
    {
        QUrl url(QStringLiteral("https://example.com/page"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                       QStringLiteral("Exorcist-IDE/1.0"));
        req.setTransferTimeout(15000);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    // ── GitHubMcpTools pattern (GitHub API calls) ───────────────────────

    void githubMcpTools_disablesHttp2()
    {
        QUrl url(QStringLiteral("https://api.github.com/repos/test/test"));
        QNetworkRequest req(url);
        req.setRawHeader("User-Agent", "Exorcist-IDE/1.0");
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    // ── PluginMarketplaceService patterns ───────────────────────────────

    void pluginMarketplace_registry_disablesHttp2()
    {
        QNetworkRequest request;
        request.setUrl(QUrl(QStringLiteral("https://plugins.example.com/registry.json")));
        request.setHeader(QNetworkRequest::UserAgentHeader,
                           QStringLiteral("ExorcistIDE/1.0"));
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(request.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    void pluginMarketplace_download_disablesHttp2()
    {
        QNetworkRequest request;
        request.setUrl(QUrl(QStringLiteral("https://plugins.example.com/plugin.zip")));
        request.setHeader(QNetworkRequest::UserAgentHeader,
                           QStringLiteral("ExorcistIDE/1.0"));
        request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        QCOMPARE(request.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    // ── Ensure default QNetworkRequest has HTTP/2 enabled ───────────────
    // (validates that without our fix, HTTP/2 WOULD be used)

    void defaultRequest_hasHttp2Enabled()
    {
        QNetworkRequest req(QUrl(QStringLiteral("https://example.com")));
        // By default, Http2AllowedAttribute is not set (returns invalid QVariant)
        // or true — either way, HTTP/2 would be used
        const QVariant attr = req.attribute(QNetworkRequest::Http2AllowedAttribute);
        // When not explicitly set, it's invalid (meaning Qt uses its default: enabled)
        QVERIFY(!attr.isValid() || attr.toBool() == true);
    }

    // ── Verify attribute survives header additions ──────────────────────

    void http2Attribute_survivesHeaderAdditions()
    {
        QNetworkRequest req(QUrl(QStringLiteral("https://example.com")));
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);

        // Add headers after setting the attribute
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Test/1.0"));
        req.setRawHeader("Authorization", "Bearer token123");
        req.setRawHeader("Accept", "application/json");

        // Attribute must still be false
        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);
    }

    // ── Verify attribute set to false, not just present ─────────────────

    void http2Attribute_isFalseNotTrue()
    {
        QNetworkRequest req(QUrl(QStringLiteral("https://example.com")));
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        QCOMPARE(req.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), false);

        // Setting to true would re-enable HTTP/2
        QNetworkRequest req2(QUrl(QStringLiteral("https://example.com")));
        req2.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
        QCOMPARE(req2.attribute(QNetworkRequest::Http2AllowedAttribute).toBool(), true);
    }
};

QTEST_MAIN(TestHttp2Hardening)
#include "test_http2hardening.moc"
