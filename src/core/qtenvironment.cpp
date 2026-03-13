#include "qtenvironment.h"

#include <QDir>
#include <QProcessEnvironment>
#include <QStandardPaths>

QtEnvironment::QtEnvironment(QObject *parent)
    : QObject(parent)
{
}

QString QtEnvironment::variable(const QString &name) const
{
    return QProcessEnvironment::systemEnvironment().value(name);
}

bool QtEnvironment::hasVariable(const QString &name) const
{
    return QProcessEnvironment::systemEnvironment().contains(name);
}

void QtEnvironment::setVariable(const QString &name, const QString &value)
{
    qputenv(name.toUtf8().constData(), value.toUtf8());
}

QStringList QtEnvironment::variableNames() const
{
    return QProcessEnvironment::systemEnvironment().keys();
}

QString QtEnvironment::platform() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MAC)
    return QStringLiteral("macos");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux");
#else
    return QStringLiteral("unknown");
#endif
}

QString QtEnvironment::homeDirectory() const
{
    return QDir::homePath();
}

QString QtEnvironment::tempDirectory() const
{
    return QDir::tempPath();
}
