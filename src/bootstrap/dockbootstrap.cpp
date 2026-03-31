#include "dockbootstrap.h"

#include "../agent/chat/chatpanelwidget.h"
#include "../agent/ui/agentdashboardpanel.h"
#include "../agent/memorybrowserpanel.h"
#include "../agent/requestlogpanel.h"
#include "../agent/settingspanel.h"
#include "../agent/trajectorypanel.h"
#include "../editor/diffviewerpanel.h"
#include "../editor/proposededitpanel.h"
#include "../editor/symboloutlinepanel.h"
#include "../lsp/referencespanel.h"
#include "../mcp/mcpclient.h"
#include "../mcp/mcppanel.h"
#include "../plugin/plugingallerypanel.h"
#include "../serviceregistry.h"
#include "../ui/themegallerypanel.h"
#include "../thememanager.h"
#include "../process/bridgeclient.h"
#include "../agent/agentorchestrator.h"
#include "../sdk/ihostservices.h"

#include "../ui/dock/DockManager.h"
#include "../ui/dock/ExDockWidget.h"

#include <QLabel>
#include <QStandardPaths>

// ── Side area constants (mirrors IDockManager::Area) ─────────────────────────
static constexpr int kLeft   = 0;
static constexpr int kRight  = 1;
static constexpr int kBottom = 2;

DockBootstrap::DockBootstrap(QObject *parent)
    : QObject(parent)
{
}

void DockBootstrap::registerDock(exdock::DockManager *mgr, QWidget *parent,
                                  const QString &id, const QString &title,
                                  QWidget *widget, int area, bool startVisible)
{
    exdock::SideBarArea side = exdock::SideBarArea::Left;
    if (area == kRight)  side = exdock::SideBarArea::Right;
    if (area == kBottom) side = exdock::SideBarArea::Bottom;

    auto *dockWidget = new exdock::ExDockWidget(title, parent);
    dockWidget->setDockId(id);
    dockWidget->setContentWidget(widget);
    mgr->addDockWidget(dockWidget, side, startVisible);
    if (!startVisible)
        mgr->closeDock(dockWidget);
}

void DockBootstrap::registerLazyDock(exdock::DockManager *mgr, QWidget *parent,
                                      const QString &id, const QString &title,
                                      int area, std::function<QWidget *()> factory)
{
    exdock::SideBarArea side = exdock::SideBarArea::Left;
    if (area == kRight)  side = exdock::SideBarArea::Right;
    if (area == kBottom) side = exdock::SideBarArea::Bottom;

    // Register the dock shell with a lightweight placeholder.
    auto *dockWidget = new exdock::ExDockWidget(title, parent);
    dockWidget->setDockId(id);
    auto *placeholder = new QLabel(QString(), parent);
    dockWidget->setContentWidget(placeholder);
    mgr->addDockWidget(dockWidget, side, false);
    mgr->closeDock(dockWidget);

    // On first open, replace the placeholder with the real widget.
    connect(dockWidget, &exdock::ExDockWidget::stateChanged,
            this, [dockWidget, factory](exdock::DockState state) {
        if (state != exdock::DockState::Closed) {
            QWidget *real = factory();
            if (real && dockWidget->contentWidget() != real)
                dockWidget->setContentWidget(real);
        }
    });
}

void DockBootstrap::initialize(const Deps &deps)
{
    m_deps = deps;  // save for lazy creation

    QWidget             *parent  = deps.parent;
    ServiceRegistry     *svc     = deps.services;
    exdock::DockManager *mgr     = deps.dockManager;

    // ── Chat panel (EAGER — needs native HWND parent, see UltralightWidget) ──
    m_chatPanel = new ChatPanelWidget(deps.orchestrator, parent);
    if (svc)
        svc->registerService(QStringLiteral("chatPanel"), m_chatPanel);
    registerDock(mgr, parent, QStringLiteral("AIDock"), tr("AI"),
                 m_chatPanel, kRight);

    // ── Symbol Outline (eager — signals wired immediately in MainWindow) ──
    m_symbolPanel = new SymbolOutlinePanel(parent);
    registerDock(mgr, parent, QStringLiteral("OutlineDock"), tr("Outline"),
                 m_symbolPanel, kLeft);

    // ── References (eager — signal wired immediately in MainWindow) ───────
    m_referencesPanel = new ReferencesPanel(parent);
    registerDock(mgr, parent, QStringLiteral("ReferencesDock"), tr("References"),
                 m_referencesPanel, kBottom);

    // ── Settings (lightweight dialog widget, always created) ──────────────
    m_settingsPanel = new SettingsPanel(parent);
    m_settingsPanel->hide();

    // ── All other panels — LAZY (created on first open or first getter call)
    //    Saves ~8-10 widget constructors from the startup critical path.

    registerLazyDock(mgr, parent, QStringLiteral("AgentDashboardDock"),
                     tr("Agent Dashboard"), kRight, [this]() {
        return agentDashboard();
    });

    registerLazyDock(mgr, parent, QStringLiteral("RequestLogDock"),
                     tr("Request Log"), kBottom, [this]() {
        return requestLogPanel();
    });

    registerLazyDock(mgr, parent, QStringLiteral("TrajectoryDock"),
                     tr("Trajectory"), kBottom, [this]() {
        return trajectoryPanel();
    });

    registerLazyDock(mgr, parent, QStringLiteral("MemoryDock"),
                     tr("Memory"), kRight, [this]() {
        return memoryBrowser();
    });

    registerLazyDock(mgr, parent, QStringLiteral("ThemeDock"),
                     tr("Themes"), kLeft, [this]() {
        return themeGallery();
    });

    registerLazyDock(mgr, parent, QStringLiteral("DiffDock"),
                     tr("Diff Viewer"), kBottom, [this]() {
        return diffViewer();
    });

    registerLazyDock(mgr, parent, QStringLiteral("ProposedEditDock"),
                     tr("Proposed Edits"), kBottom, [this]() {
        return proposedEditPanel();
    });

    registerLazyDock(mgr, parent, QStringLiteral("McpDock"),
                     tr("MCP Servers"), kRight, [this]() {
        return mcpPanel();
    });

    registerLazyDock(mgr, parent, QStringLiteral("PluginDock"),
                     tr("Extensions"), kLeft, [this]() {
        return pluginGallery();
    });
}

// ── Lazy accessors — create on demand, usable before dock is opened ──────────

AgentDashboardPanel *DockBootstrap::agentDashboard()
{
    if (!m_agentDashboard) {
        m_agentDashboard = new AgentDashboardPanel(nullptr);
        if (m_deps.services)
            m_deps.services->registerService(QStringLiteral("agentDashboardPanel"),
                                             m_agentDashboard);
    }
    return m_agentDashboard;
}

SymbolOutlinePanel *DockBootstrap::symbolPanel()
{
    if (!m_symbolPanel)
        m_symbolPanel = new SymbolOutlinePanel(m_deps.parent);
    return m_symbolPanel;
}

ReferencesPanel *DockBootstrap::referencesPanel()
{
    if (!m_referencesPanel)
        m_referencesPanel = new ReferencesPanel(m_deps.parent);
    return m_referencesPanel;
}

RequestLogPanel *DockBootstrap::requestLogPanel()
{
    if (!m_requestLogPanel)
        m_requestLogPanel = new RequestLogPanel(m_deps.parent);
    return m_requestLogPanel;
}

TrajectoryPanel *DockBootstrap::trajectoryPanel()
{
    if (!m_trajectoryPanel)
        m_trajectoryPanel = new TrajectoryPanel(m_deps.parent);
    return m_trajectoryPanel;
}

MemoryBrowserPanel *DockBootstrap::memoryBrowser()
{
    if (!m_memoryBrowser) {
        const QString memPath = m_deps.memoryPath.isEmpty()
            ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
              + QStringLiteral("/memories")
            : m_deps.memoryPath;
        m_memoryBrowser = new MemoryBrowserPanel(memPath, m_deps.parent);
    }
    return m_memoryBrowser;
}

ThemeGalleryPanel *DockBootstrap::themeGallery()
{
    if (!m_themeGallery) {
        m_themeGallery = new ThemeGalleryPanel(m_deps.parent);
        if (m_deps.themeManager)
            m_themeGallery->setThemeManager(m_deps.themeManager);
    }
    return m_themeGallery;
}

DiffViewerPanel *DockBootstrap::diffViewer()
{
    if (!m_diffViewer)
        m_diffViewer = new DiffViewerPanel(m_deps.parent);
    return m_diffViewer;
}

ProposedEditPanel *DockBootstrap::proposedEditPanel()
{
    if (!m_proposedEditPanel)
        m_proposedEditPanel = new ProposedEditPanel(m_deps.parent);
    return m_proposedEditPanel;
}

McpClient *DockBootstrap::mcpClient()
{
    if (!m_mcpClient) {
        m_mcpClient = new McpClient(this);
        if (m_deps.bridgeClient)
            m_mcpClient->setBridgeClient(m_deps.bridgeClient);
    }
    return m_mcpClient;
}

McpPanel *DockBootstrap::mcpPanel()
{
    if (!m_mcpPanel)
        m_mcpPanel = new McpPanel(mcpClient(), m_deps.parent);
    return m_mcpPanel;
}

PluginGalleryPanel *DockBootstrap::pluginGallery()
{
    if (!m_pluginGallery)
        m_pluginGallery = new PluginGalleryPanel(m_deps.parent);
    return m_pluginGallery;
}
