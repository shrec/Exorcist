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
#include "component/outputpanel.h"
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
    // Use SIGNAL/SLOT string-based connect — PMF connect silently fails
    // across DLL boundaries (IDebugService MOC is in both exe and libdebug.dll).
    if (auto *debugSvc = services->service<IDebugService>(
            QStringLiteral("debugService"))) {
        m_editorMgr = editorMgr;

        connect(debugSvc, SIGNAL(navigateToSource(QString,int)),
                this, SLOT(onDebugNavigateToSource(QString,int)));
        connect(debugSvc, SIGNAL(debugStopped(QList<DebugFrame>)),
                this, SLOT(onDebugStopped(QList<DebugFrame>)));
        connect(debugSvc, SIGNAL(debugTerminated()),
                this, SLOT(onDebugTerminated()));

        // Editor breakpoint sync moved to plugins/debug/ per rules L1+L5.
        // Variables snapshots → editor tooltip stays here (it's an agent /
        // editor concern bridged by this bootstrap, not a plugin-owned
        // signal route).
        if (auto *adapter = services->service<IDebugAdapter>(
                QStringLiteral("debugAdapter"))) {
            connect(adapter, SIGNAL(variablesReceived(int,QList<DebugVariable>)),
                    this, SLOT(onAdapterVariablesReceived(int,QList<DebugVariable>)));
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

    // \u2500\u2500 Build / debug status indicator \u2500\u2500
    // Subscribe to IBuildSystem + IDebugService/IDebugAdapter signals so the
    // compact status label on the bottom-left shows current build/debug state
    // (Ready / Building.../ Build failed / Debugging / Paused).
    if (deps.statusBarMgr && services)
        deps.statusBarMgr->wireBuildDebugStatus(services);
}

// ── Cross-DLL debug service slots ────────────────────────────────────────────

void PostPluginBootstrap::onDebugNavigateToSource(const QString &filePath, int line)
{
    emit navigateToSource(filePath, line - 1, 0);
}

void PostPluginBootstrap::onDebugStopped(const QList<DebugFrame> &frames)
{
    if (!m_editorMgr) return;

    // Clear previous debug line marker from all open editors
    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        auto *ed = m_editorMgr->editorAt(i);
        if (ed) ed->setCurrentDebugLine(0);
    }
    if (frames.isEmpty()) return;

    const auto &top = frames.first();
    if (top.filePath.isEmpty()) return;

    // navigateToSource is queued/asynchronous: it triggers MainWindow to
    // open the file in a new tab. We must wait until that's done before
    // searching for the editor that holds the file. Defer the search +
    // setCurrentDebugLine to the next event loop iteration so the file
    // open completes first.
    emit navigateToSource(top.filePath, top.line - 1, 0);

    const QString targetPath = top.filePath;
    const int     targetLine = top.line;
    QTimer::singleShot(0, this, [this, targetPath, targetLine]() {
        if (!m_editorMgr) return;
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *ed = m_editorMgr->editorAt(i);
            if (ed && ed->property("filePath").toString() == targetPath) {
                ed->setCurrentDebugLine(targetLine);
                return;
            }
        }
    });
}

void PostPluginBootstrap::onDebugTerminated()
{
    if (!m_editorMgr) return;
    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        auto *ed = m_editorMgr->editorAt(i);
        if (ed) {
            ed->setCurrentDebugLine(0);
            ed->clearLocalsSnapshot();
        }
    }
}

void PostPluginBootstrap::onAdapterVariablesReceived(
    int /*reference*/, const QList<DebugVariable> &vars)
{
    QHash<QString, QString> snap;
    for (const DebugVariable &v : vars) {
        if (v.name.isEmpty()) continue;
        snap.insert(v.name,
                    v.type.isEmpty() ? v.value
                                     : v.type + QStringLiteral(" = ") + v.value);
    }
    if (!m_editorMgr) return;
    for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
        auto *ed = m_editorMgr->editorAt(i);
        if (ed) ed->setLocalsSnapshot(snap);
    }
}
