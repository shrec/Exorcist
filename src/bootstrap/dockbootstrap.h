#pragma once

#include <QObject>
#include <QString>
#include <functional>

class ServiceRegistry;
class ThemeManager;
class BridgeClient;
class AgentOrchestrator;
class HostServices;

class ChatPanelWidget;
class AgentDashboardPanel;
class SymbolOutlinePanel;
class ReferencesPanel;
class RequestLogPanel;
class TrajectoryPanel;
class SettingsPanel;
class MemoryBrowserPanel;
class ThemeGalleryPanel;
class DiffViewerPanel;
class ProposedEditPanel;
class McpClient;
class McpPanel;
class PluginGalleryPanel;

namespace exdock { class DockManager; class ExDockWidget; }

/// DockBootstrap — creates and registers all non-central dock panels.
///
/// Most panels are created **lazily** on first open to speed up startup.
/// Only ChatPanelWidget (needs native HWND parent) and SettingsPanel
/// (lightweight dialog) are created eagerly.
///
/// Ownership: DockBootstrap is a QObject; all created panels are Qt children
/// of the parent widget passed in initialize(), so they are destroyed with it.
class DockBootstrap : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget           *parent        = nullptr;
        ServiceRegistry   *services      = nullptr;
        exdock::DockManager *dockManager = nullptr;
        ThemeManager      *themeManager  = nullptr;
        BridgeClient      *bridgeClient  = nullptr;
        AgentOrchestrator *orchestrator  = nullptr;
        HostServices      *hostServices  = nullptr;
        QString            memoryPath;
    };

    explicit DockBootstrap(QObject *parent = nullptr);

    void initialize(const Deps &deps);

    // ── Panel accessors (lazy panels created on first call) ───────────────
    ChatPanelWidget     *chatPanel()          const { return m_chatPanel; }
    AgentDashboardPanel *agentDashboard();
    SymbolOutlinePanel  *symbolPanel();
    ReferencesPanel     *referencesPanel();
    RequestLogPanel     *requestLogPanel();
    TrajectoryPanel     *trajectoryPanel();
    SettingsPanel       *settingsPanel()      const { return m_settingsPanel; }
    MemoryBrowserPanel  *memoryBrowser();
    ThemeGalleryPanel   *themeGallery();
    DiffViewerPanel     *diffViewer();
    ProposedEditPanel   *proposedEditPanel();
    McpClient           *mcpClient();
    McpPanel            *mcpPanel();
    PluginGalleryPanel  *pluginGallery();

private:
    void registerDock(exdock::DockManager *mgr, QWidget *parent,
                      const QString &id, const QString &title,
                      QWidget *widget, int area, bool startVisible = true);

    /// Register a lazy dock — creates shell only, widget built on first open.
    void registerLazyDock(exdock::DockManager *mgr, QWidget *parent,
                          const QString &id, const QString &title,
                          int area, std::function<QWidget *()> factory);

    Deps m_deps;  // saved for lazy creation

    ChatPanelWidget     *m_chatPanel         = nullptr;
    AgentDashboardPanel *m_agentDashboard    = nullptr;
    SymbolOutlinePanel  *m_symbolPanel       = nullptr;
    ReferencesPanel     *m_referencesPanel   = nullptr;
    RequestLogPanel     *m_requestLogPanel   = nullptr;
    TrajectoryPanel     *m_trajectoryPanel   = nullptr;
    SettingsPanel       *m_settingsPanel     = nullptr;
    MemoryBrowserPanel  *m_memoryBrowser     = nullptr;
    ThemeGalleryPanel   *m_themeGallery      = nullptr;
    DiffViewerPanel     *m_diffViewer        = nullptr;
    ProposedEditPanel   *m_proposedEditPanel = nullptr;
    McpClient           *m_mcpClient         = nullptr;
    McpPanel            *m_mcpPanel          = nullptr;
    PluginGalleryPanel  *m_pluginGallery     = nullptr;
};
