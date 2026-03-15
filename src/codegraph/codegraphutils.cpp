#include "codegraphutils.h"

#include <QLatin1Char>

static int countChar(const QString &s, QChar ch)
{
    int n = 0;
    for (const auto c : s)
        if (c == ch) ++n;
    return n;
}

int CodeGraphUtils::findBraceBody(const QStringList &lines, int startIdx, int n)
{
    for (int j = startIdx; j < qMin(startIdx + 15, n); ++j) {
        if (lines[j].contains(QLatin1Char('{'))) {
            int depth = 0;
            for (int k = j; k < n; ++k) {
                depth += countChar(lines[k], QLatin1Char('{'))
                       - countChar(lines[k], QLatin1Char('}'));
                if (depth <= 0)
                    return k;
            }
            return -1;
        }
        if (lines[j].contains(QLatin1Char(';')) &&
            !lines[j].contains(QLatin1Char('{')))
            return -1;
    }
    return -1;
}
