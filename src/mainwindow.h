#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include <memory>

#include "core/ifilesystem.h"
#include "pluginmanager.h"
#include "serviceregistry.h"

class QFileSystemModel;
class QTabWidget;
class QTreeView;
class EditorView;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void setupUi();
    void setupMenus();
    void setupToolBar();
    void createDockWidgets();
    void openNewTab();
    void openFileFromIndex(const QModelIndex &index);
    void openFile(const QString &path);
    void saveCurrentTab();
    EditorView *currentEditor() const;
    void loadPlugins();

    QFileSystemModel *m_fileModel;
    QTreeView *m_fileTree;
    QTabWidget *m_tabs;
    std::unique_ptr<PluginManager> m_pluginManager;
    std::unique_ptr<ServiceRegistry> m_services;
    std::unique_ptr<IFileSystem> m_fileSystem;
};
