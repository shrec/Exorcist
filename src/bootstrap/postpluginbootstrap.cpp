#include "postpluginbootstrap.h"

#include <QLabel>
#include <QTimer>

#include "serviceregistry.h"
#include "sdk/hostservices.h"
#include "sdk/idebugservice.h"
#include "sdk/idebugadapter.h"
#include "sdk/isearchservice.h"
#include "sdk/ilspservice.h"
#include "agent/agentplatformbootstrap.h"
#include "agent/agentorchestrator.h"
#include "agent/inlinecompletionengine.h"
#include "agent/nexteditengine.h"
#include "agent/diagnosticsnotifier.h"
#include "aiinterface.h"
#include "bootstrap/statusbarmanager.h"
#include "editor/editormanager.h"
#include "editor/editorview.h"
#include "lsp/lspclient.h"
#include "problems/problemspanel.h"
#include "build/outputpanel.h"
#include "pluginmanager.h"

PostPluginBootstrap::PostPluginBootstrap(QObject *parent)
    : QObject(parent)
{
}

void PostPluginBootstrap::wire(const Deps &deps)
{
    auto *services    = deps.services;
    auto *editorMgr   = deps.editorMgr;

    // ── ProblemsPanel → OutputPanel ──
    if (auto *outPanel = qobject_cast<OutputPanel *>(
            services->service(QStringLiteral("outputPanel")))) {
        if (auto *pp = qobject_cast<ProblemsPanel *>(
                services->service(QStringLiteral("problemsPanel")))) {
            pp->setOutputPanel(outPanel);
        }
    }

    // ── Debug service → editor integration ──
    if (auto *debugSvc = services->service<IDebugService>(
            QStringLiteral("debugService"))) {

        // Navigate to source on stack frame double-click
        connect(debugSvc, &IDebugService::navigateToSource,
                this, [this](const QString &filePath, int line) {
            emit navigateToSource(filePath, line - 1, 0);
        });

        // Highlight current stopped line in editor
        connect(debugSvc, &IDebugService::debugStopped,
                this, [this, editorMgr](const QList<DebugFrame> &frames) {
            // Clear previous debug line marker from all open editors
            for (int i = 0; i < editorMgr->tabs()->count(); ++i) {
                auto *ed = editorMgr->editorAt(i);
                if (ed) ed->setCurrentDebugLine(0);
            }
            if (!frames.isEmpty()) {
                const auto &top = frames.first();
                if (!top.filePath.isEmpty())
                    emit navigateToSource(top.filePath, top.line - 1, 0);
                // Set the yellow current-execution-line indicator
                for (int i = 0; i < editorMgr->tabs()->count(); ++i) {
                    auto *ed = editorMgr->editorAt(i);
                    if (ed && ed->property("filePath").toString() == top.filePath) {
                        ed->setCurrentDebugLine(top.line);
                        break;
                    }
                }
            }
        });

        // Clear debug line on session end
        connect(debugSvc, &IDebugService::debugTerminated,
                this, [editorMgr]() {
            for (int i = 0; i < editorMgr->tabs()->count(); ++i) {
                auto *ed = editorMgr->editorAt(i);
                if (ed) ed->setCurrentDebugLine(0);
            }
        });

        // Sync all existing editor breakpoints into the adapter
        if (auto *adapter = services->service<IDebugAdapter>(
                QStringLiteral("debugAdapter"))) {
            for (int i = 0; i < editorMgr->tabs()->count(); ++i) {
                auto *ed = editorMgr->editorAt(i);
                if (!ed) continue;
                const QString fp = ed->property("filePath").toString();
                if (fp.isEmpty()) continue;
                for (int ln : ed->breakpointLines()) {
                    DebugBreakpoint bp;
                    bp.filePath = fp;
                    bp.line = ln;
                    adapter->addBreakpoint(bp);
                }
            }
        }
    }

    // ── Search service → navigation ──
    if (auto *srch = dynamic_cast<ISearchService *>(
            services->service(QStringLiteral("searchService")))) {
        connect(srch, &ISearchService::resultActivated,
                this, [this](const QString &filePath, int line, int col) {
            emit navigateToSource(filePath, line - 1, col);
        });
    }

    // ── LSP service → editor integration ──
    if (auto *lspSvc = services->service<ILspService>(
            QStringLiteral("lspService"))) {
        auto *lspClient = services->service<LspClient>(
            QStringLiteral("lspClient"));

        // Server ready → initialize LSP client with workspace root
        connect(lspSvc, &ILspService::serverReady,
                this, [editorMgr, lspClient]() {
            if (lspClient)
                lspClient->initialize(editorMgr->currentFolder());
        });

        // Go-to-Definition (F12)
        connect(lspSvc, &ILspService::navigateToLocation,
                this, &PostPluginBootstrap::navigateToSource);

        // Wire LSP diagnostics push to agent
        if (lspClient && deps.agentPlatform) {
            if (auto *notifier = deps.agentPlatform->diagnosticsNotifier()) {
                connect(lspClient, &LspClient::diagnosticsPublished,
                        notifier, &DiagnosticsNotifier::onDiagnosticsPublished);
            }
        }

        // Wire LSP diagnostics into the SDK DiagnosticsService
        if (lspClient && deps.hostServices) {
            connect(lspClient, &LspClient::diagnosticsPublished,
                    deps.hostServices->diagnosticsService(),
                    &DiagnosticsServiceImpl::onDiagnosticsPublished);
        }
    }

    // ── Agent platform → plugin providers ──
    if (deps.agentPlatform && deps.pluginManager)
        deps.agentPlatform->registerPluginProviders(deps.pluginManager);

    // ── Inline completion engine + NES wiring ──
    if (deps.inlineEngine && deps.agentOrchestrator) {
        for (IAgentProvider *p : deps.agentOrchestrator->providers()) {
            if (p->capabilities() & AgentCapability::InlineCompletion) {
                deps.inlineEngine->setProvider(p);
                if (deps.nesEngine)
                    deps.nesEngine->setProvider(p);
                break;
            }
        }

        // Update inline engine provider when active provider changes
        connect(deps.agentOrchestrator, &AgentOrchestrator::activeProviderChanged,
                this, [deps](const QString &) {
            auto *active = deps.agentOrchestrator->activeProvider();
            if (active && (active->capabilities() & AgentCapability::InlineCompletion)) {
                deps.inlineEngine->setProvider(active);
                if (deps.nesEngine)
                    deps.nesEngine->setProvider(active);
            }
        });
    }

    // ── AI status bar updates ──
    if (deps.agentOrchestrator && deps.statusBarMgr) {
        auto *orch = deps.agentOrchestrator;
        auto *sbm  = deps.statusBarMgr;

        // Update status bar on AI errors
        connect(orch, &AgentOrchestrator::responseError,
                this, [this, sbm, orch](const QString &, const AgentError &error) {
            switch (error.code) {
            case AgentError::Code::AuthError:
                sbm->copilotStatusLabel()->setText(tr("\u26A0 Auth Error"));
                sbm->copilotStatusLabel()->setStyleSheet(
                    QStringLiteral("padding: 0 8px; color:#f44747;"));
                sbm->copilotStatusLabel()->setToolTip(
                    tr("Authentication failed \u2014 check API key"));
                break;
            case AgentError::Code::RateLimited:
                sbm->copilotStatusLabel()->setText(tr("\u23F2 Rate Limited"));
                sbm->copilotStatusLabel()->setStyleSheet(
                    QStringLiteral("padding: 0 8px; color:#ffb74d;"));
                sbm->copilotStatusLabel()->setToolTip(
                    tr("Rate limited \u2014 waiting for cooldown"));
                QTimer::singleShot(60000, this, [sbm]() {
                    sbm->copilotStatusLabel()->setText(
                        QStringLiteral("\u2713 AI Ready"));
                    sbm->copilotStatusLabel()->setStyleSheet(
                        QStringLiteral("padding: 0 8px; color:#89d185;"));
                    sbm->copilotStatusLabel()->setToolTip(
                        QStringLiteral("AI Assistant status"));
                });
                break;
            case AgentError::Code::NetworkError:
                sbm->copilotStatusLabel()->setText(tr("\u26A0 Offline"));
                sbm->copilotStatusLabel()->setStyleSheet(
                    QStringLiteral("padding: 0 8px; color:#f44747;"));
                sbm->copilotStatusLabel()->setToolTip(
                    tr("Network error \u2014 check connection"));
                QTimer::singleShot(10000, this, [orch, sbm]() {
                    if (orch->activeProvider()
                        && orch->activeProvider()->isAvailable()) {
                        sbm->copilotStatusLabel()->setText(
                            QStringLiteral("\u2713 AI Ready"));
                        sbm->copilotStatusLabel()->setStyleSheet(
                            QStringLiteral("padding: 0 8px; color:#89d185;"));
                        sbm->copilotStatusLabel()->setToolTip(
                            QStringLiteral("AI Assistant status"));
                    }
                });
                break;
            default:
                break;
            }
        });

        // Restore status on successful response
        connect(orch, &AgentOrchestrator::responseFinished,
                this, [sbm](const QString &, const AgentResponse &) {
            if (sbm->copilotStatusLabel()->text()
                != QStringLiteral("\u2713 AI Ready")) {
                sbm->copilotStatusLabel()->setText(
                    QStringLiteral("\u2713 AI Ready"));
                sbm->copilotStatusLabel()->setStyleSheet(
                    QStringLiteral("padding: 0 8px; color:#89d185;"));
                sbm->copilotStatusLabel()->setToolTip(
                    QStringLiteral("AI Assistant status"));
            }
        });

        // Update status bar when provider becomes available
        connect(orch, &AgentOrchestrator::providerAvailabilityChanged,
                this, [sbm](bool available) {
            if (available) {
                sbm->copilotStatusLabel()->setText(
                    QStringLiteral("\u2713 AI Ready"));
                sbm->copilotStatusLabel()->setStyleSheet(
                    QStringLiteral("padding: 0 8px; color:#89d185;"));
                sbm->copilotStatusLabel()->setToolTip(
                    tr("AI Assistant \u2014 connected"));
            } else {
                sbm->copilotStatusLabel()->setText(
                    tr("\u26A0 AI Offline"));
                sbm->copilotStatusLabel()->setStyleSheet(
                    QStringLiteral("padding: 0 8px; color:#f44747;"));
                sbm->copilotStatusLabel()->setToolTip(
                    tr("AI provider not available"));
            }
        });

        // Update status bar when provider registers
        connect(orch, &AgentOrchestrator::providerRegistered,
                this, [orch, sbm](const QString &) {
            const auto *active = orch->activeProvider();
            if (active) {
                if (active->isAvailable()) {
                    sbm->copilotStatusLabel()->setText(
                        QStringLiteral("\u2713 AI Ready"));
                    sbm->copilotStatusLabel()->setStyleSheet(
                        QStringLiteral("padding: 0 8px; color:#89d185;"));
                } else {
                    sbm->copilotStatusLabel()->setText(
                        QStringLiteral("\u2026 Connecting"));
                    sbm->copilotStatusLabel()->setStyleSheet(
                        QStringLiteral("padding: 0 8px; color:#75bfff;"));
                    sbm->copilotStatusLabel()->setToolTip(
                        tr("Connecting to AI provider..."));
                }
            }
        });
    }
}
