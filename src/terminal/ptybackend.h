#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QThread;
class QProcess;

// ── PtyBackend ────────────────────────────────────────────────────────────────
// Cross-platform PTY backend.
//
//   Windows 10 1903+  → ConPTY via CreatePseudoConsole (dynamically loaded)
//   Windows fallback  → QProcess (merged stdout/stderr; no cursor addressing)
//   Linux / macOS     → POSIX openpty + forkpty
//
// Thread model:
//   • start / write / resize / terminate — call from UI thread
//   • dataReceived signal — emitted on UI thread via QMetaObject::invokeMethod
//     (safe to connect with default Auto connection)
class PtyBackend : public QObject
{
    Q_OBJECT

public:
    explicit PtyBackend(QObject *parent = nullptr);
    ~PtyBackend() override;

    bool start(const QString &program, const QStringList &args,
               const QString &workDir);
    void write(const QByteArray &data);
    void resize(int cols, int rows);
    void terminate();
    bool isRunning() const { return m_running; }

signals:
    void dataReceived(const QByteArray &data);
    void finished(int exitCode);

private:
    void cleanup();

    QThread  *m_readerThread = nullptr;
    QProcess *m_fallback     = nullptr;
    bool      m_running      = false;

#ifdef _WIN32
    void *m_hPc      = nullptr;  // HPCON  – ConPTY handle
    void *m_hProcess = nullptr;  // HANDLE – child process
    void *m_hPipeIn  = nullptr;  // HANDLE – write end → child stdin
    void *m_hPipeOut = nullptr;  // HANDLE – read  end ← child stdout
#else
    int m_masterFd = -1;
    int m_childPid = -1;
#endif
};
