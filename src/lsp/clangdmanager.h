#pragma once

#include <QObject>
#include <QString>

#include "lsptransport.h"

class QProcess;

class ClangdManager : public QObject
{
    Q_OBJECT

public:
    explicit ClangdManager(QObject *parent = nullptr);
    ~ClangdManager() override;

    void start(const QString &workspaceRoot, const QStringList &args = {});
    void stop();

    LspTransport *transport() const { return m_transport; }

signals:
    void serverReady();
    void serverStopped();
    void serverCrashed();
    void messageReceived(const LspMessage &message);

private:
    QProcess *m_process;
    QString m_workspaceRoot;
    LspTransport *m_transport;
};
