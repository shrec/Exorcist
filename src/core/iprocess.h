#pragma once

#include <QString>
#include <QStringList>

struct ProcessResult
{
    int exitCode = -1;
    QString stdOut;
    QString stdErr;
    bool success = false;
    bool timedOut = false;
};

class IProcess
{
public:
    virtual ~IProcess() = default;

    virtual ProcessResult run(const QString &program,
                              const QStringList &args,
                              int timeoutMs) = 0;
};
