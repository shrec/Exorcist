#pragma once

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>

#include <functional>

/// Async wrapper around the `gh` CLI tool.
/// All operations run asynchronously and return results via callbacks.
class GhService : public QObject
{
    Q_OBJECT

public:
    explicit GhService(QObject *parent = nullptr);

    /// Set the working directory for gh commands (repo root).
    void setWorkingDirectory(const QString &path);
    QString workingDirectory() const { return m_workDir; }

    /// Check if `gh` CLI is available on PATH.
    bool isAvailable() const { return m_available; }

    /// Check gh auth status asynchronously.
    void checkAuth(std::function<void(bool authenticated, const QString &user)> callback);

    // ── Pull Requests ──────────────────────────────
    void listPullRequests(const QString &state,
                          std::function<void(bool ok, const QJsonArray &prs)> callback);
    void createPullRequest(const QString &title, const QString &body,
                           const QString &base, const QString &head,
                           std::function<void(bool ok, const QJsonObject &pr)> callback);
    void viewPullRequest(int number,
                         std::function<void(bool ok, const QJsonObject &pr)> callback);
    void mergePullRequest(int number, const QString &method,
                          std::function<void(bool ok, const QString &output)> callback);
    void checkoutPullRequest(int number,
                             std::function<void(bool ok, const QString &output)> callback);

    // ── Issues ─────────────────────────────────────
    void listIssues(const QString &state,
                    std::function<void(bool ok, const QJsonArray &issues)> callback);
    void createIssue(const QString &title, const QString &body,
                     std::function<void(bool ok, const QJsonObject &issue)> callback);
    void viewIssue(int number,
                   std::function<void(bool ok, const QJsonObject &issue)> callback);
    void closeIssue(int number,
                    std::function<void(bool ok, const QString &output)> callback);
    void commentOnIssue(int number, const QString &body,
                        std::function<void(bool ok, const QString &output)> callback);

    // ── Repos ──────────────────────────────────────
    void repoView(std::function<void(bool ok, const QJsonObject &repo)> callback);
    void listRepos(std::function<void(bool ok, const QJsonArray &repos)> callback);

    // ── Releases ───────────────────────────────────
    void listReleases(std::function<void(bool ok, const QJsonArray &releases)> callback);
    void createRelease(const QString &tag, const QString &title,
                       const QString &notes, bool draft, bool prerelease,
                       std::function<void(bool ok, const QJsonObject &release)> callback);

    // ── Workflow Runs ──────────────────────────────
    void listWorkflowRuns(int limit,
                          std::function<void(bool ok, const QJsonArray &runs)> callback);
    void viewWorkflowRun(int runId,
                         std::function<void(bool ok, const QJsonObject &run)> callback);

signals:
    void availabilityChanged(bool available);
    void authStatusChanged(bool authenticated, const QString &user);
    void errorOccurred(const QString &message);

private:
    /// Run a gh command asynchronously. Calls callback with (exitCode==0, stdout).
    void runGh(const QStringList &args,
               std::function<void(bool ok, const QString &output)> callback);

    /// Run a gh command that returns JSON.
    void runGhJson(const QStringList &args,
                   std::function<void(bool ok, const QJsonArray &arr)> arrayCallback);
    void runGhJsonObject(const QStringList &args,
                         std::function<void(bool ok, const QJsonObject &obj)> objectCallback);

    void detectAvailability();

    QString m_workDir;
    bool m_available = false;
};
