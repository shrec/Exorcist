#include "qtfilesystem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

bool QtFileSystem::exists(const QString &path) const
{
    return QFileInfo::exists(path);
}

QString QtFileSystem::readTextFile(const QString &path, QString *error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return QString();
    }

    return QString::fromUtf8(file.readAll());
}

bool QtFileSystem::writeTextFile(const QString &path, const QString &content, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = file.errorString();
        }
        return false;
    }

    file.write(content.toUtf8());
    return true;
}

QStringList QtFileSystem::listDir(const QString &path, QString *error) const
{
    QDir dir(path);
    if (!dir.exists()) {
        if (error) {
            *error = "Directory does not exist";
        }
        return {};
    }

    return dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
}
