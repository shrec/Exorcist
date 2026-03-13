#pragma once

#include <QCloseEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMetaObject>
#include <QModelIndex>
#include <memory>

#include "core/ifilesystem.h"
#include "pluginmanager.h"
#include "serviceregistry.h"
#include "agent/tools/buildtools.h"

namespace exdock { class ExDockWidget; class DockManager; }
class QDialog;
class QTabWidget;
class QTreeView;
class EditorView;
class SearchService;
class SearchPanel;
class ProjectManager;
class SolutionTreeModel;
class GitService;
class GitPanel;
class AgentOrchestrator;
class ChatPanelWidget;
class AgentController;
class SessionStore;
class PromptVariableResolver;
class ToolRegistry;
class ContextBuilder;
class AgentPlatformBootstrap;
class ClangdManager;
class LspClient;
class ReferencesPanel;
class SymbolOutlinePanel;
class TerminalPanel;
class InlineCompletionEngine;
class NextEditEngine;
class WorkspaceIndexer;
class SymbolIndex;
class InlineChatWidget;
class QuickChatDialog;
class RequestLogPanel;
class TrajectoryPanel;
class SettingsPanel;
class MemoryBrowserPanel;
class McpClient;
class McpPanel;
class PluginGalleryPanel;
class ThemeManager;
class ThemeGalleryPanel;
class DiffViewerPanel;
class ProposedEditPanel;
class BreadcrumbBar;
class OutputPanel;
class RunLaunchPanel;
class DebugBootstrap;
class FileWatchService;
class KeymapManager;
class SecureKeyStorage;
class ModelRegistry;
class NetworkMonitor;
class WorkspaceChunkIndex;

class DebugPanel;
class GdbMiAdapter;
class HostServices;
class ContributionRegistry;
class LspBootstrap;
class StatusBarManager;
class AIServicesBootstrap;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    // Call after show() to defer heavy initialization (plugins, last folder).
    void deferredInit();

    /// Current workspace root folder (used by SDK services).
    QString currentFolder() const { return m_currentFolder; }

    /// Open a file in the editor (used by SDK EditorService).
    void openFile(const QString &path);

    /// Get the currently active editor (used by SDK EditorService).
    EditorView *currentEditor() const;

    /// Access the dock manager (used by ContributionRegistry).
    exdock::DockManager *dockManager() const { return m_dockManager; }

private:
    void setupUi();
    void setupMenus();
    void setupToolBar();
    void createDockWidgets();
    void openNewTab();
    void openFileFromIndex(const QModelIndex &index);
    void openFolder(const QString &path = {});
    void newSolution();
    void openSolutionFile();
    void addProjectToSolution();
    void saveCurrentTab();
    void showFilePalette();
    void showSymbolPalette();
    void showCommandPalette();
    void updateEditorStatus(EditorView *editor);
    void onTabChanged(int index);
    void loadPlugins();
    void loadSettings();
    void saveSettings();
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showTabContextMenu(int tabIndex, const QPoint &globalPos);

    QTreeView        *m_fileTree;
    QTabWidget       *m_tabs;
    SearchPanel      *m_searchPanel;
    ProjectManager   *m_projectManager;
    SolutionTreeModel *m_treeModel;
    GitService       *m_gitService;
    GitPanel         *m_gitPanel;

    /// Shorthand for m_dockManager->dockWidget(id).
    exdock::ExDockWidget *dock(const QString &id) const;

    StatusBarManager *m_statusBarMgr = nullptr;

    PluginGalleryPanel *m_pluginGallery = nullptr;
    ThemeGalleryPanel  *m_themeGallery = nullptr;
    std::unique_ptr<PluginManager>            m_pluginManager;
    std::unique_ptr<ServiceRegistry>            m_services;
    std::unique_ptr<IFileSystem>     m_fileSystem;
    SearchService    *m_searchService;
    AgentOrchestrator *m_agentOrchestrator;
    ChatPanelWidget  *m_chatPanel;
    AgentController  *m_agentController;
    SessionStore     *m_sessionStore;
    PromptVariableResolver *m_promptResolver;
    ToolRegistry     *m_toolRegistry;
    ContextBuilder   *m_contextBuilder;
    AgentPlatformBootstrap *m_agentPlatform = nullptr;
    ReferencesPanel  *m_referencesPanel;
    SymbolOutlinePanel *m_symbolPanel;
    TerminalPanel   *m_terminal;
    LspBootstrap     *m_lspBootstrap = nullptr;
    ClangdManager    *m_clangd;
    LspClient        *m_lspClient;
    QString           m_currentFolder;
    QStringList       m_includePaths;       // from compile_commands.json
    QMetaObject::Connection m_cursorConn;   // tracks current editor cursor signal
    InlineCompletionEngine *m_inlineEngine;
    InlineChatWidget       *m_inlineChat = nullptr;
    QuickChatDialog         *m_quickChat  = nullptr;
    RequestLogPanel          *m_requestLogPanel;
    TrajectoryPanel          *m_trajectoryPanel;
    SettingsPanel            *m_settingsPanel;
    QDialog                  *m_settingsDialog = nullptr;
    MemoryBrowserPanel       *m_memoryBrowser;
    class NextEditEngine     *m_nesEngine      = nullptr;
    McpClient               *m_mcpClient         = nullptr;
    McpPanel                *m_mcpPanel          = nullptr;
    ThemeManager             *m_themeManager      = nullptr;
    DiffViewerPanel          *m_diffViewer        = nullptr;
    ProposedEditPanel         *m_proposedEditPanel = nullptr;
    BreadcrumbBar            *m_breadcrumb        = nullptr;
    WorkspaceIndexer         *m_workspaceIndexer = nullptr;
    SymbolIndex              *m_symbolIndex      = nullptr;
    FileWatchService         *m_fileWatcher = nullptr;
    AIServicesBootstrap      *m_aiServices = nullptr;
    KeymapManager            *m_keymapManager = nullptr;
    exdock::DockManager       *m_dockManager = nullptr;
    DebugPanel               *m_debugPanel = nullptr;
    GdbMiAdapter             *m_debugAdapter = nullptr;

    DebugBootstrap           *m_debugBootstrap = nullptr;

    RunTestsTool::TestResult  m_lastTestResult;  // cached from last run_tests invocation

    HostServices             *m_hostServices = nullptr;
    ContributionRegistry     *m_contributions = nullptr;

    void updateDiffRanges(EditorView *editor);
    void onLspInitialized();
    void showInlineChat(EditorView *editor, const QString &selectedText,
                        const QString &filePath, const QString &languageId);
    void createLspBridge(EditorView *editor, const QString &path);
    void navigateToLocation(const QString &path, int line, int character);
    void applyWorkspaceEdit(const QJsonObject &workspaceEdit);
    void pushBuildDiagnostics();
    void onTreeContextMenu(const QPoint &pos);
};
