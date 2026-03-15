#include "projectmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ProjectManager::ProjectManager(QObject *parent)
    : QObject(parent)
{
}

bool ProjectManager::openFolder(const QString &dirPath)
{
    QDir dir(dirPath);
    const QStringList slnFiles = dir.entryList(QStringList() << "*.exsln", QDir::Files);
    if (!slnFiles.isEmpty()) {
        const QString slnPath = dir.absoluteFilePath(slnFiles.first());
        return openSolution(slnPath);
    }

    setTransientSolution(dirPath);
    emit solutionChanged();
    return false;
}

bool ProjectManager::openSolution(const QString &slnPath)
{
    if (!loadFromJson(slnPath)) {
        return false;
    }
    emit solutionChanged();
    return true;
}

void ProjectManager::closeSolution()
{
    m_solution = {};
    emit solutionChanged();
}

bool ProjectManager::saveSolution()
{
    if (m_solution.filePath.isEmpty()) {
        return false;
    }
    const bool ok = writeToJson(m_solution.filePath);
    if (ok) {
        emit solutionSaved(m_solution.filePath);
    }
    return ok;
}

bool ProjectManager::saveSolutionAs(const QString &slnPath)
{
    const bool ok = writeToJson(slnPath);
    if (ok) {
        m_solution.filePath = slnPath;
        emit solutionSaved(slnPath);
        emit solutionChanged();
    }
    return ok;
}

bool ProjectManager::createSolution(const QString &name, const QString &slnPath)
{
    m_solution = {};
    m_solution.name = name;
    m_solution.filePath = slnPath;
    m_solution.projects.clear();
    const bool ok = writeToJson(slnPath);
    if (ok) {
        emit solutionChanged();
        emit solutionSaved(slnPath);
    }
    return ok;
}

bool ProjectManager::addProject(const QString &name, const QString &rootPath,
                                const QString &language,
                                const QString &templateId)
{
    if (rootPath.isEmpty()) {
        return false;
    }

    ExProject project;
    project.name = name;
    project.rootPath = QDir(rootPath).absolutePath();
    project.language = language;
    project.templateId = templateId;

    // Set project file name based on language
    if (!language.isEmpty()) {
        const QString ext = ExProjectExt::forLanguage(language);
        project.projectFile = name + ext;
    }

    m_solution.projects.append(project);
    emit projectAdded(m_solution.projects.size() - 1);
    emit solutionChanged();
    return true;
}

bool ProjectManager::removeProject(int index)
{
    if (index < 0 || index >= m_solution.projects.size()) {
        return false;
    }
    m_solution.projects.removeAt(index);
    emit projectRemoved(index);
    emit solutionChanged();
    return true;
}

const ExSolution &ProjectManager::solution() const
{
    return m_solution;
}

QString ProjectManager::activeSolutionDir() const
{
    if (!m_solution.filePath.isEmpty()) {
        return QFileInfo(m_solution.filePath).absolutePath();
    }
    if (!m_solution.projects.isEmpty()) {
        return m_solution.projects.first().rootPath;
    }
    return QString();
}

bool ProjectManager::loadFromJson(const QString &slnPath)
{
    QFile file(slnPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    const QJsonObject obj = doc.object();
    ExSolution solution;
    solution.name = obj.value("name").toString();
    solution.filePath = slnPath;

    const QDir slnDir = QFileInfo(slnPath).absoluteDir();
    const QJsonArray projects = obj.value("projects").toArray();
    for (const QJsonValue &v : projects) {
        const QJsonObject p = v.toObject();
        ExProject proj;
        proj.name = p.value("name").toString();
        const QString relPath = p.value("path").toString();
        proj.rootPath = slnDir.absoluteFilePath(relPath);
        proj.projectFile = p.value("projectFile").toString();
        proj.language = p.value("language").toString();
        proj.templateId = p.value("templateId").toString();

        // Auto-detect language from project file extension if missing
        if (proj.language.isEmpty() && !proj.projectFile.isEmpty()) {
            const int dot = proj.projectFile.lastIndexOf(QLatin1Char('.'));
            if (dot >= 0)
                proj.language = ExProjectExt::languageFromExtension(
                    proj.projectFile.mid(dot));
        }

        solution.projects.append(proj);
    }

    m_solution = solution;
    return true;
}

bool ProjectManager::writeToJson(const QString &slnPath)
{
    QJsonObject obj;
    obj["name"] = m_solution.name;

    QJsonArray projects;
    const QDir slnDir = QFileInfo(slnPath).absoluteDir();
    for (const ExProject &proj : m_solution.projects) {
        QJsonObject p;
        p["name"] = proj.name;
        p["path"] = slnDir.relativeFilePath(proj.rootPath);
        if (!proj.projectFile.isEmpty())
            p["projectFile"] = proj.projectFile;
        if (!proj.language.isEmpty())
            p["language"] = proj.language;
        if (!proj.templateId.isEmpty())
            p["templateId"] = proj.templateId;
        projects.append(p);
    }
    obj["projects"] = projects;

    QJsonDocument doc(obj);
    QFile file(slnPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

void ProjectManager::setTransientSolution(const QString &dirPath)
{
    QDir dir(dirPath);
    m_solution = {};
    m_solution.name = dir.dirName();
    m_solution.filePath.clear();
    m_solution.projects.clear();

    ExProject proj;
    proj.name = dir.dirName();
    proj.rootPath = dir.absolutePath();
    m_solution.projects.append(proj);
}
