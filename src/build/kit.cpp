#include "kit.h"

QString Kit::buildDisplayName(const QString &compilerName,
                              const QString &compilerVer,
                              const QString &gen,
                              const QString &dbg)
{
    QStringList parts;

    if (!compilerName.isEmpty()) {
        QString comp = compilerName;
        if (!compilerVer.isEmpty())
            comp += QLatin1Char(' ') + compilerVer;
        parts << comp;
    }

    if (!gen.isEmpty())
        parts << gen;

    if (!dbg.isEmpty())
        parts << dbg;

    if (parts.isEmpty())
        return QStringLiteral("(unknown kit)");

    return parts.join(QStringLiteral(" \u2014 ")); // em-dash separator
}
