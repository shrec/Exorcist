#pragma once

#include "iterminal.h"

class QtTerminal : public ITerminal
{
public:
    QString backendName() const override;
};
