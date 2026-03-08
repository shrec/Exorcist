#pragma once

#include <QWidget>

class QTreeWidget;
class QTextEdit;
class QToolButton;
class QTreeWidgetItem;
class QTabWidget;
class QTableWidget;
class ProjectBrainService;

/// Panel to browse and edit AI memory files and Project Brain data.
/// Has tabs: Files (original memory file browser), Facts, Rules, Sessions.
class MemoryBrowserPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryBrowserPanel(const QString &basePath, QWidget *parent = nullptr);

    void refresh();

    /// Set the project brain service to enable Facts/Rules/Sessions tabs.
    void setBrainService(ProjectBrainService *brain);

private slots:
    void onItemClicked(QTreeWidgetItem *item);
    void saveCurrentFile();
    void deleteCurrentFile();
    void createNewFile();

    void refreshFacts();
    void refreshRules();
    void refreshSessions();
    void addFact();
    void deleteFact();
    void addRule();
    void deleteRule();

private:
    void populateTree(const QString &dirPath, QTreeWidgetItem *parentItem);
    void setupFilesTab();
    void setupFactsTab();
    void setupRulesTab();
    void setupSessionsTab();

    QString              m_basePath;
    ProjectBrainService *m_brain = nullptr;

    // Tabs
    QTabWidget   *m_tabWidget;

    // Files tab
    QWidget      *m_filesTab;
    QTreeWidget  *m_tree;
    QTextEdit    *m_editor;
    QToolButton  *m_saveBtn;
    QToolButton  *m_deleteBtn;
    QToolButton  *m_newBtn;
    QToolButton  *m_refreshBtn;
    QString       m_currentFilePath;

    // Facts tab
    QWidget      *m_factsTab;
    QTableWidget *m_factsTable;

    // Rules tab
    QWidget      *m_rulesTab;
    QTableWidget *m_rulesTable;

    // Sessions tab
    QWidget      *m_sessionsTab;
    QTableWidget *m_sessionsTable;
};
