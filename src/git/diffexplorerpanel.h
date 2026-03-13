#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class GitService;

/// Panel for comparing two git revisions (branches, commits, tags).
/// Shows a file list of changes on the left, side-by-side diff on the right.
class DiffExplorerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit DiffExplorerPanel(GitService *git, QWidget *parent = nullptr);

    /// Set revisions to compare (branch names, commit hashes, tags).
    void compare(const QString &rev1, const QString &rev2);

signals:
    /// Emitted when user wants to open a file at a specific revision.
    void openFileRequested(const QString &filePath);

private:
    void refreshBranches();
    void onCompareClicked();
    void onFileSelected(int row);
    void showFileDiff(const QString &relPath);
    void highlightDifferences();

    GitService *m_git;

    // Left panel — revision picker + file list
    QComboBox    *m_fromCombo   = nullptr;
    QComboBox    *m_toCombo     = nullptr;
    QPushButton  *m_compareBtn  = nullptr;
    QLabel       *m_summaryLabel = nullptr;
    QListWidget  *m_fileList    = nullptr;

    // Right panel — side-by-side diff
    QPlainTextEdit *m_leftPane  = nullptr;
    QPlainTextEdit *m_rightPane = nullptr;
    QLabel         *m_diffTitle = nullptr;

    // State
    QString m_rev1;
    QString m_rev2;
    QStringList m_changedPaths;
};
