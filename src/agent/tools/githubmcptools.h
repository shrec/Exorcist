#pragma once

#include "../itool.h"

// ─────────────────────────────────────────────────────────────────────────────
// GitHubMCPTools — built-in GitHub MCP tools for searching/reading
// GitHub issues, pull requests, code, and repository metadata.
//
// Uses the GitHub REST API via QNetworkAccessManager.
// Requires a GitHub token for authenticated access.
// ─────────────────────────────────────────────────────────────────────────────

namespace GitHubMCPInternal {

struct HttpResponse {
    bool ok = false;
    QString body;
    QString error;
};

HttpResponse httpGet(const QString &url, const QString &token,
                     const QString &accept = QStringLiteral("application/vnd.github+json"));

} // namespace GitHubMCPInternal

class GitHubIssuesTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_token;
};

class GitHubPRTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_token;
};

class GitHubCodeSearchTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_token;
};

class GitHubRepoInfoTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    QString m_token;
};
