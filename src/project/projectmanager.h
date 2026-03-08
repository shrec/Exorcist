#pragma once

#include <QObject>
#include <QString>

#include "projecttypes.h"

class ProjectManager : public QObject
{
    Q_OBJECT

public:
    explicit ProjectManager(QObject *parent = nullptr);

    bool openFolder(const QString &dirPath);
    bool openSolution(const QString &slnPath);
    void closeSolution();
    bool saveSolution();
    bool saveSolutionAs(const QString &slnPath);
    bool createSolution(const QString &name, const QString &slnPath);
    bool addProject(const QString &name, const QString &rootPath);
    bool removeProject(int index);

    const ExSolution &solution() const;
    QString activeSolutionDir() const;

signals:
    void solutionChanged();
    void projectAdded(int index);
    void projectRemoved(int index);
    void solutionSaved(const QString &path);

private:
    bool loadFromJson(const QString &slnPath);
    bool writeToJson(const QString &slnPath);
    void setTransientSolution(const QString &dirPath);

    ExSolution m_solution;
};
