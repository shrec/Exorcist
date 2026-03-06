#pragma once

class INetwork
{
public:
    virtual ~INetwork() = default;

    virtual bool isAvailable() const = 0;
};
