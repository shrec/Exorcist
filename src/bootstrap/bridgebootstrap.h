#pragma once

#include <QObject>

class ProcessManager;
class BridgeClient;

// ── BridgeBootstrap ──────────────────────────────────────────────────────────
//
// Creates and wires the ExoBridge subsystem:
//
//   ProcessManager — owns local child processes, delegates shared ones
//   BridgeClient   — IPC connection to the shared ExoBridge daemon
//
// ProcessManager is registered in ServiceRegistry as "processManager".
// BridgeClient is registered as "bridgeClient".

class BridgeBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit BridgeBootstrap(QObject *parent = nullptr);

    /// Create the subsystem objects.  The optional ServiceRegistry pointer
    /// is used to register services (pass nullptr to skip).
    void initialize(QObject *serviceRegistry = nullptr);

    ProcessManager *processManager() const { return m_processManager; }
    BridgeClient   *bridgeClient()   const { return m_bridgeClient; }

signals:
    void bridgeConnected();
    void bridgeDisconnected();

private:
    ProcessManager *m_processManager = nullptr;
    BridgeClient   *m_bridgeClient   = nullptr;
};
