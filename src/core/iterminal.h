#pragma once

#include <QString>

class ITerminal
{
public:
    virtual ~ITerminal() = default;

    virtual QString backendName() const = 0;
};
