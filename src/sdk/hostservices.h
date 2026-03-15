#pragma once

#include "sdk/ihostservices.h"
#include "sdk/icommandservice.h"
#include "sdk/iworkspaceservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/iviewservice.h"
#include "sdk/inotificationservice.h"
#include "sdk/igitservice.h"
#include "sdk/iterminalservice.h"
#include "sdk/idiagnosticsservice.h"
#include "sdk/itaskservice.h"

#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class MainWindow;
class EditorView;
class ServiceRegistry;

namespace exdock { class ExDockWidget; }

// ── Concrete SDK Implementations ─────────────────────────────────────────────
//
// These classes bridge the stable SDK interfaces to the real IDE subsystems.
// They live in the core executable and must never be exposed to plugins.
// Plugins only see the I* interfaces.

// ── Command Service ──────────────────────────────────────────────────────────

class CommandServiceImpl : public QObject, public ICommandService
{
    Q_OBJECT

public:
    explicit CommandServiceImpl(QObject *parent = nullptr);

    void registerCommand(const QString &id, const QString &title,
                         std::function<void()> handler) override;
    void unregisterCommand(const QString &id) override;
    bool executeCommand(const QString &id) override;
    bool hasCommand(const QString &id) const override;

    QStringList commandIds() const;
    QString commandTitle(const QString &id) const;

private:
    struct CommandEntry {
        QString title;
        std::function<void()> handler;
    };
    QHash<QString, CommandEntry> m_commands;
};

// ── Workspace Service ────────────────────────────────────────────────────────

class IFileSystem;

class WorkspaceServiceImpl : public QObject, public IWorkspaceService
{
    Q_OBJECT

public:
    explicit WorkspaceServiceImpl(MainWindow *window, IFileSystem *fs,
                                  QObject *parent = nullptr);

    QString rootPath() const override;
    QStringList openFiles() const override;
    QByteArray readFile(const QString &path) const override;
    bool writeFile(const QString &path, const QByteArray &data) override;
    QStringList listDirectory(const QString &path) const override;
    bool exists(const QString &path) const override;

private:
    MainWindow *m_window;
    IFileSystem *m_fs;
};

// ── Editor Service ───────────────────────────────────────────────────────────

class EditorServiceImpl : public QObject, public IEditorService
{
    Q_OBJECT

public:
    explicit EditorServiceImpl(MainWindow *window, QObject *parent = nullptr);

    QString activeFilePath() const override;
    QString activeLanguageId() const override;
    QString selectedText() const override;
    QString activeDocumentText() const override;
    void replaceSelection(const QString &text) override;
    void insertText(const QString &text) override;
    void openFile(const QString &path, int line, int column) override;
    int cursorLine() const override;
    int cursorColumn() const override;

private:
    EditorView *activeEditor() const;
    MainWindow *m_window;
};

// ── View Service ─────────────────────────────────────────────────────────────

class ViewServiceImpl : public QObject, public IViewService
{
    Q_OBJECT

public:
    explicit ViewServiceImpl(MainWindow *window, QObject *parent = nullptr);

    void registerPanel(const QString &viewId, const QString &title,
                       QWidget *widget) override;
    void unregisterPanel(const QString &viewId) override;
    void showView(const QString &viewId) override;
    void hideView(const QString &viewId) override;
    bool isViewVisible(const QString &viewId) const override;

private:
    MainWindow *m_window;
    QHash<QString, QWidget *> m_panels;
    QHash<QString, exdock::ExDockWidget *> m_docks;
};

// ── Notification Service ─────────────────────────────────────────────────────

class NotificationServiceImpl : public QObject, public INotificationService
{
    Q_OBJECT

public:
    explicit NotificationServiceImpl(MainWindow *window, QObject *parent = nullptr);

    void info(const QString &text) override;
    void warning(const QString &text) override;
    void error(const QString &text) override;
    void statusMessage(const QString &text, int timeoutMs) override;

private:
    MainWindow *m_window;
};

// ── Git Service ──────────────────────────────────────────────────────────────

class GitService;

class GitServiceImpl : public QObject, public IGitService
{
    Q_OBJECT

public:
    explicit GitServiceImpl(GitService *git, QObject *parent = nullptr);

    bool isGitRepo() const override;
    QString currentBranch() const override;
    QString diff(bool staged) const override;
    QStringList changedFiles() const override;
    QStringList branches() const override;

private:
    GitService *m_git;
};

// ── Terminal Service ─────────────────────────────────────────────────────────

class TerminalPanel;

class TerminalServiceImpl : public QObject, public ITerminalService
{
    Q_OBJECT

public:
    explicit TerminalServiceImpl(TerminalPanel *terminal, QObject *parent = nullptr);

    void runCommand(const QString &command) override;
    void sendInput(const QString &text) override;
    QString recentOutput(int maxLines) const override;
    void openTerminal() override;

private:
    TerminalPanel *m_terminal;
};

// ── Diagnostics Service ──────────────────────────────────────────────────────

class DiagnosticsServiceImpl : public QObject, public IDiagnosticsService
{
    Q_OBJECT

public:
    explicit DiagnosticsServiceImpl(MainWindow *window, QObject *parent = nullptr);

    QList<SDKDiagnostic> diagnostics() const override;
    QList<SDKDiagnostic> diagnosticsForFile(const QString &filePath) const override;
    int errorCount() const override;
    int warningCount() const override;

public slots:
    void onDiagnosticsPublished(const QString &uri, const QJsonArray &diags);

private:
    MainWindow *m_window;
    QHash<QString, QList<SDKDiagnostic>> m_cache;
};

// ── Task Service ─────────────────────────────────────────────────────────────

class TaskServiceImpl : public QObject, public ITaskService
{
    Q_OBJECT

public:
    explicit TaskServiceImpl(QObject *parent = nullptr);

    void runTask(const QString &taskId) override;
    void cancelTask(const QString &taskId) override;
    bool isTaskRunning(const QString &taskId) const override;
};

// ── Host Services (root) ─────────────────────────────────────────────────────

class HostServices : public QObject, public IHostServices
{
    Q_OBJECT

public:
    explicit HostServices(MainWindow *window, QObject *parent = nullptr);

    /// Deferred initialization — called after MainWindow subsystems are created.
    void initSubsystemServices(IFileSystem *fs, GitService *git, TerminalPanel *terminal);

    /// Set the ServiceRegistry instance for dynamic service registration.
    void setServiceRegistry(ServiceRegistry *reg) { m_registry = reg; }

    ICommandService *commands() override;
    IWorkspaceService *workspace() override;
    IEditorService *editor() override;
    IViewService *views() override;
    INotificationService *notifications() override;
    IGitService *git() override;
    ITerminalService *terminal() override;
    IDiagnosticsService *diagnostics() override;
    ITaskService *tasks() override;

    IDockManager *docks() override;
    IMenuManager *menus() override;
    IToolBarManager *toolbars() override;
    IStatusBarManager *statusBar() override;
    IWorkspaceManager *workspaceManager() override;

    void registerService(const QString &name, QObject *service) override;
    QObject *queryService(const QString &name) override;

    /// Access the command service for command palette integration.
    CommandServiceImpl *commandService() { return m_commands.get(); }

    /// Access the diagnostics service for LSP signal wiring.
    DiagnosticsServiceImpl *diagnosticsService() { return m_diagnostics.get(); }

    /// Set UI manager instances (called by MainWindow after managers are created).
    void setDockManager(IDockManager *dm)         { m_dockMgr = dm; }
    void setMenuManager(IMenuManager *mm)          { m_menuMgr = mm; }
    void setToolBarManager(IToolBarManager *tbm)    { m_toolBarMgr = tbm; }
    void setStatusBarManager(IStatusBarManager *sbm) { m_statusBarMgr = sbm; }
    void setWorkspaceManager(IWorkspaceManager *wm)  { m_workspaceMgr = wm; }

private:
    MainWindow *m_window;
    ServiceRegistry *m_registry = nullptr;
    std::unique_ptr<CommandServiceImpl>       m_commands;
    std::unique_ptr<WorkspaceServiceImpl>     m_workspace;
    std::unique_ptr<EditorServiceImpl>        m_editor;
    std::unique_ptr<ViewServiceImpl>          m_views;
    std::unique_ptr<NotificationServiceImpl>  m_notifications;
    std::unique_ptr<GitServiceImpl>           m_git;
    std::unique_ptr<TerminalServiceImpl>      m_terminal;
    std::unique_ptr<DiagnosticsServiceImpl>   m_diagnostics;
    std::unique_ptr<TaskServiceImpl>          m_tasks;

    // UI manager pointers (non-owning — owned by MainWindow bootstraps)
    IDockManager      *m_dockMgr      = nullptr;
    IMenuManager      *m_menuMgr      = nullptr;
    IToolBarManager   *m_toolBarMgr   = nullptr;
    IStatusBarManager *m_statusBarMgr = nullptr;
    IWorkspaceManager *m_workspaceMgr = nullptr;
};
