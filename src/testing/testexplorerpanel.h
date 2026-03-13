#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QLabel;
class TestDiscoveryService;

/// Dock panel for discovering, running, and viewing test results.
class TestExplorerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TestExplorerPanel(QWidget *parent = nullptr);

    /// Set the discovery service (not owned).
    void setDiscoveryService(TestDiscoveryService *svc);

signals:
    void navigateToTest(const QString &file, int line);

private slots:
    void onDiscoveryFinished();
    void onTestStarted(int index);
    void onTestFinished(int index);
    void onAllTestsFinished();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    void refreshTree();
    void updateSummary();
    QIcon statusIcon(int status) const;

    TestDiscoveryService *m_svc = nullptr;
    QTreeWidget  *m_tree        = nullptr;
    QPushButton  *m_runAllBtn   = nullptr;
    QPushButton  *m_refreshBtn  = nullptr;
    QLabel       *m_summaryLabel = nullptr;
};
