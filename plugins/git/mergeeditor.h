#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class GitService;

/// 3-way merge conflict editor: shows base/ours/theirs and produces a resolved result.
/// Lists conflicted files, lets user pick resolution per-conflict.
class MergeEditor : public QWidget
{
    Q_OBJECT

public:
    explicit MergeEditor(GitService *git, QWidget *parent = nullptr);

    /// Reload the list of conflicted files from git.
    void refresh();

signals:
    /// Emitted when a file has been resolved (written to disk + git add).
    void fileResolved(const QString &filePath);

    /// Emitted when all conflicts are resolved.
    void allResolved();

private:
    void onFileSelected(int row);
    void loadConflict(const QString &absPath);
    void acceptOurs();
    void acceptTheirs();
    void acceptBoth();
    void acceptCustom();
    void writeResolution(const QString &content);

    GitService *m_git;

    // Left — file list
    QListWidget *m_fileList = nullptr;
    QLabel      *m_statusLabel = nullptr;

    // Right — conflict view: ours | theirs | result
    QLabel         *m_conflictTitle = nullptr;
    QPlainTextEdit *m_oursPane      = nullptr;
    QPlainTextEdit *m_theirsPane    = nullptr;
    QPlainTextEdit *m_resultPane    = nullptr;

    // Action buttons
    QPushButton *m_acceptOursBtn   = nullptr;
    QPushButton *m_acceptTheirsBtn = nullptr;
    QPushButton *m_acceptBothBtn   = nullptr;
    QPushButton *m_refreshBtn      = nullptr;

    // State
    QString m_currentFile;
    QStringList m_conflictPaths;
};
