#include "qtprocess.h"

#include <QEventLoop>
#include <QProcess>
#include <QTimer>

ProcessResult QtProcess::run(const QString &program,
                             const QStringList &args,
                             int timeoutMs)
{
    ProcessResult result;
    QProcess process;

    process.start(program, args);
    if (!process.waitForStarted()) {
        result.stdErr = "Failed to start process";
        return result;
    }

    // Use local event loop so the main thread stays responsive
    QEventLoop loop;
    bool timedOut = false;

    QObject::connect(&process,
                     QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     &loop, &QEventLoop::quit);

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        loop.quit();
    });
    timer.start(timeoutMs);

    loop.exec(QEventLoop::ExcludeSocketNotifiers);
    timer.stop();

    if (timedOut) {
        process.kill();
        process.waitForFinished(2000);
        result.timedOut = true;
        result.stdErr = "Process timed out";
        return result;
    }

    result.exitCode = process.exitCode();
    result.stdOut = QString::fromUtf8(process.readAllStandardOutput());
    result.stdErr = QString::fromUtf8(process.readAllStandardError());
    result.success = (process.exitStatus() == QProcess::NormalExit && result.exitCode == 0);
    return result;
}
