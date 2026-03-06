#include "mainwindow.h"

#include <QAction>
#include <QDirIterator>
#include <QDockWidget>
#include <QFileSystemModel>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QCoreApplication>
#include <QPlainTextEdit>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeView>

#include "commandpalette.h"
#include "editor/editorview.h"
#include "editor/largefileloader.h"
#include "editor/syntaxhighlighter.h"
#include "agent/agentorchestrator.h"
#include "agent/agentchatpanel.h"
#include "pluginmanager.h"
#include "serviceregistry.h"
#include "core/qtfilesystem.h"
#include "search/searchpanel.h"
#include "search/searchservice.h"
#include "lsp/clangdmanager.h"
#include "lsp/lspclient.h"
#include "lsp/lspeditorbridge.h"

#include <QCloseEvent>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_fileModel(nullptr),
      m_fileTree(nullptr),
      m_tabs(nullptr),
      m_searchPanel(nullptr),
      m_projectDock(nullptr),
      m_searchDock(nullptr),
      m_terminalDock(nullptr),
      m_aiDock(nullptr),
      m_posLabel(nullptr),
      m_encodingLabel(nullptr),
      m_backgroundLabel(nullptr),
      m_searchService(nullptr),
      m_agentOrchestrator(nullptr),
      m_chatPanel(nullptr),
      m_clangd(nullptr),
      m_lspClient(nullptr)
{
    setWindowTitle(tr("Exorcist"));

    // Services that dock widgets depend on must exist before setupUi().
    m_searchService     = new SearchService(this);
    m_agentOrchestrator = new AgentOrchestrator(this);

    // LSP — create before setupUi so openFile() can create bridges immediately.
    m_clangd    = new ClangdManager(this);
    m_lspClient = new LspClient(m_clangd->transport(), this);
    connect(m_clangd, &ClangdManager::serverReady, this, [this]() {
        m_lspClient->initialize(m_currentFolder);
    });
    connect(m_lspClient, &LspClient::initialized,
            this, &MainWindow::onLspInitialized);

    setupUi();
    setupMenus();
    setupToolBar();
    setupStatusBar();

    m_services = std::make_unique<ServiceRegistry>();
    m_services->registerService("mainwindow", this);
    m_services->registerService("agentOrchestrator", m_agentOrchestrator);
    m_fileSystem = std::make_unique<QtFileSystem>();
    loadPlugins();

    statusBar()->showMessage(tr("Ready"), 3000);
    loadSettings();
}

// ── UI setup ────────────────────────────────────────────────────────────────

void MainWindow::setupUi()
{
    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(true);
    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    setCentralWidget(m_tabs);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *widget = m_tabs->widget(index);
        m_tabs->removeTab(index);
        delete widget;
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);

    createDockWidgets();
    openNewTab();
}

void MainWindow::setupMenus()
{
    // ── File ──────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *newTabAction = fileMenu->addAction(tr("New &Tab"));
    newTabAction->setShortcut(QKeySequence::New);

    QAction *openAction = fileMenu->addAction(tr("&Open File..."));
    openAction->setShortcut(QKeySequence::Open);

    QAction *openFolderAction = fileMenu->addAction(tr("Open &Folder..."));
    openFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);

    connect(newTabAction, &QAction::triggered, this, &MainWindow::openNewTab);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    connect(openFolderAction, &QAction::triggered, this, [this]() { openFolder(); });
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentTab);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // ── View ──────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QAction *cmdPaletteAction = viewMenu->addAction(tr("&Command Palette"));
    cmdPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

    QAction *filePaletteAction = viewMenu->addAction(tr("&Go to File..."));
    filePaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));

    viewMenu->addSeparator();

    // Dock toggle actions (QDockWidget provides them automatically).
    // They are added after createDockWidgets() populates the dock pointers, so
    // we use a lambda that defers the addAction until after the constructor.
    QAction *toggleProjectAction   = viewMenu->addAction(tr("&Project panel"));
    QAction *toggleSearchAction    = viewMenu->addAction(tr("&Search panel"));
    QAction *toggleTerminalAction  = viewMenu->addAction(tr("&Terminal panel"));
    QAction *toggleAiAction        = viewMenu->addAction(tr("&AI panel"));

    for (QAction *a : {toggleProjectAction, toggleSearchAction,
                       toggleTerminalAction, toggleAiAction}) {
        a->setCheckable(true);
        a->setChecked(true);
    }

    connect(cmdPaletteAction, &QAction::triggered, this, &MainWindow::showCommandPalette);
    connect(filePaletteAction, &QAction::triggered, this, &MainWindow::showFilePalette);

    // Wire dock toggles after createDockWidgets() runs (called from setupUi)
    // by using lambdas that capture the dock pointer at call time.
    connect(toggleProjectAction,  &QAction::toggled, this, [this](bool on) {
        m_projectDock->setVisible(on);
    });
    connect(toggleSearchAction,   &QAction::toggled, this, [this](bool on) {
        m_searchDock->setVisible(on);
    });
    connect(toggleTerminalAction, &QAction::toggled, this, [this](bool on) {
        m_terminalDock->setVisible(on);
    });
    connect(toggleAiAction,       &QAction::toggled, this, [this](bool on) {
        m_aiDock->setVisible(on);
    });
    // Sync checkbox state when user closes a dock via its own X button.
    connect(m_projectDock,  &QDockWidget::visibilityChanged,
            toggleProjectAction,  &QAction::setChecked);
    connect(m_searchDock,   &QDockWidget::visibilityChanged,
            toggleSearchAction,   &QAction::setChecked);
    connect(m_terminalDock, &QDockWidget::visibilityChanged,
            toggleTerminalAction, &QAction::setChecked);
    connect(m_aiDock,       &QDockWidget::visibilityChanged,
            toggleAiAction,       &QAction::setChecked);
}

void MainWindow::setupToolBar()
{
    auto *bar = addToolBar(tr("Main"));
    bar->setMovable(false);

    bar->addAction(tr("New"),  this, &MainWindow::openNewTab);
    bar->addAction(tr("Open"), this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    bar->addAction(tr("Save"), this, &MainWindow::saveCurrentTab);
    bar->addSeparator();
    bar->addAction(tr("Folder"), this, [this]() { openFolder(); });
}

void MainWindow::setupStatusBar()
{
    m_posLabel        = new QLabel(tr("Ln 1, Col 1"), this);
    m_encodingLabel   = new QLabel(tr("UTF-8"), this);
    m_backgroundLabel = new QLabel(this);

    m_posLabel->setMinimumWidth(110);
    m_encodingLabel->setMinimumWidth(55);

    const QString labelStyle = "padding: 0 8px;";
    m_posLabel->setStyleSheet(labelStyle);
    m_encodingLabel->setStyleSheet(labelStyle);
    m_backgroundLabel->setStyleSheet(labelStyle);

    statusBar()->addPermanentWidget(m_backgroundLabel);
    statusBar()->addPermanentWidget(m_encodingLabel);
    statusBar()->addPermanentWidget(m_posLabel);
}

void MainWindow::createDockWidgets()
{
    // ── Project tree ──────────────────────────────────────────────────────
    m_projectDock = new QDockWidget(tr("Project"), this);
    m_projectDock->setObjectName("ProjectDock");
    m_projectDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_fileModel = new QFileSystemModel(m_projectDock);
    m_fileModel->setRootPath(QDir::currentPath());

    m_fileTree = new QTreeView(m_projectDock);
    m_fileTree->setModel(m_fileModel);
    m_fileTree->setRootIndex(m_fileModel->index(QDir::currentPath()));
    m_fileTree->setHeaderHidden(true);
    m_fileTree->setIndentation(14);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &MainWindow::openFileFromIndex);

    m_projectDock->setWidget(m_fileTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_projectDock);

    // ── Search ────────────────────────────────────────────────────────────
    m_searchPanel = new SearchPanel(m_searchService, this);
    m_searchDock  = new QDockWidget(tr("Search"), this);
    m_searchDock->setObjectName("SearchDock");
    m_searchDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_searchDock->setWidget(m_searchPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_searchDock);
    tabifyDockWidget(m_projectDock, m_searchDock);
    m_projectDock->raise();   // project visible by default

    // ── Terminal ──────────────────────────────────────────────────────────
    m_terminalDock = new QDockWidget(tr("Terminal"), this);
    m_terminalDock->setObjectName("TerminalDock");
    m_terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_terminalDock->setWidget(new QPlainTextEdit(tr("Terminal — not yet wired"), m_terminalDock));
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);

    // ── AI ────────────────────────────────────────────────────────────────
    m_aiDock = new QDockWidget(tr("AI"), this);
    m_aiDock->setObjectName("AIDock");
    m_aiDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_chatPanel = new AgentChatPanel(m_agentOrchestrator, m_aiDock);
    m_aiDock->setWidget(m_chatPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_aiDock);
}

// ── Tabs / editor ────────────────────────────────────────────────────────────

void MainWindow::openNewTab()
{
    auto *editor = new EditorView();
    int index = m_tabs->addTab(editor, tr("Untitled"));
    m_tabs->setCurrentIndex(index);
    updateEditorStatus(editor);
}

void MainWindow::onTabChanged(int /*index*/)
{
    updateEditorStatus(currentEditor());
}

void MainWindow::updateEditorStatus(EditorView *editor)
{
    // Disconnect previous editor's cursor signal before wiring a new one.
    disconnect(m_cursorConn);

    if (!m_posLabel) return;   // called before setupStatusBar() during init

    if (!editor) {
        m_posLabel->setText(tr("—"));
        return;
    }

    auto update = [this, editor]() {
        const QTextCursor c = editor->textCursor();
        m_posLabel->setText(tr("Ln %1, Col %2")
            .arg(c.blockNumber() + 1)
            .arg(c.columnNumber() + 1));
    };

    update();
    m_cursorConn = connect(editor, &QPlainTextEdit::cursorPositionChanged, this, update);
}

// ── File operations ──────────────────────────────────────────────────────────

void MainWindow::openFileFromIndex(const QModelIndex &index)
{
    if (!index.isValid() || m_fileModel->isDir(index)) return;
    openFile(m_fileModel->filePath(index));
}

void MainWindow::openFile(const QString &path)
{
    if (!m_fileSystem->exists(path)) {
        statusBar()->showMessage(tr("File not found: %1").arg(path), 4000);
        return;
    }

    for (int i = 0; i < m_tabs->count(); ++i) {
        if (m_tabs->widget(i)->property("filePath").toString() == path) {
            m_tabs->setCurrentIndex(i);
            return;
        }
    }

    auto *editor = new EditorView();
    constexpr qint64 kLargeFileThreshold = 10 * 1024 * 1024;
    LargeFileLoader::applyToEditor(editor, path, kLargeFileThreshold);
    editor->setProperty("filePath", path);
    SyntaxHighlighter::create(path, editor->document());
    createLspBridge(editor, path);

    const QString title = QFileInfo(path).fileName();
    int index = m_tabs->addTab(editor, title);
    m_tabs->setCurrentIndex(index);

    if (editor->isLargeFilePreview()) {
        statusBar()->showMessage(tr("Large file preview (read-only)"), 5000);
        connect(editor, &EditorView::requestMoreData, this, [editor]() {
            LargeFileLoader::appendNextChunk(editor, 2 * 1024 * 1024);
        });
    }
}

void MainWindow::openFolder(const QString &path)
{
    QString folder = path;
    if (folder.isEmpty()) {
        folder = QFileDialog::getExistingDirectory(this, tr("Open Folder"));
    }
    if (folder.isEmpty()) return;

    m_fileModel->setRootPath(folder);
    m_fileTree->setRootIndex(m_fileModel->index(folder));
    m_searchPanel->setRootPath(folder);
    setWindowTitle(tr("Exorcist — %1").arg(QDir(folder).dirName()));
    statusBar()->showMessage(tr("Opened: %1").arg(folder), 4000);

    m_currentFolder = folder;
    m_clangd->start(folder);
}

void MainWindow::saveCurrentTab()
{
    EditorView *editor = currentEditor();
    if (!editor) return;

    QString path = editor->property("filePath").toString();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save File"));
        if (path.isEmpty()) return;
        editor->setProperty("filePath", path);
    }

    QString error;
    if (!m_fileSystem->writeTextFile(path, editor->toPlainText(), &error)) {
        statusBar()->showMessage(tr("Save failed: %1").arg(error), 5000);
        return;
    }

    const QString title = QFileInfo(path).fileName();
    m_tabs->setTabText(m_tabs->currentIndex(), title);
    statusBar()->showMessage(tr("Saved %1").arg(title), 3000);
}

// ── Command palette ───────────────────────────────────────────────────────────

void MainWindow::showFilePalette()
{
    const QString root = m_fileModel->rootPath();
    if (root.isEmpty()) {
        statusBar()->showMessage(tr("Open a folder first (Ctrl+Shift+O)"), 3000);
        return;
    }

    m_backgroundLabel->setText(tr("Indexing..."));
    QStringList files;
    QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        files << it.next();
    }
    m_backgroundLabel->clear();

    CommandPalette palette(CommandPalette::FileMode, this);
    palette.setFiles(files);

    // Center on main window.
    const QRect geo = geometry();
    palette.move(geo.center().x() - palette.minimumWidth() / 2,
                 geo.top() + 80);

    if (palette.exec() == QDialog::Accepted) {
        const QString selected = palette.selectedFile();
        if (!selected.isEmpty()) openFile(selected);
    }
}

void MainWindow::showCommandPalette()
{
    const QStringList commands = {
        tr("New Tab\tCtrl+N"),
        tr("Open File...\tCtrl+O"),
        tr("Open Folder...\tCtrl+Shift+O"),
        tr("Save\tCtrl+S"),
        tr("Go to File...\tCtrl+P"),
        tr("Find / Replace\tCtrl+F"),
        tr("Toggle Project Panel"),
        tr("Toggle Search Panel"),
        tr("Toggle Terminal Panel"),
        tr("Toggle AI Panel"),
        tr("Quit\tCtrl+Q"),
    };

    CommandPalette palette(CommandPalette::CommandMode, this);
    palette.setCommands(commands);

    const QRect geo = geometry();
    palette.move(geo.center().x() - palette.minimumWidth() / 2,
                 geo.top() + 80);

    if (palette.exec() != QDialog::Accepted) return;

    switch (palette.selectedCommandIndex()) {
    case 0:  openNewTab();                                                     break;
    case 1: { const QString p = QFileDialog::getOpenFileName(this, tr("Open File"));
              if (!p.isEmpty()) openFile(p); }                                 break;
    case 2:  openFolder();                                                     break;
    case 3:  saveCurrentTab();                                                 break;
    case 4:  showFilePalette();                                                break;
    case 5:  if (auto *e = currentEditor()) e->showFindBar();                 break;
    case 6:  m_projectDock->setVisible(!m_projectDock->isVisible());          break;
    case 7:  m_searchDock->setVisible(!m_searchDock->isVisible());            break;
    case 8:  m_terminalDock->setVisible(!m_terminalDock->isVisible());        break;
    case 9:  m_aiDock->setVisible(!m_aiDock->isVisible());                    break;
    case 10: close();                                                          break;
    default: break;
    }
}

// ── Settings ──────────────────────────────────────────────────────────────────

void MainWindow::loadSettings()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));

    const QByteArray geometry = s.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const QString lastFolder = s.value(QStringLiteral("lastFolder")).toString();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists())
        openFolder(lastFolder);
}

void MainWindow::saveSettings()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.setValue(QStringLiteral("geometry"),    saveGeometry());
    s.setValue(QStringLiteral("lastFolder"),  m_fileModel->rootPath());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

EditorView *MainWindow::currentEditor() const
{
    return qobject_cast<EditorView *>(m_tabs->currentWidget());
}

// ── LSP ───────────────────────────────────────────────────────────────────────

void MainWindow::createLspBridge(EditorView *editor, const QString &path)
{
    // Bridge is parented to the editor — auto-deleted when the tab is closed.
    new LspEditorBridge(editor, m_lspClient, path, editor);
}

void MainWindow::onLspInitialized()
{
    // Open bridges for any tabs that were already open before LSP was ready.
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *editor = qobject_cast<EditorView *>(m_tabs->widget(i));
        if (!editor) continue;
        const QString path = editor->property("filePath").toString();
        if (path.isEmpty()) continue;
        // Avoid double-bridging (bridge already exists as a child of editor)
        if (editor->findChild<LspEditorBridge *>()) continue;
        createLspBridge(editor, path);
    }
}

void MainWindow::loadPlugins()
{
    m_pluginManager = std::make_unique<PluginManager>();
    const QString pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
    int loaded = m_pluginManager->loadPluginsFrom(pluginDir);
    m_pluginManager->initializeAll(m_services.get());

    if (loaded > 0) {
        statusBar()->showMessage(tr("Loaded %1 plugins").arg(loaded), 4000);
    }
}
