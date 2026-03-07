#pragma once

#include <QString>
#include <QList>

struct ExProject
{
    QString name;
    QString rootPath;
};

struct ExSolution
{
    QString name;
    QString filePath;
    QList<ExProject> projects;
};
