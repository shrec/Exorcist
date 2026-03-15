#include "hostservices.h"

#include "../mainwindow.h"
#include "../bootstrap/dockmanageradapter.h"
#include "../bootstrap/menumanagerimpl.h"
#include "../bootstrap/statusbarmanageradapter.h"
#include "../bootstrap/toolbarmanageradapter.h"
#include "../bootstrap/workspacemanagerimpl.h"
#include "../core/ifilesystem.h"
#include "../profile/profilemanager.h"
#include "../component/componentregistry.h"
#include "../component/componentserviceadapter.h"
#include "../editor/editorview.h"
#include "../git/gitservice.h"
#include "../serviceregistry.h"
#include "../terminal/terminalpanel.h"
#include "../ui/dock/DockManager.h"
#include "../ui/dock/DockToolBarManager.h"
#include "../ui/dock/ExDockWidget.h"

#include <QJsonObject>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>

// ── CommandServiceImpl ───────────────────────────────────────────────────────

CommandServiceImpl::CommandServiceImpl(QObject *parent)
    : QObject(parent)
{
}

void CommandServiceImpl::registerCommand(const QString &id, const QString &title,
                                          std::function<void()> handler)
{
    m_commands[id] = {title, std::move(handler)};
}

void CommandServiceImpl::unregisterCommand(const QString &id)
{
    m_commands.remove(id);
}

bool CommandServiceImpl::executeCommand(const QString &id)
{
    auto it = m_commands.find(id);
    if (it == m_commands.end())
        return false;
    it->handler();
    return true;
}

bool CommandServiceImpl::hasCommand(const QString &id) const
{
    return m_commands.contains(id);
}

QStringList CommandServiceImpl::commandIds() const
{
    return m_commands.keys();
}

QString CommandServiceImpl::commandTitle(const QString &id) const
{
    auto it = m_commands.find(id);
    return it != m_commands.end() ? it->title : QString();
}

// ── WorkspaceServiceImpl ─────────────────────────────────────────────────────

WorkspaceServiceImpl::WorkspaceServiceImpl(MainWindow *window, IFileSystem *fs,
                                            QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_fs(fs)
{
}

QString WorkspaceServiceImpl::rootPath() const
{
    return m_window->currentFolder();
}

QStringList WorkspaceServiceImpl::openFiles() const
{
    QStringList files;
    auto *tabs = m_window->findChild<QTabWidget *>();
    if (!tabs) return files;
    for (int i = 0; i < tabs->count(); ++i) {
        const QString path = tabs->widget(i)->property("filePath").toString();
        if (!path.isEmpty())
            files.append(path);
    }
    return files;
}

QByteArray WorkspaceServiceImpl::readFile(const QString &path) const
{
    QString error;
    const QString content = m_fs->readTextFile(path, &error);
    if (!error.isEmpty())
        return {};
    return content.toUtf8();
}

bool WorkspaceServiceImpl::writeFile(const QString &path, const QByteArray &data)
{
    QString error;
    return m_fs->writeTextFile(path, QString::fromUtf8(data), &error);
}

QStringList WorkspaceServiceImpl::listDirectory(const QString &path) const
{
    QString error;
    return m_fs->listDir(path, &error);
}

bool WorkspaceServiceImpl::exists(const QString &path) const
{
    return m_fs->exists(path);
}

// ── EditorServiceImpl ────────────────────────────────────────────────────────

EditorServiceImpl::EditorServiceImpl(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

EditorView *EditorServiceImpl::activeEditor() const
{
    return m_window->currentEditor();
}

QString EditorServiceImpl::activeFilePath() const
{
    auto *e = activeEditor();
    return e ? e->property("filePath").toString() : QString();
}

QString EditorServiceImpl::activeLanguageId() const
{
    auto *e = activeEditor();
    return e ? e->languageId() : QString();
}

QString EditorServiceImpl::selectedText() const
{
    auto *e = activeEditor();
    return e ? e->textCursor().selectedText() : QString();
}

QString EditorServiceImpl::activeDocumentText() const
{
    auto *e = activeEditor();
    return e ? e->toPlainText() : QString();
}

void EditorServiceImpl::replaceSelection(const QString &text)
{
    auto *e = activeEditor();
    if (!e) return;
    auto cursor = e->textCursor();
    cursor.insertText(text);
    e->setTextCursor(cursor);
}

void EditorServiceImpl::insertText(const QString &text)
{
    auto *e = activeEditor();
    if (!e) return;
    auto cursor = e->textCursor();
    cursor.insertText(text);
    e->setTextCursor(cursor);
}

void EditorServiceImpl::openFile(const QString &path, int line, int column)
{
    m_window->openFile(path);

    if (line > 0) {
        // Defer cursor navigation to after file is opened and painted
        QTimer::singleShot(100, m_window, [this, line, column]() {
            auto *e = activeEditor();
            if (!e) return;
            auto cursor = e->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
            if (column > 0)
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
            e->setTextCursor(cursor);
            e->centerCursor();
        });
    }
}

int EditorServiceImpl::cursorLine() const
{
    auto *e = activeEditor();
    return e ? e->textCursor().blockNumber() + 1 : 0;
}

int EditorServiceImpl::cursorColumn() const
{
    auto *e = activeEditor();
    return e ? e->textCursor().columnNumber() : 0;
}

// ── ViewServiceImpl ──────────────────────────────────────────────────────────

ViewServiceImpl::ViewServiceImpl(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

void ViewServiceImpl::registerPanel(const QString &viewId, const QString &title,
                                     QWidget *widget)
{
    m_panels[viewId] = widget;

    // Create an ExDockWidget and register with DockManager
    auto *dm = m_window->dockManager();
    if (dm) {
        auto *dock = new exdock::ExDockWidget(title, m_window);
        dock->setDockId(viewId);
        dock->setContentWidget(widget);
        dm->addDockWidget(dock, exdock::SideBarArea::Bottom, true);
        m_docks[viewId] = dock;
    } else {
        widget->setWindowTitle(title);
        widget->setObjectName(viewId);
    }
}

void ViewServiceImpl::unregisterPanel(const QString &viewId)
{
    auto *dm = m_window->dockManager();
    if (auto *dock = m_docks.value(viewId)) {
        if (dm) dm->removeDockWidget(dock);
        m_docks.remove(viewId);
    }
    m_panels.remove(viewId);
}

void ViewServiceImpl::showView(const QString &viewId)
{
    auto *dm = m_window->dockManager();
    if (auto *dock = m_docks.value(viewId)) {
        if (dm) dm->showDock(dock);
    } else if (auto *w = m_panels.value(viewId)) {
        w->show();
    }
}

void ViewServiceImpl::hideView(const QString &viewId)
{
    auto *dm = m_window->dockManager();
    if (auto *dock = m_docks.value(viewId)) {
        if (dm) dm->closeDock(dock);
    } else if (auto *w = m_panels.value(viewId)) {
        w->hide();
    }
}

bool ViewServiceImpl::isViewVisible(const QString &viewId) const
{
    if (auto *dock = m_docks.value(viewId)) {
        auto *dm = m_window->dockManager();
        return dm && dm->isPinned(dock);
    }
    if (auto *w = m_panels.value(viewId))
        return w->isVisible();
    return false;
}

// ── NotificationServiceImpl ──────────────────────────────────────────────────

NotificationServiceImpl::NotificationServiceImpl(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

void NotificationServiceImpl::info(const QString &text)
{
    QMessageBox::information(m_window, tr("Info"), text);
}

void NotificationServiceImpl::warning(const QString &text)
{
    QMessageBox::warning(m_window, tr("Warning"), text);
}

void NotificationServiceImpl::error(const QString &text)
{
    QMessageBox::critical(m_window, tr("Error"), text);
}

void NotificationServiceImpl::statusMessage(const QString &text, int timeoutMs)
{
    m_window->statusBar()->showMessage(text, timeoutMs);
}

// ── GitServiceImpl ───────────────────────────────────────────────────────────

GitServiceImpl::GitServiceImpl(GitService *git, QObject *parent)
    : QObject(parent)
    , m_git(git)
{
}

bool GitServiceImpl::isGitRepo() const
{
    return m_git && m_git->isGitRepo();
}

QString GitServiceImpl::currentBranch() const
{
    return m_git ? m_git->currentBranch() : QString();
}

QString GitServiceImpl::diff(bool staged) const
{
    if (!m_git) return {};
    // GitService::diff takes optional filePath. Empty = full diff.
    // staged support would need extension; for now return working tree diff.
    Q_UNUSED(staged)
    return m_git->diff();
}

QStringList GitServiceImpl::changedFiles() const
{
    if (!m_git) return {};
    const auto statuses = m_git->fileStatuses();
    return statuses.keys();
}

QStringList GitServiceImpl::branches() const
{
    return m_git ? m_git->localBranches() : QStringList();
}

// ── TerminalServiceImpl ──────────────────────────────────────────────────────

TerminalServiceImpl::TerminalServiceImpl(TerminalPanel *terminal, QObject *parent)
    : QObject(parent)
    , m_terminal(terminal)
{
}

void TerminalServiceImpl::runCommand(const QString &command)
{
    if (m_terminal)
        m_terminal->sendCommand(command);
}

void TerminalServiceImpl::sendInput(const QString &text)
{
    if (m_terminal)
        m_terminal->sendInput(text);
}

QString TerminalServiceImpl::recentOutput(int maxLines) const
{
    return m_terminal ? m_terminal->recentOutput(maxLines) : QString();
}

void TerminalServiceImpl::openTerminal()
{
    // TerminalPanel manages tabs internally
    if (m_terminal)
        m_terminal->sendCommand(QString());
}

// ── DiagnosticsServiceImpl ───────────────────────────────────────────────────

DiagnosticsServiceImpl::DiagnosticsServiceImpl(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
}

void DiagnosticsServiceImpl::onDiagnosticsPublished(const QString &uri,
                                                     const QJsonArray &diags)
{
    const QString filePath = QUrl(uri).toLocalFile();
    if (filePath.isEmpty()) return;

    QList<SDKDiagnostic> parsed;
    for (const QJsonValue &val : diags) {
        const QJsonObject d = val.toObject();
        SDKDiagnostic sd;
        sd.filePath = filePath;
        sd.message = d[QLatin1String("message")].toString();
        sd.source = d[QLatin1String("source")].toString();

        const QJsonObject range = d[QLatin1String("range")].toObject();
        sd.line = range[QLatin1String("start")].toObject()[QLatin1String("line")].toInt() + 1;
        sd.column = range[QLatin1String("start")].toObject()[QLatin1String("character")].toInt();

        const int severity = d[QLatin1String("severity")].toInt(1);
        switch (severity) {
        case 1: sd.severity = SDKDiagnostic::Error; break;
        case 2: sd.severity = SDKDiagnostic::Warning; break;
        case 3: sd.severity = SDKDiagnostic::Info; break;
        default: sd.severity = SDKDiagnostic::Hint; break;
        }

        parsed.append(sd);
    }

    m_cache[filePath] = parsed;
}

QList<SDKDiagnostic> DiagnosticsServiceImpl::diagnostics() const
{
    QList<SDKDiagnostic> all;
    for (auto it = m_cache.cbegin(); it != m_cache.cend(); ++it)
        all.append(it.value());
    return all;
}

QList<SDKDiagnostic> DiagnosticsServiceImpl::diagnosticsForFile(const QString &filePath) const
{
    return m_cache.value(filePath);
}

int DiagnosticsServiceImpl::errorCount() const
{
    int count = 0;
    for (auto it = m_cache.cbegin(); it != m_cache.cend(); ++it) {
        for (const SDKDiagnostic &d : it.value()) {
            if (d.severity == SDKDiagnostic::Error)
                ++count;
        }
    }
    return count;
}

int DiagnosticsServiceImpl::warningCount() const
{
    int count = 0;
    for (auto it = m_cache.cbegin(); it != m_cache.cend(); ++it) {
        for (const SDKDiagnostic &d : it.value()) {
            if (d.severity == SDKDiagnostic::Warning)
                ++count;
        }
    }
    return count;
}

// ── TaskServiceImpl ──────────────────────────────────────────────────────────

TaskServiceImpl::TaskServiceImpl(QObject *parent)
    : QObject(parent)
{
}

void TaskServiceImpl::runTask(const QString &taskId)
{
    Q_UNUSED(taskId)
    // Will be wired to build system in a future phase.
}

void TaskServiceImpl::cancelTask(const QString &taskId)
{
    Q_UNUSED(taskId)
}

bool TaskServiceImpl::isTaskRunning(const QString &taskId) const
{
    Q_UNUSED(taskId)
    return false;
}

// ── HostServices (root) ──────────────────────────────────────────────────────

HostServices::HostServices(MainWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
{
    m_commands      = std::make_unique<CommandServiceImpl>(this);
    m_editor        = std::make_unique<EditorServiceImpl>(window, this);
    m_views         = std::make_unique<ViewServiceImpl>(window, this);
    m_notifications = std::make_unique<NotificationServiceImpl>(window, this);
    m_diagnostics   = std::make_unique<DiagnosticsServiceImpl>(window, this);
    m_tasks         = std::make_unique<TaskServiceImpl>(this);
}

void HostServices::initSubsystemServices(IFileSystem *fs, GitService *git,
                                          TerminalPanel *terminal)
{
    m_workspace = std::make_unique<WorkspaceServiceImpl>(m_window, fs, this);
    m_git       = std::make_unique<GitServiceImpl>(git, this);
    m_terminal  = std::make_unique<TerminalServiceImpl>(terminal, this);
}

ICommandService *HostServices::commands()       { return m_commands.get(); }
IWorkspaceService *HostServices::workspace()    { return m_workspace.get(); }
IEditorService *HostServices::editor()          { return m_editor.get(); }
IViewService *HostServices::views()             { return m_views.get(); }
INotificationService *HostServices::notifications() { return m_notifications.get(); }
IGitService *HostServices::git()                { return m_git.get(); }
ITerminalService *HostServices::terminal()      { return m_terminal.get(); }
IDiagnosticsService *HostServices::diagnostics() { return m_diagnostics.get(); }
ITaskService *HostServices::tasks()             { return m_tasks.get(); }

IDockManager *HostServices::docks()             { return m_dockMgr.get(); }
IMenuManager *HostServices::menus()             { return m_menuMgr.get(); }
IToolBarManager *HostServices::toolbars()       { return m_toolBarMgr.get(); }
IStatusBarManager *HostServices::statusBar()    { return m_statusBarMgr.get(); }
IWorkspaceManager *HostServices::workspaceManager() { return m_workspaceMgr.get(); }
IProfileManager *HostServices::profiles()       { return m_profileMgr; }
IComponentService *HostServices::components()   { return m_componentService.get(); }

void HostServices::setComponentRegistry(ComponentRegistry *reg)
{
    m_componentRegistry = reg;
    m_componentService = std::make_unique<ComponentServiceAdapter>(reg, this);
}

void HostServices::initUIManagers()
{
    m_dockMgr    = std::make_unique<DockManagerAdapter>(m_window->dockManager(), this);
    m_menuMgr    = std::make_unique<MenuManagerImpl>(m_window, this);
    m_statusBarMgr = std::make_unique<StatusBarManagerAdapter>(m_window->statusBar(), this);
    m_workspaceMgr = std::make_unique<WorkspaceManagerImpl>(m_window, this);

    // DockToolBarManager is a standalone component — create it here.
    auto *tbMgr  = new exdock::DockToolBarManager(m_window, this);
    m_toolBarMgr = std::make_unique<ToolBarManagerAdapter>(tbMgr, this);
}

void HostServices::registerService(const QString &name, QObject *service)
{
    if (m_registry)
        m_registry->registerService(name, service);
}

QObject *HostServices::queryService(const QString &name)
{
    return m_registry ? m_registry->service(name) : nullptr;
}
