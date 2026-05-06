// MainWindow workspace lifecycle — extracted from mainwindow.cpp.
//
// Open/close folder + solution lifecycle.  Methods stay on MainWindow class;
// this is a partition of the implementation across multiple .cpp files.
// Qt allows method bodies of one class to live in many .cpp.  A future pass
// will extract these into a true WorkspaceLifecycleBootstrap class.
//
// Methods in this file:
//   • openFolder            — main workspace open path (~450 lines)
//   • newSolution           — wizard + .exsln file
//   • openSolutionFile      — load existing .exsln
//   • addProjectToSolution  — extend solution with new/existing project
//   • writeCMakePresetsJson — file-scope helper used by new* methods

#include "mainwindow.h"

#include "agent/agentplatformbootstrap.h"
#include "agent/chat/chatpanelwidget.h"
#include "agent/contextbuilder.h"
#include "agent/diagnosticsnotifier.h"
#include "agent/promptfileloader.h"
#include "agent/promptfilemanager.h"
#include "git/commitmessagegenerator.h"
#include "bootstrap/aiservicesbootstrap.h"
#include "bootstrap/workbenchservicesbootstrap.h"
#include "core/ifilesystem.h"
#include "editor/codefoldingengine.h"
#include "editor/editormanager.h"
#include "editor/editorview.h"
#include "lsp/lspeditorbridge.h"
#include "git/gitservice.h"
#include "lsp/lspclient.h"
#include "profile/profilemanager.h"
#include "project/newprojectwizard.h"
#include "project/projectmanager.h"
#include "project/projecttemplateregistry.h"
#include "sdk/hostservices.h"
#include "sdk/ibuildsystem.h"
#include "sdk/idebugadapter.h"
#include "sdk/ilspservice.h"
#include "sdk/isearchservice.h"
#include "sdk/iterminalservice.h"
#include "sdk/kit.h"  // also defines IKitManager
#include "settings/scopedsettings.h"
#include "settings/workspacesettings.h"
#include "thememanager.h"
#include "ui/dock/DockManager.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextCursor>
#include <QTimer>
void MainWindow::openFolder(const QString &path)
{
    QString folder = path;
    if (folder.isEmpty()) {
        folder = QFileDialog::getExistingDirectory(this, tr("Open Folder"));
    }
    if (folder.isEmpty()) return;

    // Switch from welcome page to editor view
    if (m_centralStack && m_centralStack->currentIndex() == 0) {
        m_centralStack->setCurrentIndex(1);
        // Open a blank tab if no tabs exist yet
        if (m_editorMgr->tabs()->count() == 0)
            openNewTab();
    }

    // Save to recent folders list (MRU, max 10)
    {
        QSettings rs(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        QStringList recent = rs.value(QStringLiteral("recentFolders")).toStringList();
        recent.removeAll(folder);
        recent.prepend(folder);
        while (recent.size() > 10)
            recent.removeLast();
        rs.setValue(QStringLiteral("recentFolders"), recent);
    }

    const bool hasSolution = m_editorMgr->projectManager()->openFolder(folder);
    m_gitService->setWorkingDirectory(folder);
    const QString root = m_editorMgr->projectManager()->activeSolutionDir();
    if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
        srch->setRootPath(root);
    setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
    statusBar()->showMessage(tr("Opened: %1").arg(root), 4000);
    if (hasSolution) {
        statusBar()->showMessage(tr("Solution: %1")
            .arg(QFileInfo(m_editorMgr->projectManager()->solution().filePath).fileName()), 4000);
    }

    m_editorMgr->setCurrentFolder(root);

    // Apply workspace-level settings (.exorcist/settings.json)
    if (auto *wss = m_services->service<WorkspaceSettings>(QStringLiteral("workspaceSettings")))
        wss->setWorkspaceRoot(root);

    // Activate workspace overlay for ScopedSettings (User vs Workspace scope).
    ScopedSettings::instance().setWorkspaceRoot(root);

    // Activate plugins with workspaceContains activation events,
    // then notify all active plugins of the new workspace root.
    // Each plugin owns its own workspace-change response (cmake setup,
    // terminal cwd, config reload, etc.) — MainWindow knows nothing about them.
    if (m_pluginManager) {
        m_pluginManager->activateByWorkspace(root);
        m_pluginManager->notifyWorkspaceChanged(root);
    }

    // Wire LSP service signals that loadPlugins() post-wiring may have
    // missed for deferred plugins (e.g. CppLanguagePlugin activated above).
    if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService"))) {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        qWarning() << "[LSP-DIAG] openFolder deferred wiring: lspSvc found, lspClient="
                   << (lspClient ? "yes" : "null");
        if (lspClient) {
            connect(lspClient, &LspClient::initialized,
                    this, &MainWindow::onLspInitialized, Qt::UniqueConnection);
        }
        connect(lspSvc, &ILspService::navigateToLocation,
                this, &MainWindow::navigateToLocation, Qt::UniqueConnection);
        // Qt::UniqueConnection requires a pointer-to-member-function slot
        // (lambdas can't be deduplicated and Qt asserts at runtime — fatal
        // on the second openFolder call).  Route these through dedicated
        // member methods instead.
        connect(lspSvc, &ILspService::referencesReady,
                this, &MainWindow::onLspReferencesReady, Qt::UniqueConnection);
        connect(lspSvc, &ILspService::workspaceEditRequested,
                this, &MainWindow::applyWorkspaceEdit, Qt::UniqueConnection);
        connect(lspSvc, &ILspService::statusMessage,
                this, &MainWindow::onLspStatusMessage, Qt::UniqueConnection);
        if (lspClient && m_agentPlatform) {
            if (auto *notifier = m_agentPlatform->diagnosticsNotifier()) {
                connect(lspClient, &LspClient::diagnosticsPublished,
                        notifier, &DiagnosticsNotifier::onDiagnosticsPublished,
                        Qt::UniqueConnection);
            }
        }
        if (lspClient && m_hostServices) {
            connect(lspClient, &LspClient::diagnosticsPublished,
                    m_hostServices->diagnosticsService(),
                    &DiagnosticsServiceImpl::onDiagnosticsPublished,
                    Qt::UniqueConnection);
        }
    }

    if (auto *profileMgr = qobject_cast<ProfileManager *>(
            m_services->service(QStringLiteral("profileManager")))) {
        const QString detectedProfile = profileMgr->autoDetectEnabled()
            ? profileMgr->detectProfile(root)
            : profileMgr->activeProfile();
        if (!detectedProfile.isEmpty())
            profileMgr->activateProfile(detectedProfile);
    }

    // ── Contextual dock activation ─────────────────────────────────────
    // Show core panels relevant to the opened workspace.
    // Plugin-contributed docks (Output, Run, Debug) are shown by their
    // owning plugins, NOT by MainWindow — see plugin-first extension model.
    //
    // Deferred via singleShot(0) so the editor tab (and its LineNumberArea)
    // are fully laid out before the dock resize cascade fires. Calling
    // showDock synchronously here triggers Qt layout reflows that reach
    // EditorView::resizeEvent while the widget is still being initialised,
    // resulting in an access violation inside Qt6Widgets.dll.
    QTimer::singleShot(0, this, [this]() {
        m_dockManager->showDock(dock(QStringLiteral("ProjectDock")), exdock::SideBarArea::Left);
        if (m_gitService && m_gitService->isGitRepo())
            m_dockManager->showDock(dock(QStringLiteral("GitDock")), exdock::SideBarArea::Left);
        if (dock(QStringLiteral("AIDock")))
            m_dockManager->showDock(dock(QStringLiteral("AIDock")), exdock::SideBarArea::Right);
        m_dockManager->showDock(dock(QStringLiteral("TerminalDock")), exdock::SideBarArea::Bottom);
        if (dock(QStringLiteral("ProblemsDock")))
            m_dockManager->showDock(dock(QStringLiteral("ProblemsDock")), exdock::SideBarArea::Bottom);
    });

    // Auto-detect workspace languages and activate their profiles.
    // Two-pass approach:
    //   1. Marker files (build system indicators) — instant, top-level only
    //   2. File extension scan (depth-limited) — runs async to not block startup
    if (m_pluginManager) {
        // Pass 1: project file markers (instant)
        static const struct { const char *file; const char *lang; } markers[] = {
            {"CMakeLists.txt", "cpp"}, {"Cargo.toml", "rust"},
            {"package.json", "javascript"}, {"tsconfig.json", "typescript"},
            {"pyproject.toml", "python"}, {"setup.py", "python"},
            {"go.mod", "go"}, {"pom.xml", "java"},
            {"build.gradle", "java"}, {"*.sln", "csharp"},
        };
        QDir ws(root);
        for (const auto &m : markers) {
            if (!ws.entryList(QStringList{QString::fromLatin1(m.file)},
                              QDir::Files).isEmpty()) {
                const QString lang = QString::fromLatin1(m.lang);
                m_pluginManager->addWorkspaceLanguage(lang);
                m_pluginManager->activateByLanguageProfile(lang);
            }
        }

        // Pass 2: extension scan (async, depth-limited)
        // Scans actual files to discover languages not covered by markers
        QTimer::singleShot(100, this, [this, root]() {
            if (!m_pluginManager)
                return;

            // Extension → language ID mapping (subset of LspClient::languageIdForPath)
            static const QHash<QString, QString> extMap = {
                {"cpp", "cpp"}, {"cxx", "cpp"}, {"cc", "cpp"}, {"c", "cpp"},
                {"h", "cpp"}, {"hpp", "cpp"}, {"hxx", "cpp"},
                {"py", "python"}, {"pyw", "python"},
                {"js", "javascript"}, {"mjs", "javascript"},
                {"ts", "typescript"}, {"tsx", "typescript"},
                {"rs", "rust"}, {"go", "go"}, {"java", "java"},
                {"kt", "kotlin"}, {"kts", "kotlin"},
                {"swift", "swift"}, {"dart", "dart"},
                {"rb", "ruby"}, {"php", "php"}, {"lua", "lua"},
                {"scala", "scala"}, {"cs", "csharp"},
            };

            QSet<QString> detected;
            // Depth-limited BFS (max depth 3, max 2000 entries)
            struct Level { QString path; int depth; };
            QVector<Level> queue;
            queue.append({root, 0});
            int scanned = 0;
            constexpr int kMaxDepth = 3;
            constexpr int kMaxEntries = 2000;

            while (!queue.isEmpty() && scanned < kMaxEntries) {
                const auto cur = queue.takeFirst();
                QDir dir(cur.path);
                const auto entries = dir.entryInfoList(
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &fi : entries) {
                    if (++scanned > kMaxEntries)
                        break;
                    if (fi.isDir()) {
                        // Skip hidden dirs, build dirs, node_modules, .git
                        const QString name = fi.fileName();
                        if (name.startsWith(QLatin1Char('.'))
                            || name == QLatin1String("node_modules")
                            || name == QLatin1String("build")
                            || name == QLatin1String("dist")
                            || name == QLatin1String("__pycache__")
                            || name.startsWith(QLatin1String("build-")))
                            continue;
                        if (cur.depth < kMaxDepth)
                            queue.append({fi.absoluteFilePath(), cur.depth + 1});
                    } else {
                        const QString ext = fi.suffix().toLower();
                        auto it = extMap.find(ext);
                        if (it != extMap.end())
                            detected.insert(it.value());
                    }
                }
            }

            // Activate detected languages as workspace-level profiles
            for (const QString &lang : detected) {
                if (!m_pluginManager->activeLanguageProfiles().contains(lang)) {
                    m_pluginManager->addWorkspaceLanguage(lang);
                    m_pluginManager->activateByLanguageProfile(lang);
                }
            }

            if (!detected.isEmpty())
                qInfo("Workspace language scan: detected %lld language(s): %s",
                      static_cast<long long>(detected.size()),
                      qUtf8Printable(QStringList(detected.begin(), detected.end()).join(QStringLiteral(", "))));
        });
    }

    // ── Toolchain & CMake detection (via build plugin service) ──────────
    // Clangd start is deferred until after CMake detection so we can pass
    // --compile-commands-dir if compile_commands.json exists in a build dir.
    QTimer::singleShot(0, this, [this, root]() {
        auto *buildSys = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));

        QStringList clangdArgs;
        if (buildSys && buildSys->hasProject()) {
            // Find compile_commands.json — try build service first, then scan
            QString ccjson = buildSys->compileCommandsPath();
            if (ccjson.isEmpty()) {
                // Scan common build dirs for compile_commands.json
                const QStringList candidates = {
                    QStringLiteral("build-llvm"), QStringLiteral("build"),
                    QStringLiteral("build-debug"), QStringLiteral("build-release"),
                    QStringLiteral("build-ci"), QStringLiteral("cmake-build-debug"),
                };
                for (const auto &d : candidates) {
                    const QString p = root + QLatin1Char('/') + d
                                    + QStringLiteral("/compile_commands.json");
                    if (QFileInfo::exists(p)) { ccjson = p; break; }
                }
            }
            if (!ccjson.isEmpty()) {
                // Point clangd to the directory containing compile_commands.json
                const QString ccDir = QFileInfo(ccjson).absolutePath();
                clangdArgs << QStringLiteral("--compile-commands-dir=") + ccDir;

                // Extract include paths for #include file opening
                QFile ccFile(ccjson);
                if (ccFile.open(QIODevice::ReadOnly)) {
                    const QJsonArray arr = QJsonDocument::fromJson(ccFile.readAll()).array();
                    if (!arr.isEmpty()) {
                        const QString cmd = arr[0].toObject()[QStringLiteral("command")].toString();
                        QSet<QString> seen;
                        // Match -I<path>, -I <path>, -isystem <path>
                        const auto parts = cmd.split(QLatin1Char(' '));
                        for (int i = 0; i < parts.size(); ++i) {
                            QString dir;
                            if (parts[i].startsWith(QStringLiteral("-isystem")) && i + 1 < parts.size()) {
                                dir = parts[i + 1];
                                ++i;
                            } else if (parts[i].startsWith(QStringLiteral("-I"))) {
                                dir = parts[i].mid(2);
                                if (dir.isEmpty() && i + 1 < parts.size()) {
                                    dir = parts[++i];
                                }
                            }
                            if (!dir.isEmpty() && !seen.contains(dir)) {
                                seen.insert(dir);
                                m_editorMgr->addIncludePath(QDir::cleanPath(dir));
                            }
                        }
                        qWarning() << "Loaded" << m_editorMgr->includePaths().size() << "include paths from compile_commands.json";
                    }
                }
            } else {
                // No compile_commands.json yet — auto-configure if the active profile
                // requests it or if the project has CMakePresets.json.
                bool shouldAutoConfigure = false;
                if (auto *wss = m_services->service<WorkspaceSettings>(
                        QStringLiteral("workspaceSettings"))) {
                    shouldAutoConfigure = wss->value(QStringLiteral("build"),
                                                     QStringLiteral("autoConfigure"),
                                                     false).toBool();
                }

                const QString presetsPath = root + QStringLiteral("/CMakePresets.json");
                if (shouldAutoConfigure || QFileInfo::exists(presetsPath)) {
                    statusBar()->showMessage(
                        tr("CMake project detected \u2014 auto-configuring..."), 4000);

                    // Defer clangd start until configure completes
                    auto conn = std::make_shared<QMetaObject::Connection>();
                    *conn = connect(buildSys, &IBuildSystem::configureFinished,
                                    this, [this, root, buildSys, conn](bool ok, const QString &) {
                        QObject::disconnect(*conn);
                        if (!ok) return;

                        // After configure, compile_commands.json should exist
                        QString ccPath = buildSys->compileCommandsPath();
                        QStringList args;
                        if (!ccPath.isEmpty())
                            args << QStringLiteral("--compile-commands-dir=")
                                    + QFileInfo(ccPath).absolutePath();

                        if (auto *lsp = m_services->service<ILspService>(QStringLiteral("lspService")))
                            lsp->startServer(root, args);

                        statusBar()->showMessage(tr("CMake configured — LSP started"), 4000);
                    });

                    buildSys->configure();
                    return; // clangd will start from configureFinished callback
                } else {
                    statusBar()->showMessage(
                        tr("CMake project detected \u2014 click Configure to generate compile_commands.json"),
                        6000);
                }
            }
        }

        // Start language server (with compile_commands.json dir if available)
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->startServer(root, clangdArgs);
    });

    // Load custom workspace theme (.exorcist/theme.json)
    {
        const QString themePath = root + QStringLiteral("/.exorcist/theme.json");
        if (QFileInfo::exists(themePath)) {
            QString err;
            if (m_workbenchServices->themeManager()->loadCustomTheme(themePath, &err))
                m_workbenchServices->themeManager()->setTheme(ThemeManager::Custom);
            else
                statusBar()->showMessage(tr("Theme error: %1").arg(err), 5000);
        }
    }

    // Update agent tools with new workspace root
    if (m_agentPlatform)
        m_agentPlatform->setWorkspaceRoot(root);

    // Load custom AI instructions from workspace (.github/copilot-instructions.md etc.)
    const QString instructions = PromptFileLoader::load(root);
    m_agentPlatform->contextBuilder()->setCustomInstructions(instructions);

    // Wire workspace root to chat panel for prompt files
    m_chatPanel->setWorkspaceRoot(root);

    // Scan for .prompt.md files
    if (m_aiServices && m_aiServices->promptFileManager())
        m_aiServices->promptFileManager()->scanWorkspace(root);

    // Set workspace root for commit message generator
    if (m_aiServices && m_aiServices->commitMsgGen())
        m_aiServices->commitMsgGen()->setWorkspaceRoot(root);

    // Start workspace indexer — delegated to search plugin (plugins/search/)
    // SearchPlugin owns WorkspaceIndexer and wires status bar updates internally.
    if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
        srch->indexWorkspace(root);

    // Restore previously open editor tabs for this workspace
    {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        const QString wsKey = QStringLiteral("workspace/")
                              + QString::fromUtf8(QCryptographicHash::hash(
                                    root.toUtf8(), QCryptographicHash::Md5).toHex());
        s.beginGroup(wsKey);

        const QString tabsJson = s.value(QStringLiteral("openTabs")).toString();
        const int savedActive  = s.value(QStringLiteral("activeTab"), -1).toInt();
        const QString bpsJson  = s.value(QStringLiteral("breakpoints")).toString();
        s.endGroup();

        if (!tabsJson.isEmpty()) {
            const QJsonArray tabs = QJsonDocument::fromJson(tabsJson.toUtf8()).array();
            if (!tabs.isEmpty()) {
                // Close the blank tab that was created above
                if (m_editorMgr->tabs()->count() == 1) {
                    QWidget *first = m_editorMgr->tabs()->widget(0);
                    if (first && first->property("filePath").toString().isEmpty())
                        m_editorMgr->closeTab(0);
                }

                for (const QJsonValue &v : tabs) {
                    const QJsonObject entry = v.toObject();
                    const QString fp = entry[QLatin1String("path")].toString();
                    if (fp.isEmpty() || !QFileInfo::exists(fp))
                        continue;

                    m_editorMgr->openFile(fp);

                    // Restore cursor and scroll position
                    const int line   = entry[QLatin1String("line")].toInt(-1);
                    const int col    = entry[QLatin1String("column")].toInt(0);
                    const int scroll = entry[QLatin1String("scroll")].toInt(-1);
                    if (line >= 0) {
                        if (auto *ev = m_editorMgr->currentEditor()) {
                            QTextCursor tc = ev->textCursor();
                            tc.movePosition(QTextCursor::Start);
                            tc.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line);
                            tc.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, col);
                            ev->setTextCursor(tc);
                            if (scroll >= 0)
                                ev->verticalScrollBar()->setValue(scroll);
                        }
                    }
                }

                // Restore the active tab
                if (savedActive >= 0 && savedActive < m_editorMgr->tabs()->count())
                    m_editorMgr->tabs()->setCurrentIndex(savedActive);
            }
        }

        // Restore breakpoints: set gutter markers and queue for next debug session
        if (!bpsJson.isEmpty()) {
            const QJsonArray bps = QJsonDocument::fromJson(bpsJson.toUtf8()).array();
            auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
            for (const QJsonValue &v : bps) {
                const QJsonObject bp = v.toObject();
                const QString fp   = bp[QLatin1String("file")].toString();
                const int line     = bp[QLatin1String("line")].toInt(0);
                if (fp.isEmpty() || line <= 0) continue;

                // Restore gutter marker on open editor (if the file is open)
                for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
                    auto *ev = qobject_cast<EditorView *>(m_editorMgr->tabs()->widget(i));
                    if (ev && ev->property("filePath").toString() == fp) {
                        QSet<int> lines = ev->breakpointLines();
                        lines.insert(line);
                        ev->setBreakpointLines(lines);
                        break;
                    }
                }

                // Queue breakpoint for the next debug session
                if (adapter) {
                    DebugBreakpoint dbp;
                    dbp.filePath = fp;
                    dbp.line     = line;
                    adapter->addBreakpoint(dbp);
                }
            }
        }
    }
}

// Write CMakePresets.json for the selected kit so CMake uses the right
// compiler and generator without manual reconfiguration.  When no kit is
// selected the preset still defaults to Ninja so CMake never falls back
// to the Visual Studio / MSVC generator on Windows.
static void writeCMakePresetsJson(const QString &projectDir, const Kit &kit)
{
    const QString generator = kit.generator.isEmpty()
                                ? QStringLiteral("Ninja") : kit.generator;

    // Base cache variables: compiler paths if known
    QJsonObject baseCacheVars;
    if (!kit.cCompilerPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_C_COMPILER")]   = kit.cCompilerPath;
    if (!kit.cxxCompilerPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_CXX_COMPILER")] = kit.cxxCompilerPath;
    if (!kit.debuggerPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_DEBUGGER")]     = kit.debuggerPath;
    if (!kit.generatorPath.isEmpty())
        baseCacheVars[QStringLiteral("CMAKE_MAKE_PROGRAM")] = kit.generatorPath;

    // Three presets: debug, release, relwithdebinfo
    struct PresetInfo { QString suffix; QString display; QString buildType; };
    const PresetInfo infos[] = {
        { QStringLiteral("debug"),          QStringLiteral("Debug"),                QStringLiteral("Debug") },
        { QStringLiteral("release"),        QStringLiteral("Release"),              QStringLiteral("Release") },
        { QStringLiteral("relwithdebinfo"), QStringLiteral("Release with Debug Info"), QStringLiteral("RelWithDebInfo") },
    };

    QJsonArray configurePresets;
    QJsonArray buildPresets;

    for (const auto &p : infos) {
        QJsonObject vars = baseCacheVars;
        vars[QStringLiteral("CMAKE_BUILD_TYPE")] = p.buildType;

        QJsonObject cfg;
        cfg[QStringLiteral("name")]           = p.suffix;
        cfg[QStringLiteral("displayName")]    = p.display;
        cfg[QStringLiteral("generator")]      = generator;
        cfg[QStringLiteral("binaryDir")]      = QStringLiteral("${sourceDir}/build-") + p.suffix;
        cfg[QStringLiteral("cacheVariables")] = vars;
        configurePresets.append(cfg);

        QJsonObject bld;
        bld[QStringLiteral("name")]            = p.suffix;
        bld[QStringLiteral("configurePreset")] = p.suffix;
        buildPresets.append(bld);
    }

    QJsonObject root;
    root[QStringLiteral("version")]          = 3;
    root[QStringLiteral("configurePresets")] = configurePresets;
    root[QStringLiteral("buildPresets")]     = buildPresets;

    const QString presetsPath = projectDir + QStringLiteral("/CMakePresets.json");
    QFile f(presetsPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    }
}

void MainWindow::newSolution()
{
    auto *registry = m_services->service<ProjectTemplateRegistry>(
        QStringLiteral("projectTemplateRegistry"));
    if (!registry)
        return;

    auto *kitMgr = m_services->service<IKitManager>(QStringLiteral("kitManager"));
    NewProjectWizard wizard(registry, m_editorMgr->currentFolder(), kitMgr, this);
    if (wizard.exec() != QDialog::Accepted)
        return;

    const QString projectDir = wizard.createdProjectPath();
    if (projectDir.isEmpty())
        return;

    // Generate CMakePresets.json in the project dir so CMake uses the
    // right compiler + generator without manual configuration.  Even when
    // no kit is selected the preset defaults to the Ninja generator so
    // CMake never falls back to the Visual Studio / MSVC generator.
    writeCMakePresetsJson(projectDir, wizard.selectedKit());

    // Single-project creation: the .exsln lives alongside the project files
    // in the same directory.  No VS-style nesting for the common case (one
    // project = one solution).  Multi-project solutions are built later via
    // "Add Project to Solution" which places each project in its own subdir.

    const QDir projDir(projectDir);
    const QString projectName = projDir.dirName();
    const QString solutionRoot = projDir.absolutePath();

    // Create .exsln solution file alongside project files
    const QString slnPath = solutionRoot + QStringLiteral("/") + projectName + QStringLiteral(".exsln");
    auto *pm = m_editorMgr->projectManager();
    pm->createSolution(projectName, slnPath);
    pm->addProject(projectName, solutionRoot,
                   wizard.selectedLanguage(), wizard.selectedTemplateId());
    pm->saveSolution();

    // Open the project root (which is also the solution root)
    openFolder(solutionRoot);
}

void MainWindow::openSolutionFile()
{
    const QString slnPath = QFileDialog::getOpenFileName(this, tr("Open Solution"),
                                                        m_editorMgr->currentFolder(), tr("Exorcist Solution (*.exsln)"));
    if (slnPath.isEmpty()) {
        return;
    }

    if (m_editorMgr->projectManager()->openSolution(slnPath)) {
        const QString root = m_editorMgr->projectManager()->activeSolutionDir();
        m_gitService->setWorkingDirectory(root);
        if (auto *srch = dynamic_cast<ISearchService *>(m_services->service(QStringLiteral("searchService"))))
            srch->setRootPath(root);
        setWindowTitle(tr("Exorcist - %1").arg(QDir(root).dirName()));
        statusBar()->showMessage(tr("Solution: %1").arg(QFileInfo(slnPath).fileName()), 4000);

        m_editorMgr->setCurrentFolder(root);
        if (auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService"))))
            ts->setWorkingDirectory(root);
        if (auto *lspSvc = m_services->service<ILspService>(QStringLiteral("lspService")))
            lspSvc->startServer(root);
    }
}

void MainWindow::addProjectToSolution()
{
    auto *pm = m_editorMgr->projectManager();
    if (pm->solution().name.isEmpty()) {
        QMessageBox::information(this, tr("No Solution"),
                                 tr("Open or create a solution first."));
        return;
    }

    // Offer two choices: create new project from template, or add existing folder
    QStringList options;
    options << tr("New Project (from template)...")
            << tr("Existing Folder...");

    bool ok = false;
    const QString choice = QInputDialog::getItem(
        this, tr("Add Project to Solution"), tr("Choose:"), options, 0, false, &ok);
    if (!ok)
        return;

    if (choice == options.at(0)) {
        // Create new project via wizard, place inside solution root
        auto *registry = m_services->service<ProjectTemplateRegistry>(
            QStringLiteral("projectTemplateRegistry"));
        if (!registry)
            return;

        auto *kitMgr2 = m_services->service<IKitManager>(QStringLiteral("kitManager"));
        NewProjectWizard wizard(registry, pm->activeSolutionDir(), kitMgr2, this);
        if (wizard.exec() != QDialog::Accepted)
            return;

        const QString projectDir = wizard.createdProjectPath();
        if (projectDir.isEmpty())
            return;

        writeCMakePresetsJson(projectDir, wizard.selectedKit());

        const QString projectName = QDir(projectDir).dirName();
        pm->addProject(projectName, projectDir,
                       wizard.selectedLanguage(), wizard.selectedTemplateId());
        pm->saveSolution();
        // Refresh tree
        emit pm->solutionChanged();
    } else {
        // Add existing folder
        const QString folder = QFileDialog::getExistingDirectory(
            this, tr("Add Folder to Solution"), pm->activeSolutionDir());
        if (folder.isEmpty())
            return;

        const QString projectName = QDir(folder).dirName();
        pm->addProject(projectName, folder);
        if (!pm->solution().filePath.isEmpty())
            pm->saveSolution();
    }
}

