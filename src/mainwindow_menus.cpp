// MainWindow::setupMenus — extracted from mainwindow.cpp.
//
// All menu bar setup lives here.  This is a partition of MainWindow's
// implementation across multiple .cpp files (Qt allows this — class
// definition stays in one .h, method bodies can be in many .cpp).  The
// goal is to keep mainwindow.cpp readable; a future pass will extract
// this into a true MenuBootstrap class with its own state.
//
// Sections, in order:
//   • File menu      (open / save / solution / close / quit)
//   • Edit menu      (undo / redo / cut / copy / paste / find / goto)
//   • View menu      (command palette / dock toggles / theme / focus)
//   • Window menu    (close-tab / next-prev tab / dynamic tab list)
//   • Tools menu     (empty rail — plugins fill it)
//   • Help menu      (About + status bar shortcut)
//   • AI shortcuts   (focus chat / toggle AI / quick chat / model picker)
//   • Dock-toggle wiring (View checkboxes ↔ DockManager state)

#include "mainwindow.h"

#include "agent/agentorchestrator.h"
#include "agent/agentplatformbootstrap.h"
#include "agent/chat/chatpanelwidget.h"
#include "agent/inlinechat/inlinechatwidget.h"
#include "agent/memorybrowserpanel.h"
#include "agent/modelpickerdialog.h"
#include "agent/quickchatdialog.h"
#include "agent/settingspanel.h"
#include "bootstrap/dockbootstrap.h"
#include "bootstrap/workbenchservicesbootstrap.h"
#include "core/itoolbarmanager.h"
#include "editor/diffviewerpanel.h"
#include "editor/editormanager.h"
#include "editor/editorview.h"
#include "git/gitservice.h"
#include "lsp/lspclient.h"
#include "project/filetemplatedialog.h"
#include "project/newpluginwizard.h"
#include "sdk/icommandservice.h"
#include "sdk/isearchservice.h"
#include "settings/languageprofile.h"
#include "settings/scopedsettings.h"
#include "settings/workspacesettings.h"
#include "ui/aboutdialog.h"
#include "ui/dock/DockManager.h"
#include "ui/extrasettingspages.h"
#include "ui/gotolinedialog.h"
#include "ui/keymapdialog.h"
#include "ui/quickopendialog.h"
#include "ui/recentfilesmanager.h"
#include "ui/settingsdialog.h"
#include "thememanager.h"
#include "ui/thememanager.h"
#include "ui/workspacesymbolsdialog.h"

#include "project/projectmanager.h"
#include "editor/filewatchservice.h"
#include "editor/codefoldingengine.h"
#include "ui/dock/ExDockWidget.h"

#include <QAbstractItemView>
#include <QTextBlock>
#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextOption>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

void MainWindow::setupMenus()
{
    // ── File ──────────────────────────────────────────────────────────────
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

    QAction *newTabAction = fileMenu->addAction(tr("New &Tab"));
    newTabAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));

    QAction *newFromTemplateAction = fileMenu->addAction(tr("New From &Template..."));
    newFromTemplateAction->setShortcut(QKeySequence::New);

    QAction *newPluginAction = fileMenu->addAction(tr("New &Plugin..."));
    // "New Qt Class" action is contributed by the qt-tools plugin via
    // IMenuManager::File menu location (command: qt.newClass).

    QAction *openAction = fileMenu->addAction(tr("&Open File..."));
    openAction->setShortcut(QKeySequence::Open);

    QAction *openFolderAction = fileMenu->addAction(tr("Open &Folder..."));
    openFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

    QAction *quickOpenAction = fileMenu->addAction(tr("Quick &Open File..."));
    quickOpenAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));
    connect(quickOpenAction, &QAction::triggered, this, [this]() {
        const QString root = m_editorMgr ? m_editorMgr->currentFolder() : QString();
        if (root.isEmpty()) return;
        QuickOpenDialog dlg(root, this);
        connect(&dlg, &QuickOpenDialog::fileSelected,
                this, &MainWindow::openFile);
        dlg.exec();
    });

    QAction *saveAction = fileMenu->addAction(tr("&Save"));
    saveAction->setShortcut(QKeySequence::Save);

    QAction *saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);

    fileMenu->addSeparator();

    auto *recentMgr = new RecentFilesManager(this);
    recentMgr->setObjectName(QStringLiteral("recentFilesManager"));

    auto *langProfileMgr = new LanguageProfileManager(this);
    langProfileMgr->setObjectName(QStringLiteral("languageProfileManager"));
    m_editorMgr->setLanguageProfileManager(langProfileMgr);
    QMenu *recentMenu = fileMenu->addMenu(tr("Recent &Files"));
    recentMgr->attachMenu(recentMenu);
    connect(recentMgr, &RecentFilesManager::fileSelected,
            this, [this](const QString &path) { openFile(path); });

    fileMenu->addSeparator();

    QMenu *solutionMenu = fileMenu->addMenu(tr("Solution"));
    QAction *newSolutionAction = solutionMenu->addAction(tr("New Solution..."));
    QAction *openSolutionAction = solutionMenu->addAction(tr("Open Solution (.exsln)..."));
    solutionMenu->addSeparator();
    QAction *addProjectAction = solutionMenu->addAction(tr("Add Folder to Solution..."));
    QAction *saveSolutionAction = solutionMenu->addAction(tr("Save Solution"));
    solutionMenu->addSeparator();
    QAction *closeSolutionAction = solutionMenu->addAction(tr("Close Solution"));

    QAction *closeFolderAction = fileMenu->addAction(tr("Close Folder"));

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);

    connect(newTabAction, &QAction::triggered, this, &MainWindow::openNewTab);
    connect(newFromTemplateAction, &QAction::triggered, this, [this]() {
        QString defaultFolder;
        if (m_editorMgr)
            defaultFolder = m_editorMgr->currentFolder();
        if (defaultFolder.isEmpty())
            defaultFolder = QDir::homePath();
        FileTemplateDialog dlg(defaultFolder, this);
        connect(&dlg, &FileTemplateDialog::templateSelected,
                this, [this](const QString &filePath, const QString & /*content*/) {
            openFile(filePath);
        });
        dlg.exec();
    });
    connect(newPluginAction, &QAction::triggered, this, [this]() {
        NewPluginWizard dlg(this);
        if (dlg.exec() == QDialog::Accepted) {
            const QString primary = dlg.primarySourceFile();
            if (!primary.isEmpty() && QFileInfo::exists(primary))
                openFile(primary);
        }
    });
    // newQtClassAction handler is in qt-tools plugin (command: qt.newClass).
    connect(openAction, &QAction::triggered, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
        if (!path.isEmpty()) openFile(path);
    });
    connect(openFolderAction, &QAction::triggered, this, [this]() { openFolder(); });
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveCurrentTab);
    connect(saveAsAction, &QAction::triggered, this, [this]() {
        EditorView *editor = currentEditor();
        if (!editor) return;
        const QString path = QFileDialog::getSaveFileName(this, tr("Save File As"));
        if (path.isEmpty()) return;
        editor->setProperty("filePath", path);
        saveCurrentTab();
    });
    connect(newSolutionAction, &QAction::triggered, this, &MainWindow::newSolution);
    connect(openSolutionAction, &QAction::triggered, this, &MainWindow::openSolutionFile);
    connect(addProjectAction, &QAction::triggered, this, &MainWindow::addProjectToSolution);
    connect(saveSolutionAction, &QAction::triggered, this, [this]() {
        m_editorMgr->projectManager()->saveSolution();
    });
    auto closeWorkspace = [this](const QString &statusText, bool unwatchFiles) {
        // Container concerns: project manager + editor tabs + scoped settings.
        m_editorMgr->projectManager()->closeSolution();
        m_editorMgr->closeAllTabs();
        m_editorMgr->setCurrentFolder(QString());
        ScopedSettings::instance().setWorkspaceRoot({});

        // Workspace teardown beyond container concerns is the plugins'
        // responsibility — broadcast the close event and each plugin
        // (cpp-language stops clangd, git resets working dir, search clears
        // root, terminal resets cwd, build clears project root, etc.) reacts
        // in its own onWorkspaceClosed() handler.  Rule L2.
        if (m_pluginManager)
            m_pluginManager->notifyWorkspaceChanged(QString());

        // File watcher is workbench infrastructure (container-level service,
        // not a plugin), so it stays here.
        if (unwatchFiles && m_workbenchServices && m_workbenchServices->fileWatcher())
            m_workbenchServices->fileWatcher()->unwatchAll();

        // Switch the central stack back to the welcome page (page 0).
        // The actual refresh of recent folders is fired via a command —
        // start-page plugin owns the welcome widget and handles refresh
        // itself in onWorkspaceClosed (rule L1, L3).
        if (m_centralStack)
            m_centralStack->setCurrentIndex(0);

        setWindowTitle(tr("Exorcist"));
        statusBar()->showMessage(statusText, 3000);
    };
    connect(closeSolutionAction, &QAction::triggered, this, [closeWorkspace]() {
        closeWorkspace(tr("Solution closed"), false);
    });
    connect(closeFolderAction, &QAction::triggered, this, [closeWorkspace]() {
        closeWorkspace(tr("Folder closed"), true);
    });
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // ── Edit ──────────────────────────────────────────────────────────────
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

    // Helper: dispatch edit actions to whichever text widget currently has focus.
    // Non-EditorView text widgets (output panels, search boxes, chat input etc.) must
    // receive Ctrl+C / Ctrl+A / Ctrl+X / Ctrl+V rather than the code editor.
    auto dispatchEdit = [this](auto editorCall, auto pteFn, auto teFn, auto leFn,
                               QKeySequence::StandardKey stdKey) {
        QWidget *fw = QApplication::focusWidget();
        if (fw && !qobject_cast<EditorView *>(fw)) {
            if (auto *pte = qobject_cast<QPlainTextEdit *>(fw)) { (pte->*pteFn)(); return; }
            if (auto *te  = qobject_cast<QTextEdit *>(fw))      { (te->*teFn)();  return; }
            if (auto *le  = qobject_cast<QLineEdit *>(fw))      { (le->*leFn)();  return; }
            // Tree/table/list views — let the widget's own handler process it.
            // Re-deliver the key sequence as a synthetic event since our menu
            // shortcut already consumed the original one.
            if (qobject_cast<QAbstractItemView *>(fw)
                || fw->inherits("QWebEngineView")
                || fw->inherits("ChatTranscriptView")) {
                const QKeySequence seq = QKeySequence(stdKey);
                if (!seq.isEmpty()) {
                    const int k = seq[0].toCombined();
                    QKeyEvent press(QEvent::KeyPress, k & ~Qt::KeyboardModifierMask,
                                    Qt::KeyboardModifiers(k & Qt::KeyboardModifierMask));
                    QApplication::sendEvent(fw, &press);
                }
                return;
            }
        }
        if (auto *e = m_editorMgr->currentEditor())
            editorCall(e);
    };

    QAction *undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->undo(); },
                     &QPlainTextEdit::undo, &QTextEdit::undo, &QLineEdit::undo,
                     QKeySequence::Undo);
    });

    QAction *redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->redo(); },
                     &QPlainTextEdit::redo, &QTextEdit::redo, &QLineEdit::redo,
                     QKeySequence::Redo);
    });

    editMenu->addSeparator();

    QAction *cutAction = editMenu->addAction(tr("Cu&t"));
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->cut(); },
                     &QPlainTextEdit::cut, &QTextEdit::cut, &QLineEdit::cut,
                     QKeySequence::Cut);
    });

    QAction *copyAction = editMenu->addAction(tr("&Copy"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->copy(); },
                     &QPlainTextEdit::copy, &QTextEdit::copy, &QLineEdit::copy,
                     QKeySequence::Copy);
    });

    QAction *pasteAction = editMenu->addAction(tr("&Paste"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->paste(); },
                     &QPlainTextEdit::paste, &QTextEdit::paste, &QLineEdit::paste,
                     QKeySequence::Paste);
    });

    editMenu->addSeparator();

    QAction *selectAllAction = editMenu->addAction(tr("Select &All"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, [this, dispatchEdit]() {
        dispatchEdit([](EditorView *e){ e->selectAll(); },
                     &QPlainTextEdit::selectAll, &QTextEdit::selectAll, &QLineEdit::selectAll,
                     QKeySequence::SelectAll);
    });

    editMenu->addSeparator();

    QAction *findAction = editMenu->addAction(tr("&Find..."));
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, [this]() {
        if (auto *e = m_editorMgr->currentEditor())
            e->showFindBar();
    });

    QAction *findReplAction = editMenu->addAction(tr("Find && &Replace..."));
    findReplAction->setShortcut(QKeySequence::Replace);
    connect(findReplAction, &QAction::triggered, this, [this]() {
        if (auto *e = m_editorMgr->currentEditor())
            e->showFindBar();
    });

    QAction *gotoLineAction = editMenu->addAction(tr("&Go to Line..."));
    gotoLineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(gotoLineAction, &QAction::triggered, this, [this]() {
        auto *editor = m_editorMgr->currentEditor();
        if (!editor) return;
        const int curLine = editor->textCursor().blockNumber() + 1;
        const int total   = editor->document()->blockCount();
        GoToLineDialog dlg(curLine, total, this);
        connect(&dlg, &GoToLineDialog::lineSelected, this, [editor](int line, int col) {
            QTextCursor cur(editor->document()->findBlockByNumber(line - 1));
            if (col > 0) cur.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col - 1);
            editor->setTextCursor(cur);
            editor->centerCursor();
        });
        dlg.exec();
    });

    QAction *gotoSymbolAction = editMenu->addAction(tr("Go to Symbol in &Workspace..."));
    gotoSymbolAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(gotoSymbolAction, &QAction::triggered, this, [this]() {
        WorkspaceSymbolsDialog dlg(this);
        dlg.setLspClient(m_services->service<LspClient>(QStringLiteral("lspClient")));
        connect(&dlg, &WorkspaceSymbolsDialog::symbolActivated, this,
                [this](const QString &fp, int line, int ch) {
            navigateToLocation(fp, line, ch);
        });
        dlg.exec();
    });

    editMenu->addSeparator();

    QAction *prefsAction = editMenu->addAction(tr("&Preferences..."));
    prefsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(prefsAction, &QAction::triggered, this, [this]() {
        SettingsDialog dlg(m_workbenchServices->themeManager(), this);

        // Add Language and Terminal settings pages
        auto *langMgr = findChild<LanguageProfileManager *>(
            QStringLiteral("languageProfileManager"));
        if (langMgr) {
            dlg.addPage(new LanguageSettingsPage(langMgr, &dlg));
        }
        dlg.addPage(new TerminalSettingsPage(&dlg));

        connect(&dlg, &SettingsDialog::settingsApplied, this, [this]() {
            // Notify WorkspaceSettings so it re-resolves global → workspace hierarchy
            if (auto *wss = m_services->service<WorkspaceSettings>(
                    QStringLiteral("workspaceSettings"))) {
                emit wss->settingsChanged();
            } else {
                // Fallback: apply editor settings directly from QSettings
                QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
                s.beginGroup(QStringLiteral("editor"));
                const QFont font(s.value(QStringLiteral("fontFamily"), QStringLiteral("Consolas")).toString(),
                                 s.value(QStringLiteral("fontSize"), 11).toInt());
                const int tabSize = s.value(QStringLiteral("tabSize"), 4).toInt();
                const bool wrap = s.value(QStringLiteral("wordWrap"), false).toBool();
                const bool minimap = s.value(QStringLiteral("showMinimap"), false).toBool();
                s.endGroup();

                for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                    if (auto *e = m_editorMgr->editorAt(i)) {
                        e->setFont(font);
                        e->setTabStopDistance(QFontMetricsF(font).horizontalAdvance(QLatin1Char(' ')) * tabSize);
                        e->setWordWrapMode(wrap ? QTextOption::WordWrap : QTextOption::NoWrap);
                        e->setMinimapVisible(minimap);
                    }
                }
            }

            // Re-index workspace if indexer settings changed
            if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
                srch->indexWorkspace(m_editorMgr->currentFolder());

            // Sync language profiles → plugin activation
            if (auto *lpm2 = findChild<LanguageProfileManager *>(
                    QStringLiteral("languageProfileManager"))) {
                const QSet<QString> active = lpm2->activeLanguageIds();
                m_pluginManager->setActiveLanguageProfiles(active);
                // Activate any deferred plugins whose profiles just became active
                for (const QString &lid : active)
                    m_pluginManager->activateByLanguageProfile(lid);
            }
        });
        dlg.exec();
    });

    QAction *keymapAction = editMenu->addAction(tr("&Keyboard Shortcuts..."));
    keymapAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_K, Qt::CTRL | Qt::Key_S));
    connect(keymapAction, &QAction::triggered, this, [this]() {
        KeymapDialog dlg(m_workbenchServices->keymapManager(), this);
        dlg.exec();
    });

    // ── View ──────────────────────────────────────────────────────────────
    QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

    QAction *cmdPaletteAction = viewMenu->addAction(tr("&Command Palette"));
    cmdPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));

    // Note: Ctrl+P is now bound to the new "Quick Open File..." action under
    // File menu (QuickOpenDialog). The legacy palette stays accessible from
    // the View menu without a shortcut.
    QAction *filePaletteAction = viewMenu->addAction(tr("&Go to File (legacy)..."));
QAction *symbolPaletteAction = viewMenu->addAction(tr("Go to &Symbol..."));
    symbolPaletteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));


    viewMenu->addSeparator();

    // Dock toggle actions.
    // They are added after createDockWidgets() populates the dock pointers, so
    // we use a lambda that defers the addAction until after the constructor.
    QAction *toggleProjectAction   = viewMenu->addAction(tr("&Project panel"));
    QAction *toggleSearchAction    = viewMenu->addAction(tr("&Search panel"));
    toggleSearchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
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
    QAction *togglePluginAction    = viewMenu->addAction(tr("E&xtensions"));
    QAction *toggleThemeAction     = viewMenu->addAction(tr("&Themes"));
    QAction *toggleOutputAction    = viewMenu->addAction(tr("&Output / Build"));
    QAction *toggleRunAction       = viewMenu->addAction(tr("&Run panel"));
    QAction *toggleDebugAction     = viewMenu->addAction(tr("&Debug panel"));
    QAction *toggleProblemsAction  = viewMenu->addAction(tr("&Problems panel"));
    QAction *toggleMarkdownPreviewAction = viewMenu->addAction(tr("Toggle &Markdown Preview"));
    // "Toggle Qt Help" view action: contributed by the qt-tools plugin via
    // IDockManager::panelToggleAction("QtHelpDock").
    viewMenu->addSeparator();
    QAction *toggleBlameAction     = viewMenu->addAction(tr("Toggle Git &Blame"));
    toggleBlameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_B));
    toggleBlameAction->setCheckable(true);
    toggleBlameAction->setChecked(false);

    QAction *themeToggleAction = viewMenu->addAction(tr("Toggle &Dark/Light Theme"));
    themeToggleAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F10));
    connect(themeToggleAction, &QAction::triggered, this, [this]() {
        m_workbenchServices->themeManager()->toggle();
    });

    QAction *minimapAction = viewMenu->addAction(tr("Toggle &Minimap"));
    minimapAction->setCheckable(true);
    minimapAction->setChecked(true);
    connect(minimapAction, &QAction::toggled, this, [this](bool on) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            if (auto *ev = m_editorMgr->editorAt(i))
                ev->setMinimapVisible(on);
        }
    });

    QAction *indentGuidesAction = viewMenu->addAction(tr("Toggle Indent &Guides"));
    indentGuidesAction->setCheckable(true);
    indentGuidesAction->setChecked(true);
    connect(indentGuidesAction, &QAction::toggled, this, [this](bool on) {
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            if (auto *ev = m_editorMgr->editorAt(i))
                ev->setIndentGuidesVisible(on);
        }
    });

    QAction *unfoldAllAction = viewMenu->addAction(tr("Unfold &All"));
    unfoldAllAction->setShortcut(QKeySequence(tr("Ctrl+Shift+]")));
    connect(unfoldAllAction, &QAction::triggered, this, [this]() {
        if (auto *ev = currentEditor())
            ev->foldingEngine()->unfoldAll();
    });

    connect(toggleBlameAction, &QAction::toggled, this, [this](bool checked) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        if (checked && m_gitService && m_gitService->isGitRepo()) {
            const QString path = editor->property("filePath").toString();
            if (path.isEmpty()) return;
            // Async blame — Manifesto #2: never block UI thread
            m_gitService->blameAsync(path);
        } else {
            editor->clearBlameData();
        }
    });
    connect(m_gitService, &GitService::blameReady, this,
            [this](const QString &filePath, const QList<GitService::BlameEntry> &entries) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        const QString editorPath = editor->property("filePath").toString();
        if (editorPath != filePath) return;
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
    });

    QAction *compareHeadAction = viewMenu->addAction(tr("Compare with &HEAD"));
    compareHeadAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_D));
    connect(compareHeadAction, &QAction::triggered, this, [this]() {
        EditorView *editor = currentEditor();
        if (!editor || !m_gitService || !m_gitService->isGitRepo()) return;
        const QString path = editor->property("filePath").toString();
        if (path.isEmpty()) return;
        // Async — Manifesto #2: never block UI thread
        m_gitService->showAtHeadAsync(path);
    });
    connect(m_gitService, &GitService::showAtHeadReady, this,
            [this](const QString &filePath, const QString &headText) {
        EditorView *editor = currentEditor();
        if (!editor) return;
        const QString currentText = editor->toPlainText();
        m_dockBootstrap->diffViewer()->showDiff(filePath, headText, currentText);
        m_dockManager->showDock(dock(QStringLiteral("DiffDock")), exdock::SideBarArea::Bottom);
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
    toggleMemoryAction->setCheckable(true);
    toggleMemoryAction->setChecked(false);
    toggleMcpAction->setCheckable(true);
    toggleMcpAction->setChecked(false);
    togglePluginAction->setCheckable(true);
    togglePluginAction->setChecked(false);
    toggleThemeAction->setCheckable(true);
    toggleThemeAction->setChecked(false);
    toggleOutputAction->setCheckable(true);
    toggleOutputAction->setChecked(false);
    toggleRunAction->setCheckable(true);
    toggleRunAction->setChecked(false);
    toggleDebugAction->setCheckable(true);
    toggleDebugAction->setChecked(false);
    toggleProblemsAction->setCheckable(true);
    toggleProblemsAction->setChecked(false);
    toggleMarkdownPreviewAction->setCheckable(true);
    toggleMarkdownPreviewAction->setChecked(false);

    // ── Focus / Zen Mode ──────────────────────────────────────────────────
    // Hides toolbars + dock sidebars + status bar — distraction-free editing.
    // Persisted via QSettings("Exorcist","Exorcist") key "focusMode".
    viewMenu->addSeparator();
    QAction *focusModeAction = viewMenu->addAction(tr("Toggle Focus &Mode"));
    focusModeAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K, Z")));
    focusModeAction->setShortcutContext(Qt::ApplicationShortcut);
    focusModeAction->setCheckable(true);
    focusModeAction->setToolTip(tr("Toggle Focus Mode (Ctrl+K, Z) — hide all panels and toolbars"));
    connect(focusModeAction, &QAction::triggered, this, [this, focusModeAction](bool checked) {
        m_focusMode = checked;
        if (statusBar()) statusBar()->setVisible(!m_focusMode);
        for (QToolBar *tb : findChildren<QToolBar *>())
            tb->setVisible(!m_focusMode);
        if (m_dockManager) m_dockManager->setDockLayoutVisible(!m_focusMode);
        QSettings(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"))
            .setValue(QStringLiteral("focusMode"), m_focusMode);
        if (statusBar())
            statusBar()->showMessage(m_focusMode
                ? tr("Focus Mode On — Ctrl+K Z to exit")
                : tr("Focus Mode Off"), 3000);
    });

    // ── Window ────────────────────────────────────────────────────────────
    QMenu *windowMenu = menuBar()->addMenu(tr("&Window"));

    QAction *closeTabAction = windowMenu->addAction(tr("&Close Tab"));
    closeTabAction->setShortcut(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, this, [this]() {
        if (m_editorMgr && m_editorMgr->tabs())
            m_editorMgr->closeTab(m_editorMgr->tabs()->currentIndex());
    });

    QAction *closeAllAction = windowMenu->addAction(tr("Close &All Tabs"));
    connect(closeAllAction, &QAction::triggered, this, [this]() {
        if (m_editorMgr) m_editorMgr->closeAllTabs();
    });

    QAction *closeOthersAction = windowMenu->addAction(tr("Close &Other Tabs"));
    connect(closeOthersAction, &QAction::triggered, this, [this]() {
        if (m_editorMgr && m_editorMgr->tabs())
            m_editorMgr->closeOtherTabs(m_editorMgr->tabs()->currentIndex());
    });

    windowMenu->addSeparator();

    QAction *nextTab = windowMenu->addAction(tr("&Next Tab"));
    nextTab->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab));
    connect(nextTab, &QAction::triggered, this, [this]() {
        if (!m_editorMgr || !m_editorMgr->tabs()) return;
        auto *tabs = m_editorMgr->tabs();
        if (tabs->count() == 0) return;
        int next = (tabs->currentIndex() + 1) % tabs->count();
        tabs->setCurrentIndex(next);
    });

    QAction *prevTab = windowMenu->addAction(tr("&Previous Tab"));
    prevTab->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab));
    connect(prevTab, &QAction::triggered, this, [this]() {
        if (!m_editorMgr || !m_editorMgr->tabs()) return;
        auto *tabs = m_editorMgr->tabs();
        if (tabs->count() == 0) return;
        int prev = (tabs->currentIndex() - 1 + tabs->count()) % tabs->count();
        tabs->setCurrentIndex(prev);
    });

    windowMenu->addSeparator();

    // Dynamic list of currently-open editor tabs. Rebuilt on each menu show.
    connect(windowMenu, &QMenu::aboutToShow, this, [this, windowMenu]() {
        // Remove dynamic actions added on previous show.
        const auto actions = windowMenu->actions();
        for (auto *a : actions) {
            if (a->property("dynamic").toBool()) {
                windowMenu->removeAction(a);
                a->deleteLater();
            }
        }
        if (!m_editorMgr || !m_editorMgr->tabs()) return;
        auto *tabs = m_editorMgr->tabs();
        for (int i = 0; i < tabs->count(); ++i) {
            const QString title = tabs->tabText(i);
            QAction *a = windowMenu->addAction(QStringLiteral("%1 %2").arg(i + 1).arg(title));
            a->setProperty("dynamic", true);
            a->setCheckable(true);
            a->setChecked(i == tabs->currentIndex());
            connect(a, &QAction::triggered, this, [tabs, i]() { tabs->setCurrentIndex(i); });
        }
    });

    // ── Tools ─────────────────────────────────────────────────────────────
    // Container creates the empty Tools menu; plugins (qt-tools, etc.)
    // contribute actions via IMenuManager::Tools.
    menuBar()->addMenu(tr("&Tools"));

    // ── Help ──────────────────────────────────────────────────────────────
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

    auto showAboutDialog = [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    };

    QAction *aboutAction = helpMenu->addAction(tr("&About Exorcist..."));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, showAboutDialog);

    // Status bar quick-access action: clickable "About" entry on the right side
    // of the status bar, surfaces version + build info without opening the menu.
    auto *aboutStatusBtn = new QToolButton(this);
    aboutStatusBtn->setText(tr("About"));
    aboutStatusBtn->setAutoRaise(true);
    aboutStatusBtn->setCursor(Qt::PointingHandCursor);
    aboutStatusBtn->setToolTip(tr("About Exorcist"));
    aboutStatusBtn->setStyleSheet(QStringLiteral(
        "QToolButton { color: #9cdcfe; border: none; padding: 0 8px; }"
        "QToolButton:hover { color: #ffffff; }"));
    connect(aboutStatusBtn, &QToolButton::clicked, this, showAboutDialog);
    statusBar()->addPermanentWidget(aboutStatusBtn);

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
        m_dockManager->toggleDock(dock(QStringLiteral("AIDock")));
        if (m_dockManager->isPinned(dock(QStringLiteral("AIDock"))))
            m_chatPanel->focusInput();
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

    // Wire dock toggles via DockManager.
    auto dockToggle = [this](const QString &dockId) {
        return [this, dockId](bool on) {
            auto *d = dock(dockId);
            if (!d) return;
            if (on)
                m_dockManager->showDock(d, m_dockManager->inferSide(d));
            else
                m_dockManager->closeDock(d);
        };
    };
    connect(toggleProjectAction,  &QAction::toggled, this, dockToggle(QStringLiteral("ProjectDock")));
    connect(toggleSearchAction,   &QAction::toggled, this, [this](bool on) {
        if (on) {
            m_dockManager->showDock(dock(QStringLiteral("SearchDock")), m_dockManager->inferSide(dock(QStringLiteral("SearchDock"))));
            if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
                srch->activateSearch();
        } else {
            m_dockManager->closeDock(dock(QStringLiteral("SearchDock")));
        }
    });
    connect(toggleGitAction,      &QAction::toggled, this, dockToggle(QStringLiteral("GitDock")));
    connect(toggleTerminalAction, &QAction::toggled, this, dockToggle(QStringLiteral("TerminalDock")));
    connect(toggleAiAction,       &QAction::toggled, this, dockToggle(QStringLiteral("AIDock")));
    connect(toggleOutlineAction,  &QAction::toggled, this, dockToggle(QStringLiteral("OutlineDock")));
    connect(toggleRefsAction,     &QAction::toggled, this, dockToggle(QStringLiteral("ReferencesDock")));
    connect(toggleLogAction,      &QAction::toggled, this, dockToggle(QStringLiteral("RequestLogDock")));
    connect(toggleTrajectoryAction, &QAction::toggled, this, dockToggle(QStringLiteral("TrajectoryDock")));
    connect(toggleSettingsAction, &QAction::triggered, this, [this]() {
        if (!m_settingsDialog) {
            m_settingsDialog = new QDialog(this);
            m_settingsDialog->setWindowTitle(tr("AI Settings"));
            m_settingsDialog->setMinimumSize(420, 480);
            auto *lay = new QVBoxLayout(m_settingsDialog);
            lay->setContentsMargins(0, 0, 0, 0);
            m_settingsPanel->setParent(m_settingsDialog);
            lay->addWidget(m_settingsPanel);
        }
        m_settingsDialog->show();
        m_settingsDialog->raise();
        m_settingsDialog->activateWindow();
    });
    connect(toggleMemoryAction,   &QAction::toggled, this, [this](bool on) {
        if (on)
            m_dockManager->showDock(dock(QStringLiteral("MemoryDock")), exdock::SideBarArea::Right);
        else
            m_dockManager->closeDock(dock(QStringLiteral("MemoryDock")));
        if (on) m_dockBootstrap->memoryBrowser()->refresh();
    });
    connect(toggleMcpAction,    &QAction::toggled, this, dockToggle(QStringLiteral("McpDock")));
    connect(togglePluginAction, &QAction::toggled, this, dockToggle(QStringLiteral("PluginDock")));
    connect(toggleThemeAction,  &QAction::toggled, this, dockToggle(QStringLiteral("ThemeDock")));
    connect(toggleOutputAction, &QAction::toggled, this, dockToggle(QStringLiteral("OutputDock")));
    connect(toggleRunAction,    &QAction::toggled, this, dockToggle(QStringLiteral("RunDock")));
    connect(toggleDebugAction,  &QAction::toggled, this, dockToggle(QStringLiteral("DebugDock")));
    connect(toggleProblemsAction, &QAction::toggled, this, dockToggle(QStringLiteral("ProblemsDock")));
    connect(toggleMarkdownPreviewAction, &QAction::toggled, this, dockToggle(QStringLiteral("MarkdownPreviewDock")));

    // Sync View-menu checkbox with dock state changes.
    // Use QSignalBlocker to prevent setChecked from firing toggled,
    // which would re-enter showDock/closeDock via dockToggle.
    auto syncAction = [](QAction *action, exdock::ExDockWidget *dock) {
        if (!dock) return; // Plugin-contributed docks may not exist yet
        QObject::connect(dock, &exdock::ExDockWidget::stateChanged,
                         action, [action](exdock::DockState s) {
            QSignalBlocker blocker(action);
            action->setChecked(s == exdock::DockState::Docked);
        });
    };
    syncAction(toggleProjectAction,    dock(QStringLiteral("ProjectDock")));
    syncAction(toggleSearchAction,     dock(QStringLiteral("SearchDock")));
    syncAction(toggleGitAction,        dock(QStringLiteral("GitDock")));
    syncAction(toggleTerminalAction,   dock(QStringLiteral("TerminalDock")));
    syncAction(toggleAiAction,         dock(QStringLiteral("AIDock")));
    syncAction(toggleOutlineAction,    dock(QStringLiteral("OutlineDock")));
    syncAction(toggleRefsAction,       dock(QStringLiteral("ReferencesDock")));
    syncAction(toggleLogAction,        dock(QStringLiteral("RequestLogDock")));
    syncAction(toggleTrajectoryAction, dock(QStringLiteral("TrajectoryDock")));
    syncAction(toggleMemoryAction,     dock(QStringLiteral("MemoryDock")));
    syncAction(toggleMcpAction,        dock(QStringLiteral("McpDock")));
    syncAction(togglePluginAction,     dock(QStringLiteral("PluginDock")));
    syncAction(toggleThemeAction,      dock(QStringLiteral("ThemeDock")));
    syncAction(toggleOutputAction,     dock(QStringLiteral("OutputDock")));
    syncAction(toggleRunAction,        dock(QStringLiteral("RunDock")));
    syncAction(toggleDebugAction,      dock(QStringLiteral("DebugDock")));
    syncAction(toggleProblemsAction,   dock(QStringLiteral("ProblemsDock")));
    syncAction(toggleMarkdownPreviewAction, dock(QStringLiteral("MarkdownPreviewDock")));
}
