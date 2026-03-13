#include "ghpanel.h"
#include "ghservice.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

// ────────────────────────────────────────────────────────────────
// Construction
// ────────────────────────────────────────────────────────────────

GhPanel::GhPanel(GhService *gh, QWidget *parent)
    : QWidget(parent), m_gh(gh)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Header row ─────────────────────────────────
    auto *header = new QHBoxLayout;
    m_authLabel = new QLabel(tr("Checking gh auth…"), this);
    m_refreshBtn = new QPushButton(tr("⟳"), this);
    m_refreshBtn->setToolTip(tr("Refresh"));
    m_refreshBtn->setFixedWidth(32);
    header->addWidget(m_authLabel, 1);
    header->addWidget(m_refreshBtn);
    root->addLayout(header);

    // ── Stacked widget: list page vs detail page ───
    m_stack = new QStackedWidget(this);

    // List page (tabs)
    m_listPage = new QWidget;
    auto *listLayout = new QVBoxLayout(m_listPage);
    listLayout->setContentsMargins(0, 0, 0, 0);
    m_tabs = new QTabWidget(m_listPage);
    m_tabs->addTab(buildPrTab(),       tr("Pull Requests"));
    m_tabs->addTab(buildIssueTab(),    tr("Issues"));
    m_tabs->addTab(buildActionsTab(),  tr("Actions"));
    m_tabs->addTab(buildReleasesTab(), tr("Releases"));
    listLayout->addWidget(m_tabs);
    m_stack->addWidget(m_listPage);

    // Detail page
    m_detailPage = new QWidget;
    auto *detailLayout = new QVBoxLayout(m_detailPage);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    m_backBtn = new QPushButton(tr("← Back"), m_detailPage);
    m_detailView = new QTextEdit(m_detailPage);
    m_detailView->setReadOnly(true);
    detailLayout->addWidget(m_backBtn);
    detailLayout->addWidget(m_detailView);
    m_stack->addWidget(m_detailPage);

    root->addWidget(m_stack);
    m_stack->setCurrentWidget(m_listPage);

    // ── Connections ────────────────────────────────
    connect(m_refreshBtn, &QPushButton::clicked, this, &GhPanel::refresh);
    connect(m_backBtn, &QPushButton::clicked, this, &GhPanel::hideDetail);

    connect(this, &GhPanel::openUrlRequested, this, [](const QString &url) {
        QDesktopServices::openUrl(QUrl(url));
    });

    // Auth check on construction
    m_gh->checkAuth([this](bool ok, const QString &user) {
        if (ok)
            m_authLabel->setText(tr("GitHub: %1").arg(user));
        else
            m_authLabel->setText(tr("GitHub: not authenticated (run gh auth login)"));
    });
}

// ────────────────────────────────────────────────────────────────
// Tab builders
// ────────────────────────────────────────────────────────────────

QWidget *GhPanel::buildPrTab()
{
    auto *w = new QWidget;
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 0);

    // Filter + action bar
    auto *bar = new QHBoxLayout;
    m_prStateFilter = new QComboBox(w);
    m_prStateFilter->addItems({tr("open"), tr("closed"), tr("merged"), tr("all")});
    bar->addWidget(new QLabel(tr("State:"), w));
    bar->addWidget(m_prStateFilter);
    bar->addStretch();

    auto *createBtn = new QPushButton(tr("New PR"), w);
    auto *mergeBtn = new QPushButton(tr("Merge"), w);
    auto *checkoutBtn = new QPushButton(tr("Checkout"), w);
    bar->addWidget(createBtn);
    bar->addWidget(mergeBtn);
    bar->addWidget(checkoutBtn);
    layout->addLayout(bar);

    // Tree
    m_prTree = new QTreeWidget(w);
    m_prTree->setHeaderLabels({tr("#"), tr("Title"), tr("Author"), tr("Branch"), tr("State")});
    m_prTree->setRootIsDecorated(false);
    m_prTree->setAlternatingRowColors(true);
    m_prTree->header()->setStretchLastSection(false);
    m_prTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_prTree);

    // Connections
    connect(m_prStateFilter, &QComboBox::currentIndexChanged, this, [this]() { refreshPrs(); });
    connect(createBtn, &QPushButton::clicked, this, &GhPanel::onCreatePr);
    connect(mergeBtn, &QPushButton::clicked, this, &GhPanel::onMergePr);
    connect(checkoutBtn, &QPushButton::clicked, this, &GhPanel::onCheckoutPr);
    connect(m_prTree, &QTreeWidget::itemDoubleClicked, this, &GhPanel::onPrDoubleClicked);

    return w;
}

QWidget *GhPanel::buildIssueTab()
{
    auto *w = new QWidget;
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 0);

    auto *bar = new QHBoxLayout;
    m_issueStateFilter = new QComboBox(w);
    m_issueStateFilter->addItems({tr("open"), tr("closed"), tr("all")});
    bar->addWidget(new QLabel(tr("State:"), w));
    bar->addWidget(m_issueStateFilter);
    bar->addStretch();

    auto *createBtn = new QPushButton(tr("New Issue"), w);
    auto *closeBtn = new QPushButton(tr("Close"), w);
    auto *commentBtn = new QPushButton(tr("Comment"), w);
    bar->addWidget(createBtn);
    bar->addWidget(closeBtn);
    bar->addWidget(commentBtn);
    layout->addLayout(bar);

    m_issueTree = new QTreeWidget(w);
    m_issueTree->setHeaderLabels({tr("#"), tr("Title"), tr("Author"), tr("Labels"), tr("State")});
    m_issueTree->setRootIsDecorated(false);
    m_issueTree->setAlternatingRowColors(true);
    m_issueTree->header()->setStretchLastSection(false);
    m_issueTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_issueTree);

    connect(m_issueStateFilter, &QComboBox::currentIndexChanged, this, [this]() { refreshIssues(); });
    connect(createBtn, &QPushButton::clicked, this, &GhPanel::onCreateIssue);
    connect(closeBtn, &QPushButton::clicked, this, &GhPanel::onCloseIssue);
    connect(commentBtn, &QPushButton::clicked, this, &GhPanel::onCommentIssue);
    connect(m_issueTree, &QTreeWidget::itemDoubleClicked, this, &GhPanel::onIssueDoubleClicked);

    return w;
}

QWidget *GhPanel::buildActionsTab()
{
    auto *w = new QWidget;
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 0);

    m_actionsTree = new QTreeWidget(w);
    m_actionsTree->setHeaderLabels({tr("ID"), tr("Title"), tr("Status"), tr("Branch"), tr("Event")});
    m_actionsTree->setRootIsDecorated(false);
    m_actionsTree->setAlternatingRowColors(true);
    m_actionsTree->header()->setStretchLastSection(false);
    m_actionsTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_actionsTree);

    connect(m_actionsTree, &QTreeWidget::itemDoubleClicked, this, &GhPanel::onRunDoubleClicked);

    return w;
}

QWidget *GhPanel::buildReleasesTab()
{
    auto *w = new QWidget;
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 2, 0, 0);

    auto *bar = new QHBoxLayout;
    bar->addStretch();
    auto *createBtn = new QPushButton(tr("New Release"), w);
    bar->addWidget(createBtn);
    layout->addLayout(bar);

    m_releasesTree = new QTreeWidget(w);
    m_releasesTree->setHeaderLabels({tr("Tag"), tr("Name"), tr("Published"), tr("Status")});
    m_releasesTree->setRootIsDecorated(false);
    m_releasesTree->setAlternatingRowColors(true);
    m_releasesTree->header()->setStretchLastSection(false);
    m_releasesTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    layout->addWidget(m_releasesTree);

    connect(createBtn, &QPushButton::clicked, this, &GhPanel::onCreateRelease);

    return w;
}

// ────────────────────────────────────────────────────────────────
// Refresh all
// ────────────────────────────────────────────────────────────────

void GhPanel::refresh()
{
    m_gh->checkAuth([this](bool ok, const QString &user) {
        if (ok)
            m_authLabel->setText(tr("GitHub: %1").arg(user));
        else
            m_authLabel->setText(tr("GitHub: not authenticated"));
    });

    refreshPrs();
    refreshIssues();
    refreshActions();
    refreshReleases();
}

// ────────────────────────────────────────────────────────────────
// Pull Requests
// ────────────────────────────────────────────────────────────────

void GhPanel::refreshPrs()
{
    const QString state = m_prStateFilter->currentText();
    m_gh->listPullRequests(state, [this](bool ok, const QJsonArray &prs) {
        m_prTree->clear();
        if (!ok) return;

        for (const auto &val : prs) {
            const auto pr = val.toObject();
            auto *item = new QTreeWidgetItem(m_prTree);
            item->setText(0, QString::number(pr[QStringLiteral("number")].toInt()));
            item->setText(1, pr[QStringLiteral("title")].toString());
            item->setText(2, pr[QStringLiteral("author")].toObject()[QStringLiteral("login")].toString());
            item->setText(3, pr[QStringLiteral("headRefName")].toString());
            item->setText(4, pr[QStringLiteral("state")].toString());
            item->setData(0, Qt::UserRole, pr[QStringLiteral("url")].toString());
            item->setData(0, Qt::UserRole + 1, pr[QStringLiteral("number")].toInt());
        }
    });
}

void GhPanel::onPrDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    const int num = item->data(0, Qt::UserRole + 1).toInt();
    m_gh->viewPullRequest(num, [this](bool ok, const QJsonObject &pr) {
        if (!ok) return;

        QString html = QStringLiteral(
            "<h2>PR #%1: %2</h2>"
            "<p><b>Author:</b> %3 | <b>State:</b> %4 | <b>Review:</b> %5</p>"
            "<p><b>Branch:</b> %6 → %7</p>"
            "<p><b>Changes:</b> +%8 / -%9</p>"
            "<hr><p>%10</p>"
            "<p><a href=\"%11\">Open in browser</a></p>")
            .arg(pr[QStringLiteral("number")].toInt())
            .arg(pr[QStringLiteral("title")].toString())
            .arg(pr[QStringLiteral("author")].toObject()[QStringLiteral("login")].toString())
            .arg(pr[QStringLiteral("state")].toString())
            .arg(pr[QStringLiteral("reviewDecision")].toString())
            .arg(pr[QStringLiteral("headRefName")].toString())
            .arg(pr[QStringLiteral("baseRefName")].toString())
            .arg(pr[QStringLiteral("additions")].toInt())
            .arg(pr[QStringLiteral("deletions")].toInt())
            .arg(pr[QStringLiteral("body")].toString().toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>")))
            .arg(pr[QStringLiteral("url")].toString());

        // Append comments
        const auto comments = pr[QStringLiteral("comments")].toArray();
        if (!comments.isEmpty()) {
            html += QStringLiteral("<hr><h3>Comments (%1)</h3>").arg(comments.size());
            for (const auto &c : comments) {
                const auto co = c.toObject();
                html += QStringLiteral("<p><b>%1</b> (%2):<br>%3</p>")
                    .arg(co[QStringLiteral("author")].toObject()[QStringLiteral("login")].toString())
                    .arg(co[QStringLiteral("createdAt")].toString().left(10))
                    .arg(co[QStringLiteral("body")].toString().toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>")));
            }
        }
        showDetail(html);
    });
}

void GhPanel::onCreatePr()
{
    bool ok1 = false, ok2 = false;
    const QString title = QInputDialog::getText(this, tr("New Pull Request"), tr("Title:"),
                                                 QLineEdit::Normal, {}, &ok1);
    if (!ok1 || title.isEmpty()) return;

    const QString body = QInputDialog::getMultiLineText(this, tr("New Pull Request"),
                                                         tr("Description:"), {}, &ok2);
    if (!ok2) return;

    m_gh->createPullRequest(title, body, {}, {}, [this](bool ok, const QJsonObject &pr) {
        if (ok) {
            QMessageBox::information(this, tr("PR Created"),
                                      tr("Pull request created: %1").arg(pr[QStringLiteral("url")].toString()));
            refreshPrs();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to create pull request."));
        }
    });
}

void GhPanel::onMergePr()
{
    auto *current = m_prTree->currentItem();
    if (!current) return;

    const int num = current->data(0, Qt::UserRole + 1).toInt();
    QStringList methods = {tr("merge"), tr("squash"), tr("rebase")};
    bool ok = false;
    const QString method = QInputDialog::getItem(this, tr("Merge PR #%1").arg(num),
                                                  tr("Merge method:"), methods, 0, false, &ok);
    if (!ok) return;

    m_gh->mergePullRequest(num, method, [this](bool ok, const QString &output) {
        Q_UNUSED(output)
        if (ok) {
            QMessageBox::information(this, tr("Merged"), tr("Pull request merged successfully."));
            refreshPrs();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to merge pull request."));
        }
    });
}

void GhPanel::onCheckoutPr()
{
    auto *current = m_prTree->currentItem();
    if (!current) return;

    const int num = current->data(0, Qt::UserRole + 1).toInt();
    m_gh->checkoutPullRequest(num, [this, num](bool ok, const QString &output) {
        Q_UNUSED(output)
        if (ok)
            QMessageBox::information(this, tr("Checked Out"), tr("PR #%1 branch checked out.").arg(num));
        else
            QMessageBox::warning(this, tr("Error"), tr("Failed to checkout PR branch."));
    });
}

// ────────────────────────────────────────────────────────────────
// Issues
// ────────────────────────────────────────────────────────────────

void GhPanel::refreshIssues()
{
    const QString state = m_issueStateFilter->currentText();
    m_gh->listIssues(state, [this](bool ok, const QJsonArray &issues) {
        m_issueTree->clear();
        if (!ok) return;

        for (const auto &val : issues) {
            const auto issue = val.toObject();
            auto *item = new QTreeWidgetItem(m_issueTree);
            item->setText(0, QString::number(issue[QStringLiteral("number")].toInt()));
            item->setText(1, issue[QStringLiteral("title")].toString());
            item->setText(2, issue[QStringLiteral("author")].toObject()[QStringLiteral("login")].toString());

            // Labels as comma-separated
            QStringList labels;
            for (const auto &l : issue[QStringLiteral("labels")].toArray())
                labels << l.toObject()[QStringLiteral("name")].toString();
            item->setText(3, labels.join(QStringLiteral(", ")));
            item->setText(4, issue[QStringLiteral("state")].toString());
            item->setData(0, Qt::UserRole, issue[QStringLiteral("url")].toString());
            item->setData(0, Qt::UserRole + 1, issue[QStringLiteral("number")].toInt());
        }
    });
}

void GhPanel::onIssueDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    const int num = item->data(0, Qt::UserRole + 1).toInt();
    m_gh->viewIssue(num, [this](bool ok, const QJsonObject &issue) {
        if (!ok) return;

        QString html = QStringLiteral(
            "<h2>#%1: %2</h2>"
            "<p><b>Author:</b> %3 | <b>State:</b> %4</p>"
            "<hr><p>%5</p>"
            "<p><a href=\"%6\">Open in browser</a></p>")
            .arg(issue[QStringLiteral("number")].toInt())
            .arg(issue[QStringLiteral("title")].toString())
            .arg(issue[QStringLiteral("author")].toObject()[QStringLiteral("login")].toString())
            .arg(issue[QStringLiteral("state")].toString())
            .arg(issue[QStringLiteral("body")].toString().toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>")))
            .arg(issue[QStringLiteral("url")].toString());

        const auto comments = issue[QStringLiteral("comments")].toArray();
        if (!comments.isEmpty()) {
            html += QStringLiteral("<hr><h3>Comments (%1)</h3>").arg(comments.size());
            for (const auto &c : comments) {
                const auto co = c.toObject();
                html += QStringLiteral("<p><b>%1</b> (%2):<br>%3</p>")
                    .arg(co[QStringLiteral("author")].toObject()[QStringLiteral("login")].toString())
                    .arg(co[QStringLiteral("createdAt")].toString().left(10))
                    .arg(co[QStringLiteral("body")].toString().toHtmlEscaped().replace(QStringLiteral("\n"), QStringLiteral("<br>")));
            }
        }
        showDetail(html);
    });
}

void GhPanel::onCreateIssue()
{
    bool ok1 = false, ok2 = false;
    const QString title = QInputDialog::getText(this, tr("New Issue"), tr("Title:"),
                                                 QLineEdit::Normal, {}, &ok1);
    if (!ok1 || title.isEmpty()) return;

    const QString body = QInputDialog::getMultiLineText(this, tr("New Issue"),
                                                         tr("Description:"), {}, &ok2);
    if (!ok2) return;

    m_gh->createIssue(title, body, [this](bool ok, const QJsonObject &issue) {
        if (ok) {
            QMessageBox::information(this, tr("Issue Created"),
                                      tr("Issue created: %1").arg(issue[QStringLiteral("url")].toString()));
            refreshIssues();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to create issue."));
        }
    });
}

void GhPanel::onCloseIssue()
{
    auto *current = m_issueTree->currentItem();
    if (!current) return;

    const int num = current->data(0, Qt::UserRole + 1).toInt();
    m_gh->closeIssue(num, [this](bool ok, const QString &) {
        if (ok) refreshIssues();
        else QMessageBox::warning(this, tr("Error"), tr("Failed to close issue."));
    });
}

void GhPanel::onCommentIssue()
{
    auto *current = m_issueTree->currentItem();
    if (!current) return;

    const int num = current->data(0, Qt::UserRole + 1).toInt();
    bool ok = false;
    const QString body = QInputDialog::getMultiLineText(this, tr("Comment on Issue #%1").arg(num),
                                                         tr("Comment:"), {}, &ok);
    if (!ok || body.isEmpty()) return;

    m_gh->commentOnIssue(num, body, [this](bool ok2, const QString &) {
        if (ok2) {
            QMessageBox::information(this, tr("Comment Added"), tr("Comment posted successfully."));
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to post comment."));
        }
    });
}

// ────────────────────────────────────────────────────────────────
// Actions (Workflow Runs)
// ────────────────────────────────────────────────────────────────

void GhPanel::refreshActions()
{
    m_gh->listWorkflowRuns(20, [this](bool ok, const QJsonArray &runs) {
        m_actionsTree->clear();
        if (!ok) return;

        for (const auto &val : runs) {
            const auto run = val.toObject();
            auto *item = new QTreeWidgetItem(m_actionsTree);
            item->setText(0, QString::number(run[QStringLiteral("databaseId")].toInt()));
            item->setText(1, run[QStringLiteral("displayTitle")].toString());

            const QString status = run[QStringLiteral("status")].toString();
            const QString conclusion = run[QStringLiteral("conclusion")].toString();
            item->setText(2, conclusion.isEmpty() ? status : conclusion);
            item->setText(3, run[QStringLiteral("headBranch")].toString());
            item->setText(4, run[QStringLiteral("event")].toString());
            item->setData(0, Qt::UserRole, run[QStringLiteral("url")].toString());
            item->setData(0, Qt::UserRole + 1, run[QStringLiteral("databaseId")].toInt());

            // Color coding
            if (conclusion == QStringLiteral("success"))
                for (int c = 0; c < 5; ++c) item->setForeground(c, QColor(0x4e, 0xc9, 0x4e));
            else if (conclusion == QStringLiteral("failure"))
                for (int c = 0; c < 5; ++c) item->setForeground(c, QColor(0xe0, 0x50, 0x50));
            else if (status == QStringLiteral("in_progress"))
                for (int c = 0; c < 5; ++c) item->setForeground(c, QColor(0xf0, 0xc0, 0x40));
        }
    });
}

void GhPanel::onRunDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    const QString url = item->data(0, Qt::UserRole).toString();
    if (!url.isEmpty())
        emit openUrlRequested(url);
}

// ────────────────────────────────────────────────────────────────
// Releases
// ────────────────────────────────────────────────────────────────

void GhPanel::refreshReleases()
{
    m_gh->listReleases([this](bool ok, const QJsonArray &releases) {
        m_releasesTree->clear();
        if (!ok) return;

        for (const auto &val : releases) {
            const auto rel = val.toObject();
            auto *item = new QTreeWidgetItem(m_releasesTree);
            item->setText(0, rel[QStringLiteral("tagName")].toString());
            item->setText(1, rel[QStringLiteral("name")].toString());
            item->setText(2, rel[QStringLiteral("publishedAt")].toString().left(10));

            QString status;
            if (rel[QStringLiteral("isDraft")].toBool()) status = tr("Draft");
            else if (rel[QStringLiteral("isPrerelease")].toBool()) status = tr("Pre-release");
            else status = tr("Published");
            item->setText(3, status);
            item->setData(0, Qt::UserRole, rel[QStringLiteral("url")].toString());
        }
    });
}

void GhPanel::onCreateRelease()
{
    bool ok1 = false, ok2 = false, ok3 = false;
    const QString tag = QInputDialog::getText(this, tr("New Release"), tr("Tag (e.g. v1.0.0):"),
                                               QLineEdit::Normal, {}, &ok1);
    if (!ok1 || tag.isEmpty()) return;

    const QString title = QInputDialog::getText(this, tr("New Release"), tr("Title:"),
                                                 QLineEdit::Normal, tag, &ok2);
    if (!ok2) return;

    const QString notes = QInputDialog::getMultiLineText(this, tr("New Release"),
                                                          tr("Release notes:"), {}, &ok3);
    if (!ok3) return;

    m_gh->createRelease(tag, title, notes, false, false,
                        [this](bool ok, const QJsonObject &rel) {
        if (ok) {
            QMessageBox::information(this, tr("Release Created"),
                                      tr("Release created: %1").arg(rel[QStringLiteral("url")].toString()));
            refreshReleases();
        } else {
            QMessageBox::warning(this, tr("Error"), tr("Failed to create release."));
        }
    });
}

// ────────────────────────────────────────────────────────────────
// Detail view
// ────────────────────────────────────────────────────────────────

void GhPanel::showDetail(const QString &html)
{
    m_detailView->setHtml(html);
    m_stack->setCurrentWidget(m_detailPage);
}

void GhPanel::hideDetail()
{
    m_stack->setCurrentWidget(m_listPage);
}
