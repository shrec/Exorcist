#include "ghservice.h"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>

GhService::GhService(QObject *parent)
    : QObject(parent)
{
    detectAvailability();
}

void GhService::setWorkingDirectory(const QString &path)
{
    m_workDir = path;
}

void GhService::detectAvailability()
{
    const QString ghPath = QStandardPaths::findExecutable(QStringLiteral("gh"));
    const bool was = m_available;
    m_available = !ghPath.isEmpty();
    if (was != m_available)
        emit availabilityChanged(m_available);
}

// ── Core runner ────────────────────────────────────────────────

void GhService::runGh(const QStringList &args,
                       std::function<void(bool ok, const QString &output)> callback)
{
    if (!m_available) {
        if (callback) callback(false, tr("gh CLI not found on PATH"));
        return;
    }

    auto *proc = new QProcess(this);
    if (!m_workDir.isEmpty())
        proc->setWorkingDirectory(m_workDir);

    connect(proc, &QProcess::finished, this,
            [this, proc, callback](int exitCode, QProcess::ExitStatus status) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput());
        const QString err = QString::fromUtf8(proc->readAllStandardError());
        const bool ok = (status == QProcess::NormalExit && exitCode == 0);

        if (!ok && !err.trimmed().isEmpty())
            emit errorOccurred(err.trimmed());

        if (callback) callback(ok, ok ? out : err);
        proc->deleteLater();
    });

    proc->start(QStringLiteral("gh"), args);
}

void GhService::runGhJson(const QStringList &args,
                           std::function<void(bool ok, const QJsonArray &arr)> arrayCallback)
{
    runGh(args, [this, arrayCallback](bool ok, const QString &output) {
        if (!ok) {
            if (arrayCallback) arrayCallback(false, {});
            return;
        }
        const auto doc = QJsonDocument::fromJson(output.toUtf8());
        if (arrayCallback)
            arrayCallback(true, doc.isArray() ? doc.array() : QJsonArray{doc.object()});
    });
}

void GhService::runGhJsonObject(const QStringList &args,
                                 std::function<void(bool ok, const QJsonObject &obj)> objectCallback)
{
    runGh(args, [this, objectCallback](bool ok, const QString &output) {
        if (!ok) {
            if (objectCallback) objectCallback(false, {});
            return;
        }
        const auto doc = QJsonDocument::fromJson(output.toUtf8());
        if (objectCallback)
            objectCallback(true, doc.object());
    });
}

// ── Auth ───────────────────────────────────────────────────────

void GhService::checkAuth(std::function<void(bool authenticated, const QString &user)> callback)
{
    runGh({QStringLiteral("auth"), QStringLiteral("status")},
          [this, callback](bool ok, const QString &output) {
        // gh auth status outputs: "Logged in to github.com account USER ..."
        QString user;
        if (ok) {
            // Parse "account <user>" from output
            const auto lines = output.split(QLatin1Char('\n'));
            for (const auto &line : lines) {
                int idx = line.indexOf(QStringLiteral("account "));
                if (idx >= 0) {
                    const int start = idx + 8;
                    int end = line.indexOf(QLatin1Char(' '), start);
                    if (end < 0) end = line.length();
                    user = line.mid(start, end - start).trimmed();
                    break;
                }
            }
        }
        emit authStatusChanged(ok, user);
        if (callback) callback(ok, user);
    });
}

// ── Pull Requests ──────────────────────────────────────────────

void GhService::listPullRequests(const QString &state,
                                  std::function<void(bool ok, const QJsonArray &prs)> callback)
{
    QStringList args = {QStringLiteral("pr"), QStringLiteral("list"),
                        QStringLiteral("--json"),
                        QStringLiteral("number,title,state,author,createdAt,headRefName,baseRefName,url"),
                        QStringLiteral("--state"), state.isEmpty() ? QStringLiteral("open") : state};
    runGhJson(args, callback);
}

void GhService::createPullRequest(const QString &title, const QString &body,
                                   const QString &base, const QString &head,
                                   std::function<void(bool ok, const QJsonObject &pr)> callback)
{
    QStringList args = {QStringLiteral("pr"), QStringLiteral("create"),
                        QStringLiteral("--title"), title,
                        QStringLiteral("--body"), body};
    if (!base.isEmpty()) args << QStringLiteral("--base") << base;
    if (!head.isEmpty()) args << QStringLiteral("--head") << head;
    args << QStringLiteral("--json") << QStringLiteral("number,title,url,state");
    runGhJsonObject(args, callback);
}

void GhService::viewPullRequest(int number,
                                 std::function<void(bool ok, const QJsonObject &pr)> callback)
{
    runGhJsonObject({QStringLiteral("pr"), QStringLiteral("view"),
                     QString::number(number), QStringLiteral("--json"),
                     QStringLiteral("number,title,state,body,author,createdAt,headRefName,baseRefName,url,additions,deletions,reviewDecision,comments")},
                    callback);
}

void GhService::mergePullRequest(int number, const QString &method,
                                  std::function<void(bool ok, const QString &output)> callback)
{
    QStringList args = {QStringLiteral("pr"), QStringLiteral("merge"),
                        QString::number(number)};
    if (method == QStringLiteral("squash"))
        args << QStringLiteral("--squash");
    else if (method == QStringLiteral("rebase"))
        args << QStringLiteral("--rebase");
    else
        args << QStringLiteral("--merge");
    runGh(args, callback);
}

void GhService::checkoutPullRequest(int number,
                                     std::function<void(bool ok, const QString &output)> callback)
{
    runGh({QStringLiteral("pr"), QStringLiteral("checkout"), QString::number(number)}, callback);
}

// ── Issues ─────────────────────────────────────────────────────

void GhService::listIssues(const QString &state,
                            std::function<void(bool ok, const QJsonArray &issues)> callback)
{
    QStringList args = {QStringLiteral("issue"), QStringLiteral("list"),
                        QStringLiteral("--json"),
                        QStringLiteral("number,title,state,author,createdAt,labels,url"),
                        QStringLiteral("--state"), state.isEmpty() ? QStringLiteral("open") : state};
    runGhJson(args, callback);
}

void GhService::createIssue(const QString &title, const QString &body,
                             std::function<void(bool ok, const QJsonObject &issue)> callback)
{
    QStringList args = {QStringLiteral("issue"), QStringLiteral("create"),
                        QStringLiteral("--title"), title,
                        QStringLiteral("--body"), body};
    // gh issue create doesn't support --json output; parse the URL from stdout
    runGh(args, [callback](bool ok, const QString &output) {
        QJsonObject obj;
        obj[QStringLiteral("url")] = output.trimmed();
        if (callback) callback(ok, obj);
    });
}

void GhService::viewIssue(int number,
                           std::function<void(bool ok, const QJsonObject &issue)> callback)
{
    runGhJsonObject({QStringLiteral("issue"), QStringLiteral("view"),
                     QString::number(number), QStringLiteral("--json"),
                     QStringLiteral("number,title,state,body,author,createdAt,labels,comments,url")},
                    callback);
}

void GhService::closeIssue(int number,
                            std::function<void(bool ok, const QString &output)> callback)
{
    runGh({QStringLiteral("issue"), QStringLiteral("close"), QString::number(number)}, callback);
}

void GhService::commentOnIssue(int number, const QString &body,
                                std::function<void(bool ok, const QString &output)> callback)
{
    runGh({QStringLiteral("issue"), QStringLiteral("comment"), QString::number(number),
           QStringLiteral("--body"), body}, callback);
}

// ── Repos ──────────────────────────────────────────────────────

void GhService::repoView(std::function<void(bool ok, const QJsonObject &repo)> callback)
{
    runGhJsonObject({QStringLiteral("repo"), QStringLiteral("view"),
                     QStringLiteral("--json"),
                     QStringLiteral("name,owner,description,url,stargazerCount,forkCount,isPrivate,defaultBranchRef")},
                    callback);
}

void GhService::listRepos(std::function<void(bool ok, const QJsonArray &repos)> callback)
{
    runGhJson({QStringLiteral("repo"), QStringLiteral("list"),
               QStringLiteral("--json"),
               QStringLiteral("name,owner,description,url,isPrivate,updatedAt"),
               QStringLiteral("--limit"), QStringLiteral("30")},
              callback);
}

// ── Releases ───────────────────────────────────────────────────

void GhService::listReleases(std::function<void(bool ok, const QJsonArray &releases)> callback)
{
    runGhJson({QStringLiteral("release"), QStringLiteral("list"),
               QStringLiteral("--json"),
               QStringLiteral("tagName,name,isDraft,isPrerelease,publishedAt,url")},
              callback);
}

void GhService::createRelease(const QString &tag, const QString &title,
                               const QString &notes, bool draft, bool prerelease,
                               std::function<void(bool ok, const QJsonObject &release)> callback)
{
    QStringList args = {QStringLiteral("release"), QStringLiteral("create"),
                        tag, QStringLiteral("--title"), title,
                        QStringLiteral("--notes"), notes};
    if (draft) args << QStringLiteral("--draft");
    if (prerelease) args << QStringLiteral("--prerelease");

    // gh release create outputs the URL
    runGh(args, [callback](bool ok, const QString &output) {
        QJsonObject obj;
        obj[QStringLiteral("url")] = output.trimmed();
        obj[QStringLiteral("tagName")] = output.trimmed();
        if (callback) callback(ok, obj);
    });
}

// ── Workflow Runs ──────────────────────────────────────────────

void GhService::listWorkflowRuns(int limit,
                                  std::function<void(bool ok, const QJsonArray &runs)> callback)
{
    runGhJson({QStringLiteral("run"), QStringLiteral("list"),
               QStringLiteral("--json"),
               QStringLiteral("databaseId,displayTitle,status,conclusion,event,createdAt,headBranch,url"),
               QStringLiteral("--limit"), QString::number(qBound(1, limit, 50))},
              callback);
}

void GhService::viewWorkflowRun(int runId,
                                 std::function<void(bool ok, const QJsonObject &run)> callback)
{
    runGhJsonObject({QStringLiteral("run"), QStringLiteral("view"),
                     QString::number(runId), QStringLiteral("--json"),
                     QStringLiteral("databaseId,displayTitle,status,conclusion,event,createdAt,headBranch,url,jobs")},
                    callback);
}
