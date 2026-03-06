#include "mainwindow.h"

#include <QAction>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QCoreApplication>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>

#include "editor/editorview.h"
#include "pluginmanager.h"
#include "serviceregistry.h"
#include "core/qtfilesystem.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_fileModel(nullptr),
      m_fileTree(nullptr),
      m_tabs(nullptr)
{
    setWindowTitle(tr("Exorcist"));
    resize(1100, 720);

    setupUi();
    setupMenus();
    setupToolBar();
    m_services = std::make_unique<ServiceRegistry>();
    m_services->registerService("mainwindow", this);
    m_fileSystem = std::make_unique<QtFileSystem>();
    loadPlugins();
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *widget = m_tabs->widget(index);
        m_tabs->removeTab(index);
        delete widget;
    });

    createDockWidgets();
    openNewTab();
}

void MainWindow::setupMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *newTabAction = fileMenu->addAction(tr("New &Tab"));
    QAction *openAction = fileMenu->addAction(tr("&Open..."));
    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    QAction *quitAction = fileMenu->addAction(tr("&Quit"));

    connect(newTabAction, &QAction::triggered, this, &MainWindow::openNewTab);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) {
            openFile(path);
        }
    });
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentTab);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);
}

void MainWindow::setupToolBar()
{
    auto *mainToolBar = addToolBar(tr("Main"));
    QAction *newTabAction = mainToolBar->addAction(tr("New"));
    QAction *openAction = mainToolBar->addAction(tr("Open"));
    QAction *saveAction = mainToolBar->addAction(tr("Save"));

    connect(newTabAction, &QAction::triggered, this, &MainWindow::openNewTab);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) {
            openFile(path);
        }
    });
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentTab);
}

void MainWindow::createDockWidgets()
{
    auto *projectDock = new QDockWidget(tr("Project"), this);
    projectDock->setObjectName("ProjectDock");
    projectDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_fileModel = new QFileSystemModel(projectDock);
    m_fileModel->setRootPath(QDir::currentPath());

    m_fileTree = new QTreeView(projectDock);
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setRootIndex(m_fileModel->index(QDir::currentPath()));
    m_fileTree->setHeaderHidden(true);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &MainWindow::openFileFromIndex);

    projectDock->setWidget(m_fileTree);
    addDockWidget(Qt::LeftDockWidgetArea, projectDock);

    auto *searchDock = new QDockWidget(tr("Search"), this);
    searchDock->setObjectName("SearchDock");
    searchDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    searchDock->setWidget(new QTextEdit(tr("Search panel placeholder"), searchDock));
    addDockWidget(Qt::LeftDockWidgetArea, searchDock);

    auto *terminalDock = new QDockWidget(tr("Terminal"), this);
    terminalDock->setObjectName("TerminalDock");
    terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    terminalDock->setWidget(new QPlainTextEdit(tr("Terminal placeholder"), terminalDock));
    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);

    auto *aiDock = new QDockWidget(tr("AI"), this);
    aiDock->setObjectName("AIDock");
    aiDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    aiDock->setWidget(new QTextEdit(tr("AI assistant placeholder"), aiDock));
    addDockWidget(Qt::RightDockWidgetArea, aiDock);
}

void MainWindow::openNewTab()
{
    auto *editor = new EditorView();
    editor->setPlainText(tr("Exorcist editor tab."));
    int index = m_tabs->addTab(editor, tr("Untitled"));
    m_tabs->setCurrentIndex(index);
}

void MainWindow::openFileFromIndex(const QModelIndex &index)
{
    if (!index.isValid() || m_fileModel->isDir(index)) {
        return;
    }

    openFile(m_fileModel->filePath(index));
}

void MainWindow::openFile(const QString &path)
{
    for (int i = 0; i < m_tabs->count(); ++i) {
        QWidget *widget = m_tabs->widget(i);
        if (widget && widget->property("filePath").toString() == path) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }

    QString error;
    const QString content = m_fileSystem->readTextFile(path, &error);
    if (content.isEmpty() && !m_fileSystem->exists(path)) {
        statusBar()->showMessage(tr("Failed to open %1").arg(path));
        return;
    }

    auto *editor = new EditorView();
    editor->setPlainText(content);
    editor->setProperty("filePath", path);

    const QString title = QFileInfo(path).fileName();
    int index = m_tabs->addTab(editor, title);
    m_tabs->setCurrentIndex(index);
}

void MainWindow::saveCurrentTab()
{
    EditorView *editor = currentEditor();
    if (!editor) {
        return;
    }

    QString path = editor->property("filePath").toString();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save File"));
        if (path.isEmpty()) {
            return;
        }
        editor->setProperty("filePath", path);
    }

    QString error;
    if (!m_fileSystem->writeTextFile(path, editor->toPlainText(), &error)) {
        statusBar()->showMessage(tr("Failed to save %1").arg(path));
        return;
    }

    const QString title = QFileInfo(path).fileName();
    m_tabs->setTabText(m_tabs->currentIndex(), title);
    statusBar()->showMessage(tr("Saved %1").arg(title));
}

EditorView *MainWindow::currentEditor() const
{
    return qobject_cast<EditorView *>(m_tabs->currentWidget());
}

void MainWindow::loadPlugins()
{
    m_pluginManager = std::make_unique<PluginManager>();
    const QString pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
    int loaded = m_pluginManager->loadPluginsFrom(pluginDir);
    m_pluginManager->initializeAll(m_services.get());

    if (loaded > 0) {
        statusBar()->showMessage(tr("Loaded %1 plugins").arg(loaded));
    }
}
