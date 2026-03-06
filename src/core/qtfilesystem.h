#pragma once

#include "ifilesystem.h"

class QtFileSystem : public IFileSystem
{
public:
    bool exists(const QString &path) const override;
    QString readTextFile(const QString &path, QString *error) const override;
    bool writeTextFile(const QString &path, const QString &content, QString *error) override;
    QStringList listDir(const QString &path, QString *error) const override;
};
