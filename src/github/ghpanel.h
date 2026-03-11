#pragma once

#include <QWidget>
#include <QJsonArray>
#include <QJsonObject>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTabWidget;
class QTextEdit;
class QTreeWidget;
class QTreeWidgetItem;

class GhService;

/// GitHub integration panel — PRs, Issues, Actions, Releases.
class GhPanel : public QWidget
{
    Q_OBJECT

public:
    explicit GhPanel(GhService *gh, QWidget *parent = nullptr);

    /// Refresh all tabs.
    void refresh();

signals:
    void openUrlRequested(const QString &url);

private:
    // Tab builders
    QWidget *buildPrTab();
    QWidget *buildIssueTab();
    QWidget *buildActionsTab();
    QWidget *buildReleasesTab();

    // PR actions
    void refreshPrs();
    void onPrDoubleClicked(QTreeWidgetItem *item, int column);
    void onCreatePr();
    void onMergePr();
    void onCheckoutPr();

    // Issue actions
    void refreshIssues();
    void onIssueDoubleClicked(QTreeWidgetItem *item, int column);
    void onCreateIssue();
    void onCloseIssue();
    void onCommentIssue();

    // Actions
    void refreshActions();
    void onRunDoubleClicked(QTreeWidgetItem *item, int column);

    // Releases
    void refreshReleases();
    void onCreateRelease();

    // Detail view
    void showDetail(const QString &html);
    void hideDetail();

    GhService *m_gh;

    // Header
    QLabel *m_authLabel;
    QPushButton *m_refreshBtn;

    // Tabs
    QTabWidget *m_tabs;

    // PR tab
    QComboBox *m_prStateFilter;
    QTreeWidget *m_prTree;

    // Issue tab
    QComboBox *m_issueStateFilter;
    QTreeWidget *m_issueTree;

    // Actions tab
    QTreeWidget *m_actionsTree;

    // Releases tab
    QTreeWidget *m_releasesTree;

    // Detail panel (shared)
    QStackedWidget *m_stack;
    QWidget *m_listPage;
    QWidget *m_detailPage;
    QTextEdit *m_detailView;
    QPushButton *m_backBtn;
};
