#pragma once

#include <QWidget>

class QDir;
class QLabel;
class QListWidget;
class QPlainTextEdit;

// ─────────────────────────────────────────────────────────────────────────────
// MemoryFileEditor — UI for viewing and editing memory files.
//
// Shows a file browser for /memories/ directory with user, session,
// and repo scopes. Supports create/edit/delete operations.
// ─────────────────────────────────────────────────────────────────────────────

class MemoryFileEditor : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryFileEditor(QWidget *parent = nullptr);

    void setMemoryRoot(const QString &root);

signals:
    void fileChanged(const QString &path);

private slots:
    void refresh();
    void createFile();
    void deleteFile();
    void saveFile();

private:
    void loadFile(const QString &path);
    void scanDir(const QDir &dir, const QString &prefix);

    QString m_memoryRoot;
    QListWidget *m_fileList;
    QPlainTextEdit *m_editor;
};
