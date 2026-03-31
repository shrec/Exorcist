#pragma once

#include "core/imenumanager.h"
#include "core/itoolbarmanager.h"
#include "plugininterface.h"
#include "sdk/ihostservices.h"

#include <QKeySequence>
#include <QList>
#include <QString>

class QAction;
class QMenu;
class QObject;
class QWidget;
class ICommandService;
class IWorkspaceService;
class IEditorService;
class IViewService;
class INotificationService;
class IGitService;
class ITerminalService;
class IDiagnosticsService;
class ITaskService;
class IDockManager;
class IStatusBarManager;
class IWorkspaceManager;
class IProfileManager;
class IComponentService;

/// Shared base for workbench-facing plugins.
///
/// Keeps MainWindow as a shell/container while giving plugins a consistent
/// place to own their commands, menus, toolbars, and docks.
class WorkbenchPluginBase : public IPlugin
{
public:
    struct CommandSpec {
        QString text;
        QString commandId;
        QKeySequence shortcut;
        bool separatorBefore = false;
        QString iconName;    // optional: name of icon in ThemeIcons (e.g. "play")
    };

    bool initialize(IHostServices *host) final;
    void shutdown() final;

protected:
    WorkbenchPluginBase() = default;
    ~WorkbenchPluginBase() override = default;

    virtual bool initializePlugin() = 0;
    virtual void shutdownPlugin() {}

    IHostServices *host() const { return m_host; }

    ICommandService *commands() const;
    IWorkspaceService *workspace() const;
    IEditorService *editor() const;
    IViewService *views() const;
    INotificationService *notifications() const;
    IGitService *git() const;
    ITerminalService *terminal() const;
    IDiagnosticsService *diagnostics() const;
    ITaskService *tasks() const;
    IDockManager *docks() const;
    IMenuManager *menus() const;
    IToolBarManager *toolbars() const;
    IStatusBarManager *statusBar() const;
    IWorkspaceManager *workspaceManager() const;
    IProfileManager *profiles() const;
    IComponentService *components() const;

    template<typename T>
    T *service(const QString &name) const
    {
        return m_host ? m_host->service<T>(name) : nullptr;
    }

    void registerService(const QString &name, QObject *service) const;
    QObject *queryService(const QString &name) const;

    bool executeCommand(const QString &commandId) const;

    QString workspaceRoot() const;
    QString activeFilePath() const;
    QString activeLanguageId() const;
    QString selectedText() const;

    void openFile(const QString &path, int line = -1, int column = -1) const;

    void showInfo(const QString &text) const;
    void showWarning(const QString &text) const;
    void showError(const QString &text) const;
    void showStatusMessage(const QString &text, int timeoutMs = 5000) const;

    void showPanel(const QString &panelId) const;
    void hidePanel(const QString &panelId) const;
    void togglePanel(const QString &panelId) const;

    QAction *makeCommandAction(const QString &text,
                               const QString &commandId,
                               QObject *owner,
                               const QKeySequence &shortcut = {}) const;

    QAction *addMenuCommand(IMenuManager::MenuLocation location,
                            const QString &text,
                            const QString &commandId,
                            QObject *owner,
                            const QKeySequence &shortcut = {}) const;

    QAction *addMenuCommand(const QString &menuId,
                            const QString &text,
                            const QString &commandId,
                            QObject *owner,
                            const QKeySequence &shortcut = {}) const;

    void addMenuSeparator(IMenuManager::MenuLocation location) const;
    QMenu *ensureMenu(const QString &menuId, const QString &title) const;

    bool createToolBar(const QString &id,
                       const QString &title,
                       IToolBarManager::Edge edge = IToolBarManager::Top) const;
    QAction *addToolBarCommand(const QString &toolBarId,
                               const QString &text,
                               const QString &commandId,
                               QObject *owner,
                               const QKeySequence &shortcut = {}) const;
    void addToolBarCommands(const QString &toolBarId,
                            const QList<CommandSpec> &commands,
                            QObject *owner) const;
    void addToolBarWidget(const QString &id, QWidget *widget) const;
    void addToolBarSeparator(const QString &id) const;

    void addMenuCommands(IMenuManager::MenuLocation location,
                         const QList<CommandSpec> &commands,
                         QObject *owner) const;

private:
    IHostServices *m_host = nullptr;
};
