#pragma once

#include <QCloseEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMetaObject>
#include <QModelIndex>
#include <memory>

#include "core/ifilesystem.h"
#include "pluginmanager.h"
#include "serviceregistry.h"

class QDockWidget;
class QFileSystemModel;
class QTabWidget;
class QTreeView;
class EditorView;
class SearchService;
class SearchPanel;
class AgentOrchestrator;
class AgentChatPanel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void setupUi();
    void setupMenus();
    void setupToolBar();
    void setupStatusBar();
    void createDockWidgets();
    void openNewTab();
    void openFileFromIndex(const QModelIndex &index);
    void openFile(const QString &path);
    void openFolder(const QString &path = {});
    void saveCurrentTab();
    void showFilePalette();
    void showCommandPalette();
    void updateEditorStatus(EditorView *editor);
    void onTabChanged(int index);
    EditorView *currentEditor() const;
    void loadPlugins();
    void loadSettings();
    void saveSettings();
    void closeEvent(QCloseEvent *event) override;

    QFileSystemModel *m_fileModel;
    QTreeView        *m_fileTree;
    QTabWidget       *m_tabs;
    SearchPanel      *m_searchPanel;

    QDockWidget *m_projectDock;
    QDockWidget *m_searchDock;
    QDockWidget *m_terminalDock;
    QDockWidget *m_aiDock;

    QLabel *m_posLabel;
    QLabel *m_encodingLabel;
    QLabel *m_backgroundLabel;   // shows active background jobs

    std::unique_ptr<PluginManager>   m_pluginManager;
    std::unique_ptr<ServiceRegistry> m_services;
    std::unique_ptr<IFileSystem>     m_fileSystem;
    SearchService    *m_searchService;
    AgentOrchestrator *m_agentOrchestrator;
    AgentChatPanel   *m_chatPanel;
    QMetaObject::Connection m_cursorConn;   // tracks current editor cursor signal
};
