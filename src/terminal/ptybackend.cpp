#include "ptybackend.h"

#include <QProcess>
#include <QThread>
#include <memory>

// ═════════════════════════════════════════════════════════════════════════════
// WINDOWS — ConPTY implementation
// ═════════════════════════════════════════════════════════════════════════════
#if defined(Q_OS_WIN)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// ConPTY types — not declared in older MinGW SDK headers
#ifndef HPCON
typedef VOID *HPCON;
#endif
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#  define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016u
#endif

// ConPTY function pointer types.  Available from Windows 10 build 1903 (18362).
typedef HRESULT (WINAPI *PFN_CreatePC)(COORD, HANDLE, HANDLE, DWORD, HPCON*);
typedef HRESULT (WINAPI *PFN_ResizePC)(HPCON, COORD);
typedef void    (WINAPI *PFN_ClosePC )(HPCON);

static PFN_CreatePC gCreatePC = nullptr;
static PFN_ResizePC gResizePC = nullptr;
static PFN_ClosePC  gClosePC  = nullptr;

static bool conPtyAvailable()
{
    static int state = -1;
    if (state >= 0) return state == 1;
    HMODULE h = GetModuleHandleW(L"kernel32.dll");
    if (h) {
        gCreatePC = (PFN_CreatePC)GetProcAddress(h, "CreatePseudoConsole");
        gResizePC = (PFN_ResizePC)GetProcAddress(h, "ResizePseudoConsole");
        gClosePC  = (PFN_ClosePC )GetProcAddress(h, "ClosePseudoConsole");
    }
    state = (gCreatePC && gResizePC && gClosePC) ? 1 : 0;
    return state == 1;
}

// Reader thread: blocking ReadFile loop on the ConPTY output pipe.
// Uses QMetaObject::invokeMethod to marshal signals to the UI thread —
// no Q_OBJECT needed on this class.
class WinPtyReader : public QThread
{
public:
    HANDLE      pipe    = INVALID_HANDLE_VALUE;
    PtyBackend *backend = nullptr;

    void run() override
    {
        char  buf[4096];
        DWORD nRead = 0;
        while (ReadFile(pipe, buf, sizeof(buf), &nRead, nullptr) && nRead > 0) {
            QByteArray data(buf, (int)nRead);
            QMetaObject::invokeMethod(backend,
                [b = backend, data]() { emit b->dataReceived(data); },
                Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(backend,
            [b = backend]() { emit b->finished(0); },
            Qt::QueuedConnection);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PtyBackend — Windows
// ─────────────────────────────────────────────────────────────────────────────

PtyBackend::PtyBackend(QObject *parent) : QObject(parent) {}

PtyBackend::~PtyBackend()
{
    terminate();
    cleanup();
}

bool PtyBackend::start(const QString &program, const QStringList &args,
                       const QString &workDir, int cols, int rows)
{
    if (m_running) terminate();

    if (conPtyAvailable()) {
        // ── Create I/O pipe pair ──────────────────────────────────────────
        HANDLE hInR = INVALID_HANDLE_VALUE, hInW  = INVALID_HANDLE_VALUE;
        HANDLE hOutR= INVALID_HANDLE_VALUE, hOutW = INVALID_HANDLE_VALUE;
        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), nullptr, FALSE };

        if (!CreatePipe(&hInR, &hInW, &sa, 0))                         return false;
        if (!CreatePipe(&hOutR, &hOutW, &sa, 0))
            { CloseHandle(hInR); CloseHandle(hInW);                     return false; }

        // ── Create ConPTY ─────────────────────────────────────────────────
        COORD size = {(SHORT)qBound(1, cols, 999), (SHORT)qBound(1, rows, 999)};
        HPCON hPc  = nullptr;
        if (FAILED(gCreatePC(size, hInR, hOutW, 0, &hPc))) {
            CloseHandle(hInR);  CloseHandle(hInW);
            CloseHandle(hOutR); CloseHandle(hOutW);
            return false;
        }

        // ConPTY now owns hInR and hOutW — close our copies
        CloseHandle(hInR);
        CloseHandle(hOutW);

        // ── Build extended startup info ───────────────────────────────────
        SIZE_T attrSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
        auto *attrList = (LPPROC_THREAD_ATTRIBUTE_LIST)
                         HeapAlloc(GetProcessHeap(), 0, attrSize);
        InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize);
        UpdateProcThreadAttribute(attrList, 0,
            PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            hPc, sizeof(HPCON), nullptr, nullptr);

        STARTUPINFOEXW siEx = {};
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        siEx.lpAttributeList = attrList;

        // Build null-terminated command line
        QString cmdLine = '"' + program + '"';
        for (const QString &a : args)
            cmdLine += ' ' + (a.contains(' ') ? '"' + a + '"' : a);
        auto cmdW = cmdLine.toStdWString();
        auto wdW  = workDir.toStdWString();

        PROCESS_INFORMATION pi = {};
        BOOL ok = CreateProcessW(
            nullptr, cmdW.data(), nullptr, nullptr, FALSE,
            EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            nullptr,
            workDir.isEmpty() ? nullptr : wdW.data(),
            reinterpret_cast<LPSTARTUPINFOW>(&siEx), &pi);

        DeleteProcThreadAttributeList(attrList);
        HeapFree(GetProcessHeap(), 0, attrList);

        if (!ok) {
            gClosePC(hPc);
            CloseHandle(hInW);
            CloseHandle(hOutR);
            return false;
        }
        CloseHandle(pi.hThread);

        m_hPc      = hPc;
        m_hProcess = pi.hProcess;
        m_hPipeIn  = hInW;
        m_hPipeOut = hOutR;

        auto reader = std::make_unique<WinPtyReader>();
        reader->pipe    = (HANDLE)m_hPipeOut;
        reader->backend = this;
        reader->start();
        m_readerThread = std::move(reader);

        m_running = true;
        return true;
    }

    // ── QProcess fallback (no PTY) ────────────────────────────────────────
    m_fallback = std::make_unique<QProcess>();
    m_fallback->setProgram(program);
    m_fallback->setArguments(args);
    if (!workDir.isEmpty()) m_fallback->setWorkingDirectory(workDir);
    m_fallback->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_fallback.get(), &QProcess::readyReadStandardOutput, this, [this]() {
        emit dataReceived(m_fallback->readAllStandardOutput());
    });
    connect(m_fallback.get(),
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
        m_running = false;
        emit finished(code);
    });

    m_fallback->start();
    m_running = m_fallback->waitForStarted(5000);
    return m_running;
}

void PtyBackend::write(const QByteArray &data)
{
    if (!m_running) return;
    if (m_fallback) {
        // QProcess on Windows cannot translate \x03 into CTRL_C_EVENT.
        // Terminate the child process as the closest equivalent.
        if (data.contains('\x03')) {
            m_fallback->terminate();
            if (!m_fallback->waitForFinished(2000))
                m_fallback->kill();
            return;
        }
        m_fallback->write(data);
        return;
    }
    if (m_hPipeIn) {
        DWORD written = 0;
        WriteFile((HANDLE)m_hPipeIn, data.constData(), (DWORD)data.size(),
                  &written, nullptr);
    }
}

void PtyBackend::resize(int cols, int rows)
{
    if (m_hPc && gResizePC) {
        COORD c = {(SHORT)qBound(1, cols, 999), (SHORT)qBound(1, rows, 999)};
        gResizePC((HPCON)m_hPc, c);
    }
}

void PtyBackend::terminate()
{
    if (!m_running) return;
    m_running = false;

    if (m_fallback) {
        m_fallback->terminate();
        m_fallback->waitForFinished(3000);
        m_fallback.reset();
    }
    if (m_hProcess) {
        TerminateProcess((HANDLE)m_hProcess, 1);
    }
    if (m_readerThread) {
        m_readerThread->wait(3000);
        m_readerThread.reset();
    }
}

void PtyBackend::cleanup()
{
    if (m_hPipeIn ) { CloseHandle((HANDLE)m_hPipeIn ); m_hPipeIn  = nullptr; }
    if (m_hPipeOut) { CloseHandle((HANDLE)m_hPipeOut); m_hPipeOut = nullptr; }
    if (m_hPc     ) { gClosePC((HPCON)m_hPc);          m_hPc      = nullptr; }
    if (m_hProcess) { CloseHandle((HANDLE)m_hProcess);  m_hProcess = nullptr; }
}

// ═════════════════════════════════════════════════════════════════════════════
// POSIX — openpty / forkpty implementation
// ═════════════════════════════════════════════════════════════════════════════
#else

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#ifdef __APPLE__
#  include <util.h>
#else
#  include <pty.h>
#endif

class PosixPtyReader : public QThread
{
public:
    int         masterFd = -1;
    int         childPid = -1;
    PtyBackend *backend  = nullptr;

    void run() override
    {
        char buf[4096];
        while (true) {
            ssize_t n = ::read(masterFd, buf, sizeof(buf));
            if (n <= 0) break;
            QByteArray data(buf, (int)n);
            QMetaObject::invokeMethod(backend,
                [b = backend, data]() { emit b->dataReceived(data); },
                Qt::QueuedConnection);
        }

        // Reap child to avoid zombie
        int status = 0;
        if (childPid > 0) waitpid(childPid, &status, 0);
        int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 1;

        QMetaObject::invokeMethod(backend,
            [b = backend, exitCode]() { emit b->finished(exitCode); },
            Qt::QueuedConnection);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// PtyBackend — POSIX
// ─────────────────────────────────────────────────────────────────────────────

PtyBackend::PtyBackend(QObject *parent) : QObject(parent) {}

PtyBackend::~PtyBackend()
{
    terminate();
    cleanup();
}

bool PtyBackend::start(const QString &program, const QStringList &args,
                       const QString &workDir, int cols, int rows)
{
    if (m_running) terminate();

    struct winsize ws = {};
    ws.ws_col = (unsigned short)qBound(1, cols, 999);
    ws.ws_row = (unsigned short)qBound(1, rows, 999);

    int master = -1, slave = -1;
    if (openpty(&master, &slave, nullptr, nullptr, &ws) < 0)
        return false;

    pid_t pid = fork();
    if (pid < 0) { ::close(master); ::close(slave); return false; }

    if (pid == 0) {
        // Child process
        ::close(master);
        setsid();
        if (ioctl(slave, TIOCSCTTY, 0) < 0) _exit(1);
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        ::close(slave);

        if (!workDir.isEmpty())
            chdir(workDir.toLocal8Bit().constData());

        // Prepare argv
        QList<QByteArray> argBytes;
        argBytes.push_back(program.toLocal8Bit());
        for (const auto &a : args) argBytes.push_back(a.toLocal8Bit());

        std::vector<char*> argv;
        for (auto &b : argBytes) argv.push_back(b.data());
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(1);
    }

    // Parent
    ::close(slave);
    m_masterFd = master;
    m_childPid = pid;

    auto reader = std::make_unique<PosixPtyReader>();
    reader->masterFd = master;
    reader->childPid = pid;
    reader->backend  = this;
    reader->start();
    m_readerThread = std::move(reader);

    m_running = true;
    return true;
}

void PtyBackend::write(const QByteArray &data)
{
    if (!m_running || m_masterFd < 0) return;
    ::write(m_masterFd, data.constData(), (size_t)data.size());
}

void PtyBackend::resize(int cols, int rows)
{
    if (m_masterFd < 0) return;
    struct winsize ws = {};
    ws.ws_col = (unsigned short)qBound(1, cols, 999);
    ws.ws_row = (unsigned short)qBound(1, rows, 999);
    ioctl(m_masterFd, TIOCSWINSZ, &ws);
}

void PtyBackend::terminate()
{
    if (!m_running) return;
    m_running = false;

    if (m_childPid > 0) {
        kill(m_childPid, SIGTERM);
        m_childPid = -1;
    }
    if (m_masterFd >= 0) {
        ::close(m_masterFd);
        m_masterFd = -1;
    }
    if (m_readerThread) {
        m_readerThread->wait(3000);
        m_readerThread.reset();
    }
}

void PtyBackend::cleanup()
{
    // All handles closed in terminate()
}

#endif  // !Q_OS_WIN
