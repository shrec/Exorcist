#include "gitplugin.h"

#include "git/gitservice.h"
#include "git/gitpanel.h"
#include "git/diffexplorerpanel.h"
#include "git/mergeeditor.h"
#include "inlineblameoverlay.h"

#include "editor/editormanager.h"
#include "editor/editorview.h"
#include "sdk/icommandservice.h"
#include "core/idockmanager.h"
#include "core/imenumanager.h"
#include "agent/agentorchestrator.h"
#include "agent/chat/chatpanelwidget.h"
#include "agent/tools/gitopstool.h"

#include <QAction>
#include <QApplication>
#include <QEventLoop>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QObject>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QTimer>
#include <QVariant>
#include <QWidget>

// ── GitPlugin ────────────────────────────────────────────────────────────────

GitPlugin::GitPlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo GitPlugin::info() const
{
    return { QStringLiteral("org.exorcist.git"),
             QStringLiteral("Git"),
             QStringLiteral("1.0.0"),
             QStringLiteral("Git source control panel, diff explorer, and merge editor") };
}

bool GitPlugin::initializePlugin()
{
    m_git = service<GitService>(QStringLiteral("gitService"));
    if (!m_git) {
        // GitService not yet registered — defer wiring until available
        return true;
    }

    // Panels are created with nullptr parent; createView() re-parents them into
    // their dock widget when ContributionRegistry calls it.
    m_gitPanel     = new GitPanel(m_git, nullptr);
    m_diffExplorer = new DiffExplorerPanel(m_git, nullptr);
    m_mergeEditor  = new MergeEditor(m_git, nullptr);

    wireCommitMessageGeneration();
    wireConflictResolution();
    wireInlineBlame();

    return true;
}

void GitPlugin::shutdownPlugin()
{
}

void GitPlugin::onWorkspaceClosed()
{
    // Rule L2: plugin owns its workspace teardown.  Clear the git working
    // directory + cached panels so the next workspace starts fresh.
    if (m_git)
        m_git->setWorkingDirectory(QString());
}

QWidget *GitPlugin::createView(const QString &viewId, QWidget *parent)
{
    if (viewId == QLatin1String("GitDock") && m_gitPanel) {
        m_gitPanel->setParent(parent);
        return m_gitPanel;
    }
    if (viewId == QLatin1String("DiffExplorerDock") && m_diffExplorer) {
        m_diffExplorer->setParent(parent);
        return m_diffExplorer;
    }
    if (viewId == QLatin1String("MergeEditorDock") && m_mergeEditor) {
        m_mergeEditor->setParent(parent);
        return m_mergeEditor;
    }
    return nullptr;
}

// ── IAgentToolPlugin ──────────────────────────────────────────────────────────

std::vector<std::unique_ptr<ITool>> GitPlugin::createTools()
{
    // Capture m_git via QPointer so the executor gracefully handles plugin
    // unload (QPointer becomes null, executor returns an error).
    QPointer<GitService> gitPtr = m_git;

    auto executor = [gitPtr](const QString &operation,
                             const QJsonObject &args) -> GitOpsTool::GitResult {
        GitService *git = gitPtr.data();
        if (!git || !git->isGitRepo())
            return {false, {}, QStringLiteral("No Git repository.")};

        // ── Helper: run git directly via a non-blocking QEventLoop ──────
        auto runGitDirect = [&git](const QStringList &gitArgs) -> GitOpsTool::GitResult {
            QProcess proc;
            proc.setWorkingDirectory(git->workingDirectory());
            proc.start(QStringLiteral("git"), gitArgs);
            QEventLoop loop;
            QObject::connect(&proc,
                             qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                             &loop, &QEventLoop::quit);
            QTimer::singleShot(15000, &loop, &QEventLoop::quit);
            if (proc.state() != QProcess::NotRunning)
                loop.exec();
            const QString out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
            const QString err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
            if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
                return {false, {}, err.isEmpty() ? QStringLiteral("git failed") : err};
            return {true, out.isEmpty() ? QStringLiteral("OK") : out, {}};
        };

        if (operation == QLatin1String("add")) {
            const QJsonArray files = args[QLatin1String("files")].toArray();
            for (const auto &f : files)
                git->stageFile(f.toString());
            return {true, QStringLiteral("Staged %1 file(s).").arg(files.size()), {}};
        }
        if (operation == QLatin1String("commit")) {
            const QString msg = args[QLatin1String("message")].toString();
            if (msg.isEmpty())
                return {false, {}, QStringLiteral("Commit message required.")};
            bool ok = git->commit(msg);
            return {ok,
                    ok ? QStringLiteral("Committed.") : QStringLiteral("Commit failed."),
                    ok ? QString() : QStringLiteral("Commit failed.")};
        }
        if (operation == QLatin1String("branch")) {
            if (args[QLatin1String("list")].toBool())
                return {true, git->localBranches().join(QLatin1Char('\n')), {}};
            const QString name = args[QLatin1String("name")].toString();
            if (!name.isEmpty()) {
                bool ok = git->createBranch(name);
                return {ok,
                        ok ? QStringLiteral("Branch '%1' created.").arg(name)
                           : QStringLiteral("Failed to create branch."),
                        ok ? QString() : QStringLiteral("Failed.")};
            }
            return {true, git->currentBranch(), {}};
        }
        if (operation == QLatin1String("checkout")) {
            const QString target = args[QLatin1String("target")].toString();
            bool ok = git->checkoutBranch(target);
            return {ok,
                    ok ? QStringLiteral("Checked out '%1'.").arg(target)
                       : QStringLiteral("Checkout failed."),
                    ok ? QString() : QStringLiteral("Failed.")};
        }
        if (operation == QLatin1String("blame")) {
            const QString fp = args[QLatin1String("filePath")].toString();
            const auto entries = git->blame(fp);
            QString result;
            for (const auto &e : entries)
                result += QStringLiteral("%1 %2 %3\n")
                    .arg(e.commitHash.left(8), e.author, e.summary);
            return {true, result, {}};
        }
        if (operation == QLatin1String("reset")) {
            const QJsonArray files = args[QLatin1String("files")].toArray();
            for (const auto &f : files)
                git->unstageFile(f.toString());
            return {true, QStringLiteral("Unstaged %1 file(s).").arg(files.size()), {}};
        }
        if (operation == QLatin1String("log")) {
            int count = args[QLatin1String("count")].toInt(10);
            if (count < 1) count = 1;
            if (count > 50) count = 50;
            QStringList gitArgs = {QStringLiteral("log"),
                                   QStringLiteral("--format=%h|%an|%as|%s"),
                                   QStringLiteral("-n"), QString::number(count)};
            const QString fp = args[QLatin1String("filePath")].toString();
            if (!fp.isEmpty())
                gitArgs << QStringLiteral("--") << fp;
            const QString author = args[QLatin1String("author")].toString();
            if (!author.isEmpty())
                gitArgs << QStringLiteral("--author=%1").arg(author);
            const QString since = args[QLatin1String("since")].toString();
            if (!since.isEmpty())
                gitArgs << QStringLiteral("--since=%1").arg(since);
            const QString grepPat = args[QLatin1String("grep")].toString();
            if (!grepPat.isEmpty())
                gitArgs << QStringLiteral("--grep=%1").arg(grepPat);
            auto result = runGitDirect(gitArgs);
            if (result.ok && result.output.isEmpty())
                result.output = QStringLiteral("No commits found.");
            return result;
        }
        if (operation == QLatin1String("stash")) {
            const QString action = args[QLatin1String("action")].toString(
                QStringLiteral("push"));
            if (action == QLatin1String("push"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("push")});
            if (action == QLatin1String("pop"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("pop")});
            if (action == QLatin1String("list"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("list")});
            if (action == QLatin1String("drop"))
                return runGitDirect({QStringLiteral("stash"), QStringLiteral("drop")});
            return {false, {}, QStringLiteral("Unknown stash action: %1").arg(action)};
        }
        if (operation == QLatin1String("tag")) {
            if (args[QLatin1String("list")].toBool())
                return runGitDirect({QStringLiteral("tag"), QStringLiteral("--list")});
            const QString name = args[QLatin1String("name")].toString();
            if (name.isEmpty())
                return {false, {}, QStringLiteral("Tag name required.")};
            const QString msg = args[QLatin1String("message")].toString();
            if (!msg.isEmpty())
                return runGitDirect({QStringLiteral("tag"), QStringLiteral("-a"),
                                     name, QStringLiteral("-m"), msg});
            return runGitDirect({QStringLiteral("tag"), name});
        }
        if (operation == QLatin1String("cherry_pick")) {
            const QString hash = args[QLatin1String("commitHash")].toString();
            if (hash.isEmpty())
                return {false, {}, QStringLiteral("commitHash required.")};
            return runGitDirect({QStringLiteral("cherry-pick"), hash});
        }
        if (operation == QLatin1String("merge")) {
            const QString branch = args[QLatin1String("branch")].toString();
            if (branch.isEmpty())
                return {false, {}, QStringLiteral("'branch' required for merge.")};
            QStringList mArgs = {QStringLiteral("merge"), branch};
            const QString msg = args[QLatin1String("message")].toString();
            if (!msg.isEmpty())
                mArgs << QStringLiteral("-m") << msg;
            return runGitDirect(mArgs);
        }
        if (operation == QLatin1String("rebase")) {
            const QString branch = args[QLatin1String("branch")].toString();
            if (branch.isEmpty())
                return {false, {}, QStringLiteral("'branch' required for rebase.")};
            return runGitDirect({QStringLiteral("rebase"), branch});
        }
        if (operation == QLatin1String("pull")) {
            const QString remote = args[QLatin1String("remote")].toString(
                QStringLiteral("origin"));
            QStringList pArgs = {QStringLiteral("pull"), remote};
            const QString branch = args[QLatin1String("branch")].toString();
            if (!branch.isEmpty())
                pArgs << branch;
            return runGitDirect(pArgs);
        }
        if (operation == QLatin1String("push")) {
            const QString remote = args[QLatin1String("remote")].toString(
                QStringLiteral("origin"));
            QStringList pArgs = {QStringLiteral("push")};
            if (args[QLatin1String("setUpstream")].toBool())
                pArgs << QStringLiteral("-u");
            pArgs << remote;
            const QString branch = args[QLatin1String("branch")].toString();
            if (!branch.isEmpty())
                pArgs << branch;
            return runGitDirect(pArgs);
        }
        if (operation == QLatin1String("remote")) {
            const QString action = args[QLatin1String("action")].toString(
                QStringLiteral("list"));
            if (action == QLatin1String("show")) {
                const QString name = args[QLatin1String("name")].toString(
                    QStringLiteral("origin"));
                return runGitDirect({QStringLiteral("remote"),
                                     QStringLiteral("show"), name});
            }
            return runGitDirect({QStringLiteral("remote"), QStringLiteral("-v")});
        }
        if (operation == QLatin1String("status"))
            return runGitDirect({QStringLiteral("status"),
                                 QStringLiteral("--short"),
                                 QStringLiteral("--branch")});
        if (operation == QLatin1String("diff")) {
            QStringList dArgs = {QStringLiteral("diff")};
            if (args[QLatin1String("staged")].toBool())
                dArgs << QStringLiteral("--cached");
            const QString commitA = args[QLatin1String("commitA")].toString();
            const QString commitB = args[QLatin1String("commitB")].toString();
            if (!commitA.isEmpty() && !commitB.isEmpty())
                dArgs << QStringLiteral("%1..%2").arg(commitA, commitB);
            else if (!commitA.isEmpty())
                dArgs << commitA;
            const QString fp = args[QLatin1String("filePath")].toString();
            if (!fp.isEmpty())
                dArgs << QStringLiteral("--") << fp;
            return runGitDirect(dArgs);
        }
        return {false, {}, QStringLiteral("Unknown operation: %1").arg(operation)};
    };

    std::vector<std::unique_ptr<ITool>> tools;
    tools.push_back(std::make_unique<GitOpsTool>(std::move(executor)));
    return tools;
}

// ── AI commit message generation ─────────────────────────────────────────────

void GitPlugin::wireCommitMessageGeneration()
{
    if (!m_gitPanel || !m_git) return;

    // Step 1: GitPanel requests commit message → trigger async diff
    // GitPanel lives in this plugin DLL, so PMF connect is safe here.
    connect(m_gitPanel, &GitPanel::generateCommitMessageRequested,
            this, &GitPlugin::onGenerateCommitMessageRequested);

    // Step 2: Diff arrives → generate via AgentOrchestrator → fill commit input
    // SIGNAL/SLOT string-based connect — m_git (GitService) lives in the main
    // binary; PMF connect silently fails when SDK MOC is duplicated.
    connect(m_git, SIGNAL(diffReady(QString,QString)),
            this, SLOT(onGitDiffReady(QString,QString)));
}

void GitPlugin::onGenerateCommitMessageRequested()
{
    if (!m_git || !m_git->isGitRepo()) return;
    // Manifesto §2: never block UI thread — use async diff
    m_git->diffAsync({});
}

void GitPlugin::onGitDiffReady(const QString & /*filePath*/, const QString &diff)
{
    if (diff.trimmed().isEmpty()) return;

    auto *orchestrator = service<AgentOrchestrator>(
        QStringLiteral("agentOrchestrator"));
    if (!orchestrator || !orchestrator->activeProvider()) return;

    const QString prompt = tr("Generate a concise, conventional commit message "
                              "for these changes:\n\n```diff\n%1\n```")
                               .arg(diff.left(8000));

    AgentRequest req;
    req.requestId  = QUuid::createUuid().toString(QUuid::WithoutBraces);
    req.intent     = AgentIntent::SuggestCommitMessage;
    req.userPrompt = prompt;

    // One-shot connection: receive response and fill the commit message input.
    // SIGNAL/SLOT string-based connect — AgentOrchestrator lives in the main
    // binary; PMF connect silently fails when SDK MOC is duplicated.
    if (m_pendingCommitMsgConn)
        QObject::disconnect(m_pendingCommitMsgConn);
    m_pendingCommitMsgConn = connect(
        orchestrator,
        SIGNAL(responseFinished(QString,AgentResponse)),
        this,
        SLOT(onAgentResponseFinished(QString,AgentResponse)));

    orchestrator->sendRequest(req);
}

void GitPlugin::onAgentResponseFinished(const QString & /*requestId*/,
                                        const AgentResponse &resp)
{
    // One-shot — disconnect so subsequent responses don't refill commit msg.
    if (m_pendingCommitMsgConn) {
        QObject::disconnect(m_pendingCommitMsgConn);
        m_pendingCommitMsgConn = QMetaObject::Connection();
    }
    QString msg = resp.text.trimmed();
    // Strip any markdown code fence wrapping
    if (msg.startsWith(QLatin1String("```")))
        msg = msg.mid(msg.indexOf(QLatin1Char('\n')) + 1);
    if (msg.endsWith(QLatin1String("```")))
        msg.chop(3);
    msg = msg.trimmed();
    if (m_gitPanel)
        m_gitPanel->commitMessageInput()->setText(msg);
}

// ── Merge-conflict AI resolution ──────────────────────────────────────────────

void GitPlugin::wireConflictResolution()
{
    if (!m_gitPanel || !m_git) return;

    // GitPanel lives in this plugin DLL, so PMF connect is safe here.
    connect(m_gitPanel, &GitPanel::resolveConflictsRequested,
            this, &GitPlugin::onResolveConflictsRequested);
}

void GitPlugin::onResolveConflictsRequested()
{
    if (!m_git || !m_git->isGitRepo()) return;

    const QStringList conflicts = m_git->conflictFiles();
    if (conflicts.isEmpty()) return;

    QString prompt = tr("Resolve the following merge conflicts. "
                        "For each file, output the fully resolved content.\n\n");
    for (const QString &file : conflicts) {
        const QString content = m_git->conflictContent(file);
        prompt += QStringLiteral("### %1\n```\n%2\n```\n\n")
                      .arg(file, content.left(4000));
    }
    const QString fullPrompt = QStringLiteral("/resolve ") + prompt.left(12000);

    // Forward to ChatPanelWidget if registered
    auto *chat = service<ChatPanelWidget>(QStringLiteral("chatPanel"));
    if (chat) {
        chat->setInputText(fullPrompt);
        chat->focusInput();
    }

    // Show the AI dock so the user sees the pre-filled request
    showPanel(QStringLiteral("AIDock"));
}

// ── Inline blame annotations (GitLens-style) ──────────────────────────────────
//
// Discovery model: EditorManager is not registered as a service, so we locate
// it via QApplication::topLevelWidgets() → QMainWindow → findChild<>. The
// editorOpened signal is connected through SIGNAL/SLOT (string-matched) for
// cross-DLL safety, even though EditorManager lives in the main binary.

bool GitPlugin::inlineBlameEnabled() const
{
    QSettings s;
    // Default ON per task spec; user can flip in QSettings.
    return s.value(QStringLiteral("git/inlineBlame"), true).toBool();
}

void GitPlugin::wireInlineBlame()
{
    if (!m_git) return;

    // Locate the EditorManager.
    EditorManager *em = nullptr;
    const auto tops = QApplication::topLevelWidgets();
    for (QWidget *w : tops) {
        if (auto *mw = qobject_cast<QMainWindow*>(w)) {
            em = mw->findChild<EditorManager*>();
            if (em) break;
        }
    }
    if (!em) {
        // EditorManager not present yet — the IDE may still be bootstrapping.
        // Retry once after the current event loop pass finishes.
        QTimer::singleShot(0, this, [this]() { wireInlineBlame(); });
        return;
    }

    // Cross-DLL safe: SIGNAL/SLOT string-matched.
    connect(em, SIGNAL(editorOpened(EditorView*,QString)),
            this, SLOT(onEditorOpened(EditorView*,QString)));

    // Wire any editors already open at plugin-init time.
    if (auto *tabs = em->tabs()) {
        const int n = tabs->count();
        for (int i = 0; i < n; ++i) {
            QWidget *w = tabs->widget(i);
            if (!w) continue;
            const QString path = w->property("filePath").toString();
            if (path.isEmpty()) continue;
            if (auto *ev = qobject_cast<EditorView*>(w))
                installBlameOverlay(ev, path);
        }
    }

    // View menu toggle + Ctrl+Shift+G shortcut.
    if (auto *cmds = host() ? host()->commands() : nullptr) {
        const QString cmdId = QStringLiteral("git.toggleInlineBlame");
        cmds->registerCommand(cmdId,
                              tr("Toggle Git Blame"),
                              [this]() {
            const bool now = !inlineBlameEnabled();
            QSettings s;
            s.setValue(QStringLiteral("git/inlineBlame"), now);
            toggleInlineBlame(now);
        });
        addMenuCommand(IMenuManager::View,
                       tr("Toggle Git Blame"),
                       cmdId,
                       this,
                       QKeySequence(QStringLiteral("Ctrl+Shift+G")));
    }
}

void GitPlugin::onEditorOpened(EditorView *editor, const QString &path)
{
    if (!editor || path.isEmpty()) return;
    installBlameOverlay(editor, path);
}

void GitPlugin::installBlameOverlay(EditorView *editor, const QString &path)
{
    if (!editor || !m_git) return;
    if (m_blameOverlays.contains(editor)) {
        // Editor reused for a new file → just retarget existing overlay.
        if (auto *existing = m_blameOverlays.value(editor)) {
            existing->setFilePath(path);
            existing->setEnabled(inlineBlameEnabled());
        }
        return;
    }
    auto *overlay = new InlineBlameOverlay(editor, m_git, editor->viewport());
    overlay->setFilePath(path);
    overlay->setEnabled(inlineBlameEnabled());
    overlay->show();
    m_blameOverlays.insert(editor, overlay);
    connect(editor, &QObject::destroyed, this, [this, editor]() {
        m_blameOverlays.remove(editor);
    });
}

void GitPlugin::toggleInlineBlame(bool on)
{
    for (auto it = m_blameOverlays.begin(); it != m_blameOverlays.end(); ++it) {
        if (it.value()) it.value()->setEnabled(on);
    }
}

void GitPlugin::applyBlameEnabledFromSettings()
{
    toggleInlineBlame(inlineBlameEnabled());
}
