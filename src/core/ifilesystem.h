#pragma once

#include <QString>
#include <QStringList>

class IFileSystem
{
public:
    virtual ~IFileSystem() = default;

    virtual bool exists(const QString &path) const = 0;
    virtual QString readTextFile(const QString &path, QString *error) const = 0;
    virtual bool writeTextFile(const QString &path, const QString &content, QString *error) = 0;
    virtual QStringList listDir(const QString &path, QString *error) const = 0;
};
