#include "promptfilemanager.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

PromptFileManager::PromptFileManager(QObject *parent)
    : QObject(parent)
{
}

void PromptFileManager::scanWorkspace(const QString &workspaceRoot)
{
    m_files.clear();
    m_workspaceRoot = workspaceRoot;
    if (workspaceRoot.isEmpty()) return;

    const QStringList searchDirs = {
        workspaceRoot + QStringLiteral("/.github/prompts"),
        workspaceRoot + QStringLiteral("/.prompts"),
    };

    for (const QString &dirPath : searchDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        const auto entries = dir.entryInfoList(
            {QStringLiteral("*.prompt.md")}, QDir::Files);
        for (const QFileInfo &fi : entries) {
            PromptFile pf = parseFile(fi.absoluteFilePath());
            if (pf.isValid())
                m_files[pf.name] = pf;
        }
    }

    emit promptFilesChanged();
}

QString PromptFileManager::interpolate(const QString &body,
                                       const QString &currentFile,
                                       const QString &selection,
                                       const QString &language) const
{
    QString result = body;
    result.replace(QStringLiteral("${workspaceFolder}"), m_workspaceRoot);
    result.replace(QStringLiteral("${file}"), currentFile);
    result.replace(QStringLiteral("${selection}"), selection);
    result.replace(QStringLiteral("${currentFileLanguage}"), language);
    result.replace(QStringLiteral("${date}"),
                   QDate::currentDate().toString(Qt::ISODate));
    return result;
}

PromptFile PromptFileManager::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&file);
    const QString content = in.readAll();
    file.close();

    PromptFile pf;
    pf.path = filePath;
    pf.name = QFileInfo(filePath).baseName();
    if (pf.name.endsWith(QStringLiteral(".prompt")))
        pf.name.chop(7);

    // Parse YAML frontmatter
    if (content.startsWith(QStringLiteral("---"))) {
        const int endIdx = content.indexOf(QStringLiteral("\n---"), 3);
        if (endIdx > 0) {
            const QString yaml = content.mid(4, endIdx - 4).trimmed();
            pf.rawBody = content.mid(endIdx + 4).trimmed();
            parseYaml(yaml, pf);
        } else {
            pf.rawBody = content;
        }
    } else {
        pf.rawBody = content;
    }

    return pf;
}

void PromptFileManager::parseYaml(const QString &yaml, PromptFile &pf)
{
    const auto lines = yaml.split('\n');
    for (const QString &line : lines) {
        const int colonIdx = line.indexOf(':');
        if (colonIdx < 0) continue;

        const QString key = line.left(colonIdx).trimmed().toLower();
        QString value = line.mid(colonIdx + 1).trimmed();

        if (key == QStringLiteral("description")) {
            pf.description = value;
        } else if (key == QStringLiteral("mode")) {
            pf.mode = value.toLower();
        } else if (key == QStringLiteral("tools")) {
            if (value.startsWith('[') && value.endsWith(']')) {
                value = value.mid(1, value.size() - 2);
            }
            const auto parts = value.split(',');
            for (const QString &part : parts) {
                const QString t = part.trimmed();
                if (!t.isEmpty())
                    pf.tools << t;
            }
        }
    }
}
