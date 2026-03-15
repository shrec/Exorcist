#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

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
    QString name;
    QString path;
    QString description;
    QString mode;
    QStringList tools;
    QString body;
    QString rawBody;

    bool isValid() const { return !name.isEmpty() && !rawBody.isEmpty(); }
};

class PromptFileManager : public QObject
{
    Q_OBJECT

public:
    explicit PromptFileManager(QObject *parent = nullptr);

    void scanWorkspace(const QString &workspaceRoot);

    QList<PromptFile> promptFiles() const { return m_files.values(); }
    PromptFile promptFile(const QString &name) const { return m_files.value(name); }
    QStringList promptNames() const { return m_files.keys(); }

    QString interpolate(const QString &body,
                        const QString &currentFile = {},
                        const QString &selection = {},
                        const QString &language = {}) const;

signals:
    void promptFilesChanged();

private:
    static PromptFile parseFile(const QString &filePath);
    static void parseYaml(const QString &yaml, PromptFile &pf);

    QString m_workspaceRoot;
    QMap<QString, PromptFile> m_files;
};
