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
namespace luabridge { class LuaScriptEngine; }
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
class FileWatchService;
class KeymapManager;
class ContextPruner;
class AutoCompactor;
class TestScaffold;
class AuthManager;
class RenameSuggestionWidget;
class SecureKeyStorage;
class PromptFileManager;
class MergeConflictResolver;
class InlineReviewWidget;
class ModelRegistry;
class OAuthManager;
class DomainFilter;
class CommitMessageGenerator;
class MultiChunkEditor;
class LanguageCompletionConfig;
class ContextEditor;
class TrajectoryRecorder;
class FeatureFlags;
class NotebookManager;
class ReviewManager;
class TestGenerator;
class AuthStatusIndicator;
class PromptFilePicker;
class ChatThemeAdapter;
class APIKeyManagerWidget;
class NetworkMonitor;
class CitationManager;
class MCPServerManager;
class WorkspaceChunkIndex;
class ModelConfigWidget;
class ContextInspector;
class ToolCallTrace;
class TrajectoryReplayWidget;
class PromptArchiveExporter;
class MemoryFileEditor;
class BackgroundCompactor;
class ErrorStateWidget;
class SetupTestsWizard;
class RenameIntegration;
class AccessibilityHelper;
class ToolToggleManager;
class ContextScopeConfig;
class EmbeddingIndex;
class GitignoreFilter;
class RegexSearchEngine;

class DebugPanel;
class GdbMiAdapter;
class SshConnectionManager;
class RemoteFilePanel;
class RemoteSyncService;
class ToolchainManager;
class CMakeIntegration;
class BuildToolbar;
class DebugLaunchController;
class HostServices;
class ContributionRegistry;
class LspBootstrap;
class BuildDebugBootstrap;

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
    void setupStatusBar();
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

    exdock::ExDockWidget *m_projectDock;
    exdock::ExDockWidget *m_searchDock;
    exdock::ExDockWidget *m_gitDock;
    exdock::ExDockWidget *m_terminalDock;
    exdock::ExDockWidget *m_aiDock;
    exdock::ExDockWidget *m_referencesDock;
    exdock::ExDockWidget *m_symbolDock;
    exdock::ExDockWidget *m_requestLogDock;
    exdock::ExDockWidget *m_trajectoryDock;
    exdock::ExDockWidget *m_memoryDock;
    exdock::ExDockWidget *m_mcpDock;
    exdock::ExDockWidget *m_pluginDock;
    exdock::ExDockWidget *m_themeDock;

    QLabel *m_posLabel;
    QLabel *m_encodingLabel;
    QLabel *m_backgroundLabel;   // shows active background jobs
    QLabel *m_branchLabel;
    QLabel *m_copilotStatusLabel;

    PluginGalleryPanel *m_pluginGallery = nullptr;
    ThemeGalleryPanel  *m_themeGallery = nullptr;
    std::unique_ptr<PluginManager>            m_pluginManager;
    std::unique_ptr<luabridge::LuaScriptEngine> m_luaEngine;
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
    exdock::ExDockWidget      *m_diffDock          = nullptr;
    ProposedEditPanel         *m_proposedEditPanel = nullptr;
    exdock::ExDockWidget      *m_proposedEditDock  = nullptr;
    BreadcrumbBar            *m_breadcrumb        = nullptr;
    WorkspaceIndexer         *m_workspaceIndexer = nullptr;
    SymbolIndex              *m_symbolIndex      = nullptr;
    QLabel                   *m_indexLabel = nullptr;
    OutputPanel              *m_outputPanel = nullptr;
    exdock::ExDockWidget      *m_outputDock  = nullptr;
    RunLaunchPanel           *m_runPanel    = nullptr;
    exdock::ExDockWidget      *m_runDock     = nullptr;
    FileWatchService         *m_fileWatcher = nullptr;
    ContextPruner            *m_contextPruner = nullptr;
    AutoCompactor            *m_autoCompactor = nullptr;
    TestScaffold             *m_testScaffold = nullptr;
    AuthManager              *m_authManager = nullptr;
    RenameSuggestionWidget   *m_renameSuggestion = nullptr;
    SecureKeyStorage         *m_keyStorage = nullptr;
    PromptFileManager        *m_promptFileManager = nullptr;
    MergeConflictResolver    *m_mergeResolver = nullptr;
    InlineReviewWidget       *m_inlineReview = nullptr;
    ModelRegistry            *m_modelRegistry = nullptr;
    OAuthManager             *m_oauthManager = nullptr;
    DomainFilter             *m_domainFilter = nullptr;
    CommitMessageGenerator   *m_commitMsgGen = nullptr;
    MultiChunkEditor         *m_multiChunkEditor = nullptr;
    LanguageCompletionConfig *m_langCompletionConfig = nullptr;
    ContextEditor            *m_contextEditor = nullptr;
    TrajectoryRecorder       *m_trajectoryRecorder = nullptr;
    FeatureFlags             *m_featureFlags = nullptr;
    NotebookManager          *m_notebookManager = nullptr;
    ReviewManager            *m_reviewManager = nullptr;
    TestGenerator            *m_testGenerator = nullptr;
    AuthStatusIndicator      *m_authIndicator = nullptr;
    PromptFilePicker         *m_promptPicker = nullptr;
    ChatThemeAdapter         *m_chatTheme = nullptr;
    APIKeyManagerWidget      *m_apiKeyManager = nullptr;
    NetworkMonitor           *m_networkMonitor = nullptr;
    CitationManager          *m_citationManager = nullptr;
    MCPServerManager         *m_mcpServerManager = nullptr;
    WorkspaceChunkIndex      *m_chunkIndex = nullptr;
    ModelConfigWidget        *m_modelConfig = nullptr;
    ContextInspector         *m_contextInspector = nullptr;
    ToolCallTrace            *m_toolCallTrace = nullptr;
    TrajectoryReplayWidget   *m_trajectoryReplay = nullptr;
    PromptArchiveExporter    *m_promptArchive = nullptr;
    MemoryFileEditor         *m_memoryFileEditor = nullptr;
    BackgroundCompactor      *m_bgCompactor = nullptr;
    ErrorStateWidget         *m_errorState = nullptr;
    SetupTestsWizard         *m_setupTestsWizard = nullptr;
    RenameIntegration        *m_renameIntegration = nullptr;
    AccessibilityHelper      *m_accessibilityHelper = nullptr;
    ToolToggleManager        *m_toolToggleManager = nullptr;
    ContextScopeConfig       *m_contextScopeConfig = nullptr;
    EmbeddingIndex           *m_embeddingIndex = nullptr;
    RegexSearchEngine        *m_regexSearch = nullptr;
    KeymapManager            *m_keymapManager = nullptr;
    QLabel                   *m_memoryLabel = nullptr;
    exdock::DockManager       *m_dockManager = nullptr;
    DebugPanel               *m_debugPanel = nullptr;
    exdock::ExDockWidget      *m_debugDock = nullptr;
    GdbMiAdapter             *m_debugAdapter = nullptr;

    SshConnectionManager     *m_sshManager = nullptr;
    RemoteFilePanel          *m_remotePanel = nullptr;
    exdock::ExDockWidget      *m_remoteDock = nullptr;
    RemoteSyncService        *m_syncService = nullptr;
    BuildDebugBootstrap      *m_buildDebugBootstrap = nullptr;
    ToolchainManager         *m_toolchainMgr = nullptr;
    CMakeIntegration         *m_cmakeIntegration = nullptr;
    BuildToolbar             *m_buildToolbar = nullptr;
    DebugLaunchController    *m_debugLauncher = nullptr;

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
