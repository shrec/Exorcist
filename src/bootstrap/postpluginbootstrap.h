#pragma once

#include <QObject>

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
};
