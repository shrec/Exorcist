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

void DockBootstrap::initialize(const Deps &deps)
{
    QWidget             *parent  = deps.parent;
    ServiceRegistry     *svc     = deps.services;
    exdock::DockManager *mgr     = deps.dockManager;

    // ── Chat panel ────────────────────────────────────────────────────────
    m_chatPanel = new ChatPanelWidget(deps.orchestrator, nullptr);
    if (svc)
        svc->registerService(QStringLiteral("chatPanel"), m_chatPanel);
    registerDock(mgr, parent, QStringLiteral("AIDock"), tr("AI"),
                 m_chatPanel, kRight);

    // ── Agent Dashboard ───────────────────────────────────────────────────
    m_agentDashboard = new AgentDashboardPanel(nullptr);
    if (svc)
        svc->registerService(QStringLiteral("agentDashboardPanel"), m_agentDashboard);
    registerDock(mgr, parent, QStringLiteral("AgentDashboardDock"),
                 tr("Agent Dashboard"), m_agentDashboard, kRight);

    // ── Symbol Outline ────────────────────────────────────────────────────
    m_symbolPanel = new SymbolOutlinePanel(parent);
    registerDock(mgr, parent, QStringLiteral("OutlineDock"), tr("Outline"),
                 m_symbolPanel, kLeft);

    // ── References ────────────────────────────────────────────────────────
    m_referencesPanel = new ReferencesPanel(parent);
    registerDock(mgr, parent, QStringLiteral("ReferencesDock"), tr("References"),
                 m_referencesPanel, kBottom);

    // ── Request Log ───────────────────────────────────────────────────────
    m_requestLogPanel = new RequestLogPanel(parent);
    registerDock(mgr, parent, QStringLiteral("RequestLogDock"), tr("Request Log"),
                 m_requestLogPanel, kBottom);

    // ── Trajectory Viewer ─────────────────────────────────────────────────
    m_trajectoryPanel = new TrajectoryPanel(parent);
    registerDock(mgr, parent, QStringLiteral("TrajectoryDock"), tr("Trajectory"),
                 m_trajectoryPanel, kBottom);

    // ── Settings (used as dialog widget, not directly docked) ────────────
    m_settingsPanel = new SettingsPanel(parent);
    m_settingsPanel->hide();

    // ── Memory Browser ────────────────────────────────────────────────────
    const QString memPath = deps.memoryPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
          + QStringLiteral("/memories")
        : deps.memoryPath;
    m_memoryBrowser = new MemoryBrowserPanel(memPath, parent);
    registerDock(mgr, parent, QStringLiteral("MemoryDock"), tr("Memory"),
                 m_memoryBrowser, kRight);

    // ── Theme Gallery ─────────────────────────────────────────────────────
    m_themeGallery = new ThemeGalleryPanel(parent);
    if (deps.themeManager)
        m_themeGallery->setThemeManager(deps.themeManager);
    registerDock(mgr, parent, QStringLiteral("ThemeDock"), tr("Themes"),
                 m_themeGallery, kLeft);

    // ── Diff Viewer ───────────────────────────────────────────────────────
    m_diffViewer = new DiffViewerPanel(parent);
    registerDock(mgr, parent, QStringLiteral("DiffDock"), tr("Diff Viewer"),
                 m_diffViewer, kBottom);

    // ── Proposed Edit ─────────────────────────────────────────────────────
    m_proposedEditPanel = new ProposedEditPanel(parent);
    registerDock(mgr, parent, QStringLiteral("ProposedEditDock"),
                 tr("Proposed Edits"), m_proposedEditPanel, kBottom);

    // ── MCP Client + Panel ────────────────────────────────────────────────
    m_mcpClient = new McpClient(this);
    if (deps.bridgeClient)
        m_mcpClient->setBridgeClient(deps.bridgeClient);
    m_mcpPanel = new McpPanel(m_mcpClient, parent);
    registerDock(mgr, parent, QStringLiteral("McpDock"), tr("MCP Servers"),
                 m_mcpPanel, kRight);

    // ── Plugin Gallery ────────────────────────────────────────────────────
    m_pluginGallery = new PluginGalleryPanel(parent);
    registerDock(mgr, parent, QStringLiteral("PluginDock"), tr("Extensions"),
                 m_pluginGallery, kLeft);
}
