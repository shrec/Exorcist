#include "testexplorerpanel.h"
#include "testdiscoveryservice.h"

#include <QTreeWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>

TestExplorerPanel::TestExplorerPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);

    m_runAllBtn = new QPushButton(tr("Run All"));
    m_runAllBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_runAllBtn->setEnabled(false);
    toolbar->addWidget(m_runAllBtn);

    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    m_refreshBtn->setEnabled(false);
    toolbar->addWidget(m_refreshBtn);

    toolbar->addStretch();

    m_summaryLabel = new QLabel;
    toolbar->addWidget(m_summaryLabel);

    layout->addLayout(toolbar);

    // Tree
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Test"), tr("Status"), tr("Duration")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(false);
    layout->addWidget(m_tree);

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &TestExplorerPanel::onItemDoubleClicked);
}

void TestExplorerPanel::setDiscoveryService(TestDiscoveryService *svc)
{
    if (m_svc) {
        disconnect(m_svc, nullptr, this, nullptr);
    }

    m_svc = svc;
    if (!m_svc)
        return;

    connect(m_svc, &TestDiscoveryService::discoveryFinished,
            this, &TestExplorerPanel::onDiscoveryFinished);
    connect(m_svc, &TestDiscoveryService::testStarted,
            this, &TestExplorerPanel::onTestStarted);
    connect(m_svc, &TestDiscoveryService::testFinished,
            this, &TestExplorerPanel::onTestFinished);
    connect(m_svc, &TestDiscoveryService::allTestsFinished,
            this, &TestExplorerPanel::onAllTestsFinished);

    connect(m_runAllBtn, &QPushButton::clicked, m_svc, &TestDiscoveryService::runAllTests);
    connect(m_refreshBtn, &QPushButton::clicked, m_svc, &TestDiscoveryService::discoverTests);

    m_runAllBtn->setEnabled(true);
    m_refreshBtn->setEnabled(true);
}

void TestExplorerPanel::onDiscoveryFinished()
{
    refreshTree();
    updateSummary();
}

void TestExplorerPanel::onTestStarted(int index)
{
    if (index < 0 || index >= m_tree->topLevelItemCount())
        return;

    auto *item = m_tree->topLevelItem(index);
    item->setIcon(1, style()->standardIcon(QStyle::SP_BrowserReload));
    item->setText(1, tr("Running"));
    item->setText(2, {});
}

void TestExplorerPanel::onTestFinished(int index)
{
    if (!m_svc || index < 0 || index >= m_tree->topLevelItemCount())
        return;

    const auto &test = m_svc->tests().at(index);
    auto *item = m_tree->topLevelItem(index);

    item->setIcon(1, statusIcon(test.status));
    switch (test.status) {
    case TestItem::Passed:  item->setText(1, tr("Passed"));  break;
    case TestItem::Failed:  item->setText(1, tr("Failed"));  break;
    case TestItem::Skipped: item->setText(1, tr("Skipped")); break;
    default:                item->setText(1, tr("Unknown")); break;
    }

    if (test.duration > 0.0) {
        if (test.duration < 1.0)
            item->setText(2, QStringLiteral("%1 ms").arg(test.duration * 1000.0, 0, 'f', 0));
        else
            item->setText(2, QStringLiteral("%1 s").arg(test.duration, 0, 'f', 2));
    }

    // Store output as tooltip for quick access
    item->setToolTip(0, test.output.left(2000));

    updateSummary();
}

void TestExplorerPanel::onAllTestsFinished()
{
    m_runAllBtn->setEnabled(true);
    updateSummary();
}

void TestExplorerPanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!m_svc)
        return;

    int idx = m_tree->indexOfTopLevelItem(item);
    if (idx >= 0 && idx < m_svc->tests().size()) {
        // Run individual test on double-click
        m_svc->runTest(idx);
    }
}

void TestExplorerPanel::refreshTree()
{
    m_tree->clear();
    if (!m_svc)
        return;

    for (const auto &test : m_svc->tests()) {
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, test.name);
        item->setIcon(1, statusIcon(test.status));
        item->setText(1, tr("Not run"));
    }
}

void TestExplorerPanel::updateSummary()
{
    if (!m_svc || m_svc->tests().isEmpty()) {
        m_summaryLabel->setText({});
        return;
    }

    int passed = 0, failed = 0, total = m_svc->tests().size();
    for (const auto &t : m_svc->tests()) {
        if (t.status == TestItem::Passed)  ++passed;
        if (t.status == TestItem::Failed)  ++failed;
    }

    m_summaryLabel->setText(tr("%1/%2 passed").arg(passed).arg(total));

    if (failed > 0) {
        m_summaryLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
    } else if (passed == total) {
        m_summaryLabel->setStyleSheet(QStringLiteral("color: #2ecc71; font-weight: bold;"));
    } else {
        m_summaryLabel->setStyleSheet({});
    }
}

QIcon TestExplorerPanel::statusIcon(int status) const
{
    switch (status) {
    case TestItem::Passed:  return style()->standardIcon(QStyle::SP_DialogApplyButton);
    case TestItem::Failed:  return style()->standardIcon(QStyle::SP_DialogCancelButton);
    case TestItem::Running: return style()->standardIcon(QStyle::SP_BrowserReload);
    case TestItem::Skipped: return style()->standardIcon(QStyle::SP_MessageBoxWarning);
    default:                return style()->standardIcon(QStyle::SP_FileIcon);
    }
}
