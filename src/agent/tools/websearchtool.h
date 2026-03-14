#pragma once

#include "agent/itool.h"

class QUrl;

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
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    ToolExecResult searxngSearch(const QString &baseUrl, const QString &query,
                                 int maxResults);
    ToolExecResult fetchUrl(const QUrl &url);
    static QString searxngBaseUrl();
};
