#pragma once

#include "iprocess.h"

class QtProcess : public IProcess
{
public:
    ProcessResult run(const QString &program,
                      const QStringList &args,
                      int timeoutMs) override;
};
