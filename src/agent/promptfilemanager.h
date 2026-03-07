#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTextStream>

// ─────────────────────────────────────────────────────────────────────────────
// PromptFileManager — handles .prompt.md files
//
// Format: YAML frontmatter delimited by --- + Markdown body.
//   ---
//   description: Explain this code
//   mode: ask
//   tools: [readFile, searchWorkspace]
//   ---
//   Explain the selected code in detail.
//   Focus on ${selection}.
//
// Variables interpolated: ${workspaceFolder}, ${file}, ${selection},
//   ${currentFileLanguage}, ${date}
// ─────────────────────────────────────────────────────────────────────────────

struct PromptFile {
    QString name;         // filename without extension
    QString path;         // absolute path
    QString description;  // from YAML frontmatter
    QString mode;         // ask / edit / agent
    QStringList tools;    // tool names to enable
    QString body;         // markdown body (post-interpolation)
    QString rawBody;      // original body before interpolation

    bool isValid() const { return !name.isEmpty() && !rawBody.isEmpty(); }
};

class PromptFileManager : public QObject
{
    Q_OBJECT

public:
    explicit PromptFileManager(QObject *parent = nullptr)
        : QObject(parent) {}

    /// Scan workspace for .prompt.md files in .github/prompts/
    void scanWorkspace(const QString &workspaceRoot)
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

    /// Get all loaded prompt files
    QList<PromptFile> promptFiles() const { return m_files.values(); }

    /// Get prompt file by name
    PromptFile promptFile(const QString &name) const { return m_files.value(name); }

    /// Get names for slash menu
    QStringList promptNames() const { return m_files.keys(); }

    /// Interpolate variables in a prompt body
    QString interpolate(const QString &body,
                        const QString &currentFile = {},
                        const QString &selection = {},
                        const QString &language = {}) const
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

signals:
    void promptFilesChanged();

private:
    static PromptFile parseFile(const QString &filePath)
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
        // Remove .prompt suffix if present
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

    static void parseYaml(const QString &yaml, PromptFile &pf)
    {
        // Simple YAML parser — handles key: value and key: [list]
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
                // Parse [tool1, tool2] format
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

    QString m_workspaceRoot;
    QMap<QString, PromptFile> m_files;
};
