#pragma once

#include <QObject>
#include <QString>
#include <QList>

class GdbMiAdapter;
class DebugPanel;

struct DebugFrame;
enum class DebugStopReason;

class DebugBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit DebugBootstrap(QObject *parent = nullptr);

    void initialize();

    /// Wire debug-only connections (adapter ↔ panel).
    void wireConnections();

    GdbMiAdapter *debugAdapter() const  { return m_debugAdapter; }
    DebugPanel   *debugPanel() const    { return m_debugPanel; }

signals:
    void navigateToSource(const QString &filePath, int line);
    void debugStopped(const QList<DebugFrame> &frames);
    void debugTerminated();

private:
    GdbMiAdapter *m_debugAdapter = nullptr;
    DebugPanel   *m_debugPanel   = nullptr;
};
