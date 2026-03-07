#pragma once

#include "../itool.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

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

inline HttpResponse httpGet(const QString &url, const QString &token,
                             const QString &accept = QStringLiteral("application/vnd.github+json"))
{
    QNetworkAccessManager mgr;
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    req.setRawHeader("Accept", accept.toUtf8());
    if (!token.isEmpty())
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());
    req.setRawHeader("User-Agent", "Exorcist-IDE/1.0");

    QNetworkReply *reply = mgr.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(15000, &loop, &QEventLoop::quit);
    loop.exec();

    HttpResponse resp;
    if (reply->isFinished() && reply->error() == QNetworkReply::NoError) {
        resp.ok = true;
        resp.body = QString::fromUtf8(reply->readAll());
    } else {
        resp.error = reply->errorString();
    }
    reply->deleteLater();
    return resp;
}

} // namespace GitHubMCPInternal

class GitHubIssuesTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name = QStringLiteral("github_issues");
        s.description = QStringLiteral(
            "Search or list GitHub issues in a repository. "
            "Returns issue titles, numbers, labels, and bodies.");
        s.permission = AgentToolPermission::ReadOnly;
        s.timeoutMs = 15000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("owner"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral("Repository owner")}
                }},
                {QStringLiteral("repo"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral("Repository name")}
                }},
                {QStringLiteral("query"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral("Search query (optional)")}
                }},
                {QStringLiteral("state"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral("Filter: open, closed, all (default: open)")}
                }},
                {QStringLiteral("maxResults"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"), QStringLiteral("Max results (default: 10)")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("owner"), QStringLiteral("repo")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString owner = args.value(QStringLiteral("owner")).toString();
        const QString repo = args.value(QStringLiteral("repo")).toString();
        const QString query = args.value(QStringLiteral("query")).toString();
        const QString state = args.value(QStringLiteral("state")).toString(QStringLiteral("open"));
        const int max = args.value(QStringLiteral("maxResults")).toInt(10);

        if (owner.isEmpty() || repo.isEmpty())
            return {false, {}, {}, QStringLiteral("owner and repo are required")};

        QString urlStr;
        if (!query.isEmpty()) {
            // Use search API
            urlStr = QStringLiteral("https://api.github.com/search/issues?q=%1+repo:%2/%3+type:issue+state:%4&per_page=%5")
                         .arg(QUrl::toPercentEncoding(query), owner, repo, state)
                         .arg(max);
        } else {
            urlStr = QStringLiteral("https://api.github.com/repos/%1/%2/issues?state=%3&per_page=%4")
                         .arg(owner, repo, state).arg(max);
        }

        const auto response = GitHubMCPInternal::httpGet(urlStr, m_token);

        if (!response.ok)
            return {false, {}, {}, QStringLiteral("GitHub API error: %1").arg(response.error)};

        const QJsonDocument doc = QJsonDocument::fromJson(response.body.toUtf8());

        QJsonArray issues;
        if (doc.isObject() && doc.object().contains(QStringLiteral("items")))
            issues = doc.object().value(QStringLiteral("items")).toArray();
        else if (doc.isArray())
            issues = doc.array();

        QStringList lines;
        for (int i = 0; i < qMin(max, issues.size()); ++i) {
            const QJsonObject issue = issues[i].toObject();
            // Skip PRs when listing issues
            if (issue.contains(QStringLiteral("pull_request")) && query.isEmpty())
                continue;

            lines << QStringLiteral("#%1 [%2] %3")
                         .arg(issue.value(QStringLiteral("number")).toInt())
                         .arg(issue.value(QStringLiteral("state")).toString())
                         .arg(issue.value(QStringLiteral("title")).toString());

            QStringList labels;
            for (const auto &l : issue.value(QStringLiteral("labels")).toArray())
                labels << l.toObject().value(QStringLiteral("name")).toString();
            if (!labels.isEmpty())
                lines << QStringLiteral("  Labels: %1").arg(labels.join(QStringLiteral(", ")));

            const QString body = issue.value(QStringLiteral("body")).toString();
            if (!body.isEmpty()) {
                const QString preview = body.left(200).replace(QLatin1Char('\n'), QLatin1Char(' '));
                lines << QStringLiteral("  %1").arg(preview);
            }
            lines << QString();
        }

        return {true, {}, lines.join(QLatin1Char('\n')), {}};
    }

private:
    QString m_token;
};

class GitHubPRTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name = QStringLiteral("github_pr");
        s.description = QStringLiteral(
            "Read pull request data and diffs from a GitHub repository.");
        s.permission = AgentToolPermission::ReadOnly;
        s.timeoutMs = 15000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("owner"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                {QStringLiteral("repo"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                {QStringLiteral("number"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"), QStringLiteral("PR number. If omitted, lists recent PRs.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("owner"), QStringLiteral("repo")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString owner = args.value(QStringLiteral("owner")).toString();
        const QString repo = args.value(QStringLiteral("repo")).toString();
        const int number = args.value(QStringLiteral("number")).toInt(-1);

        if (number < 0) {
            // List recent PRs
            const QString url = QStringLiteral("https://api.github.com/repos/%1/%2/pulls?per_page=10")
                                    .arg(owner, repo);
            const auto response = GitHubMCPInternal::httpGet(url, m_token);
            if (!response.ok)
                return {false, {}, {}, response.error};

            const QJsonArray prs = QJsonDocument::fromJson(response.body.toUtf8()).array();
            QStringList lines;
            for (const auto &pr : prs) {
                const QJsonObject p = pr.toObject();
                lines << QStringLiteral("#%1 [%2] %3 (%4 → %5)")
                             .arg(p.value(QStringLiteral("number")).toInt())
                             .arg(p.value(QStringLiteral("state")).toString())
                             .arg(p.value(QStringLiteral("title")).toString())
                             .arg(p.value(QStringLiteral("head")).toObject().value(QStringLiteral("ref")).toString())
                             .arg(p.value(QStringLiteral("base")).toObject().value(QStringLiteral("ref")).toString());
            }
            return {true, {}, lines.join(QLatin1Char('\n')), {}};
        }

        // Get specific PR + diff
        const QString url = QStringLiteral("https://api.github.com/repos/%1/%2/pulls/%3")
                                .arg(owner, repo).arg(number);
        const auto response = GitHubMCPInternal::httpGet(url, m_token);
        if (!response.ok)
            return {false, {}, {}, response.error};

        const QJsonObject pr = QJsonDocument::fromJson(response.body.toUtf8()).object();
        QStringList lines;
        lines << QStringLiteral("PR #%1: %2").arg(number).arg(pr.value(QStringLiteral("title")).toString());
        lines << QStringLiteral("State: %1").arg(pr.value(QStringLiteral("state")).toString());
        lines << QStringLiteral("Author: %1").arg(pr.value(QStringLiteral("user")).toObject().value(QStringLiteral("login")).toString());
        lines << QStringLiteral("Branch: %1 → %2")
                     .arg(pr.value(QStringLiteral("head")).toObject().value(QStringLiteral("ref")).toString())
                     .arg(pr.value(QStringLiteral("base")).toObject().value(QStringLiteral("ref")).toString());
        lines << QStringLiteral("Changed files: %1, +%2 -%3")
                     .arg(pr.value(QStringLiteral("changed_files")).toInt())
                     .arg(pr.value(QStringLiteral("additions")).toInt())
                     .arg(pr.value(QStringLiteral("deletions")).toInt());

        const QString body = pr.value(QStringLiteral("body")).toString();
        if (!body.isEmpty()) {
            lines << QString();
            lines << body.left(2000);
        }

        // Fetch diff
        const auto diffResp = GitHubMCPInternal::httpGet(url, m_token, QStringLiteral("application/vnd.github.diff"));
        if (diffResp.ok && !diffResp.body.isEmpty()) {
            lines << QString();
            lines << QStringLiteral("--- Diff ---");
            lines << diffResp.body.left(8000);
        }

        return {true, {}, lines.join(QLatin1Char('\n')), {}};
    }

private:
    QString m_token;
};

class GitHubCodeSearchTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name = QStringLiteral("github_code_search");
        s.description = QStringLiteral(
            "Search code across GitHub repositories.");
        s.permission = AgentToolPermission::ReadOnly;
        s.timeoutMs = 15000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("query"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral("Code search query")}
                }},
                {QStringLiteral("owner"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral("Limit to repos by this owner (optional)")}
                }},
                {QStringLiteral("repo"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral("Limit to this repo (optional)")}
                }},
                {QStringLiteral("maxResults"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("integer")},
                    {QStringLiteral("description"), QStringLiteral("Max results (default: 10)")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("query")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        QString query = args.value(QStringLiteral("query")).toString();
        const QString owner = args.value(QStringLiteral("owner")).toString();
        const QString repo = args.value(QStringLiteral("repo")).toString();
        const int max = args.value(QStringLiteral("maxResults")).toInt(10);

        if (!owner.isEmpty() && !repo.isEmpty())
            query += QStringLiteral("+repo:%1/%2").arg(owner, repo);
        else if (!owner.isEmpty())
            query += QStringLiteral("+user:%1").arg(owner);

        const QString url = QStringLiteral("https://api.github.com/search/code?q=%1&per_page=%2")
                                .arg(QUrl::toPercentEncoding(query)).arg(max);

        const auto response = GitHubMCPInternal::httpGet(url, m_token);
        if (!response.ok)
            return {false, {}, {}, response.error};

        const QJsonObject result = QJsonDocument::fromJson(response.body.toUtf8()).object();
        const QJsonArray items = result.value(QStringLiteral("items")).toArray();

        QStringList lines;
        lines << QStringLiteral("Found %1 results").arg(result.value(QStringLiteral("total_count")).toInt());
        lines << QString();

        for (int i = 0; i < qMin(max, items.size()); ++i) {
            const QJsonObject item = items[i].toObject();
            lines << QStringLiteral("%1/%2: %3")
                         .arg(item.value(QStringLiteral("repository")).toObject().value(QStringLiteral("full_name")).toString())
                         .arg(item.value(QStringLiteral("path")).toString())
                         .arg(item.value(QStringLiteral("html_url")).toString());
        }

        return {true, {}, lines.join(QLatin1Char('\n')), {}};
    }

private:
    QString m_token;
};

class GitHubRepoInfoTool : public ITool
{
public:
    void setToken(const QString &token) { m_token = token; }

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name = QStringLiteral("github_repo");
        s.description = QStringLiteral(
            "Get repository metadata from GitHub: description, stars, "
            "forks, languages, default branch, etc.");
        s.permission = AgentToolPermission::ReadOnly;
        s.timeoutMs = 10000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("owner"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                {QStringLiteral("repo"), QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}}
            }},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("owner"), QStringLiteral("repo")}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const QString owner = args.value(QStringLiteral("owner")).toString();
        const QString repo = args.value(QStringLiteral("repo")).toString();

        const QString url = QStringLiteral("https://api.github.com/repos/%1/%2").arg(owner, repo);

        const auto response = GitHubMCPInternal::httpGet(url, m_token);
        if (!response.ok)
            return {false, {}, {}, response.error};

        const QJsonObject r = QJsonDocument::fromJson(response.body.toUtf8()).object();

        QStringList lines;
        lines << QStringLiteral("%1/%2").arg(owner, repo);
        lines << QStringLiteral("Description: %1").arg(r.value(QStringLiteral("description")).toString());
        lines << QStringLiteral("Language: %1").arg(r.value(QStringLiteral("language")).toString());
        lines << QStringLiteral("Stars: %1  Forks: %2  Watchers: %3")
                     .arg(r.value(QStringLiteral("stargazers_count")).toInt())
                     .arg(r.value(QStringLiteral("forks_count")).toInt())
                     .arg(r.value(QStringLiteral("watchers_count")).toInt());
        lines << QStringLiteral("Default branch: %1").arg(r.value(QStringLiteral("default_branch")).toString());
        lines << QStringLiteral("License: %1").arg(
            r.value(QStringLiteral("license")).toObject().value(QStringLiteral("spdx_id")).toString());
        lines << QStringLiteral("Open issues: %1").arg(r.value(QStringLiteral("open_issues_count")).toInt());
        lines << QStringLiteral("Created: %1").arg(r.value(QStringLiteral("created_at")).toString());
        lines << QStringLiteral("Updated: %1").arg(r.value(QStringLiteral("updated_at")).toString());

        return {true, {}, lines.join(QLatin1Char('\n')), {}};
    }

private:
    QString m_token;
};
