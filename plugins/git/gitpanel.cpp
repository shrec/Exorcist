#include "gitpanel.h"

#include "git/gitservice.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QStringList>
#include <QStyle>
#include <QTextEdit>
#include <QTextStream>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// VS Code "dark-modern" palette (subset)
constexpr const char *kBg          = "#1e1e1e";
constexpr const char *kBgAlt       = "#252526";
constexpr const char *kBgSelect    = "#094771";
constexpr const char *kBorder      = "#3c3c3c";
constexpr const char *kText        = "#cccccc";
constexpr const char *kTextDim     = "#9d9d9d";
constexpr const char *kAccent      = "#0e639c";
constexpr const char *kAccentHov   = "#1177bb";
constexpr const char *kDiffAddBg   = "#1e3a1e";
constexpr const char *kDiffDelBg   = "#3a1e1e";
constexpr const char *kDiffHunk    = "#264f78";

constexpr int kFileRoleAbsPath  = Qt::UserRole + 1;
constexpr int kFileRoleStatus   = Qt::UserRole + 2;
constexpr int kFileRoleStaged   = Qt::UserRole + 3;

QString joinedStyle()
{
    // Apply only to the panel and its children.
    return QStringLiteral(R"(
        QWidget#GitPanelRoot { background: %1; color: %2; }
        QFrame#GitToolbar  { background: %3; border-bottom: 1px solid %4; }
        QToolButton {
            background: transparent; color: %2; border: 1px solid transparent;
            padding: 3px 6px; border-radius: 3px;
        }
        QToolButton:hover   { background: #2a2d2e; border-color: %4; }
        QToolButton:pressed { background: %5; }
        QPushButton#GitCommitBtn {
            background: %6; color: white; border: none;
            padding: 6px 12px; border-radius: 2px; font-weight: 600;
        }
        QPushButton#GitCommitBtn:hover    { background: %7; }
        QPushButton#GitCommitBtn:disabled { background: #3a3a3a; color: %8; }
        QPushButton#GitResolveBtn {
            background: #aa5500; color: white; border: none;
            padding: 4px 10px; border-radius: 2px;
        }
        QPushButton#GitResolveBtn:hover { background: #cc6600; }
        QComboBox#GitBranchCombo {
            background: %3; color: %2; border: 1px solid %4;
            padding: 3px 6px; border-radius: 2px; min-width: 120px;
        }
        QComboBox#GitBranchCombo:hover { border-color: %6; }
        QComboBox#GitBranchCombo QAbstractItemView {
            background: %3; color: %2; border: 1px solid %4;
            selection-background-color: %5;
        }
        QPlainTextEdit#GitCommitMsg {
            background: %3; color: %2; border: 1px solid %4;
            padding: 4px; border-radius: 2px;
            selection-background-color: %5;
        }
        QPlainTextEdit#GitCommitMsg:focus { border-color: %6; }
        QTreeWidget {
            background: %1; color: %2; border: none;
            outline: 0; alternate-background-color: %3;
        }
        QTreeWidget::item { padding: 2px 4px; }
        QTreeWidget::item:hover    { background: #2a2d2e; }
        QTreeWidget::item:selected { background: %5; color: white; }
        QTreeWidget::branch { background: %1; }
        QHeaderView::section {
            background: %3; color: %8; padding: 4px;
            border: none; border-bottom: 1px solid %4;
            font-weight: 600; text-transform: uppercase; font-size: 10px;
        }
        QTextEdit#GitDiffView {
            background: %1; color: %2; border: none;
            border-top: 1px solid %4;
            selection-background-color: %5;
        }
        QSplitter::handle { background: %4; }
        QSplitter::handle:vertical { height: 3px; }
        QLabel#GitSectionLabel {
            color: %8; font-weight: 600; padding: 4px 6px;
            text-transform: uppercase; font-size: 10px; letter-spacing: 1px;
            background: %3; border-bottom: 1px solid %4;
        }
    )")
        .arg(QString::fromLatin1(kBg))         // 1
        .arg(QString::fromLatin1(kText))       // 2
        .arg(QString::fromLatin1(kBgAlt))      // 3
        .arg(QString::fromLatin1(kBorder))     // 4
        .arg(QString::fromLatin1(kBgSelect))   // 5
        .arg(QString::fromLatin1(kAccent))     // 6
        .arg(QString::fromLatin1(kAccentHov))  // 7
        .arg(QString::fromLatin1(kTextDim));   // 8
}

} // namespace

// ── CommitInputAdapter ────────────────────────────────────────────────────────

void GitPanel::CommitInputAdapter::setText(const QString &t)
{
    if (m_e) m_e->setPlainText(t);
}

// ── GitPanel ──────────────────────────────────────────────────────────────────

GitPanel::GitPanel(GitService *git, QWidget *parent)
    : QWidget(parent),
      m_git(git)
{
    setObjectName(QStringLiteral("GitPanelRoot"));
    setStyleSheet(joinedStyle());

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ─────────────────────────────────────────────────────────
    auto *toolbar = new QFrame(this);
    toolbar->setObjectName(QStringLiteral("GitToolbar"));
    auto *tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(6, 4, 6, 4);
    tbLayout->setSpacing(4);

    m_branchCombo = new QComboBox(toolbar);
    m_branchCombo->setObjectName(QStringLiteral("GitBranchCombo"));
    m_branchCombo->setToolTip(tr("Switch branch"));
    m_branchCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    tbLayout->addWidget(m_branchCombo, 1);

    auto makeToolButton = [&](const QString &text, const QString &tip,
                              QStyle::StandardPixmap fallbackIcon) {
        auto *b = new QToolButton(toolbar);
        b->setText(text);
        b->setToolTip(tip);
        b->setToolButtonStyle(Qt::ToolButtonTextOnly);
        b->setIcon(style()->standardIcon(fallbackIcon));
        b->setIconSize(QSize(14, 14));
        return b;
    };

    m_newBranchBtn = makeToolButton(QStringLiteral("+"),
                                    tr("Create new branch"),
                                    QStyle::SP_FileDialogNewFolder);
    m_pullBtn = makeToolButton(tr("Pull"), tr("Pull from origin"),
                               QStyle::SP_ArrowDown);
    m_pushBtn = makeToolButton(tr("Push"), tr("Push to origin"),
                               QStyle::SP_ArrowUp);
    m_syncBtn = makeToolButton(tr("Sync"), tr("Pull, then push"),
                               QStyle::SP_BrowserReload);
    m_refreshBtn = makeToolButton(QStringLiteral(""),
                                  tr("Refresh status"),
                                  QStyle::SP_BrowserReload);

    tbLayout->addWidget(m_newBranchBtn);
    tbLayout->addWidget(m_pullBtn);
    tbLayout->addWidget(m_pushBtn);
    tbLayout->addWidget(m_syncBtn);
    tbLayout->addWidget(m_refreshBtn);

    root->addWidget(toolbar);

    // ── Splitter (top: lists+commit, bottom: diff) ───────────────────────
    m_splitter = new QSplitter(Qt::Vertical, this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(3);

    auto *topWidget = new QWidget(m_splitter);
    auto *topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    // Commit message
    m_commitMsg = new QPlainTextEdit(topWidget);
    m_commitMsg->setObjectName(QStringLiteral("GitCommitMsg"));
    m_commitMsg->setPlaceholderText(tr("Message (Ctrl+Enter to commit)"));
    m_commitMsg->setMaximumHeight(70);
    m_commitMsg->setMinimumHeight(40);
    {
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        mono.setPointSize(qMax(8, mono.pointSize()));
        m_commitMsg->setFont(mono);
    }
    m_commitInputAdapter = new CommitInputAdapter(m_commitMsg);

    auto *commitRow = new QHBoxLayout();
    commitRow->setContentsMargins(6, 6, 6, 6);
    commitRow->setSpacing(4);
    commitRow->addWidget(m_commitMsg, 1);

    auto *commitBtnCol = new QVBoxLayout();
    commitBtnCol->setSpacing(4);

    m_commitBtn = new QPushButton(tr("Commit"), topWidget);
    m_commitBtn->setObjectName(QStringLiteral("GitCommitBtn"));
    m_commitBtn->setEnabled(false);

    m_generateMsgBtn = new QToolButton(topWidget);
    m_generateMsgBtn->setText(QString::fromUtf8("\xE2\x9C\xA8")); // sparkles
    m_generateMsgBtn->setToolTip(tr("Generate commit message with AI"));

    commitBtnCol->addWidget(m_commitBtn);
    commitBtnCol->addWidget(m_generateMsgBtn);
    commitBtnCol->addStretch(1);
    commitRow->addLayout(commitBtnCol);
    topLayout->addLayout(commitRow);

    // Resolve conflicts banner
    m_resolveBtn = new QPushButton(tr("Resolve Conflicts"), topWidget);
    m_resolveBtn->setObjectName(QStringLiteral("GitResolveBtn"));
    m_resolveBtn->setVisible(false);
    auto *resolveRow = new QHBoxLayout();
    resolveRow->setContentsMargins(6, 0, 6, 6);
    resolveRow->addWidget(m_resolveBtn);
    topLayout->addLayout(resolveRow);

    // Section: Staged
    auto *stagedLabel = new QLabel(tr("Staged Changes"), topWidget);
    stagedLabel->setObjectName(QStringLiteral("GitSectionLabel"));
    topLayout->addWidget(stagedLabel);

    m_stagedTree = new QTreeWidget(topWidget);
    m_stagedTree->setHeaderHidden(true);
    m_stagedTree->setRootIsDecorated(false);
    m_stagedTree->setUniformRowHeights(true);
    m_stagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stagedTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_stagedTree->setColumnCount(2);
    m_stagedTree->header()->setStretchLastSection(false);
    m_stagedTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_stagedTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    topLayout->addWidget(m_stagedTree, 1);

    // Section: Unstaged
    auto *unstagedLabel = new QLabel(tr("Unstaged Changes"), topWidget);
    unstagedLabel->setObjectName(QStringLiteral("GitSectionLabel"));
    topLayout->addWidget(unstagedLabel);

    m_unstagedTree = new QTreeWidget(topWidget);
    m_unstagedTree->setHeaderHidden(true);
    m_unstagedTree->setRootIsDecorated(false);
    m_unstagedTree->setUniformRowHeights(true);
    m_unstagedTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_unstagedTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_unstagedTree->setColumnCount(2);
    m_unstagedTree->header()->setStretchLastSection(false);
    m_unstagedTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_unstagedTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    topLayout->addWidget(m_unstagedTree, 1);

    m_splitter->addWidget(topWidget);

    // ── Diff view (bottom) ───────────────────────────────────────────────
    m_diffView = new QTextEdit(m_splitter);
    m_diffView->setObjectName(QStringLiteral("GitDiffView"));
    m_diffView->setReadOnly(true);
    m_diffView->setLineWrapMode(QTextEdit::NoWrap);
    {
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        m_diffView->setFont(mono);
    }
    m_diffView->setPlaceholderText(tr("Select a file to preview its diff"));
    m_splitter->addWidget(m_diffView);

    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);

    root->addWidget(m_splitter, 1);

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_git, &GitService::statusRefreshed, this, &GitPanel::populateLists);
    connect(m_git, &GitService::branchChanged, this, [this] { refreshBranches(); });

    connect(m_branchCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &GitPanel::onBranchComboChanged);
    connect(m_newBranchBtn, &QToolButton::clicked, this, [this] {
        if (!m_git || !m_git->isGitRepo()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("New Branch"),
                                                   tr("Branch name:"),
                                                   QLineEdit::Normal, {}, &ok);
        if (ok && !name.trimmed().isEmpty())
            m_git->createBranch(name.trimmed());
    });
    connect(m_refreshBtn, &QToolButton::clicked, this, [this] {
        populateLists();
        refreshBranches();
    });
    connect(m_pushBtn,   &QToolButton::clicked, this, &GitPanel::onPushClicked);
    connect(m_pullBtn,   &QToolButton::clicked, this, &GitPanel::onPullClicked);
    connect(m_syncBtn,   &QToolButton::clicked, this, &GitPanel::onSyncClicked);

    connect(m_commitBtn, &QPushButton::clicked, this, &GitPanel::commitChanges);
    connect(m_commitMsg, &QPlainTextEdit::textChanged,
            this, &GitPanel::updateCommitButtonState);

    connect(m_generateMsgBtn, &QToolButton::clicked,
            this, &GitPanel::generateCommitMessageRequested);
    connect(m_resolveBtn, &QPushButton::clicked,
            this, &GitPanel::resolveConflictsRequested);

    auto wireTree = [this](QTreeWidget *tree, bool staged) {
        connect(tree, &QTreeWidget::itemDoubleClicked, this,
                [this, staged](QTreeWidgetItem *item, int) {
                    if (!item) return;
                    if (staged)
                        unstageItem(item);
                    else
                        stageItem(item);
                });
        connect(tree, &QTreeWidget::currentItemChanged, this,
                [this, staged](QTreeWidgetItem *item, QTreeWidgetItem *) {
                    if (!item) return;
                    const QString path = item->data(0, kFileRoleAbsPath).toString();
                    renderDiff(path, staged);
                });
        connect(tree, &QTreeWidget::customContextMenuRequested, this,
                [this, tree, staged](const QPoint &p) {
                    showFileContextMenu(p, tree, staged);
                });
    };
    wireTree(m_stagedTree,   true);
    wireTree(m_unstagedTree, false);

    // Ctrl+Enter to commit
    auto *commitShortcut = new QAction(this);
    commitShortcut->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    commitShortcut->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(commitShortcut);
    connect(commitShortcut, &QAction::triggered, this, [this] {
        if (m_commitBtn->isEnabled()) commitChanges();
    });

    populateLists();
    refreshBranches();
    updateCommitButtonState();
    updateRemoteButtonsState();
}

// ── populateLists ─────────────────────────────────────────────────────────────

void GitPanel::populateLists()
{
    // Remember selection so refresh doesn't lose it.
    const QString prevStagedSel =
        (m_stagedTree && m_stagedTree->currentItem())
            ? m_stagedTree->currentItem()->data(0, kFileRoleAbsPath).toString()
            : QString();
    const QString prevUnstagedSel =
        (m_unstagedTree && m_unstagedTree->currentItem())
            ? m_unstagedTree->currentItem()->data(0, kFileRoleAbsPath).toString()
            : QString();

    m_stagedTree->clear();
    m_unstagedTree->clear();
    m_lastStatuses.clear();

    if (!m_git) return;

    const QHash<QString, QString> statuses = m_git->fileStatuses();
    m_lastStatuses = statuses;

    // Sort paths for deterministic ordering.
    QStringList paths = statuses.keys();
    std::sort(paths.begin(), paths.end());

    auto addItem = [&](QTreeWidget *tree, const QString &absPath,
                       const QString &status, bool stagedSide) {
        const QString letter = statusLetter(status, stagedSide);
        QFileInfo fi(absPath);
        const QString name = fi.fileName();
        const QString rel  = QDir(m_git->workingDirectory()).relativeFilePath(absPath);
        const QString dir  = QFileInfo(rel).path();

        auto *item = new QTreeWidgetItem(tree);
        QString display = name;
        if (!dir.isEmpty() && dir != QStringLiteral("."))
            display += QStringLiteral("  ") + dir;
        item->setText(0, display);
        item->setText(1, letter);
        item->setToolTip(0, rel);
        item->setData(0, kFileRoleAbsPath, absPath);
        item->setData(0, kFileRoleStatus,  status);
        item->setData(0, kFileRoleStaged,  stagedSide);
        item->setForeground(1, QBrush(statusColor(letter)));
        QFont f = item->font(1);
        f.setBold(true);
        item->setFont(1, f);
        item->setTextAlignment(1, Qt::AlignCenter);
        return item;
    };

    QTreeWidgetItem *restoreStaged   = nullptr;
    QTreeWidgetItem *restoreUnstaged = nullptr;

    for (const QString &path : paths) {
        const QString status = statuses.value(path);
        if (status.size() < 2) continue;

        const QChar indexChar = status[0];
        const QChar workChar  = status[1];

        if (indexChar != QLatin1Char(' ') && indexChar != QLatin1Char('?')) {
            auto *it = addItem(m_stagedTree, path, status, /*staged*/ true);
            if (path == prevStagedSel) restoreStaged = it;
        }
        if (workChar != QLatin1Char(' ') || status == QLatin1String("??")) {
            auto *it = addItem(m_unstagedTree, path, status, /*staged*/ false);
            if (path == prevUnstagedSel) restoreUnstaged = it;
        }
    }

    if (restoreStaged)
        m_stagedTree->setCurrentItem(restoreStaged);
    if (restoreUnstaged)
        m_unstagedTree->setCurrentItem(restoreUnstaged);

    const bool hasRepo = m_git->isGitRepo();
    m_branchCombo->setEnabled(hasRepo);
    m_newBranchBtn->setEnabled(hasRepo);
    m_pushBtn->setEnabled(hasRepo);
    m_pullBtn->setEnabled(hasRepo);
    m_syncBtn->setEnabled(hasRepo);
    m_refreshBtn->setEnabled(hasRepo);

    // Conflict banner
    const QStringList conflicts = m_git->conflictFiles();
    m_resolveBtn->setVisible(hasRepo && !conflicts.isEmpty());
    if (!conflicts.isEmpty())
        m_resolveBtn->setText(tr("Resolve Conflicts (%1)").arg(conflicts.size()));

    updateCommitButtonState();
}

// ── refreshBranches ───────────────────────────────────────────────────────────

void GitPanel::refreshBranches()
{
    if (!m_git) return;
    const QSignalBlocker blocker(m_branchCombo);
    m_branchCombo->clear();

    const QStringList localBr = m_git->localBranches();
    const QString current     = m_git->currentBranch();

    for (const QString &b : localBr)
        m_branchCombo->addItem(b, /*userData*/ QStringLiteral("local"));

    // Remote branches (best-effort): "git branch -r"
    const QString remoteOut = runGitSync({QStringLiteral("branch"),
                                          QStringLiteral("-r"),
                                          QStringLiteral("--no-color")});
    if (!remoteOut.isEmpty()) {
        QStringList remotes;
        for (const QString &raw : remoteOut.split(QLatin1Char('\n'))) {
            QString r = raw.trimmed();
            if (r.isEmpty()) continue;
            if (r.contains(QStringLiteral("->"))) continue; // skip "origin/HEAD -> origin/main"
            remotes << r;
        }
        if (!remotes.isEmpty()) {
            m_branchCombo->insertSeparator(m_branchCombo->count());
            for (const QString &r : remotes)
                m_branchCombo->addItem(r, QStringLiteral("remote"));
        }
    }

    const int idx = m_branchCombo->findText(current);
    if (idx >= 0)
        m_branchCombo->setCurrentIndex(idx);

    m_branchCombo->setEnabled(m_git->isGitRepo());
    m_newBranchBtn->setEnabled(m_git->isGitRepo());

    updateRemoteButtonsState();
}

// ── Branch switch ─────────────────────────────────────────────────────────────

void GitPanel::onBranchComboChanged(int index)
{
    if (index < 0 || !m_git) return;
    const QString selected = m_branchCombo->currentText();
    const QString kind     = m_branchCombo->itemData(index).toString();
    if (selected.isEmpty() || selected == m_git->currentBranch()) return;

    if (kind == QLatin1String("remote")) {
        // origin/feature-x  →  feature-x  (track remote)
        QString local = selected;
        const int slash = local.indexOf(QLatin1Char('/'));
        if (slash > 0) local = local.mid(slash + 1);
        const auto resp = QMessageBox::question(this,
            tr("Checkout Remote Branch"),
            tr("Create local branch '%1' tracking '%2'?").arg(local, selected));
        if (resp != QMessageBox::Yes) {
            // Revert combo to current branch.
            const QSignalBlocker b(m_branchCombo);
            const int cur = m_branchCombo->findText(m_git->currentBranch());
            if (cur >= 0) m_branchCombo->setCurrentIndex(cur);
            return;
        }
        runGitSync({QStringLiteral("checkout"),
                    QStringLiteral("-b"), local,
                    QStringLiteral("--track"), selected});
        // statusRefreshed/branchChanged will trigger re-population.
        return;
    }

    m_git->checkoutBranch(selected);
}

// ── Commit ────────────────────────────────────────────────────────────────────

void GitPanel::commitChanges()
{
    if (!m_git) return;
    const QString msg = m_commitMsg->toPlainText().trimmed();
    if (msg.isEmpty()) return;
    if (m_git->commit(msg))
        m_commitMsg->clear();
}

void GitPanel::updateCommitButtonState()
{
    if (!m_commitBtn || !m_commitMsg || !m_stagedTree) return;
    const bool hasMsg     = !m_commitMsg->toPlainText().trimmed().isEmpty();
    const bool hasStaged  = m_stagedTree->topLevelItemCount() > 0;
    const bool hasRepo    = m_git && m_git->isGitRepo();
    m_commitBtn->setEnabled(hasMsg && hasStaged && hasRepo);
}

void GitPanel::updateRemoteButtonsState()
{
    const QString remoteOut = runGitSync({QStringLiteral("remote")}, 5000);
    const bool hasRemote = !remoteOut.trimmed().isEmpty();
    m_pushBtn->setEnabled(hasRemote);
    m_pullBtn->setEnabled(hasRemote);
    m_syncBtn->setEnabled(hasRemote);
}

// ── Push / Pull / Sync ────────────────────────────────────────────────────────

void GitPanel::onPushClicked()
{
    if (!m_git || !m_git->isGitRepo()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QString out = runGitSync({QStringLiteral("push")}, 60000);
    QApplication::restoreOverrideCursor();
    if (!out.isEmpty())
        QMessageBox::information(this, tr("git push"), out);
    populateLists();
}

void GitPanel::onPullClicked()
{
    if (!m_git || !m_git->isGitRepo()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QString out = runGitSync({QStringLiteral("pull"),
                                    QStringLiteral("--ff-only")}, 60000);
    QApplication::restoreOverrideCursor();
    if (!out.isEmpty())
        QMessageBox::information(this, tr("git pull"), out);
    populateLists();
    refreshBranches();
}

void GitPanel::onSyncClicked()
{
    if (!m_git || !m_git->isGitRepo()) return;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QString out = runGitSync({QStringLiteral("pull"),
                              QStringLiteral("--ff-only")}, 60000);
    out += QLatin1Char('\n');
    out += runGitSync({QStringLiteral("push")}, 60000);
    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, tr("git sync"), out);
    populateLists();
    refreshBranches();
}

// ── File actions ──────────────────────────────────────────────────────────────

void GitPanel::stageItem(QTreeWidgetItem *item)
{
    if (!item || !m_git) return;
    const QString p = item->data(0, kFileRoleAbsPath).toString();
    if (!p.isEmpty()) m_git->stageFile(p);
}

void GitPanel::unstageItem(QTreeWidgetItem *item)
{
    if (!item || !m_git) return;
    const QString p = item->data(0, kFileRoleAbsPath).toString();
    if (!p.isEmpty()) m_git->unstageFile(p);
}

void GitPanel::discardItem(QTreeWidgetItem *item)
{
    if (!item || !m_git) return;
    const QString p = item->data(0, kFileRoleAbsPath).toString();
    if (p.isEmpty()) return;
    const QString status = item->data(0, kFileRoleStatus).toString();
    const auto resp = QMessageBox::warning(this, tr("Discard Changes"),
        tr("Discard changes to:\n\n%1\n\nThis cannot be undone.").arg(p),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (resp != QMessageBox::Yes) return;

    if (status == QLatin1String("??")) {
        // Untracked → just remove the file.
        QFile::remove(p);
    } else {
        runGitSync({QStringLiteral("checkout"),
                    QStringLiteral("--"), p});
    }
    populateLists();
}

void GitPanel::openItem(QTreeWidgetItem *item)
{
    if (!item) return;
    const QString p = item->data(0, kFileRoleAbsPath).toString();
    if (p.isEmpty()) return;
    emit openFileRequested(p);
    // Fallback: open via OS if no host listens.
    QDesktopServices::openUrl(QUrl::fromLocalFile(p));
}

void GitPanel::stageAll()
{
    if (!m_git) return;
    for (int i = 0; i < m_unstagedTree->topLevelItemCount(); ++i) {
        const QString p = m_unstagedTree->topLevelItem(i)
            ->data(0, kFileRoleAbsPath).toString();
        if (!p.isEmpty()) m_git->stageFile(p);
    }
}

void GitPanel::unstageAll()
{
    if (!m_git) return;
    for (int i = 0; i < m_stagedTree->topLevelItemCount(); ++i) {
        const QString p = m_stagedTree->topLevelItem(i)
            ->data(0, kFileRoleAbsPath).toString();
        if (!p.isEmpty()) m_git->unstageFile(p);
    }
}

void GitPanel::showFileContextMenu(const QPoint &pos, QTreeWidget *tree, bool staged)
{
    if (!tree) return;
    QTreeWidgetItem *item = tree->itemAt(pos);
    if (!item) {
        // Empty area: offer Stage All / Unstage All
        QMenu menu(this);
        if (staged) {
            auto *a = menu.addAction(tr("Unstage All"));
            connect(a, &QAction::triggered, this, &GitPanel::unstageAll);
        } else {
            auto *a = menu.addAction(tr("Stage All"));
            connect(a, &QAction::triggered, this, &GitPanel::stageAll);
        }
        menu.exec(tree->viewport()->mapToGlobal(pos));
        return;
    }

    QMenu menu(this);
    auto *aOpen     = menu.addAction(tr("Open"));
    menu.addSeparator();
    QAction *aStage   = nullptr;
    QAction *aUnstage = nullptr;
    if (staged) {
        aUnstage = menu.addAction(tr("Unstage"));
    } else {
        aStage   = menu.addAction(tr("Stage"));
    }
    auto *aDiscard  = menu.addAction(tr("Discard Changes"));

    QAction *chosen = menu.exec(tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;
    if (chosen == aOpen)         openItem(item);
    else if (chosen == aStage)   stageItem(item);
    else if (chosen == aUnstage) unstageItem(item);
    else if (chosen == aDiscard) discardItem(item);
}

// ── Diff rendering ────────────────────────────────────────────────────────────

void GitPanel::refreshDiffForCurrent()
{
    QTreeWidget *focused = nullptr;
    bool staged = false;
    if (m_stagedTree->currentItem() && m_stagedTree->hasFocus()) {
        focused = m_stagedTree;
        staged  = true;
    } else if (m_unstagedTree->currentItem()) {
        focused = m_unstagedTree;
    }
    if (!focused || !focused->currentItem()) return;
    const QString p = focused->currentItem()->data(0, kFileRoleAbsPath).toString();
    renderDiff(p, staged);
}

void GitPanel::renderDiff(const QString &absPath, bool staged)
{
    if (!m_diffView) return;
    if (absPath.isEmpty() || !m_git || !m_git->isGitRepo()) {
        m_diffView->clear();
        return;
    }

    const QString rel = QDir(m_git->workingDirectory()).relativeFilePath(absPath);

    QStringList args = {QStringLiteral("diff"), QStringLiteral("--no-color")};
    if (staged) args << QStringLiteral("--cached");
    args << QStringLiteral("--") << rel;

    QString diff = runGitSync(args);
    if (diff.isEmpty()) {
        // Untracked file: show its raw content (as added).
        const QString status = m_lastStatuses.value(absPath);
        if (status == QLatin1String("??")) {
            QFile f(absPath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString body = QString::fromUtf8(f.readAll());
                QString synth;
                synth += QStringLiteral("--- /dev/null\n");
                synth += QStringLiteral("+++ b/%1\n").arg(rel);
                synth += QStringLiteral("@@ untracked file @@\n");
                for (const QString &line : body.split(QLatin1Char('\n')))
                    synth += QStringLiteral("+") + line + QLatin1Char('\n');
                diff = synth;
            }
        }
    }

    if (diff.isEmpty()) {
        m_diffView->setHtml(QStringLiteral(
            "<div style='color:%1;padding:8px;'>%2</div>")
            .arg(QString::fromLatin1(kTextDim),
                 tr("No diff available for this file.")));
        return;
    }

    m_diffView->setHtml(colorizeDiff(diff));
}

QString GitPanel::colorizeDiff(const QString &diff) const
{
    QString html;
    html.reserve(diff.size() * 2);
    html += QStringLiteral("<pre style='margin:0;font-family:Consolas,monospace;"
                           "font-size:12px;color:%1;'>")
                .arg(QString::fromLatin1(kText));

    auto escape = [](const QString &s) {
        QString o = s.toHtmlEscaped();
        return o;
    };

    const QStringList lines = diff.split(QLatin1Char('\n'));
    for (const QString &raw : lines) {
        QString line = escape(raw);
        QString style;

        if (raw.startsWith(QLatin1String("+++")) ||
            raw.startsWith(QLatin1String("---")) ||
            raw.startsWith(QLatin1String("diff ")) ||
            raw.startsWith(QLatin1String("index "))) {
            style = QStringLiteral("color:%1;font-weight:600;")
                        .arg(QString::fromLatin1(kTextDim));
        } else if (raw.startsWith(QLatin1String("@@"))) {
            style = QStringLiteral("background:%1;color:%2;")
                        .arg(QString::fromLatin1(kDiffHunk),
                             QString::fromLatin1(kText));
        } else if (raw.startsWith(QLatin1Char('+'))) {
            style = QStringLiteral("background:%1;color:#a6e3a1;")
                        .arg(QString::fromLatin1(kDiffAddBg));
        } else if (raw.startsWith(QLatin1Char('-'))) {
            style = QStringLiteral("background:%1;color:#f38ba8;")
                        .arg(QString::fromLatin1(kDiffDelBg));
        }

        if (style.isEmpty())
            html += line + QStringLiteral("\n");
        else
            html += QStringLiteral("<span style='%1display:block;'>%2</span>")
                        .arg(style, line.isEmpty() ? QStringLiteral("&nbsp;") : line);
    }
    html += QStringLiteral("</pre>");
    return html;
}

// ── Status helpers ────────────────────────────────────────────────────────────

QString GitPanel::statusLetter(const QString &status, bool stagedSide)
{
    if (status.size() < 2)
        return QStringLiteral("?");
    if (status == QLatin1String("??"))
        return QStringLiteral("U"); // Untracked
    const QChar c = stagedSide ? status[0] : status[1];
    if (c == QLatin1Char('A')) return QStringLiteral("A");
    if (c == QLatin1Char('M')) return QStringLiteral("M");
    if (c == QLatin1Char('D')) return QStringLiteral("D");
    if (c == QLatin1Char('R')) return QStringLiteral("R");
    if (c == QLatin1Char('C')) return QStringLiteral("C");
    if (c == QLatin1Char('T')) return QStringLiteral("T");
    if (c == QLatin1Char('U')) return QStringLiteral("!"); // Unmerged
    if (c == QLatin1Char(' ')) return stagedSide ? QStringLiteral(" ")
                                                  : QStringLiteral(" ");
    return QString(c);
}

QColor GitPanel::statusColor(const QString &letter)
{
    if (letter == QLatin1String("A")) return QColor("#73c991"); // green - added
    if (letter == QLatin1String("M")) return QColor("#e2c08d"); // yellow - modified
    if (letter == QLatin1String("D")) return QColor("#f48771"); // red - deleted
    if (letter == QLatin1String("U")) return QColor("#73c991"); // untracked = green-ish
    if (letter == QLatin1String("R")) return QColor("#75beff"); // blue - renamed
    if (letter == QLatin1String("C")) return QColor("#75beff"); // blue - copied
    if (letter == QLatin1String("!")) return QColor("#f14c4c"); // red - conflict
    return QColor("#cccccc");
}

// ── Synchronous git runner (used for push/pull/diff/branch listing) ──────────

QString GitPanel::runGitSync(const QStringList &args, int timeoutMs) const
{
    if (!m_git || m_git->workingDirectory().isEmpty())
        return {};
    QProcess p;
    p.setWorkingDirectory(m_git->workingDirectory());
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(QStringLiteral("git"), args);
    if (!p.waitForStarted(2000)) return {};
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        return {};
    }
    return QString::fromUtf8(p.readAll()).trimmed();
}
