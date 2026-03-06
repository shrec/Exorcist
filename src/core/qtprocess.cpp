#include "qtprocess.h"

#include <QProcess>

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

    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished();
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
