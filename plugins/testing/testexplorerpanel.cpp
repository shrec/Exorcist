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

    // VS2022-style toolbar
    static const char *kToolbarStyle =
        "QWidget { background: #2d2d30; }"
        "QPushButton {"
        "  color: #d4d4d4; background: transparent; border: 1px solid transparent;"
        "  padding: 2px 10px; font-size: 12px;"
        "}"
        "QPushButton:hover { border-color: #555558; background: #3e3e42; }"
        "QPushButton:pressed { background: #094771; }"
        "QPushButton:disabled { color: #555558; }"
        "QLabel { color: #9d9d9d; font-size: 12px; background: transparent; }";

    auto *toolbarWidget = new QWidget;
    toolbarWidget->setFixedHeight(30);
    toolbarWidget->setStyleSheet(QLatin1String(kToolbarStyle));

    auto *toolbar = new QHBoxLayout(toolbarWidget);
    toolbar->setContentsMargins(6, 2, 6, 2);
    toolbar->setSpacing(4);

    m_runAllBtn = new QPushButton(tr("▶ Run All"));
    m_runAllBtn->setEnabled(false);
    toolbar->addWidget(m_runAllBtn);

    m_refreshBtn = new QPushButton(tr("↻ Refresh"));
    m_refreshBtn->setEnabled(false);
    toolbar->addWidget(m_refreshBtn);

    toolbar->addStretch();

    m_summaryLabel = new QLabel;
    toolbar->addWidget(m_summaryLabel);

    layout->addWidget(toolbarWidget);

    // Tree
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Test"), tr("Status"), tr("Duration")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget {"
        "  background: #1e1e1e; color: #d4d4d4; border: none; font-size: 12px;"
        "}"
        "QTreeWidget::item { padding: 3px 0; border: none; }"
        "QTreeWidget::item:alternate { background: #252526; }"
        "QTreeWidget::item:hover { background: #2a2d2e; }"
        "QTreeWidget::item:selected { background: #094771; color: #ffffff; }"
        "QHeaderView::section {"
        "  background: #252526; color: #858585; border: none;"
        "  border-right: 1px solid #3e3e42; border-bottom: 1px solid #3e3e42;"
        "  padding: 3px 6px; font-size: 11px;"
        "}"
        "QScrollBar:vertical { background: #1e1e1e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #424242; min-height: 20px; border-radius: 5px; }"
        "QScrollBar::handle:vertical:hover { background: #686868; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
    m_tree->setAlternatingRowColors(true);
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
        m_summaryLabel->setStyleSheet(QStringLiteral("color: #f14c4c; font-weight: bold;"));
    } else if (passed == total) {
        m_summaryLabel->setStyleSheet(QStringLiteral("color: #73c991; font-weight: bold;"));
    } else {
        m_summaryLabel->setStyleSheet(QStringLiteral("color: #9d9d9d;"));
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
