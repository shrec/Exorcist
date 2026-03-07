#pragma once

#include <QWidget>

class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;

class GitService;

class GitPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GitPanel(GitService *git, QWidget *parent = nullptr);

    QLineEdit *commitMessageInput() const { return m_commitMsg; }

signals:
    void generateCommitMessageRequested();
    void resolveConflictsRequested();

private:
    void populateLists();
    void stageSelected();
    void unstageSelected();
    void stageAll();
    void commitChanges();
    void refreshBranches();
    void onBranchComboChanged(int index);

    GitService *m_git;
    QComboBox *m_branchCombo;
    QListWidget *m_staged;
    QListWidget *m_unstaged;
    QLineEdit *m_commitMsg;
    QPushButton *m_stageBtn;
    QPushButton *m_unstageBtn;
    QPushButton *m_stageAllBtn;
    QPushButton *m_commitBtn;
    QPushButton *m_generateMsgBtn;
    QPushButton *m_resolveBtn;
    QPushButton *m_newBranchBtn;
    QStringList m_stagedPaths;
    QStringList m_unstagedPaths;
};
