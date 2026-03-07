#include "mainwindow.h"

#include <QAction>
#include <QDirIterator>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QStandardPaths>
#include <QTextCursor>
#include <QMenu>
#include <QMenuBar>
#include <QVBoxLayout>
#include <QCoreApplication>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QTreeView>
#include <QUuid>
#include <QInputDialog>
#include <QClipboard>
#include <QDesktopServices>
#include <QMessageBox>
#include <QTimer>

#include "commandpalette.h"
#include "editor/editorview.h"
#include "editor/largefileloader.h"
#include "editor/syntaxhighlighter.h"
#include "editor/inlinecompletionengine.h"
#include "editor/nexteditengine.h"
#include "agent/agentorchestrator.h"
#include "agent/agentchatpanel.h"
#include "agent/agentcontroller.h"
#include "agent/agentmodes.h"
#include "agent/contextbuilder.h"
#include "agent/quickchatdialog.h"
#include "agent/inlinechat/inlinechatwidget.h"
#include "agent/promptvariables.h"
#include "agent/promptfileloader.h"
#include "agent/sessionstore.h"
#include "agent/itool.h"
#include "agent/tools/filesystemtools.h"
#include "agent/tools/searchworkspacetool.h"
#include "agent/tools/semanticsearchtool.h"
#include "agent/modelpickerdialog.h"
#include "agent/tools/runcommandtool.h"
#include "agent/tools/applypatchtool.h"
#include "agent/tools/idetools.h"
#include "agent/tools/moretools.h"
#include "agent/tools/advancedtools.h"
#include "agent/tools/memorytool.h"
#include "agent/requestlogpanel.h"
#include "agent/trajectorypanel.h"
#include "agent/customprovider.h"
#include "mcp/mcpclient.h"
#include "mcp/mcppanel.h"
#include "mcp/mcptooladapter.h"
#include "thememanager.h"
#include "editor/diffviewerpanel.h"
#include "editor/breadcrumbbar.h"
#include "agent/settingspanel.h"
#include "agent/memorybrowserpanel.h"
#include "agent/iagentplugin.h"
#include "ui/notificationtoast.h"
#include "pluginmanager.h"
#include "serviceregistry.h"
#include "core/qtfilesystem.h"
#include "core/qtprocess.h"
#include "search/searchpanel.h"
#include "search/searchservice.h"
#include "search/workspaceindexer.h"
#include "search/symbolindex.h"
#include "lsp/clangdmanager.h"
#include "lsp/lspclient.h"
#include "lsp/lspeditorbridge.h"
#include "lsp/referencespanel.h"
#include "editor/symboloutlinepanel.h"
#include "terminal/terminalpanel.h"
#include "project/projectmanager.h"
#include "project/solutiontreemodel.h"
#include "git/gitservice.h"
#include "git/gitpanel.h"
#include "build/outputpanel.h"
#include "editor/filewatchservice.h"
#include "agent/contextpruner.h"
#include "agent/autocompactor.h"
#include "agent/testscaffold.h"
#include "agent/authmanager.h"
#include "agent/tools/websearchtool.h"
#include "editor/renamesuggestionwidget.h"
#include "agent/securekeystorage.h"
#include "agent/promptfilemanager.h"
#include "editor/mergeconflictresolver.h"
#include "editor/inlinereviewwidget.h"
#include "agent/modelregistry.h"
#include "agent/oauthmanager.h"
#include "agent/domainfilter.h"
#include "git/commitmessagegenerator.h"
#include "editor/multichunkeditor.h"
#include "editor/languagecompletionconfig.h"
#include "agent/contexteditor.h"
#include "agent/trajectoryrecorder.h"
#include "agent/featureflags.h"
#include "agent/notebookstubs.h"
#include "agent/reviewmanager.h"
#include "agent/testgenerator.h"
#include "agent/authstatusindicator.h"
#include "agent/promptfilepicker.h"
#include "agent/chatthemeadapter.h"
#include "agent/apikeymanagerwidget.h"
#include "agent/networkmonitor.h"
#include "agent/telemetrymanager.h"
#include "agent/citationmanager.h"
#include "mcp/mcpservermanager.h"
#include "search/workspacechunkindex.h"
#include "agent/modelconfigwidget.h"
#include "agent/contextinspector.h"
#include "agent/toolcalltrace.h"
#include "agent/trajectoryreplaywidget.h"
#include "agent/promptarchiveexporter.h"
#include "agent/memoryfileeditor.h"
#include "agent/backgroundcompactor.h"
#include "agent/errorstatewidget.h"
#include "agent/setuptestswizard.h"
#include "agent/renameintegration.h"
#include "agent/accessibilityhelper.h"
#include "agent/tooltogglemanager.h"
#include "agent/contextscopeconfig.h"
#include "search/embeddingindex.h"
#include "search/gitignorefilter.h"
#include "search/regexsearchengine.h"
#include "agent/tools/notebooktools.h"
#include "agent/tools/githubmcptools.h"

#include <QCloseEvent>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QSettings>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_fileTree(nullptr),
      m_tabs(nullptr),
      m_searchPanel(nullptr),
      m_projectManager(nullptr),
      m_treeModel(nullptr),
      m_gitService(nullptr),
      m_gitPanel(nullptr),
      m_projectDock(nullptr),
      m_searchDock(nullptr),
      m_gitDock(nullptr),
      m_terminalDock(nullptr),
      m_aiDock(nullptr),
      m_posLabel(nullptr),
      m_encodingLabel(nullptr),
      m_backgroundLabel(nullptr),
      m_branchLabel(nullptr),
      m_searchService(nullptr),
      m_agentOrchestrator(nullptr),
      m_chatPanel(nullptr),
      m_agentController(nullptr),
    m_sessionStore(nullptr),
    m_promptResolver(nullptr),
      m_toolRegistry(nullptr),
      m_contextBuilder(nullptr),
      m_referencesPanel(nullptr),
      m_symbolPanel(nullptr),
      m_referencesDock(nullptr),
      m_symbolDock(nullptr),
      m_terminal(nullptr),
      m_clangd(nullptr),
      m_lspClient(nullptr),
      m_inlineEngine(nullptr)
{
    setWindowTitle(tr("Exorcist"));

    // Services that dock widgets depend on must exist before setupUi().
    m_searchService     = new SearchService(this);
    m_agentOrchestrator = new AgentOrchestrator(this);
    m_projectManager    = new ProjectManager(this);
    m_gitService        = new GitService(this);

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

    // ── Agent core (tool registry, context builder, controller) ───────────
    m_toolRegistry = new ToolRegistry(this);
    m_services->registerService("toolRegistry", m_toolRegistry);

    m_contextBuilder = new ContextBuilder(this);
    m_contextBuilder->setFileSystem(m_fileSystem.get());
    m_contextBuilder->setOpenFilesGetter([this]() {
        QStringList paths;
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *ed = qobject_cast<EditorView *>(m_tabs->widget(i));
            const QString fp = ed ? ed->property("filePath").toString() : QString();
            if (!fp.isEmpty())
                paths.append(fp);
        }
        return paths;
    });
    m_contextBuilder->setGitStatusGetter([this]() -> QString {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        QString result = QStringLiteral("Branch: %1\n").arg(m_gitService->currentBranch());
        const auto statuses = m_gitService->fileStatuses();
        for (auto it = statuses.begin(); it != statuses.end(); ++it)
            result += QStringLiteral("  %1 %2\n").arg(it.value(), it.key());
        return result;
    });
    m_contextBuilder->setTerminalOutputGetter([this]() -> QString {
        if (!m_terminal) return {};
        return m_terminal->recentOutput(80);
    });

    m_agentController = new AgentController(
        m_agentOrchestrator, m_toolRegistry, m_contextBuilder, this);
    m_sessionStore = new SessionStore(this);
    m_agentController->setSessionStore(m_sessionStore);

    // Wire AI status indicator
    connect(m_agentController, &AgentController::turnStarted,
            this, [this]() {
        m_copilotStatusLabel->setText(tr("\u25CF Working..."));
        m_copilotStatusLabel->setStyleSheet(
            QStringLiteral("padding: 0 8px; color:#dcdcaa;"));
    });
    connect(m_agentController, &AgentController::turnFinished,
            this, [this]() {
        m_copilotStatusLabel->setText(tr("\u2713 AI Ready"));
        m_copilotStatusLabel->setStyleSheet(
            QStringLiteral("padding: 0 8px; color:#89d185;"));
    });
    connect(m_agentController, &AgentController::turnError,
            this, [this]() {
        m_copilotStatusLabel->setText(tr("\u2717 AI Error"));
        m_copilotStatusLabel->setStyleSheet(
            QStringLiteral("padding: 0 8px; color:#f14c4c;"));
    });

    // Wire request log panel
    connect(m_agentController, &AgentController::turnStarted,
            this, [this](const QString &msg) {
        m_requestLogPanel->logRequest(QStringLiteral("turn"), QStringLiteral("-"),
                                       1, 0);
    });
    connect(m_agentController, &AgentController::turnFinished,
            this, [this](const AgentTurn &turn) {
        const QString text = turn.steps.isEmpty() ? QString() : turn.steps.last().finalText;
        m_requestLogPanel->logResponse(QStringLiteral("turn"), text.left(500));
    });
    connect(m_agentController, &AgentController::turnError,
            this, [this](const QString &err) {
        m_requestLogPanel->logError(QStringLiteral("turn"), err);
        NotificationToast::show(this, tr("AI Error: %1").arg(err.left(100)),
                                NotificationToast::Error, 5000);
    });
    connect(m_agentController, &AgentController::toolCallStarted,
            this, [this](const QString &name, const QJsonObject &args) {
        m_requestLogPanel->logToolCall(name, args, true, QStringLiteral("started"));
    });
    connect(m_agentController, &AgentController::toolCallFinished,
            this, [this](const QString &name, const ToolExecResult &result) {
        m_requestLogPanel->logToolCall(name, {}, result.ok,
            result.ok ? result.textContent.left(200) : result.error);
    });

    // Wire trajectory panel
    connect(m_agentController, &AgentController::turnStarted,
            this, [this](const QString &) {
        m_trajectoryPanel->clear();
    });
    connect(m_agentController, &AgentController::turnFinished,
            this, [this](const AgentTurn &turn) {
        m_trajectoryPanel->setTurn(turn);
    });
    connect(m_agentController, &AgentController::toolCallStarted,
            this, [this](const QString &name, const QJsonObject &args) {
        AgentStep step;
        step.type = AgentStep::Type::ToolCall;
        step.timestamp = QDateTime::currentDateTime().toString(Qt::ISODate);
        step.toolCall.name = name;
        step.toolCall.arguments = QString::fromUtf8(
            QJsonDocument(args).toJson(QJsonDocument::Compact));
        step.toolResult = tr("running...");
        m_trajectoryPanel->appendStep(step);
    });
    // Wire chat panel now that the controller exists
    if (m_chatPanel) {
        m_chatPanel->setAgentController(m_agentController);
        m_chatPanel->setSessionStore(m_sessionStore);
        m_chatPanel->setContextBuilder(m_contextBuilder);

        // Wire review annotations → apply to active editor
        connect(m_chatPanel, &AgentChatPanel::reviewAnnotationsReady,
                this, [this](const QString &filePath,
                             const QList<QPair<int, QString>> &annotations) {
            // Find open editor tab for the file
            for (int i = 0; i < m_tabs->count(); ++i) {
                auto *ed = qobject_cast<EditorView *>(m_tabs->widget(i));
                if (!ed) continue;
                if (ed->property("filePath").toString() == filePath) {
                    QList<EditorView::ReviewAnnotation> anns;
                    for (const auto &[line, comment] : annotations)
                        anns.append({line, comment});
                    ed->setReviewAnnotations(anns);
                    m_tabs->setCurrentIndex(i);
                    break;
                }
            }
        });

        // Wire test scaffold → open file
        connect(m_chatPanel, &AgentChatPanel::openFileRequested,
                this, [this](const QString &filePath) {
            openFile(filePath);
        });
    }

    // Inline completion engine — wired after plugins load
    m_inlineEngine = new InlineCompletionEngine(this);

    // Next Edit Suggestion engine — predicts next edit after ghost text acceptance
    m_nesEngine = new NextEditEngine(this);
    connect(m_nesEngine, &NextEditEngine::suggestionReady, this, [this](int line) {
        auto *ed = currentEditor();
        if (ed)
            ed->setNesLine(line);
    });
    connect(m_nesEngine, &NextEditEngine::suggestionDismissed, this, [this] {
        auto *ed = currentEditor();
        if (ed)
            ed->clearNesLine();
    });
    connect(m_nesEngine, &NextEditEngine::navigateToFile, this,
            [this](const QString &path, int line, int) {
        openFile(path);
        // After opening, jump to line
        auto *ed = currentEditor();
        if (ed) {
            QTextBlock block = ed->document()->findBlockByNumber(line);
            if (block.isValid()) {
                QTextCursor cur(block);
                ed->setTextCursor(cur);
                ed->centerCursor();
            }
        }
    });

    // Prompt variable resolver used by chat panel before send.
    m_promptResolver = new PromptVariableResolver(this);
    m_promptResolver->registerVariable({
        QStringLiteral("file"),
        tr("Active file context"),
        [this]() -> QString {
            auto *ed = currentEditor();
            if (!ed) return tr("No active file.");
            const QString path = ed->property("filePath").toString();
            if (path.isEmpty()) return tr("No active file.");
            return tr("Active file: %1\n\n%2")
                .arg(path, ed->toPlainText().left(8000));
        }
    });
    m_promptResolver->registerVariable({
        QStringLiteral("selection"),
        tr("Selected editor text"),
        [this]() -> QString {
            auto *ed = currentEditor();
            if (!ed) return tr("No active editor selection.");
            const QString sel = ed->textCursor().selectedText();
            return sel.trimmed().isEmpty() ? tr("No selected text.") : sel;
        }
    });
    m_promptResolver->registerVariable({
        QStringLiteral("problems"),
        tr("Current diagnostics"),
        [this]() -> QString {
            auto *ed = currentEditor();
            if (!ed) return tr("No diagnostics available.");
            const auto marks = ed->diagnosticMarks();
            if (marks.isEmpty()) return tr("No diagnostics available.");
            QStringList lines;
            for (const auto &d : marks) {
                QString sev = tr("info");
                if (d.severity == DiagSeverity::Error) sev = tr("error");
                else if (d.severity == DiagSeverity::Warning) sev = tr("warning");
                lines.append(QStringLiteral("%1:%2 %3: %4")
                                .arg(d.line + 1)
                                .arg(d.startChar + 1)
                                .arg(sev, d.message));
            }
            return lines.join(QLatin1Char('\n'));
        }
    });
    m_promptResolver->registerVariable({
        QStringLiteral("terminal"),
        tr("Recent terminal output"),
        [this]() -> QString {
            if (!m_terminal) return tr("No terminal output.");
            const QString out = m_terminal->recentOutput(120);
            return out.trimmed().isEmpty() ? tr("No terminal output.") : out;
        }
    });

    if (m_chatPanel) {
        m_chatPanel->setVariableResolver(m_promptResolver);
        m_chatPanel->setInsertAtCursorCallback([this](const QString &code) {
            if (auto *ed = currentEditor()) {
                QTextCursor c = ed->textCursor();
                if (c.hasSelection())
                    c.insertText(code);
                else
                    c.insertText(code);
                ed->setFocus();
            }
        });
        m_chatPanel->setRunInTerminalCallback([this](const QString &code) {
            if (!m_terminal) return;
            m_terminalDock->raise();
            m_terminal->sendInput(code);
            m_terminal->sendInput(QStringLiteral("\r\n"));
        });
        m_chatPanel->setWorkspaceFileProvider([this]() -> QStringList {
            QStringList result;
            if (m_currentFolder.isEmpty()) return result;
            QDirIterator it(m_currentFolder,
                            QDir::Files | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            const QDir root(m_currentFolder);
            while (it.hasNext()) {
                it.next();
                const QString rel = root.relativeFilePath(it.filePath());
                if (rel.startsWith('.') || rel.contains(QStringLiteral("/."))
                    || rel.contains(QStringLiteral("\\.")))
                    continue;
                if (rel.startsWith(QStringLiteral("build"))) continue;
                result.append(rel);
                if (result.size() >= 500) break;
            }
            return result;
        });
    }

    loadPlugins();

    connect(m_gitService, &GitService::statusRefreshed,
            m_treeModel, &SolutionTreeModel::refreshGitOverlays);
    connect(m_gitService, &GitService::statusRefreshed, this, [this] {
        updateDiffRanges(currentEditor());
    });
    connect(m_gitService, &GitService::branchChanged, this, [this](const QString &branch) {
        m_branchLabel->setText(branch.isEmpty() ? QString() : QString(" ") + branch);
    });

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

    m_breadcrumb = new BreadcrumbBar(this);

    auto *centralContainer = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(centralContainer);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    centralLayout->addWidget(m_breadcrumb);
    centralLayout->addWidget(m_tabs);
    setCentralWidget(centralContainer);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *widget = m_tabs->widget(index);
        m_tabs->removeTab(index);
        if (widget) {
            widget->deleteLater();
        }
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

    QMenu *solutionMenu = fileMenu->addMenu(tr("Solution"));
    QAction *newSolutionAction = solutionMenu->addAction(tr("New Solution..."));
    QAction *openSolutionAction = solutionMenu->addAction(tr("Open Solution (.exsln)..."));
    solutionMenu->addSeparator();
    QAction *addProjectAction = solutionMenu->addAction(tr("Add Folder to Solution..."));
    QAction *saveSolutionAction = solutionMenu->addAction(tr("Save Solution"));

    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);

    connect(newTabAction, &QAction::triggered, this, &MainWindow::openNewTab);
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    connect(openFolderAction, &QAction::triggered, this, [this]() { openFolder(); });
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentTab);
    connect(newSolutionAction, &QAction::triggered, this, &MainWindow::newSolution);
    connect(openSolutionAction, &QAction::triggered, this, &MainWindow::openSolutionFile);
    connect(addProjectAction, &QAction::triggered, this, &MainWindow::addProjectToSolution);
    connect(saveSolutionAction, &QAction::triggered, this, [this]() {
        m_projectManager->saveSolution();
    });
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // ── View ──────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QAction *cmdPaletteAction = viewMenu->addAction(tr("&Command Palette"));
    cmdPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

    QAction *filePaletteAction = viewMenu->addAction(tr("&Go to File..."));
    filePaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
QAction *symbolPaletteAction = viewMenu->addAction(tr("Go to &Symbol..."));
    symbolPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));

    
    viewMenu->addSeparator();

    // Dock toggle actions (QDockWidget provides them automatically).
    // They are added after createDockWidgets() populates the dock pointers, so
    // we use a lambda that defers the addAction until after the constructor.
    QAction *toggleProjectAction   = viewMenu->addAction(tr("&Project panel"));
    QAction *toggleSearchAction    = viewMenu->addAction(tr("&Search panel"));
    QAction *toggleGitAction       = viewMenu->addAction(tr("&Git panel"));
    QAction *toggleTerminalAction  = viewMenu->addAction(tr("&Terminal panel"));
    QAction *toggleAiAction        = viewMenu->addAction(tr("&AI panel"));
    QAction *toggleOutlineAction   = viewMenu->addAction(tr("&Outline panel"));
    QAction *toggleRefsAction      = viewMenu->addAction(tr("&References panel"));
    viewMenu->addSeparator();
    QAction *toggleLogAction       = viewMenu->addAction(tr("Request &Log"));
    QAction *toggleTrajectoryAction = viewMenu->addAction(tr("&Trajectory"));
    QAction *toggleSettingsAction  = viewMenu->addAction(tr("AI &Settings"));
    QAction *toggleMemoryAction    = viewMenu->addAction(tr("AI &Memory"));
    QAction *toggleMcpAction       = viewMenu->addAction(tr("&MCP Servers"));
    QAction *toggleOutputAction    = viewMenu->addAction(tr("&Output / Build"));
    viewMenu->addSeparator();
    QAction *toggleBlameAction     = viewMenu->addAction(tr("Toggle Git &Blame"));
    toggleBlameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
    toggleBlameAction->setCheckable(true);
    toggleBlameAction->setChecked(false);

    QAction *themeToggleAction = viewMenu->addAction(tr("Toggle &Dark/Light Theme"));
    themeToggleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F10));
    connect(themeToggleAction, &QAction::triggered, this, [this]() {
        m_themeManager->toggle();
    });

    QAction *minimapAction = viewMenu->addAction(tr("Toggle &Minimap"));
    minimapAction->setCheckable(true);
    minimapAction->setChecked(true);
    connect(minimapAction, &QAction::toggled, this, [this](bool on) {
        for (int i = 0; i < m_tabs->count(); ++i) {
            if (auto *ev = qobject_cast<EditorView *>(m_tabs->widget(i)))
                ev->setMinimapVisible(on);
        }
    });
    connect(toggleBlameAction, &QAction::toggled, this, [this](bool checked) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        if (checked && m_gitService && m_gitService->isGitRepo()) {
            const QString path = editor->property("filePath").toString();
            if (path.isEmpty()) return;
            const auto entries = m_gitService->blame(path);
            QHash<int, EditorView::BlameInfo> blameMap;
            for (const auto &e : entries) {
                EditorView::BlameInfo bi;
                bi.author  = e.author;
                bi.hash    = e.commitHash;
                bi.summary = e.summary;
                blameMap.insert(e.line, bi);
            }
            editor->setBlameData(blameMap);
            editor->setBlameVisible(true);
        } else {
            editor->clearBlameData();
        }
    });

    QAction *compareHeadAction = viewMenu->addAction(tr("Compare with &HEAD"));
    compareHeadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(compareHeadAction, &QAction::triggered, this, [this]() {
        EditorView *editor = currentEditor();
        if (!editor || !m_gitService || !m_gitService->isGitRepo()) return;
        const QString path = editor->property("filePath").toString();
        if (path.isEmpty()) return;
        const QString headText = m_gitService->showAtHead(path);
        const QString currentText = editor->toPlainText();
        m_diffViewer->showDiff(path, headText, currentText);
        m_diffDock->show();
        m_diffDock->raise();
    });

    for (QAction *a : {toggleProjectAction, toggleSearchAction, toggleGitAction,
                       toggleTerminalAction, toggleAiAction,
                       toggleOutlineAction,  toggleRefsAction}) {
        a->setCheckable(true);
        a->setChecked(true);
    }
    toggleLogAction->setCheckable(true);
    toggleLogAction->setChecked(false);
    toggleTrajectoryAction->setCheckable(true);
    toggleTrajectoryAction->setChecked(false);
    toggleSettingsAction->setCheckable(true);
    toggleSettingsAction->setChecked(false);
    toggleMemoryAction->setCheckable(true);
    toggleMemoryAction->setChecked(false);
    toggleMcpAction->setCheckable(true);
    toggleMcpAction->setChecked(false);
    toggleOutputAction->setCheckable(true);
    toggleOutputAction->setChecked(false);

    // ── Build shortcut (Ctrl+Shift+B) ─────────────────────────────────────
    auto *buildAct = new QAction(tr("&Build"), this);
    buildAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    connect(buildAct, &QAction::triggered, this, [this]() {
        if (m_outputPanel) {
            m_outputPanel->setWorkingDirectory(m_currentFolder);
            m_outputPanel->runCommand(m_outputPanel->findChild<QComboBox *>()->currentText());
            m_outputDock->show();
            m_outputDock->raise();
        }
    });
    addAction(buildAct);

    // ── AI shortcuts ──────────────────────────────────────────────────────
    auto *focusChatAct = new QAction(tr("Focus AI Chat"), this);
    focusChatAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    connect(focusChatAct, &QAction::triggered, this, [this]() {
        m_chatPanel->focusInput();
    });
    addAction(focusChatAct);

    auto *toggleChatAct = new QAction(tr("Toggle AI Panel"), this);
    toggleChatAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(toggleChatAct, &QAction::triggered, this, [this]() {
        if (m_aiDock->isVisible()) {
            m_aiDock->hide();
        } else {
            m_aiDock->show();
            m_aiDock->raise();
            m_chatPanel->focusInput();
        }
    });
    addAction(toggleChatAct);

    auto *quickChatAct = new QAction(tr("Quick Chat"), this);
    quickChatAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::ALT | Qt::Key_L));
    connect(quickChatAct, &QAction::triggered, this, [this]() {
        if (!m_quickChat)
            m_quickChat = new QuickChatDialog(m_agentOrchestrator, this);
        auto *ed = currentEditor();
        if (ed) {
            m_quickChat->setEditorContext(
                ed->property("filePath").toString(),
                ed->textCursor().selectedText(),
                ed->property("languageId").toString());
        }
        m_quickChat->show();
        m_quickChat->raise();
        m_quickChat->activateWindow();
    });
    addAction(quickChatAct);

    auto *selectModelAct = new QAction(tr("Select AI Model"), this);
    selectModelAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M));
    connect(selectModelAct, &QAction::triggered, this, [this]() {
        ModelPickerDialog dlg(m_agentOrchestrator, this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString provId = dlg.selectedProviderId();
            const QString model  = dlg.selectedModel();
            if (!provId.isEmpty())
                m_agentOrchestrator->setActiveProvider(provId);
            auto *prov = m_agentOrchestrator->activeProvider();
            if (prov && !model.isEmpty())
                prov->setModel(model);
            statusBar()->showMessage(
                tr("Model: %1 (%2)").arg(model, provId), 3000);
        }
    });
    connect(symbolPaletteAction, &QAction::triggered, this, &MainWindow::showSymbolPalette);
    addAction(selectModelAct);

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
    connect(toggleGitAction,      &QAction::toggled, this, [this](bool on) {
        m_gitDock->setVisible(on);
    });
    connect(toggleTerminalAction, &QAction::toggled, this, [this](bool on) {
        m_terminalDock->setVisible(on);
    });
    connect(toggleAiAction,       &QAction::toggled, this, [this](bool on) {
        m_aiDock->setVisible(on);
    });
    connect(toggleOutlineAction,  &QAction::toggled, this, [this](bool on) {
        m_symbolDock->setVisible(on);
    });
    connect(toggleRefsAction,     &QAction::toggled, this, [this](bool on) {
        m_referencesDock->setVisible(on);
    });
    connect(toggleLogAction,      &QAction::toggled, this, [this](bool on) {
        m_requestLogDock->setVisible(on);
        if (on) m_requestLogDock->raise();
    });
    connect(toggleTrajectoryAction, &QAction::toggled, this, [this](bool on) {
        m_trajectoryDock->setVisible(on);
        if (on) m_trajectoryDock->raise();
    });
    connect(toggleSettingsAction, &QAction::toggled, this, [this](bool on) {
        m_settingsDock->setVisible(on);
        if (on) m_settingsDock->raise();
    });
    connect(toggleMemoryAction,   &QAction::toggled, this, [this](bool on) {
        m_memoryDock->setVisible(on);
        if (on) { m_memoryDock->raise(); m_memoryBrowser->refresh(); }
    });
    connect(toggleMcpAction, &QAction::toggled, this, [this](bool on) {
        m_mcpDock->setVisible(on);
        if (on) m_mcpDock->raise();
    });
    connect(toggleOutputAction, &QAction::toggled, this, [this](bool on) {
        m_outputDock->setVisible(on);
        if (on) m_outputDock->raise();
    });
    // Sync checkbox state when user closes a dock via its own X button.
    connect(m_projectDock,    &QDockWidget::visibilityChanged,
            toggleProjectAction,  &QAction::setChecked);
    connect(m_searchDock,     &QDockWidget::visibilityChanged,
            toggleSearchAction,   &QAction::setChecked);
    connect(m_gitDock,        &QDockWidget::visibilityChanged,
            toggleGitAction,      &QAction::setChecked);
    connect(m_terminalDock,   &QDockWidget::visibilityChanged,
            toggleTerminalAction, &QAction::setChecked);
    connect(m_aiDock,         &QDockWidget::visibilityChanged,
            toggleAiAction,       &QAction::setChecked);
    connect(m_symbolDock,     &QDockWidget::visibilityChanged,
            toggleOutlineAction,  &QAction::setChecked);
    connect(m_trajectoryDock, &QDockWidget::visibilityChanged,
            toggleTrajectoryAction, &QAction::setChecked);
    connect(m_referencesDock, &QDockWidget::visibilityChanged,
            toggleRefsAction,     &QAction::setChecked);
    connect(m_requestLogDock, &QDockWidget::visibilityChanged,
            toggleLogAction,      &QAction::setChecked);
    connect(m_settingsDock,   &QDockWidget::visibilityChanged,
            toggleSettingsAction, &QAction::setChecked);
    connect(m_memoryDock,     &QDockWidget::visibilityChanged,
            toggleMemoryAction,   &QAction::setChecked);
    connect(m_mcpDock,        &QDockWidget::visibilityChanged,
            toggleMcpAction,      &QAction::setChecked);
    connect(m_outputDock,     &QDockWidget::visibilityChanged,
            toggleOutputAction,   &QAction::setChecked);
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
    m_branchLabel     = new QLabel(this);
    m_copilotStatusLabel = new QLabel(tr("\u2713 AI Ready"), this);
    m_indexLabel = new QLabel(this);

    m_posLabel->setMinimumWidth(110);
    m_encodingLabel->setMinimumWidth(55);
    m_branchLabel->setMinimumWidth(80);
    m_copilotStatusLabel->setMinimumWidth(80);

    const QString labelStyle = "padding: 0 8px;";
    m_posLabel->setStyleSheet(labelStyle);
    m_encodingLabel->setStyleSheet(labelStyle);
    m_backgroundLabel->setStyleSheet(labelStyle);
    m_branchLabel->setStyleSheet(labelStyle);
    m_indexLabel->setStyleSheet(labelStyle + QStringLiteral("color:#75bfff;"));
    m_indexLabel->setToolTip(tr("Workspace index status"));
    m_copilotStatusLabel->setStyleSheet(labelStyle + QStringLiteral("color:#89d185;"));
    m_copilotStatusLabel->setCursor(Qt::PointingHandCursor);
    m_copilotStatusLabel->setToolTip(tr("AI Assistant status — click to open AI panel"));
    m_copilotStatusLabel->installEventFilter(this);

    statusBar()->addPermanentWidget(m_copilotStatusLabel);
    statusBar()->addPermanentWidget(m_indexLabel);
    statusBar()->addPermanentWidget(m_backgroundLabel);
    statusBar()->addPermanentWidget(m_branchLabel);
    statusBar()->addPermanentWidget(m_encodingLabel);
    statusBar()->addPermanentWidget(m_posLabel);
}

void MainWindow::createDockWidgets()
{
    // ── Project tree ──────────────────────────────────────────────────────
    m_projectDock = new QDockWidget(tr("Project"), this);
    m_projectDock->setObjectName("ProjectDock");
    m_projectDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_treeModel = new SolutionTreeModel(m_projectManager, m_gitService, this);

    m_fileTree = new QTreeView(m_projectDock);
    m_fileTree->setModel(m_treeModel);
    m_fileTree->setHeaderHidden(true);
    m_fileTree->setIndentation(14);
    m_fileTree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_fileTree, &QTreeView::doubleClicked, this, &MainWindow::openFileFromIndex);
    connect(m_fileTree, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);

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

    m_gitPanel = new GitPanel(m_gitService, this);
    m_gitDock  = new QDockWidget(tr("Git"), this);
    m_gitDock->setObjectName("GitDock");
    m_gitDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_gitDock->setWidget(m_gitPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_gitDock);
    tabifyDockWidget(m_searchDock, m_gitDock);

    // Generate commit message with AI
    connect(m_gitPanel, &GitPanel::generateCommitMessageRequested,
            this, [this]() {
        if (!m_gitService || !m_gitService->isGitRepo()) return;
        const QString diff = m_gitService->diff({});
        if (diff.trimmed().isEmpty()) {
            statusBar()->showMessage(tr("No changes to generate commit message for"), 3000);
            return;
        }
        // Send the diff to the AI chat with commit message intent
        const QString prompt = tr("Generate a concise, conventional commit message "
                                  "for these changes:\n\n```diff\n%1\n```").arg(diff.left(8000));
        m_chatPanel->focusInput();
        // Use the orchestrator to generate and insert into the commit input
        if (m_agentOrchestrator && m_agentOrchestrator->activeProvider()) {
            AgentRequest req;
            req.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            req.intent = AgentIntent::SuggestCommitMessage;
            req.userPrompt = prompt;
            req.workspaceRoot = m_currentFolder;
            statusBar()->showMessage(tr("Generating commit message..."), 0);
            m_agentOrchestrator->sendRequest(req);
            // Connect one-shot to receive the response
            auto conn = std::make_shared<QMetaObject::Connection>();
            *conn = connect(m_agentOrchestrator, &AgentOrchestrator::responseFinished,
                            this, [this, conn](const QString &, const AgentResponse &resp) {
                disconnect(*conn);
                QString msg = resp.text.trimmed();
                // Strip markdown code block wrapping if present
                if (msg.startsWith(QLatin1String("```")))
                    msg = msg.mid(msg.indexOf(QLatin1Char('\n')) + 1);
                if (msg.endsWith(QLatin1String("```")))
                    msg.chop(3);
                msg = msg.trimmed();
                m_gitPanel->commitMessageInput()->setText(msg);
                statusBar()->showMessage(tr("Commit message generated"), 3000);
            });
        }
    });

    // Resolve merge conflicts with AI
    connect(m_gitPanel, &GitPanel::resolveConflictsRequested,
            this, [this]() {
        if (!m_gitService || !m_gitService->isGitRepo()) return;
        const QStringList conflicts = m_gitService->conflictFiles();
        if (conflicts.isEmpty()) {
            statusBar()->showMessage(tr("No merge conflicts found"), 3000);
            return;
        }
        // Build a prompt with all conflict file contents
        QString prompt = tr("Resolve the following merge conflicts. "
                            "For each file, output the fully resolved content.\n\n");
        for (const QString &file : conflicts) {
            const QString content = m_gitService->conflictContent(file);
            const QString relPath = QDir(m_currentFolder).relativeFilePath(file);
            prompt += QStringLiteral("### %1\n```\n%2\n```\n\n")
                          .arg(relPath, content.left(4000));
        }
        m_chatPanel->setInputText(QStringLiteral("/resolve ") + prompt.left(12000));
        m_chatPanel->focusInput();
        m_aiDock->raise();
    });

    // ── Terminal ──────────────────────────────────────────────────────────
    m_terminal     = new TerminalPanel(this);
    m_terminalDock = new QDockWidget(tr("Terminal"), this);
    m_terminalDock->setObjectName("TerminalDock");
    m_terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_terminalDock->setWidget(m_terminal);
    addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);

    // ── AI ────────────────────────────────────────────────────────────────
    m_aiDock = new QDockWidget(tr("AI"), this);
    m_aiDock->setObjectName("AIDock");
    m_aiDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_chatPanel = new AgentChatPanel(m_agentOrchestrator, m_aiDock);
    // NOTE: AgentController is wired after it is created in the constructor.
    m_aiDock->setWidget(m_chatPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_aiDock);

    // ── Symbol outline ────────────────────────────────────────────────────
    m_symbolPanel = new SymbolOutlinePanel(this);
    m_symbolDock  = new QDockWidget(tr("Outline"), this);
    m_symbolDock->setObjectName("OutlineDock");
    m_symbolDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_symbolDock->setWidget(m_symbolPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_symbolDock);
    tabifyDockWidget(m_searchDock, m_symbolDock);
    m_projectDock->raise();   // project still visible by default

    connect(m_symbolPanel, &SymbolOutlinePanel::symbolActivated,
            this, [this](int line, int col) {
        auto *editor = currentEditor();
        if (!editor) return;
        const QTextBlock block = editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cur(block);
            cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                             qMin(col, block.length() - 1));
            editor->setTextCursor(cur);
            editor->centerCursor();
        }
        editor->setFocus();
    });

    // ── References ────────────────────────────────────────────────────────
    m_referencesPanel = new ReferencesPanel(this);
    m_referencesDock  = new QDockWidget(tr("References"), this);
    m_referencesDock->setObjectName("ReferencesDock");
    m_referencesDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_referencesDock->setWidget(m_referencesPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_referencesDock);
    tabifyDockWidget(m_terminalDock, m_referencesDock);
    m_terminalDock->raise();  // terminal visible by default

    connect(m_referencesPanel, &ReferencesPanel::referenceActivated,
            this, &MainWindow::navigateToLocation);

    // ── Request Log (Debug) ───────────────────────────────────────────────
    m_requestLogPanel = new RequestLogPanel(this);
    m_requestLogDock  = new QDockWidget(tr("Request Log"), this);
    m_requestLogDock->setObjectName("RequestLogDock");
    m_requestLogDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_requestLogDock->setWidget(m_requestLogPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_requestLogDock);
    tabifyDockWidget(m_terminalDock, m_requestLogDock);
    m_requestLogDock->hide(); // hidden by default — debug panel

    // ── Trajectory Viewer (Debug) ─────────────────────────────────────────
    m_trajectoryPanel = new TrajectoryPanel(this);
    m_trajectoryDock  = new QDockWidget(tr("Trajectory"), this);
    m_trajectoryDock->setObjectName("TrajectoryDock");
    m_trajectoryDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_trajectoryDock->setWidget(m_trajectoryPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_trajectoryDock);
    tabifyDockWidget(m_requestLogDock, m_trajectoryDock);
    m_trajectoryDock->hide(); // hidden by default — debug panel

    // ── Settings ──────────────────────────────────────────────────────────
    m_settingsPanel = new SettingsPanel(this);
    m_settingsDock  = new QDockWidget(tr("Settings"), this);
    m_settingsDock->setObjectName("SettingsDock");
    m_settingsDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_settingsDock->setWidget(m_settingsPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_settingsDock);
    tabifyDockWidget(m_aiDock, m_settingsDock);
    m_aiDock->raise(); // AI panel visible by default

    // ── Memory Browser ────────────────────────────────────────────────────
    const QString memPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + QStringLiteral("/memories");
    m_symbolIndex  = new SymbolIndex(this);
    m_memoryBrowser = new MemoryBrowserPanel(memPath, this);
    m_memoryDock = new QDockWidget(tr("Memory"), this);
    m_memoryDock->setObjectName("MemoryDock");
    m_memoryDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_memoryDock->setWidget(m_memoryBrowser);
    addDockWidget(Qt::RightDockWidgetArea, m_memoryDock);
    tabifyDockWidget(m_settingsDock, m_memoryDock);
    m_memoryDock->hide(); // hidden by default

    // ── Theme Manager ─────────────────────────────────────────────────────
    m_themeManager = new ThemeManager(this);

    // ── Diff Viewer panel ─────────────────────────────────────────────────
    m_diffViewer = new DiffViewerPanel(this);
    m_diffDock = new QDockWidget(tr("Diff Viewer"), this);
    m_diffDock->setObjectName("DiffDock");
    m_diffDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    m_diffDock->setWidget(m_diffViewer);
    addDockWidget(Qt::BottomDockWidgetArea, m_diffDock);
    m_diffDock->hide();

    // ── MCP client + panel ────────────────────────────────────────────────
    m_mcpClient = new McpClient(this);
    m_mcpPanel  = new McpPanel(m_mcpClient, this);
    m_mcpDock   = new QDockWidget(tr("MCP Servers"), this);
    m_mcpDock->setObjectName("McpDock");
    m_mcpDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    m_mcpDock->setWidget(m_mcpPanel);
    addDockWidget(Qt::RightDockWidgetArea, m_mcpDock);
    tabifyDockWidget(m_memoryDock, m_mcpDock);
    m_mcpDock->hide();

    // ── Output / Build panel ──────────────────────────────────────────────
    m_outputPanel = new OutputPanel(this);
    m_outputDock  = new QDockWidget(tr("Output"), this);
    m_outputDock->setObjectName("OutputDock");
    m_outputDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_outputDock->setWidget(m_outputPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_outputDock);
    tabifyDockWidget(m_terminalDock, m_outputDock);
    m_outputDock->hide();

    connect(m_outputPanel, &OutputPanel::problemClicked,
            this, &MainWindow::navigateToLocation);

    // ── File Watch Service ────────────────────────────────────────────────
    m_fileWatcher = new FileWatchService(this);
    connect(m_fileWatcher, &FileWatchService::fileChangedExternally,
            this, [this](const QString &path) {
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *editor = qobject_cast<EditorView *>(m_tabs->widget(i));
            if (!editor) continue;
            if (editor->property("filePath").toString() == path) {
                QFile f(path);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString content = QString::fromUtf8(f.readAll());
                    if (content != editor->toPlainText()) {
                        auto answer = QMessageBox::question(
                            this, tr("File Changed"),
                            tr("'%1' has been modified externally.\nReload?")
                                .arg(QFileInfo(path).fileName()),
                            QMessageBox::Yes | QMessageBox::No);
                        if (answer == QMessageBox::Yes)
                            editor->setPlainText(content);
                    }
                }
                break;
            }
        }
    });

    // ── Context Pruner ────────────────────────────────────────────────────
    m_contextPruner = new ContextPruner(this);

    // ── Auto Compactor ────────────────────────────────────────────────────
    m_autoCompactor = new AutoCompactor(this);

    // ── Test Scaffold ─────────────────────────────────────────────────────
    m_testScaffold = new TestScaffold(this);

    // ── Auth Manager ──────────────────────────────────────────────────────
    m_authManager = new AuthManager(this);
    connect(m_authManager, &AuthManager::statusChanged, this, [this]() {
        m_copilotStatusLabel->setText(
            m_authManager->statusIcon() + QStringLiteral(" ") + m_authManager->statusText());
    });

    // ── Rename Suggestion Widget ──────────────────────────────────────────
    m_renameSuggestion = new RenameSuggestionWidget(nullptr);

    // ── Secure Key Storage ────────────────────────────────────────────────
    m_keyStorage = new SecureKeyStorage(this);

    // ── Prompt File Manager ───────────────────────────────────────────────
    m_promptFileManager = new PromptFileManager(this);

    // ── Merge Conflict Resolver ───────────────────────────────────────────
    m_mergeResolver = new MergeConflictResolver(this);

    // ── Inline Review Widget ──────────────────────────────────────────────
    m_inlineReview = new InlineReviewWidget(nullptr);
    connect(m_inlineReview, &InlineReviewWidget::navigateToLine, this, [this](int line) {
        auto *editor = currentEditor();
        if (editor) {
            QTextCursor cursor(editor->document()->findBlockByLineNumber(line - 1));
            editor->setTextCursor(cursor);
        }
    });

    // ── Model Registry ────────────────────────────────────────────────────
    m_modelRegistry = new ModelRegistry(this);

    // ── OAuth Manager ─────────────────────────────────────────────────────
    m_oauthManager = new OAuthManager(this);
    connect(m_oauthManager, &OAuthManager::loginSucceeded, this, [this](const QString &token) {
        if (m_keyStorage)
            m_keyStorage->storeKey(QStringLiteral("github_oauth"), token);
        statusBar()->showMessage(tr("Signed in successfully"), 5000);
    });
    connect(m_oauthManager, &OAuthManager::loginFailed, this, [this](const QString &err) {
        statusBar()->showMessage(tr("Sign-in failed: %1").arg(err), 5000);
    });

    // ── Domain Filter ─────────────────────────────────────────────────────
    m_domainFilter = new DomainFilter(this);

    // ── Commit Message Generator ──────────────────────────────────────────
    m_commitMsgGen = new CommitMessageGenerator(this);

    // ── Multi-Chunk Editor ────────────────────────────────────────────────
    m_multiChunkEditor = new MultiChunkEditor(this);

    // ── Language Completion Config ────────────────────────────────────────
    m_langCompletionConfig = new LanguageCompletionConfig(this);

    // ── Context Editor ────────────────────────────────────────────────────
    m_contextEditor = new ContextEditor(nullptr);

    // ── Trajectory Recorder ───────────────────────────────────────────────
    m_trajectoryRecorder = new TrajectoryRecorder(this);

    // ── Feature Flags ─────────────────────────────────────────────────────
    m_featureFlags = new FeatureFlags(this);

    // ── Notebook Manager (stubs) ──────────────────────────────────────────
    m_notebookManager = new NotebookManager(this);

    // ── Review Manager ────────────────────────────────────────────────────
    m_reviewManager = new ReviewManager(this);

    // ── Test Generator ────────────────────────────────────────────────────
    m_testGenerator = new TestGenerator(this);
    connect(m_testGenerator, &TestGenerator::testsGenerated, this,
            [this](const QString &path, const QString &content) {
        Q_UNUSED(content);
        openFile(path);
        statusBar()->showMessage(tr("Tests generated: %1").arg(path), 5000);
    });

    // ── Auth Status Indicator ─────────────────────────────────────────────
    m_authIndicator = new AuthStatusIndicator(statusBar(), this);
    connect(m_oauthManager, &OAuthManager::loginStarted, this,
            [this]() { m_authIndicator->setState(AuthStatusIndicator::SigningIn); });
    connect(m_oauthManager, &OAuthManager::loginSucceeded, this,
            [this](const QString &) { m_authIndicator->setState(AuthStatusIndicator::SignedIn); });
    connect(m_oauthManager, &OAuthManager::loginFailed, this,
            [this](const QString &) { m_authIndicator->setState(AuthStatusIndicator::SignedOut); });
    connect(m_oauthManager, &OAuthManager::loggedOut, this,
            [this]() { m_authIndicator->setState(AuthStatusIndicator::SignedOut); });

    // ── Prompt File Picker ────────────────────────────────────────────────
    m_promptPicker = new PromptFilePicker(nullptr);

    // ── Chat Theme Adapter ────────────────────────────────────────────────
    m_chatTheme = new ChatThemeAdapter(this);

    // ── API Key Manager Widget ────────────────────────────────────────────
    m_apiKeyManager = new APIKeyManagerWidget(nullptr);
    connect(m_apiKeyManager, &APIKeyManagerWidget::keySaved, this,
            [this](const QString &provider, const QString &key) {
        if (m_keyStorage) m_keyStorage->storeKey(provider, key);
    });
    connect(m_apiKeyManager, &APIKeyManagerWidget::keyRemoved, this,
            [this](const QString &provider) {
        if (m_keyStorage) m_keyStorage->deleteKey(provider);
    });

    // ── Network Monitor ──────────────────────────────────────────────────
    m_networkMonitor = new NetworkMonitor(this);
    connect(m_networkMonitor, &NetworkMonitor::wentOffline, this,
            [this]() { m_authIndicator->setState(AuthStatusIndicator::Offline); });
    connect(m_networkMonitor, &NetworkMonitor::wentOnline, this,
            [this]() { m_authIndicator->setState(AuthStatusIndicator::SignedIn); });
    m_networkMonitor->start();

    // ── Telemetry Manager ─────────────────────────────────────────────────
    m_telemetry = new TelemetryManager(this);

    // ── Citation Manager ──────────────────────────────────────────────────
    m_citationManager = new CitationManager(this);

    // ── MCP Server Manager ────────────────────────────────────────────────
    m_mcpServerManager = new MCPServerManager(this);

    // ── Workspace Chunk Index ─────────────────────────────────────────────
    m_chunkIndex = new WorkspaceChunkIndex(this);
    connect(m_chunkIndex, &WorkspaceChunkIndex::indexingStarted, this,
            [this]() { if (m_indexLabel) m_indexLabel->setText(tr("Indexing...")); });
    connect(m_chunkIndex, &WorkspaceChunkIndex::indexProgress, this,
            [this](int done, int total) {
        if (m_indexLabel) m_indexLabel->setText(tr("Indexing %1/%2").arg(done).arg(total));
    });
    connect(m_chunkIndex, &WorkspaceChunkIndex::indexingFinished, this,
            [this](int count) {
        if (m_indexLabel) m_indexLabel->setText(tr("%1 chunks indexed").arg(count));
    });

    // ── Model Config Widget ───────────────────────────────────────────────
    m_modelConfig = new ModelConfigWidget(nullptr);

    // ── Context Inspector ─────────────────────────────────────────────────
    m_contextInspector = new ContextInspector(nullptr);

    // ── Tool Call Trace ───────────────────────────────────────────────────
    m_toolCallTrace = new ToolCallTrace(nullptr);

    // ── Trajectory Replay Widget ──────────────────────────────────────────
    m_trajectoryReplay = new TrajectoryReplayWidget(nullptr);

    // ── Prompt Archive Exporter ───────────────────────────────────────────
    m_promptArchive = new PromptArchiveExporter(nullptr);

    // ── Batch 7 ───────────────────────────────────────────────────────────
    m_memoryFileEditor = new MemoryFileEditor(nullptr);
    m_bgCompactor = new BackgroundCompactor(nullptr);
    m_errorState = new ErrorStateWidget(nullptr);
    m_setupTestsWizard = new SetupTestsWizard(nullptr);
    m_renameIntegration = new RenameIntegration(nullptr);
    m_accessibilityHelper = new AccessibilityHelper(nullptr);
    m_toolToggleManager = new ToolToggleManager(nullptr);
    m_contextScopeConfig = new ContextScopeConfig(nullptr);
    m_embeddingIndex = new EmbeddingIndex(nullptr);
    m_regexSearch = new RegexSearchEngine(nullptr);

    // ── Go-to-Definition (F12) ────────────────────────────────────────────
    connect(m_lspClient, &LspClient::definitionResult, this,
            [this](const QString &/*uri*/, const QJsonArray &locations) {
        if (locations.isEmpty()) {
            statusBar()->showMessage(tr("No definition found"), 3000);
            return;
        }
        const QJsonObject loc = locations.first().toObject();
        const QString targetUri = loc.value(QStringLiteral("uri")).toString();
        const QJsonObject range = loc.value(QStringLiteral("range")).toObject();
        const QJsonObject start = range.value(QStringLiteral("start")).toObject();
        const int line = start.value(QStringLiteral("line")).toInt();
        const int character = start.value(QStringLiteral("character")).toInt();

        QString path = QUrl(targetUri).toLocalFile();
        if (path.isEmpty()) path = targetUri;
        navigateToLocation(path, line, character);
    });

    // ── Find References ───────────────────────────────────────────────────
    connect(m_lspClient, &LspClient::referencesResult, this,
            [this](const QString &/*uri*/, const QJsonArray &locations) {
        if (locations.isEmpty()) {
            statusBar()->showMessage(tr("No references found"), 3000);
            return;
        }
        if (m_referencesPanel)
            m_referencesPanel->showReferences(tr("Symbol"), locations);
        if (m_referencesDock)
            m_referencesDock->show();
    });

    // ── Rename Symbol ─────────────────────────────────────────────────────
    connect(m_lspClient, &LspClient::renameResult, this,
            [this](const QString &/*uri*/, const QJsonObject &workspaceEdit) {
        applyWorkspaceEdit(workspaceEdit);
    });

    // When MCP discovers new tools, register them in the ToolRegistry
    connect(m_mcpPanel, &McpPanel::toolsChanged, this, [this]() {
        const QList<McpToolInfo> tools = m_mcpClient->allTools();
        for (const McpToolInfo &t : tools) {
            const QString regName = QStringLiteral("mcp_%1_%2").arg(t.serverName, t.name);
            if (!m_toolRegistry->hasTool(regName))
                m_toolRegistry->registerTool(new McpToolAdapter(m_mcpClient, t));
        }
    });

    // Wire settings to agent controller
    connect(m_settingsPanel, &SettingsPanel::settingsChanged, this, [this]() {
        if (m_agentController)
            m_agentController->setMaxStepsPerTurn(m_settingsPanel->maxSteps());
        if (m_contextBuilder)
            m_contextBuilder->setMaxContextChars(m_settingsPanel->contextTokenLimit() * 4);
        // Update disabled languages for inline completions
        if (m_inlineEngine) {
            const QStringList langs = m_settingsPanel->disabledCompletionLanguages();
            m_inlineEngine->setDisabledLanguages(QSet<QString>(langs.begin(), langs.end()));
            m_inlineEngine->setCompletionModel(m_settingsPanel->completionModel());
        }
        // Configure BYOK custom provider
        const QString endpoint = m_settingsPanel->customEndpoint();
        const QString apiKey   = m_settingsPanel->customApiKey();
        if (!endpoint.isEmpty() && !apiKey.isEmpty()) {
            if (!m_customProvider) {
                m_customProvider = new CustomProvider(this);
                m_agentOrchestrator->registerProvider(m_customProvider);
            }
            m_customProvider->configure(endpoint, apiKey);
        }
        // Update disabled tools
        if (m_toolRegistry) {
            const QStringList disabled = m_settingsPanel->disabledTools();
            m_toolRegistry->setDisabledTools(QSet<QString>(disabled.begin(), disabled.end()));
        }
    });
}

// ── Tabs / editor ────────────────────────────────────────────────────────────

void MainWindow::openNewTab()
{
    auto *editor = new EditorView();
    int index = m_tabs->addTab(editor, tr("Untitled"));
    m_tabs->setCurrentIndex(index);
    updateEditorStatus(editor);
    qInfo() << "  openNewTab: done";
}


void MainWindow::onTabChanged(int /*index*/)
{
    EditorView *editor = currentEditor();
    updateEditorStatus(editor);

    // Update breadcrumb
    if (editor) {
        const QString fp = editor->property("filePath").toString();
        const QString rt = m_projectManager
            ? m_projectManager->activeSolutionDir() : QString();
        m_breadcrumb->setFilePath(fp, rt);
    } else {
        m_breadcrumb->clear();
    }

    if (editor) {
        auto *bridge = editor->findChild<LspEditorBridge *>();
        if (bridge)
            bridge->requestSymbols();
        else
            m_symbolPanel->clear();

        // Attach inline completion engine to the active editor
        if (m_inlineEngine) {
            const QString path = editor->property("filePath").toString();
            const QString lang = editor->property("languageId").toString();
            m_inlineEngine->attachEditor(editor, path, lang);
        }

        // Update inline diff gutter for the active editor
        updateDiffRanges(editor);

        // Attach NES engine and wire ghost text acceptance
        if (m_nesEngine) {
            const QString path = editor->property("filePath").toString();
            m_nesEngine->attachEditor(editor, path);
            connect(editor, &EditorView::ghostTextAccepted, this, [this, editor] {
                if (!m_nesEngine) return;
                QTextCursor cur = editor->textCursor();
                // The ghost text was just inserted — the cursor is at end of insertion
                m_nesEngine->onEditAccepted(
                    editor->toPlainText().mid(
                        qMax(0, cur.position() - 200), 200),
                    cur.blockNumber(), cur.columnNumber());
            }, Qt::UniqueConnection);
        }
    } else {
        m_symbolPanel->clear();
        if (m_inlineEngine)
            m_inlineEngine->detachEditor();
        if (m_nesEngine)
            m_nesEngine->detachEditor();
    }
}

void MainWindow::updateDiffRanges(EditorView *editor)
{
    if (!editor || !m_gitService || !m_gitService->isGitRepo())
        return;
    const QString path = editor->property("filePath").toString();
    if (path.isEmpty()) {
        editor->clearDiffRanges();
        return;
    }
    const auto changes = m_gitService->lineChanges(path);
    QList<EditorView::DiffRange> ranges;
    ranges.reserve(changes.size());
    for (const auto &c : changes) {
        EditorView::DiffRange dr;
        dr.startLine = c.startLine;
        dr.lineCount = c.lineCount;
        dr.type = static_cast<EditorView::DiffType>(static_cast<int>(c.type));
        ranges.append(dr);
    }
    editor->setDiffRanges(ranges);
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
    if (!index.isValid() || m_treeModel->isDir(index)) return;
    openFile(m_treeModel->filePath(index));
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

    // Set language ID on the editor for inline chat / inline completion
    const QString langId = editor->property("languageId").toString();
    editor->setLanguageId(langId);

    // Ctrl+I inline chat
    connect(editor, &EditorView::inlineChatRequested,
            this, [this, editor, path](const QString &sel, const QString &lang) {
        showInlineChat(editor, sel, path, lang);
    });

    // AI context menu: wire all five actions to chat panel
    auto wireAiAction = [this](const QString &sel, const QString &fp,
                               const QString &lang, const QString &cmd) {
        m_chatPanel->setEditorContext(fp, sel, lang);
        m_chatPanel->focusInput();
        // Build the slash command text and insert it
        const QString prompt = sel.isEmpty()
            ? cmd
            : QStringLiteral("%1 %2").arg(cmd, sel.left(200));
        m_chatPanel->setInputText(prompt);
    };
    connect(editor, &EditorView::aiExplainRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/explain"));
    });
    connect(editor, &EditorView::aiReviewRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/review"));
    });
    connect(editor, &EditorView::aiFixRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/fix"));
    });
    connect(editor, &EditorView::aiTestRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/test"));
    });
    connect(editor, &EditorView::aiDocRequested,
            this, [wireAiAction](const QString &s, const QString &f, const QString &l) {
        wireAiAction(s, f, l, QStringLiteral("/doc"));
    });

    // Alt+\ manual completion trigger
    connect(editor, &EditorView::manualCompletionRequested,
            this, [this]() {
        if (m_inlineEngine)
            m_inlineEngine->triggerCompletion();
    });

    // Review annotation navigation (F8 next, Shift+F8 prev)
    auto *nextRevAction = new QAction(editor);
    nextRevAction->setShortcut(QKeySequence(Qt::Key_F8));
    nextRevAction->setShortcutContext(Qt::WidgetShortcut);
    connect(nextRevAction, &QAction::triggered, editor, &EditorView::nextReviewAnnotation);
    editor->addAction(nextRevAction);

    auto *prevRevAction = new QAction(editor);
    prevRevAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F8));
    prevRevAction->setShortcutContext(Qt::WidgetShortcut);
    connect(prevRevAction, &QAction::triggered, editor, &EditorView::prevReviewAnnotation);
    editor->addAction(prevRevAction);

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

    // Watch for external changes
    if (m_fileWatcher && !path.isEmpty())
        m_fileWatcher->watchFile(path);
}

void MainWindow::openFolder(const QString &path)
{
    QString folder = path;
    if (folder.isEmpty()) {
        folder = QFileDialog::getExistingDirectory(this, tr("Open Folder"));
    }
    if (folder.isEmpty()) return;

    const bool hasSolution = m_projectManager->openFolder(folder);
    m_gitService->setWorkingDirectory(folder);
    const QString root = m_projectManager->activeSolutionDir();
    m_searchPanel->setRootPath(root);
    setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
    statusBar()->showMessage(tr("Opened: %1").arg(root), 4000);
    if (hasSolution) {
        statusBar()->showMessage(tr("Solution: %1")
            .arg(QFileInfo(m_projectManager->solution().filePath).fileName()), 4000);
    }

    if (m_outputPanel)
        m_outputPanel->setWorkingDirectory(root);
    m_currentFolder = root;
    m_terminal->setWorkingDirectory(root);
    m_clangd->start(root);

    // Update agent tools with new workspace root
    m_contextBuilder->setWorkspaceRoot(root);

    // Load custom AI instructions from workspace (.github/copilot-instructions.md etc.)
    const QString instructions = PromptFileLoader::load(root);
    m_contextBuilder->setCustomInstructions(instructions);
    auto *searchTool = dynamic_cast<SearchWorkspaceTool *>(m_toolRegistry->tool(QStringLiteral("grep_search")));
    if (searchTool) searchTool->setWorkspaceRoot(root);
    auto *semSearchTool = dynamic_cast<SemanticSearchTool *>(m_toolRegistry->tool(QStringLiteral("semantic_search")));
    if (semSearchTool) semSearchTool->setWorkspaceRoot(root);
    auto *fileSrchTool = dynamic_cast<FileSearchTool *>(m_toolRegistry->tool(QStringLiteral("file_search")));
    if (fileSrchTool) fileSrchTool->setWorkspaceRoot(root);
    auto *cmdTool = dynamic_cast<RunCommandTool *>(m_toolRegistry->tool(QStringLiteral("run_in_terminal")));
    if (cmdTool) cmdTool->setWorkingDirectory(root);
    auto *readTool  = dynamic_cast<ReadFileTool  *>(m_toolRegistry->tool(QStringLiteral("read_file")));
    if (readTool)  readTool->setWorkspaceRoot(root);
    auto *listTool  = dynamic_cast<ListFilesTool *>(m_toolRegistry->tool(QStringLiteral("list_dir")));
    if (listTool)  listTool->setWorkspaceRoot(root);
    auto *writeTool = dynamic_cast<WriteFileTool *>(m_toolRegistry->tool(QStringLiteral("create_file")));
    if (writeTool) writeTool->setWorkspaceRoot(root);
    auto *projTool = dynamic_cast<ReadProjectStructureTool *>(m_toolRegistry->tool(QStringLiteral("read_project_structure")));
    if (projTool) projTool->setWorkspaceRoot(root);

    // Wire workspace root to chat panel for prompt files
    m_chatPanel->setWorkspaceRoot(root);

    // Scan for .prompt.md files
    if (m_promptFileManager)
        m_promptFileManager->scanWorkspace(root);

    // Set workspace root for commit message generator
    if (m_commitMsgGen)
        m_commitMsgGen->setWorkspaceRoot(root);

    // Start workspace indexer
    if (!m_workspaceIndexer) {
        m_workspaceIndexer = new WorkspaceIndexer(this);
        connect(m_workspaceIndexer, &WorkspaceIndexer::indexingStarted, this, [this] {
            m_indexLabel->setText(tr("\u2699 Indexing..."));
        });
        connect(m_workspaceIndexer, &WorkspaceIndexer::indexingProgress,
                this, [this](int done, int total) {
            m_indexLabel->setText(tr("\u2699 Indexing %1/%2").arg(done).arg(total));
        });
        connect(m_workspaceIndexer, &WorkspaceIndexer::indexingFinished,
                this, [this](int files, int chunks) {
            m_indexLabel->setText(tr("\u2713 %1 files (%2 chunks)")
                                 .arg(files).arg(chunks));
            // Rebuild symbol index from indexed chunks (already in memory)
            if (m_symbolIndex) {
                m_symbolIndex->clear();
                const auto allChunks = m_workspaceIndexer->search(QString(), 50000);
                for (const auto &chunk : allChunks) {
                    if (!chunk.symbolName.isEmpty())
                        m_symbolIndex->indexFile(chunk.filePath, chunk.content);
                }
            }
        });
    }
    m_workspaceIndexer->indexWorkspace(root);
}

void MainWindow::newSolution()
{
    const QString name = QInputDialog::getText(this, tr("New Solution"), tr("Solution name"));
    if (name.trimmed().isEmpty()) {
        return;
    }

    QString slnPath = QFileDialog::getSaveFileName(this, tr("Save Solution"),
                                                   QDir::currentPath(), tr("Exorcist Solution (*.exsln)"));
    if (slnPath.isEmpty()) {
        return;
    }
    if (!slnPath.endsWith(".exsln")) {
        slnPath += ".exsln";
    }

    if (m_projectManager->createSolution(name, slnPath)) {
        const QString root = m_projectManager->activeSolutionDir();
        m_gitService->setWorkingDirectory(root);
        m_searchPanel->setRootPath(root);
        setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
        m_currentFolder = root;
        m_terminal->setWorkingDirectory(root);
        m_clangd->start(root);
    }
}

void MainWindow::openSolutionFile()
{
    const QString slnPath = QFileDialog::getOpenFileName(this, tr("Open Solution"),
                                                        QDir::currentPath(), tr("Exorcist Solution (*.exsln)"));
    if (slnPath.isEmpty()) {
        return;
    }

    if (m_projectManager->openSolution(slnPath)) {
        const QString root = m_projectManager->activeSolutionDir();
        m_gitService->setWorkingDirectory(root);
        m_searchPanel->setRootPath(root);
        setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
        statusBar()->showMessage(tr("Solution: %1").arg(QFileInfo(slnPath).fileName()), 4000);

        m_currentFolder = root;
        m_terminal->setWorkingDirectory(root);
        m_clangd->start(root);
    }
}

void MainWindow::addProjectToSolution()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Add Folder to Solution"));
    if (folder.isEmpty()) {
        return;
    }
    const QString name = QInputDialog::getText(this, tr("Project Name"), tr("Project name"));
    const QString projectName = name.trimmed().isEmpty() ? QFileInfo(folder).baseName() : name.trimmed();

    if (m_projectManager->addProject(projectName, folder)) {
        if (!m_projectManager->solution().filePath.isEmpty()) {
            m_projectManager->saveSolution();
        }
    }
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
    QStringList files;
    QDirIterator it(m_currentFolder, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isFile())
            files << it.filePath();
    }

    CommandPalette palette(CommandPalette::FileMode, this);
    palette.setFiles(files);

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
        tr("Go to Symbol...\tCtrl+T"),
        tr("Quit\tCtrl+Q"),
    };

    CommandPalette palette(CommandPalette::CommandMode, this);
    palette.setCommands(commands);

    const QRect geo = geometry();
    palette.move(geo.center().x() - palette.minimumWidth() / 2,
                 geo.top() + 80);

    if (palette.exec() != QDialog::Accepted) return;

    switch (palette.selectedCommandIndex()) {
    case 0:  openNewTab();           break;
    case 1: {
        const QString p = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!p.isEmpty()) openFile(p);
        break;
    }
    case 2:  openFolder();           break;
    case 3:  saveCurrentTab();       break;
    case 4:  showFilePalette();      break;
    case 5:
        if (auto *e = currentEditor()) e->showFindBar();
        break;
    case 6:  m_projectDock->setVisible(!m_projectDock->isVisible());  break;
    case 7:  m_searchDock->setVisible(!m_searchDock->isVisible());    break;
    case 8:  m_terminalDock->setVisible(!m_terminalDock->isVisible()); break;
    case 9:  m_aiDock->setVisible(!m_aiDock->isVisible());            break;
    case 10: showSymbolPalette();    break;
    case 11: close();                break;
    default: break;
    }
}

void MainWindow::showSymbolPalette()
{
    if (!m_symbolIndex) return;

    CommandPalette palette(CommandPalette::CommandMode, this);

    const auto allSyms = m_symbolIndex->search(QString(), 500);
    QStringList items;
    for (const auto &sym : allSyms) {
        const QString kindStr = sym.kind == WorkspaceSymbol::Class ? QStringLiteral("class")
            : sym.kind == WorkspaceSymbol::Struct ? QStringLiteral("struct")
            : sym.kind == WorkspaceSymbol::Enum ? QStringLiteral("enum")
            : sym.kind == WorkspaceSymbol::Namespace ? QStringLiteral("namespace")
            : QStringLiteral("func");
        items << QStringLiteral("%1  [%2]  %3:%4")
                 .arg(sym.name, kindStr, QFileInfo(sym.filePath).fileName())
                 .arg(sym.line);
    }

    palette.setCommands(items);
    const QRect geo = geometry();
    palette.move(geo.center().x() - palette.minimumWidth() / 2,
                 geo.top() + 80);

    if (palette.exec() != QDialog::Accepted) return;
    const int idx = palette.selectedCommandIndex();
    if (idx < 0 || idx >= allSyms.size()) return;

    const auto &sym = allSyms[idx];
    openFile(sym.filePath);

    if (auto *editor = currentEditor()) {
        QTextCursor cursor = editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, sym.line - 1);
        editor->setTextCursor(cursor);
        editor->centerCursor();
    }
}

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
    s.setValue(QStringLiteral("lastFolder"),  m_projectManager->activeSolutionDir());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_copilotStatusLabel && event->type() == QEvent::MouseButtonPress) {
        m_chatPanel->focusInput();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
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
    auto *bridge = new LspEditorBridge(editor, m_lspClient, path, editor);

    connect(bridge, &LspEditorBridge::navigateToLocation,
            this,   &MainWindow::navigateToLocation);

    // Open #include files: resolve relative to current file, then project root
    connect(editor, &EditorView::openIncludeRequested,
            this, [this, path](const QString &includePath) {
        const QFileInfo fi(path);
        // Try relative to current file's directory
        const QString relPath = fi.absolutePath() + "/" + includePath;
        if (QFileInfo::exists(relPath)) {
            openFile(QFileInfo(relPath).absoluteFilePath());
            return;
        }
        // Try relative to project root
        if (!m_currentFolder.isEmpty()) {
            const QString rootPath = m_currentFolder + "/" + includePath;
            if (QFileInfo::exists(rootPath)) {
                openFile(QFileInfo(rootPath).absoluteFilePath());
                return;
            }
        }
        statusBar()->showMessage(tr("Could not find: %1").arg(includePath), 4000);
    });

    connect(bridge, &LspEditorBridge::referencesFound,
            this, [this](const QString &symbol, const QJsonArray &locations) {
        m_referencesPanel->showReferences(symbol, locations);
        m_referencesDock->raise();
    });

    connect(bridge, &LspEditorBridge::workspaceEditReady,
            this, &MainWindow::applyWorkspaceEdit);

    connect(bridge, &LspEditorBridge::symbolsUpdated,
            this, [this, editor](const QString &/*uri*/, const QJsonArray &symbols) {
        if (m_tabs->currentWidget() == editor)
            m_symbolPanel->setSymbols(symbols);
    });
}

void MainWindow::showInlineChat(EditorView *editor,
                                const QString &selectedText,
                                const QString &filePath,
                                const QString &languageId)
{
    if (!m_agentOrchestrator) return;

    // Create once, reuse (parented to this so it's deleted with the window)
    if (!m_inlineChat) {
        m_inlineChat = new InlineChatWidget(m_agentOrchestrator, this);
        m_inlineChat->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);

        connect(m_inlineChat, &InlineChatWidget::dismissed,
                this, [this]() { m_inlineChat->hide(); });

        connect(m_inlineChat, &InlineChatWidget::applyRequested,
                this, [this](const QString &code) {
            if (auto *ed = currentEditor()) {
                QTextCursor c = ed->textCursor();
                if (c.hasSelection())
                    c.insertText(code);
                else {
                    c.movePosition(QTextCursor::StartOfLine);
                    c.insertText(code + '\n');
                }
            }
        });

        connect(m_inlineChat, &InlineChatWidget::applyMultiEditsRequested,
                this, [this](const QList<EditChunk> &chunks) {
            if (auto *ed = currentEditor()) {
                QTextCursor c = ed->textCursor();
                c.beginEditBlock();
                // Apply chunks bottom-up (reversed) to avoid line shifts
                for (int i = chunks.size() - 1; i >= 0; --i) {
                    const auto &ch = chunks[i];
                    const QTextBlock startBlock = ed->document()->findBlockByLineNumber(ch.startLine - 1);
                    const QTextBlock endBlock   = ed->document()->findBlockByLineNumber(ch.endLine - 1);
                    if (!startBlock.isValid()) continue;

                    c.setPosition(startBlock.position());
                    if (endBlock.isValid())
                        c.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
                    else
                        c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
                    c.insertText(ch.newCode);
                }
                c.endEditBlock();
            }
        });
    }

    m_inlineChat->setContext(selectedText, filePath, languageId);
    m_inlineChat->setFileContent(editor->toPlainText());

    // Position widget in bottom-right corner of the editor, floating above it
    const QPoint editorGlobal = editor->mapToGlobal(QPoint(0, 0));
    const int x = editorGlobal.x() + editor->width()  - m_inlineChat->sizeHint().width()  - 16;
    const int y = editorGlobal.y() + editor->height() - m_inlineChat->sizeHint().height() - 40;
    m_inlineChat->move(x, y);
    m_inlineChat->show();
    m_inlineChat->raise();
    m_inlineChat->focusInput();
}

void MainWindow::navigateToLocation(const QString &path, int line, int character)
{
    openFile(path);   // opens or switches to the existing tab

    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *editor = qobject_cast<EditorView *>(m_tabs->widget(i));
        if (!editor || editor->property("filePath").toString() != path)
            continue;

        m_tabs->setCurrentIndex(i);

        const QTextBlock block = editor->document()->findBlockByNumber(line);
        if (block.isValid()) {
            QTextCursor cur(block);
            cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                             qMin(character, block.length() - 1));
            editor->setTextCursor(cur);
            editor->centerCursor();
        }
        editor->setFocus();
        break;
    }
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

void MainWindow::applyWorkspaceEdit(const QJsonObject &workspaceEdit)
{
    auto applyEdits = [this](const QString &filePath, const QJsonArray &edits) {
        if (filePath.isEmpty() || edits.isEmpty()) return;
        openFile(filePath);
        for (int i = 0; i < m_tabs->count(); ++i) {
            auto *editor = qobject_cast<EditorView *>(m_tabs->widget(i));
            if (!editor || editor->property("filePath").toString() != filePath) continue;
            editor->applyTextEdits(edits);
            break;
        }
    };

    if (workspaceEdit.contains("changes")) {
        const QJsonObject changes = workspaceEdit["changes"].toObject();
        for (auto it = changes.begin(); it != changes.end(); ++it)
            applyEdits(QUrl(it.key()).toLocalFile(), it.value().toArray());
    } else if (workspaceEdit.contains("documentChanges")) {
        for (const QJsonValue &v : workspaceEdit["documentChanges"].toArray()) {
            const QJsonObject dc = v.toObject();
            const QString uri = dc["textDocument"].toObject()["uri"].toString();
            applyEdits(QUrl(uri).toLocalFile(), dc["edits"].toArray());
        }
    }
}

// ── File tree context menu ────────────────────────────────────────────────────

void MainWindow::onTreeContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_fileTree->indexAt(pos);
    const QString path = index.isValid()
        ? m_treeModel->data(index, SolutionTreeModel::FilePathRole).toString()
        : QString();
    const bool isDir = index.isValid() && m_treeModel->isDir(index);
    const bool isFile = index.isValid() && !isDir && !path.isEmpty();
    const QString dirPath = isFile ? QFileInfo(path).absolutePath()
                          : (isDir && !path.isEmpty() ? path : m_currentFolder);

    QMenu menu(this);

    // ── New File / New Folder ─────────────────────────────────────────────
    QAction *newFileAction = menu.addAction(tr("New File..."));
    connect(newFileAction, &QAction::triggered, this, [this, dirPath]() {
        if (dirPath.isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("New File"), tr("File name:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        const QString filePath = dirPath + "/" + name.trimmed();
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.close();
            m_treeModel->rebuildFromSolution();
            openFile(filePath);
        }
    });

    QAction *newFolderAction = menu.addAction(tr("New Folder..."));
    connect(newFolderAction, &QAction::triggered, this, [this, dirPath]() {
        if (dirPath.isEmpty()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, tr("New Folder"), tr("Folder name:"),
            QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        QDir(dirPath).mkdir(name.trimmed());
        m_treeModel->rebuildFromSolution();
    });

    menu.addSeparator();

    // ── Rename ────────────────────────────────────────────────────────────
    QAction *renameAction = menu.addAction(tr("Rename..."));
    renameAction->setEnabled(!path.isEmpty());
    connect(renameAction, &QAction::triggered, this, [this, path, isFile]() {
        const QFileInfo fi(path);
        bool ok = false;
        const QString newName = QInputDialog::getText(
            this, tr("Rename"), tr("New name:"),
            QLineEdit::Normal, fi.fileName(), &ok);
        if (!ok || newName.trimmed().isEmpty() || newName == fi.fileName())
            return;
        const QString newPath = fi.absolutePath() + "/" + newName.trimmed();
        if (QFile::rename(path, newPath)) {
            m_treeModel->rebuildFromSolution();
            // Update any open tab pointing to the old path
            if (isFile) {
                for (int i = 0; i < m_tabs->count(); ++i) {
                    if (m_tabs->widget(i)->property("filePath").toString() == path) {
                        m_tabs->widget(i)->setProperty("filePath", newPath);
                        m_tabs->setTabText(i, QFileInfo(newPath).fileName());
                        break;
                    }
                }
            }
        }
    });

    // ── Delete ────────────────────────────────────────────────────────────
    QAction *deleteAction = menu.addAction(tr("Delete"));
    deleteAction->setEnabled(!path.isEmpty());
    connect(deleteAction, &QAction::triggered, this, [this, path, isDir]() {
        const QString msg = isDir
            ? tr("Delete folder \"%1\" and all its contents?").arg(QFileInfo(path).fileName())
            : tr("Delete \"%1\"?").arg(QFileInfo(path).fileName());
        if (QMessageBox::question(this, tr("Delete"), msg) != QMessageBox::Yes)
            return;
        bool ok = false;
        if (isDir) {
            ok = QDir(path).removeRecursively();
        } else {
            ok = QFile::remove(path);
        }
        if (ok) m_treeModel->rebuildFromSolution();
    });

    menu.addSeparator();

    // ── Copy actions ──────────────────────────────────────────────────────
    QAction *copyNameAction = menu.addAction(tr("Copy Name"));
    copyNameAction->setEnabled(!path.isEmpty());
    connect(copyNameAction, &QAction::triggered, this, [path]() {
        QGuiApplication::clipboard()->setText(QFileInfo(path).fileName());
    });

    QAction *copyPathAction = menu.addAction(tr("Copy Path"));
    copyPathAction->setEnabled(!path.isEmpty());
    connect(copyPathAction, &QAction::triggered, this, [path]() {
        QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(path));
    });

    QAction *copyRelativeAction = menu.addAction(tr("Copy Relative Path"));
    copyRelativeAction->setEnabled(!path.isEmpty() && !m_currentFolder.isEmpty());
    connect(copyRelativeAction, &QAction::triggered, this, [this, path]() {
        QDir root(m_currentFolder);
        QGuiApplication::clipboard()->setText(root.relativeFilePath(path));
    });

    menu.addSeparator();

    // ── Reveal in file manager ────────────────────────────────────────────
    QAction *revealAction = menu.addAction(tr("Reveal in File Manager"));
    revealAction->setEnabled(!path.isEmpty());
    connect(revealAction, &QAction::triggered, this, [path, isDir]() {
        const QString target = isDir ? path : QFileInfo(path).absolutePath();
        QDesktopServices::openUrl(QUrl::fromLocalFile(target));
    });

    // ── Open in terminal ──────────────────────────────────────────────────
    QAction *terminalAction = menu.addAction(tr("Open in Terminal"));
    terminalAction->setEnabled(!dirPath.isEmpty());
    connect(terminalAction, &QAction::triggered, this, [this, dirPath]() {
        m_terminalDock->raise();
        m_terminal->setWorkingDirectory(dirPath);
    });

    menu.exec(m_fileTree->viewport()->mapToGlobal(pos));
}

void MainWindow::loadPlugins()
{
    m_pluginManager = std::make_unique<PluginManager>();
    const QString pluginDir = QCoreApplication::applicationDirPath() + "/plugins";
    int loaded = m_pluginManager->loadPluginsFrom(pluginDir);
    m_pluginManager->initializeAll(m_services.get());

    // Register AI providers from agent plugins
    for (QObject *obj : m_pluginManager->pluginObjects()) {
        auto *agentPlugin = qobject_cast<IAgentPlugin *>(obj);
        if (agentPlugin) {
            const auto providers = agentPlugin->createProviders(m_agentOrchestrator);
            for (auto *provider : providers)
                m_agentOrchestrator->registerProvider(provider);
        }
    }

    // Wire inline completion engine to the first provider that supports it
    for (IAgentProvider *p : m_agentOrchestrator->providers()) {
        if (p->capabilities() & AgentCapability::InlineCompletion) {
            m_inlineEngine->setProvider(p);
            m_nesEngine->setProvider(p);
            break;
        }
    }

    // Update inline engine provider when active provider changes
    connect(m_agentOrchestrator, &AgentOrchestrator::activeProviderChanged,
            this, [this](const QString &) {
        IAgentProvider *active = m_agentOrchestrator->activeProvider();
        if (active && (active->capabilities() & AgentCapability::InlineCompletion)) {
            m_inlineEngine->setProvider(active);
            m_nesEngine->setProvider(active);
        }
    });

    // Update status bar on AI errors
    connect(m_agentOrchestrator, &AgentOrchestrator::responseError,
            this, [this](const QString &, const AgentError &error) {
        switch (error.code) {
        case AgentError::Code::AuthError:
            m_copilotStatusLabel->setText(tr("\u26A0 Auth Error"));
            m_copilotStatusLabel->setStyleSheet(QStringLiteral("padding: 0 8px; color:#f44747;"));
            m_copilotStatusLabel->setToolTip(tr("Authentication failed — check API key"));
            break;
        case AgentError::Code::RateLimited:
            m_copilotStatusLabel->setText(tr("\u23F2 Rate Limited"));
            m_copilotStatusLabel->setStyleSheet(QStringLiteral("padding: 0 8px; color:#ffb74d;"));
            m_copilotStatusLabel->setToolTip(tr("Rate limited — waiting for cooldown"));
            // Restore after 60s
            QTimer::singleShot(60000, this, [this]() {
                m_copilotStatusLabel->setText(tr("\u2713 AI Ready"));
                m_copilotStatusLabel->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
                m_copilotStatusLabel->setToolTip(tr("AI Assistant status — click to open AI panel"));
            });
            break;
        case AgentError::Code::NetworkError:
            m_copilotStatusLabel->setText(tr("\u26A0 Offline"));
            m_copilotStatusLabel->setStyleSheet(QStringLiteral("padding: 0 8px; color:#f44747;"));
            m_copilotStatusLabel->setToolTip(tr("Network error — check connection"));
            // Try to auto-recover after 10s
            QTimer::singleShot(10000, this, [this]() {
                if (m_agentOrchestrator && m_agentOrchestrator->activeProvider()
                    && m_agentOrchestrator->activeProvider()->isAvailable()) {
                    m_copilotStatusLabel->setText(tr("\u2713 AI Ready"));
                    m_copilotStatusLabel->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
                    m_copilotStatusLabel->setToolTip(tr("AI Assistant status — click to open AI panel"));
                }
            });
            break;
        default:
            break;
        }
    });

    // Restore status on successful response
    connect(m_agentOrchestrator, &AgentOrchestrator::responseFinished,
            this, [this](const QString &, const AgentResponse &) {
        if (m_copilotStatusLabel->text() != tr("\u2713 AI Ready")) {
            m_copilotStatusLabel->setText(tr("\u2713 AI Ready"));
            m_copilotStatusLabel->setStyleSheet(QStringLiteral("padding: 0 8px; color:#89d185;"));
            m_copilotStatusLabel->setToolTip(tr("AI Assistant status — click to open AI panel"));
        }
    });

    // ── Register agent tools ──────────────────────────────────────────────
    m_toolRegistry->registerTool(new ReadFileTool(m_fileSystem.get()));
    m_toolRegistry->registerTool(new ListFilesTool(m_fileSystem.get()));
    m_toolRegistry->registerTool(new WriteFileTool(m_fileSystem.get()));
    m_toolRegistry->registerTool(new ApplyPatchTool(m_fileSystem.get()));
    m_toolRegistry->registerTool(new ReplaceStringTool());
    m_toolRegistry->registerTool(new MultiReplaceStringTool());
    m_toolRegistry->registerTool(new InsertEditIntoFileTool());
    m_toolRegistry->registerTool(new SearchWorkspaceTool(m_currentFolder));
    m_toolRegistry->registerTool(new SemanticSearchTool(m_currentFolder));
    m_toolRegistry->registerTool(new FileSearchTool(m_currentFolder));
    m_toolRegistry->registerTool(new CreateDirectoryTool());
    m_toolRegistry->registerTool(new ManageTodoListTool(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/agent_todo.json")));
    m_toolRegistry->registerTool(new ReadProjectStructureTool(m_currentFolder));
    m_toolRegistry->registerTool(new RunCommandTool(
        new QtProcess(), m_currentFolder));
    m_toolRegistry->registerTool(new GitStatusTool([this]() -> QString {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        QString result = QStringLiteral("Branch: %1\n").arg(m_gitService->currentBranch());
        const auto statuses = m_gitService->fileStatuses();
        for (auto it = statuses.begin(); it != statuses.end(); ++it)
            result += QStringLiteral("  %1 %2\n").arg(it.value(), it.key());
        return result;
    }));
    m_toolRegistry->registerTool(new GetChangedFilesTool([this]() -> QHash<QString, QString> {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        return m_gitService->fileStatuses();
    }));
    m_toolRegistry->registerTool(new GitDiffTool([this](const QString &filePath) -> QString {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        return m_gitService->diff(filePath);
    }));
    m_toolRegistry->registerTool(new FetchWebpageTool());
    m_toolRegistry->registerTool(new WebSearchTool());
    m_toolRegistry->registerTool(new MemoryTool(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/memories")));
    m_toolRegistry->registerTool(new GetErrorsTool([this]() -> QList<AgentDiagnostic> {
        auto *ed = currentEditor();
        if (!ed) return {};
        QList<AgentDiagnostic> result;
        const QString path = ed->property("filePath").toString();
        for (const DiagnosticMark &d : ed->diagnosticMarks()) {
            AgentDiagnostic ag;
            ag.filePath = path;
            ag.line     = d.line;
            ag.column   = d.startChar;
            ag.message  = d.message;
            ag.severity = (d.severity == DiagSeverity::Error)
                ? AgentDiagnostic::Severity::Error
                : (d.severity == DiagSeverity::Warning)
                    ? AgentDiagnostic::Severity::Warning
                    : AgentDiagnostic::Severity::Info;
            result.append(ag);
        }
        return result;
    }));

    // Populate tool toggles in settings panel
    if (m_settingsPanel) {
        QStringList toolNames = m_toolRegistry->toolNames();
        toolNames.sort();
        m_settingsPanel->setToolNames(toolNames);
    }

    if (loaded > 0) {
        statusBar()->showMessage(tr("Loaded %1 plugins").arg(loaded), 4000);
    }
}
