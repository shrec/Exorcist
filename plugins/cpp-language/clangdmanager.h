#pragma once

#include <QObject>
#include <QString>

#include "lsp/lsptransport.h"

class QProcess;

class ClangdManager : public QObject
{
    Q_OBJECT

public:
    explicit ClangdManager(QObject *parent = nullptr);
    ~ClangdManager() override;

    void start(const QString &workspaceRoot, const QStringList &extraArgs = {});
    void stop();

    /// Stop clangd and restart it after a short delay.
    void restart();

    bool isRunning() const;
    QString workspaceRoot() const { return m_workspaceRoot; }
    QString compileCommandsDir() const;
    LspTransport *transport() const { return m_transport; }

signals:
    void serverReady();
    void serverStopped();
    void serverCrashed();
    void messageReceived(const LspMessage &message);

private:
    /// Find the directory containing compile_commands.json.
    /// Returns empty string if not found.
    QString findCompileCommandsDir(const QString &workspaceRoot) const;

    QProcess     *m_process;
    QString       m_workspaceRoot;
    LspTransport *m_transport;
};
