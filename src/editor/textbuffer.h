#pragma once

#include <QString>

class TextBuffer
{
public:
    virtual ~TextBuffer() = default;

    virtual int length() const = 0;
    virtual QString slice(int start, int length) const = 0;
};
