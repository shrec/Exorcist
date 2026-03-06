#pragma once

#include <QString>

struct SearchMatch
{
    QString filePath;
    int line = 0;
    int column = 0;
    QString preview;
};
