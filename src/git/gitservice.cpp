#include "gitservice.h"

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>
#include <QDir>
#include <QtConcurrent>

#include "process/bridgeclient.h"

GitService::GitService(QObject *parent)
    : QObject(parent),
      m_watcher(new QFileSystemWatcher(this)),
      m_refreshTimer(new QTimer(this)),
      m_isRepo(false)
{
    m_refreshTimer->setSingleShot(true);
    m_refreshTimer->setInterval(200);
    connect(m_refreshTimer, &QTimer::timeout, this, &GitService::refreshAsync);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, [this]() {
        m_refreshTimer->start();
    });
}

void GitService::setWorkingDirectory(const QString &path)
{
    // This runs once on folder open — acceptable short block (< 2s).
    // Future: could make async with callback chain.
    QProcess proc;
    proc.setWorkingDirectory(path);
    proc.start("git", {"rev-parse", "--show-toplevel"});
    proc.waitForFinished(2000);
    const QByteArray out = proc.readAllStandardOutput();
    const QByteArray err = proc.readAllStandardError();
    const bool ok = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0 && !out.isEmpty());

    if (!ok) {
        m_isRepo = false;
        m_gitRoot.clear();
        m_statusCache.clear();
        if (!m_branch.isEmpty()) {
            m_branch.clear();
            emit branchChanged(m_branch);
        }
        resetWatcher();
        emit statusRefreshed();
        return;
    }

    m_isRepo = true;
    m_gitRoot = QString::fromUtf8(out).trimmed();
    resetWatcher();
    refreshAsync();
}

QString GitService::currentBranch() const
{
    return m_branch;
}

QHash<QString, QString> GitService::fileStatuses() const
{
    return m_statusCache;
}

QChar GitService::statusChar(const QString &absPath) const
{
    const QString status = m_statusCache.value(absPath);
    if (status.size() < 2) {
        return QChar(' ');
    }
    if (status[0] != ' ') {
        return status[0];
    }
    if (status[1] != ' ') {
        return status[1];
    }
    return QChar(' ');
}

bool GitService::stageFile(const QString &absPath)
{
    QString out;
    const bool ok = runGit({"add", "--", toRelativePath(absPath)}, &out);
    refreshAsync();
    return ok;
}

bool GitService::unstageFile(const QString &absPath)
{
    QString out;
    const bool ok = runGit({"restore", "--staged", "--", toRelativePath(absPath)}, &out);
    refreshAsync();
    return ok;
}

bool GitService::commit(const QString &message)
{
    if (message.trimmed().isEmpty()) {
        return false;
    }
    QString out;
    const bool ok = runGit({"commit", "-m", message}, &out);
    refreshAsync();
    return ok;
}

bool GitService::isGitRepo() const
{
    return m_isRepo;
}

QString GitService::diff(const QString &filePath) const
{
    if (!m_isRepo || m_gitRoot.isEmpty())
        return {};

    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    QStringList args{QStringLiteral("diff"), QStringLiteral("HEAD")};
    if (!filePath.isEmpty())
        args << QStringLiteral("--") << QDir(m_gitRoot).relativeFilePath(filePath);
    proc.start(QStringLiteral("git"), args);
    proc.waitForFinished(10000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QList<GitService::LineChangeRange> GitService::lineChanges(const QString &absFilePath) const
{
    QList<LineChangeRange> result;
    const QString unifiedDiff = diff(absFilePath);
    if (unifiedDiff.isEmpty())
        return result;

    // Parse unified diff hunk headers: @@ -oldStart,oldCount +newStart,newCount @@
    static const QRegularExpression hunkRx(
        QStringLiteral(R"(@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@)"));

    const QStringList lines = unifiedDiff.split(QLatin1Char('\n'));
    int newLine = 0;
    bool inHunk = false;

    for (const QString &line : lines) {
        auto m = hunkRx.match(line);
        if (m.hasMatch()) {
            int oldCount = m.captured(2).isEmpty() ? 1 : m.captured(2).toInt();
            newLine = m.captured(3).toInt() - 1; // convert to 0-based
            int newCount = m.captured(4).isEmpty() ? 1 : m.captured(4).toInt();
            Q_UNUSED(oldCount)
            Q_UNUSED(newCount)
            inHunk = true;
            continue;
        }
        if (!inHunk) continue;
        if (line.isEmpty()) { ++newLine; continue; }

        const QChar prefix = line[0];
        if (prefix == QLatin1Char('+')) {
            if (!result.isEmpty() &&
                result.last().type == LineChange::Added &&
                result.last().startLine + result.last().lineCount == newLine) {
                result.last().lineCount++;
            } else {
                result.append({newLine, 1, LineChange::Added});
            }
            ++newLine;
        } else if (prefix == QLatin1Char('-')) {
            if (!result.isEmpty() &&
                result.last().type == LineChange::Deleted &&
                result.last().startLine == newLine) {
                result.last().lineCount++;
            } else {
                result.append({newLine, 1, LineChange::Deleted});
            }
        } else if (prefix == QLatin1Char(' ')) {
            ++newLine;
        } else if (prefix == QLatin1Char('\\')) {
            // "\ No newline at end of file" — skip
        } else {
            inHunk = false;
        }
    }

    // Merge adjacent added+deleted ranges into "modified"
    for (int i = 0; i + 1 < result.size(); ) {
        auto &a = result[i];
        auto &b = result[i + 1];
        if (a.type == LineChange::Deleted && b.type == LineChange::Added &&
            a.startLine == b.startLine) {
            b.type = LineChange::Modified;
            result.removeAt(i);
        } else {
            ++i;
        }
    }

    return result;
}

QStringList GitService::conflictFiles() const
{
    if (!m_isRepo || m_gitRoot.isEmpty())
        return {};

    QStringList result;
    for (auto it = m_statusCache.begin(); it != m_statusCache.end(); ++it) {
        const QString &status = it.value();
        if (status == QLatin1String("UU") || status == QLatin1String("AA") ||
            status == QLatin1String("DD") || status == QLatin1String("AU") ||
            status == QLatin1String("UA") || status == QLatin1String("DU") ||
            status == QLatin1String("UD")) {
            result << it.key();
        }
    }
    return result;
}

QString GitService::conflictContent(const QString &filePath) const
{
    if (!m_isRepo) return {};
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

QStringList GitService::localBranches() const
{
    if (!m_isRepo || m_gitRoot.isEmpty())
        return {};
    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    proc.start(QStringLiteral("git"), {QStringLiteral("branch"), QStringLiteral("--list")});
    proc.waitForFinished(5000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return {};
    QStringList branches;
    const QStringList lines = QString::fromUtf8(proc.readAllStandardOutput())
                                  .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QString name = line.mid(2).trimmed();
        if (!name.isEmpty())
            branches.append(name);
    }
    return branches;
}

bool GitService::checkoutBranch(const QString &branchName)
{
    QString out;
    const bool ok = runGit({QStringLiteral("checkout"), branchName}, &out);
    if (ok)
        refreshAsync();
    return ok;
}

bool GitService::createBranch(const QString &branchName)
{
    QString out;
    const bool ok = runGit({QStringLiteral("checkout"), QStringLiteral("-b"), branchName}, &out);
    if (ok)
        refreshAsync();
    return ok;
}

QList<GitService::BlameEntry> GitService::blame(const QString &absFilePath) const
{
    QList<BlameEntry> result;
    if (!m_isRepo || m_gitRoot.isEmpty())
        return result;

    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    const QString rel = QDir(m_gitRoot).relativeFilePath(absFilePath);
    proc.start(QStringLiteral("git"),
               {QStringLiteral("blame"), QStringLiteral("--porcelain"), rel});
    proc.waitForFinished(30000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return result;

    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    return parseBlameOutput(out);
}

QString GitService::showAtHead(const QString &absFilePath) const
{
    if (!m_isRepo || m_gitRoot.isEmpty())
        return {};
    const QString relPath = QDir(m_gitRoot).relativeFilePath(absFilePath);
    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    proc.start(QStringLiteral("git"),
               {QStringLiteral("show"), QStringLiteral("HEAD:") + relPath});
    proc.waitForFinished(10000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QString GitService::showAtRevision(const QString &absFilePath, const QString &rev) const
{
    if (!m_isRepo || m_gitRoot.isEmpty())
        return {};
    const QString relPath = QDir(m_gitRoot).relativeFilePath(absFilePath);
    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    proc.start(QStringLiteral("git"),
               {QStringLiteral("show"), rev + QStringLiteral(":") + relPath});
    proc.waitForFinished(10000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QString GitService::diffRevisions(const QString &rev1, const QString &rev2) const
{
    if (!m_isRepo || m_gitRoot.isEmpty())
        return {};
    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    proc.start(QStringLiteral("git"),
               {QStringLiteral("diff"), rev1, rev2});
    proc.waitForFinished(30000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QList<GitService::ChangedFile> GitService::changedFilesBetween(const QString &rev1,
                                                               const QString &rev2) const
{
    QList<ChangedFile> result;
    if (!m_isRepo || m_gitRoot.isEmpty())
        return result;
    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    proc.start(QStringLiteral("git"),
               {QStringLiteral("diff"), QStringLiteral("--name-status"), rev1, rev2});
    proc.waitForFinished(10000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return result;
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.size() < 3) continue;
        ChangedFile cf;
        cf.status = line.at(0);
        cf.path = line.mid(2).trimmed();
        // For renames (R100\toldpath\tnewpath), take the new path
        if (cf.status == QLatin1Char('R') || cf.status == QLatin1Char('C')) {
            const int tab = cf.path.indexOf(QLatin1Char('\t'));
            if (tab >= 0)
                cf.path = cf.path.mid(tab + 1);
        }
        result.append(cf);
    }
    return result;
}

QList<GitService::LogEntry> GitService::log(int maxCount) const
{
    QList<LogEntry> result;
    if (!m_isRepo || m_gitRoot.isEmpty())
        return result;
    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    proc.start(QStringLiteral("git"),
               {QStringLiteral("log"), QStringLiteral("--format=%h|%an|%as|%s"),
                QStringLiteral("-n"), QString::number(maxCount)});
    proc.waitForFinished(10000);
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return result;
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const auto parts = line.split(QLatin1Char('|'));
        if (parts.size() < 4) continue;
        LogEntry e;
        e.hash = parts[0];
        e.author = parts[1];
        e.date = parts[2];
        e.subject = parts.mid(3).join(QLatin1Char('|'));  // subject may contain |
        result.append(e);
    }
    return result;
}

// ── Async variants (Manifesto #2: never block UI thread) ─────────────────────

void GitService::blameAsync(const QString &absFilePath)
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit blameReady(absFilePath, {});
        return;
    }
    const QString gitRoot = m_gitRoot;
    const QString rel = QDir(gitRoot).relativeFilePath(absFilePath);
    QtConcurrent::run([gitRoot, rel, absFilePath, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("blame"), QStringLiteral("--porcelain"), rel});
        proc.waitForFinished(30000);
        QList<BlameEntry> entries;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0)
            entries = parseBlameOutput(QString::fromUtf8(proc.readAllStandardOutput()));
        QMetaObject::invokeMethod(this, [this, absFilePath, entries]() {
            emit blameReady(absFilePath, entries);
        }, Qt::QueuedConnection);
    });
}

void GitService::diffAsync(const QString &filePath)
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit diffReady(filePath, {});
        return;
    }
    const QString gitRoot = m_gitRoot;
    QtConcurrent::run([gitRoot, filePath, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        QStringList args{QStringLiteral("diff"), QStringLiteral("HEAD")};
        if (!filePath.isEmpty())
            args << QStringLiteral("--") << QDir(gitRoot).relativeFilePath(filePath);
        proc.start(QStringLiteral("git"), args);
        proc.waitForFinished(10000);
        QString result;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0)
            result = QString::fromUtf8(proc.readAllStandardOutput());
        QMetaObject::invokeMethod(this, [this, filePath, result]() {
            emit diffReady(filePath, result);
        }, Qt::QueuedConnection);
    });
}

void GitService::showAtHeadAsync(const QString &absFilePath)
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit showAtHeadReady(absFilePath, {});
        return;
    }
    const QString gitRoot = m_gitRoot;
    const QString relPath = QDir(gitRoot).relativeFilePath(absFilePath);
    QtConcurrent::run([gitRoot, relPath, absFilePath, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("show"), QStringLiteral("HEAD:") + relPath});
        proc.waitForFinished(10000);
        QString content;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0)
            content = QString::fromUtf8(proc.readAllStandardOutput());
        QMetaObject::invokeMethod(this, [this, absFilePath, content]() {
            emit showAtHeadReady(absFilePath, content);
        }, Qt::QueuedConnection);
    });
}

void GitService::showAtRevisionAsync(const QString &absFilePath, const QString &rev)
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit showAtRevisionReady(absFilePath, rev, {});
        return;
    }
    const QString gitRoot = m_gitRoot;
    const QString relPath = QDir(gitRoot).relativeFilePath(absFilePath);
    QtConcurrent::run([gitRoot, relPath, absFilePath, rev, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("show"), rev + QStringLiteral(":") + relPath});
        proc.waitForFinished(10000);
        QString content;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0)
            content = QString::fromUtf8(proc.readAllStandardOutput());
        QMetaObject::invokeMethod(this, [this, absFilePath, rev, content]() {
            emit showAtRevisionReady(absFilePath, rev, content);
        }, Qt::QueuedConnection);
    });
}

void GitService::diffRevisionsAsync(const QString &rev1, const QString &rev2)
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit diffRevisionsReady(rev1, rev2, {});
        return;
    }
    const QString gitRoot = m_gitRoot;
    QtConcurrent::run([gitRoot, rev1, rev2, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("diff"), rev1, rev2});
        proc.waitForFinished(30000);
        QString result;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0)
            result = QString::fromUtf8(proc.readAllStandardOutput());
        QMetaObject::invokeMethod(this, [this, rev1, rev2, result]() {
            emit diffRevisionsReady(rev1, rev2, result);
        }, Qt::QueuedConnection);
    });
}

void GitService::changedFilesBetweenAsync(const QString &rev1, const QString &rev2)
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit changedFilesBetweenReady(rev1, rev2, {});
        return;
    }
    const QString gitRoot = m_gitRoot;
    QtConcurrent::run([gitRoot, rev1, rev2, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("diff"), QStringLiteral("--name-status"), rev1, rev2});
        proc.waitForFinished(10000);
        QList<ChangedFile> files;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            const QString out = QString::fromUtf8(proc.readAllStandardOutput());
            const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                if (line.size() < 3) continue;
                ChangedFile cf;
                cf.status = line.at(0);
                cf.path = line.mid(2).trimmed();
                if (cf.status == QLatin1Char('R') || cf.status == QLatin1Char('C')) {
                    const int tab = cf.path.indexOf(QLatin1Char('\t'));
                    if (tab >= 0)
                        cf.path = cf.path.mid(tab + 1);
                }
                files.append(cf);
            }
        }
        QMetaObject::invokeMethod(this, [this, rev1, rev2, files]() {
            emit changedFilesBetweenReady(rev1, rev2, files);
        }, Qt::QueuedConnection);
    });
}

void GitService::localBranchesAsync()
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit localBranchesReady({});
        return;
    }
    const QString gitRoot = m_gitRoot;
    QtConcurrent::run([gitRoot, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("branch"), QStringLiteral("--list")});
        proc.waitForFinished(5000);
        QStringList branches;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            const QStringList lines = QString::fromUtf8(proc.readAllStandardOutput())
                                          .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                QString name = line.mid(2).trimmed();
                if (!name.isEmpty())
                    branches.append(name);
            }
        }
        QMetaObject::invokeMethod(this, [this, branches]() {
            emit localBranchesReady(branches);
        }, Qt::QueuedConnection);
    });
}

void GitService::logAsync(int maxCount)
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        emit logReady({});
        return;
    }
    const QString gitRoot = m_gitRoot;
    QtConcurrent::run([gitRoot, maxCount, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"),
                   {QStringLiteral("log"), QStringLiteral("--format=%h|%an|%as|%s"),
                    QStringLiteral("-n"), QString::number(maxCount)});
        proc.waitForFinished(10000);
        QList<LogEntry> entries;
        if (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
            const QString out = QString::fromUtf8(proc.readAllStandardOutput());
            const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const auto parts = line.split(QLatin1Char('|'));
                if (parts.size() < 4) continue;
                LogEntry e;
                e.hash = parts[0];
                e.author = parts[1];
                e.date = parts[2];
                e.subject = parts.mid(3).join(QLatin1Char('|'));
                entries.append(e);
            }
        }
        QMetaObject::invokeMethod(this, [this, entries]() {
            emit logReady(entries);
        }, Qt::QueuedConnection);
    });
}

void GitService::runGitAsync(const QStringList &args,
                             std::function<void(bool ok, const QString &output)> callback)
{
    if (m_gitRoot.isEmpty()) {
        if (callback) callback(false, {});
        return;
    }
    const QString gitRoot = m_gitRoot;
    QtConcurrent::run([gitRoot, args, callback, this]() {
        QProcess proc;
        proc.setWorkingDirectory(gitRoot);
        proc.start(QStringLiteral("git"), args);
        proc.waitForFinished(5000);
        const bool ok = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        if (callback) {
            QMetaObject::invokeMethod(this, [callback, ok, out]() {
                callback(ok, out);
            }, Qt::QueuedConnection);
        }
    });
}

// ── Async refresh (called by file watcher timer) ─────────────────────────────

void GitService::refreshAsync()
{
    if (!m_isRepo || m_gitRoot.isEmpty())
        return;

    const QString gitRoot = m_gitRoot;
    QtConcurrent::run([gitRoot, this]() {
        // Run status
        QProcess statusProc;
        statusProc.setWorkingDirectory(gitRoot);
        statusProc.start(QStringLiteral("git"),
                         {QStringLiteral("status"), QStringLiteral("--porcelain=v1"),
                          QStringLiteral("-uall")});
        statusProc.waitForFinished(5000);
        const QString statusOut = (statusProc.exitStatus() == QProcess::NormalExit
                                   && statusProc.exitCode() == 0)
            ? QString::fromUtf8(statusProc.readAllStandardOutput()) : QString();

        // Run branch
        QProcess branchProc;
        branchProc.setWorkingDirectory(gitRoot);
        branchProc.start(QStringLiteral("git"),
                         {QStringLiteral("branch"), QStringLiteral("--show-current")});
        branchProc.waitForFinished(2000);
        const QString branchOut = (branchProc.exitStatus() == QProcess::NormalExit
                                   && branchProc.exitCode() == 0)
            ? QString::fromUtf8(branchProc.readAllStandardOutput()) : QString();

        // Post results back to main thread
        QMetaObject::invokeMethod(this, [this, gitRoot, statusOut, branchOut]() {
            if (m_gitRoot != gitRoot) return; // directory changed since

            if (!statusOut.isNull()) {
                m_statusCache.clear();
                const QStringList lines = statusOut.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    if (line.size() < 3) continue;
                    const QString status = line.left(2);
                    QString path = line.mid(3).trimmed();
                    if (path.contains(QLatin1String(" -> ")))
                        path = path.section(QLatin1String(" -> "), 1, 1);
                    const QString absPath = QDir(m_gitRoot).absoluteFilePath(path);
                    m_statusCache.insert(absPath, status);
                }
            }

            if (!branchOut.isNull()) {
                const QString branch = branchOut.trimmed();
                if (branch != m_branch) {
                    m_branch = branch;
                    emit branchChanged(m_branch);
                }
            }

            emit statusRefreshed();
        }, Qt::QueuedConnection);
    });
}

// ── Sync internals (used by stage/unstage/commit which need immediate result) ─

void GitService::refresh()
{
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        return;
    }

    QString statusOut;
    if (runGit({"status", "--porcelain=v1", "-uall"}, &statusOut)) {
        m_statusCache.clear();
        const QStringList lines = statusOut.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.size() < 3) {
                continue;
            }
            const QString status = line.left(2);
            QString path = line.mid(3).trimmed();
            if (path.contains(" -> ")) {
                path = path.section(" -> ", 1, 1);
            }
            const QString absPath = QDir(m_gitRoot).absoluteFilePath(path);
            m_statusCache.insert(absPath, status);
        }
    }

    QString branchOut;
    if (runGit({"branch", "--show-current"}, &branchOut)) {
        const QString branch = branchOut.trimmed();
        if (branch != m_branch) {
            m_branch = branch;
            emit branchChanged(m_branch);
        }
    }

    emit statusRefreshed();
}

bool GitService::runGit(const QStringList &args, QString *out)
{
    if (m_gitRoot.isEmpty()) {
        return false;
    }

    QProcess proc;
    proc.setWorkingDirectory(m_gitRoot);
    proc.start("git", args);
    proc.waitForFinished(5000);

    if (out) {
        *out = QString::fromUtf8(proc.readAllStandardOutput());
    }

    return (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
}

QString GitService::toRelativePath(const QString &absPath) const
{
    return QDir(m_gitRoot).relativeFilePath(absPath);
}

void GitService::resetWatcher()
{
    m_watcher->removePaths(m_watcher->files());
    if (!m_isRepo || m_gitRoot.isEmpty()) {
        // Unwatch from bridge if applicable
        if (m_bridgeClient && !m_gitRoot.isEmpty()) {
            QJsonObject args;
            args[QLatin1String("repoPath")] = m_gitRoot;
            m_bridgeClient->callService(
                QStringLiteral("gitwatch"), QStringLiteral("unwatch"),
                args, {});
        }
        return;
    }

    // If bridge is available, delegate watching to ExoBridge
    if (m_bridgeClient && m_bridgeClient->isConnected()) {
        QJsonObject args;
        args[QLatin1String("repoPath")] = m_gitRoot;
        m_bridgeClient->callService(
            QStringLiteral("gitwatch"), QStringLiteral("watch"),
            args, {});
        return;
    }

    // Fallback: local QFileSystemWatcher
    const QString indexPath = QDir(m_gitRoot).absoluteFilePath(".git/index");
    const QString headPath = QDir(m_gitRoot).absoluteFilePath(".git/HEAD");

    QStringList files;
    if (QFileInfo::exists(indexPath)) {
        files << indexPath;
    }
    if (QFileInfo::exists(headPath)) {
        files << headPath;
    }
    if (!files.isEmpty()) {
        m_watcher->addPaths(files);
    }
}

// ── Blame output parser (shared by sync and async) ───────────────────────────

QList<GitService::BlameEntry> GitService::parseBlameOutput(const QString &out)
{
    QList<BlameEntry> result;
    static const QRegularExpression headerRx(
        QStringLiteral(R"(^([0-9a-f]{40}) \d+ (\d+))"));

    QHash<QString, BlameEntry> commitCache;
    QString currentHash;
    int currentLine = 0;

    const QStringList lines = out.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        auto m = headerRx.match(line);
        if (m.hasMatch()) {
            currentHash = m.captured(1);
            currentLine = m.captured(2).toInt();
            if (!commitCache.contains(currentHash)) {
                BlameEntry e;
                e.commitHash = currentHash;
                commitCache.insert(currentHash, e);
            }
            continue;
        }
        if (currentHash.isEmpty()) continue;

        if (line.startsWith(QLatin1String("author ")))
            commitCache[currentHash].author = line.mid(7);
        else if (line.startsWith(QLatin1String("summary ")))
            commitCache[currentHash].summary = line.mid(8);
        else if (line.startsWith(QLatin1Char('\t'))) {
            BlameEntry entry = commitCache.value(currentHash);
            entry.line = currentLine;
            if (entry.date.isEmpty() && !entry.author.isEmpty())
                entry.date = entry.commitHash.left(8);
            result.append(entry);
        }
    }

    return result;
}

// ── Bridge delegation ────────────────────────────────────────────────────────

void GitService::setBridgeClient(BridgeClient *client)
{
    m_bridgeClient = client;

    if (m_bridgeClient) {
        // Listen for git change events from ExoBridge
        connect(m_bridgeClient, &BridgeClient::serviceEvent,
                this, [this](const QString &service, const QString &event) {
            Q_UNUSED(event)
            if (service != QLatin1String("gitwatch"))
                return;
            // ExoBridge notified us that a watched repo changed —
            // trigger async refresh
            m_refreshTimer->start();
        });

        // If we already have a repo set, re-register with the bridge
        if (m_isRepo && !m_gitRoot.isEmpty())
            resetWatcher();
    }
}
