#pragma once

#include <QObject>
#include <QList>

#include "sdk/idebugadapter.h"

struct DebugFrame;
class ServiceRegistry;
class HostServices;
class AgentPlatformBootstrap;
class AgentOrchestrator;
class InlineCompletionEngine;
class NextEditEngine;
class StatusBarManager;
class EditorManager;
class PluginManager;

/// Wires plugin-registered services together after all plugins have initialized.
///
/// This bootstrap handles cross-service connections that can only be established
/// after plugins have registered their services (debug, LSP, search, AI, etc.).
/// It replaces the large post-plugin wiring block that was in MainWindow::loadPlugins().
class PostPluginBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit PostPluginBootstrap(QObject *parent = nullptr);

    struct Deps {
        ServiceRegistry        *services          = nullptr;
        HostServices           *hostServices      = nullptr;
        AgentPlatformBootstrap *agentPlatform     = nullptr;
        AgentOrchestrator      *agentOrchestrator = nullptr;
        InlineCompletionEngine *inlineEngine      = nullptr;
        NextEditEngine         *nesEngine          = nullptr;
        StatusBarManager       *statusBarMgr      = nullptr;
        EditorManager          *editorMgr         = nullptr;
        PluginManager          *pluginManager     = nullptr;
    };

    /// Call once after PluginManager::initializeAll() and manifest registration.
    void wire(const Deps &deps);

signals:
    /// Emitted when a service requests navigation to a source location.
    void navigateToSource(const QString &filePath, int line, int character);

private slots:
    // Cross-DLL slots for IDebugService signals (SIGNAL/SLOT string-based
    // connect is required — PMF connect silently fails across DLL boundaries).
    void onDebugNavigateToSource(const QString &filePath, int line);
    void onDebugStopped(const QList<DebugFrame> &frames);
    void onDebugTerminated();
    void onAdapterVariablesReceived(int reference, const QList<DebugVariable> &vars);

private:
    EditorManager *m_editorMgr = nullptr;
};
