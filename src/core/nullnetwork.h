#pragma once

#include "inetwork.h"

class NullNetwork : public INetwork
{
public:
    bool isAvailable() const override;
};
