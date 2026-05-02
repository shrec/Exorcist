#include "clangdmanager.h"

#include <QFile>
#include <QProcess>
#include <QTimer>

#include "lsp/processlsptransport.h"

ClangdManager::ClangdManager(QObject *parent)
    : QObject(parent),
      m_process(new QProcess(this)),
      m_transport(new ProcessLspTransport(m_process, this))
{
    connect(m_process, &QProcess::started, this, &ClangdManager::serverReady);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            emit serverStopped();  // clangd not found — treat as clean stop
        else
            emit serverCrashed();
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        if (status == QProcess::CrashExit || exitCode != 0)
            emit serverCrashed();
        else
            emit serverStopped();
    });
    // String-based SIGNAL/SLOT for cross-DLL connect: LspTransport's MOC
    // lives in src/lsp/ (exorcist binary), this plugin DLL has its own copy
    // when statically linked.  PMF connect silently fails in that case.
    connect(m_transport, SIGNAL(messageReceived(QJsonObject)),
            this, SIGNAL(messageReceived(QJsonObject)));
}

ClangdManager::~ClangdManager()
{
    stop();
}

void ClangdManager::start(const QString &workspaceRoot, const QStringList &extraArgs)
{
    m_workspaceRoot = workspaceRoot;
    if (m_process->state() != QProcess::NotRunning)
        return;

    QStringList args;
    // Background index for fast go-to-definition even on first run
    args << QStringLiteral("--background-index");
    // Suppress automatic #include insertions on completion
    args << QStringLiteral("--header-insertion=never");
    // clang-tidy integration for extra diagnostics
    args << QStringLiteral("--clang-tidy");
    // Use in-memory pch to reduce disk I/O
    args << QStringLiteral("--pch-storage=memory");
    // Limit compile_commands.json search to known build directories
    const QString ccDir = findCompileCommandsDir(workspaceRoot);
    if (!ccDir.isEmpty())
        args << (QStringLiteral("--compile-commands-dir=") + ccDir);

    args += extraArgs;

    m_process->setProgram(QStringLiteral("clangd"));
    m_process->setArguments(args);
    if (!workspaceRoot.isEmpty())
        m_process->setWorkingDirectory(workspaceRoot);
    m_process->start();
    m_transport->start();
}

void ClangdManager::stop()
{
    if (m_process->state() == QProcess::NotRunning)
        return;

    m_process->terminate();
    if (!m_process->waitForFinished(2000)) {
        m_process->kill();
        m_process->waitForFinished();
    }
}

void ClangdManager::restart()
{
    stop();
    QTimer::singleShot(1500, this, [this]() {
        start(m_workspaceRoot);
    });
}

bool ClangdManager::isRunning() const
{
    return m_process->state() != QProcess::NotRunning;
}

QString ClangdManager::compileCommandsDir() const
{
    return findCompileCommandsDir(m_workspaceRoot);
}

QString ClangdManager::findCompileCommandsDir(const QString &workspaceRoot) const
{
    if (workspaceRoot.isEmpty())
        return {};

    // Ordered list of candidate directories (most specific first)
    const QStringList candidates = {
        workspaceRoot,
        workspaceRoot + QStringLiteral("/build"),
        workspaceRoot + QStringLiteral("/build-llvm"),
        workspaceRoot + QStringLiteral("/build-debug"),
        workspaceRoot + QStringLiteral("/build-release"),
        workspaceRoot + QStringLiteral("/build-ci"),
        workspaceRoot + QStringLiteral("/.cmake/api/v1/reply"),
    };
    for (const QString &dir : candidates) {
        if (QFile::exists(dir + QStringLiteral("/compile_commands.json")))
            return dir;
    }
    return {};
}
