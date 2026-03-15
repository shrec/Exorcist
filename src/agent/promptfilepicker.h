#pragma once

#include <QList>
#include <QStringList>
#include <QWidget>

class QListWidget;

// ─────────────────────────────────────────────────────────────────────────────
// PromptFilePicker — UI for selecting .prompt.md files from the slash menu.
//
// Displays discovered prompt files with their descriptions. When a prompt
// is selected, emits promptSelected with the resolved body text.
// ─────────────────────────────────────────────────────────────────────────────

struct PromptFileEntry {
    QString name;        // display name from YAML
    QString description; // description from YAML
    QString filePath;    // absolute path
    QString body;        // resolved body content
    QString mode;        // ask, edit, agent
    QStringList tools;   // required tools
};

class PromptFilePicker : public QWidget
{
    Q_OBJECT

public:
    explicit PromptFilePicker(QWidget *parent = nullptr);

    void setEntries(const QList<PromptFileEntry> &entries);
    void showBelow(QWidget *anchor);

signals:
    void promptSelected(const PromptFileEntry &entry);

private:
    QListWidget *m_list;
    QList<PromptFileEntry> m_entries;
};
