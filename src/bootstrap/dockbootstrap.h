#pragma once

#include <QObject>
#include <QString>

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

namespace exdock { class DockManager; }

/// DockBootstrap — creates and registers all non-central dock panels.
///
/// Ownership: DockBootstrap is a QObject; all created panels are Qt children
/// of the parent widget passed in initialize(), so they are destroyed with it.
///
/// Signal wiring that accesses MainWindow state (editor tabs, currentEditor)
/// stays in MainWindow — use the panel getters below after initialize().
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

    // ── Panel accessors (valid after initialize()) ────────────────────────
    ChatPanelWidget     *chatPanel()          const { return m_chatPanel; }
    AgentDashboardPanel *agentDashboard()     const { return m_agentDashboard; }
    SymbolOutlinePanel  *symbolPanel()        const { return m_symbolPanel; }
    ReferencesPanel     *referencesPanel()    const { return m_referencesPanel; }
    RequestLogPanel     *requestLogPanel()    const { return m_requestLogPanel; }
    TrajectoryPanel     *trajectoryPanel()    const { return m_trajectoryPanel; }
    SettingsPanel       *settingsPanel()      const { return m_settingsPanel; }
    MemoryBrowserPanel  *memoryBrowser()      const { return m_memoryBrowser; }
    ThemeGalleryPanel   *themeGallery()       const { return m_themeGallery; }
    DiffViewerPanel     *diffViewer()         const { return m_diffViewer; }
    ProposedEditPanel   *proposedEditPanel()  const { return m_proposedEditPanel; }
    McpClient           *mcpClient()          const { return m_mcpClient; }
    McpPanel            *mcpPanel()           const { return m_mcpPanel; }
    PluginGalleryPanel  *pluginGallery()      const { return m_pluginGallery; }

private:
    void registerDock(exdock::DockManager *mgr, QWidget *parent,
                      const QString &id, const QString &title,
                      QWidget *widget, int area, bool startVisible = true);

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
