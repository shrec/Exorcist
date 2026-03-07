#include "gitpanel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "gitservice.h"

GitPanel::GitPanel(GitService *git, QWidget *parent)
    : QWidget(parent),
      m_git(git),
      m_branchCombo(new QComboBox(this)),
      m_staged(new QListWidget(this)),
      m_unstaged(new QListWidget(this)),
      m_commitMsg(new QLineEdit(this)),
      m_stageBtn(new QPushButton(tr("Stage"), this)),
      m_unstageBtn(new QPushButton(tr("Unstage"), this)),
      m_stageAllBtn(new QPushButton(tr("Stage All"), this)),
      m_commitBtn(new QPushButton(tr("Commit"), this)),
      m_generateMsgBtn(new QPushButton(tr("\u2728"), this)),
      m_resolveBtn(new QPushButton(tr("Resolve Conflicts"), this)),
      m_newBranchBtn(new QPushButton(tr("+"), this))
{
    auto *layout = new QVBoxLayout(this);

    // Branch switcher row
    auto *branchRow = new QHBoxLayout();
    m_branchCombo->setToolTip(tr("Switch branch"));
    m_branchCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    branchRow->addWidget(m_branchCombo);
    m_newBranchBtn->setFixedWidth(30);
    m_newBranchBtn->setToolTip(tr("Create new branch"));
    branchRow->addWidget(m_newBranchBtn);
    layout->addLayout(branchRow);

    layout->addWidget(new QLabel(tr("Staged"), this));
    layout->addWidget(m_staged);

    auto *unstageRow = new QHBoxLayout();
    unstageRow->addWidget(m_unstageBtn);
    layout->addLayout(unstageRow);

    layout->addWidget(new QLabel(tr("Changes"), this));
    layout->addWidget(m_unstaged);

    auto *stageRow = new QHBoxLayout();
    stageRow->addWidget(m_stageBtn);
    stageRow->addWidget(m_stageAllBtn);
    layout->addLayout(stageRow);

    m_commitMsg->setPlaceholderText(tr("Commit message..."));
    auto *commitRow = new QHBoxLayout();
    commitRow->addWidget(m_commitMsg);
    m_generateMsgBtn->setFixedWidth(30);
    m_generateMsgBtn->setToolTip(tr("Generate commit message with AI"));
    commitRow->addWidget(m_generateMsgBtn);
    layout->addLayout(commitRow);
    layout->addWidget(m_commitBtn);

    m_resolveBtn->setToolTip(tr("Resolve merge conflicts with AI"));
    m_resolveBtn->setVisible(false);
    layout->addWidget(m_resolveBtn);
    setLayout(layout);

    connect(m_stageBtn, &QPushButton::clicked, this, &GitPanel::stageSelected);
    connect(m_unstageBtn, &QPushButton::clicked, this, &GitPanel::unstageSelected);
    connect(m_stageAllBtn, &QPushButton::clicked, this, &GitPanel::stageAll);
    connect(m_commitBtn, &QPushButton::clicked, this, &GitPanel::commitChanges);
    connect(m_generateMsgBtn, &QPushButton::clicked,
            this, &GitPanel::generateCommitMessageRequested);
    connect(m_resolveBtn, &QPushButton::clicked,
            this, &GitPanel::resolveConflictsRequested);
    connect(m_git, &GitService::statusRefreshed, this, &GitPanel::populateLists);
    connect(m_git, &GitService::branchChanged, this, [this] { refreshBranches(); });
    connect(m_branchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GitPanel::onBranchComboChanged);
    connect(m_newBranchBtn, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Branch"),
                                                   tr("Branch name:"), QLineEdit::Normal,
                                                   {}, &ok);
        if (ok && !name.trimmed().isEmpty())
            m_git->createBranch(name.trimmed());
    });

    populateLists();
    refreshBranches();
}

void GitPanel::populateLists()
{
    m_staged->clear();
    m_unstaged->clear();
    m_stagedPaths.clear();
    m_unstagedPaths.clear();

    const QHash<QString, QString> statuses = m_git->fileStatuses();
    for (auto it = statuses.begin(); it != statuses.end(); ++it) {
        const QString path = it.key();
        const QString status = it.value();
        if (status.size() < 2) {
            continue;
        }

        const QChar indexChar = status[0];
        const QChar workChar = status[1];

        if (indexChar != ' ' && indexChar != '?') {
            m_staged->addItem(path);
            m_stagedPaths.append(path);
        }
        if (workChar != ' ' || status == "??") {
            m_unstaged->addItem(path);
            m_unstagedPaths.append(path);
        }
    }

    const bool hasRepo = m_git->isGitRepo();
    m_stageBtn->setEnabled(hasRepo);
    m_unstageBtn->setEnabled(hasRepo);
    m_stageAllBtn->setEnabled(hasRepo);
    m_commitBtn->setEnabled(hasRepo);

    // Show resolve button only when merge conflicts exist
    const QStringList conflicts = m_git->conflictFiles();
    m_resolveBtn->setVisible(hasRepo && !conflicts.isEmpty());
    if (!conflicts.isEmpty())
        m_resolveBtn->setText(tr("Resolve Conflicts (%1)").arg(conflicts.size()));
}

void GitPanel::stageSelected()
{
    const int row = m_unstaged->currentRow();
    if (row < 0 || row >= m_unstagedPaths.size()) {
        return;
    }
    m_git->stageFile(m_unstagedPaths.at(row));
}

void GitPanel::unstageSelected()
{
    const int row = m_staged->currentRow();
    if (row < 0 || row >= m_stagedPaths.size()) {
        return;
    }
    m_git->unstageFile(m_stagedPaths.at(row));
}

void GitPanel::stageAll()
{
    for (const QString &path : m_unstagedPaths) {
        m_git->stageFile(path);
    }
}

void GitPanel::commitChanges()
{
    const QString msg = m_commitMsg->text().trimmed();
    if (msg.isEmpty()) {
        return;
    }
    if (m_git->commit(msg)) {
        m_commitMsg->clear();
    }
}

void GitPanel::refreshBranches()
{
    const QSignalBlocker blocker(m_branchCombo);
    m_branchCombo->clear();
    const QStringList branches = m_git->localBranches();
    const QString current = m_git->currentBranch();
    m_branchCombo->addItems(branches);
    const int idx = branches.indexOf(current);
    if (idx >= 0)
        m_branchCombo->setCurrentIndex(idx);
    m_branchCombo->setEnabled(m_git->isGitRepo());
    m_newBranchBtn->setEnabled(m_git->isGitRepo());
}

void GitPanel::onBranchComboChanged(int index)
{
    if (index < 0) return;
    const QString selected = m_branchCombo->currentText();
    if (selected.isEmpty() || selected == m_git->currentBranch()) return;
    m_git->checkoutBranch(selected);
}
