// ── Agent callbacks factory (extracted from MainWindow constructor) ─────────
//
// This method populates the AgentPlatformBootstrap::Callbacks struct with
// lambdas that capture MainWindow members. Extracted to reduce constructor
// size from 1592 to ~450 lines.

// Same includes as mainwindow.cpp — the callbacks reference types from many subsystems.
#include "mainwindow.h"

#include "editor/editorview.h"
#include "editor/syntaxhighlighter.h"
#include "agent/agentcontroller.h"
#include "agent/agentorchestrator.h"
#include "agent/securekeystorage.h"
#include "agent/tools/websearchtool.h"
#include "build/runlaunchpanel.h"
#include "build/outputpanel.h"
#include "git/gitservice.h"
#include "lsp/lspclient.h"
#include "lsp/lspeditorbridge.h"
#include "problems/problemspanel.h"
#include "sdk/ibuildsystem.h"
#include "sdk/idebugservice.h"
#include "sdk/idebugadapter.h"
#include "sdk/itestrunner.h"
#include "sdk/ioutputservice.h"
#include "sdk/irunoutputservice.h"

#include "sdk/iterminalservice.h"
#include "ui/dock/ExDockWidget.h"
#include "agent/chat/chatpanelwidget.h"
#include "agent/chat/chatsessionmodel.h"
#include "project/projectmanager.h"
#include "project/solutiontreemodel.h"
#include "search/searchindex.h"
#include "search/symbolindex.h"
#include "bootstrap/aiservicesbootstrap.h"
#include "agent/modelregistry.h"
#include "search/workspacechunkindex.h"
#include "settings/workspacesettings.h"
#include "sdk/hostservices.h"
#include "agent/treesitteragenthelper.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QScreen>
#include <QTabWidget>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QTextCursor>
#include <QDockWidget>
#include <QVBoxLayout>

AgentPlatformBootstrap::Callbacks MainWindow::buildAgentCallbacks()
{
    AgentPlatformBootstrap::Callbacks cb;

    // ── Basic context getters (already working) ───────────────────────────
    cb.openFilesGetter = [this]() {
        QStringList paths;
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *ed = m_editorMgr->editorAt(i);
            const QString fp = ed ? ed->property("filePath").toString() : QString();
            if (!fp.isEmpty())
                paths.append(fp);
        }
        return paths;
    };

    cb.gitStatusGetter = [this]() -> QString {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        QString result = QStringLiteral("Branch: %1\n").arg(m_gitService->currentBranch());
        const auto statuses = m_gitService->fileStatuses();
        for (auto it = statuses.begin(); it != statuses.end(); ++it)
            result += QStringLiteral("  %1 %2\n").arg(it.value(), it.key());
        return result;
    };

    cb.terminalOutputGetter = [this]() -> QString {
        auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService")));
        if (!ts) return {};
        return ts->recentOutput(80);
    };

    cb.diagnosticsGetter = [this]() -> QList<AgentDiagnostic> {
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
    };

    cb.changedFilesGetter = [this]() -> QHash<QString, QString> {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        return m_gitService->fileStatuses();
    };

    cb.gitDiffGetter = [this](const QString &filePath) -> QString {
        if (!m_gitService || !m_gitService->isGitRepo()) return {};
        return m_gitService->diff(filePath);
    };

    // ── Debug adapter callbacks ───────────────────────────────────────────
    cb.debugBreakpointSetter =
        [this](const QString &filePath, int line, const QString &condition) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter) return {};
        DebugBreakpoint bp;
        bp.filePath  = filePath;
        bp.line      = line;
        bp.condition = condition;
        bp.enabled   = true;
        adapter->addBreakpoint(bp);
        return QStringLiteral("Breakpoint set at %1:%2").arg(filePath).arg(line);
    };

    cb.debugBreakpointRemover =
        [this](const QString &filePath, int line) -> bool {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter) return false;
        // Find breakpoint ID by file:line and remove it
        Q_UNUSED(filePath)
        adapter->removeBreakpoint(line);
        return true;
    };

    cb.debugStackGetter = [this](int threadId) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return {};
        QString result;
        QEventLoop loop;
        auto conn = connect(adapter, &IDebugAdapter::stackTraceReceived,
            &loop, [&](int tid, const QList<DebugFrame> &frames) {
                if (tid != threadId) return;
                for (const auto &f : frames)
                    result += QStringLiteral("#%1 %2 at %3:%4\n")
                        .arg(f.id).arg(f.name, f.filePath).arg(f.line);
                loop.quit();
            });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        adapter->requestStackTrace(threadId);
        loop.exec();
        disconnect(conn);
        return result;
    };

    cb.debugVariablesGetter = [this](int variablesRef) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return {};
        QString result;
        QEventLoop loop;
        auto conn = connect(adapter, &IDebugAdapter::variablesReceived,
            &loop, [&](int ref, const QList<DebugVariable> &vars) {
                if (ref != variablesRef) return;
                for (const auto &v : vars)
                    result += QStringLiteral("%1 %2 = %3\n")
                        .arg(v.type, v.name, v.value);
                loop.quit();
            });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        adapter->requestVariables(variablesRef);
        loop.exec();
        disconnect(conn);
        return result;
    };

    cb.debugEvaluator = [this](const QString &expression, int frameId) -> QString {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return {};
        QString result;
        QEventLoop loop;
        auto conn = connect(adapter, &IDebugAdapter::evaluateResult,
            &loop, [&](const QString &expr, const QString &val) {
                if (expr == expression) {
                    result = val;
                    loop.quit();
                }
            });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        adapter->evaluate(expression, frameId);
        loop.exec();
        disconnect(conn);
        return result;
    };

    cb.debugStepper = [this](const QString &action) -> bool {
        auto *adapter = m_services->service<IDebugAdapter>(QStringLiteral("debugAdapter"));
        if (!adapter || !adapter->isRunning()) return false;
        if (action == QLatin1String("over"))     adapter->stepOver();
        else if (action == QLatin1String("into")) adapter->stepInto();
        else if (action == QLatin1String("out"))  adapter->stepOut();
        else if (action == QLatin1String("continue")) adapter->continueExecution();
        else return false;
        return true;
    };

    // ── Screenshot ────────────────────────────────────────────────────────
    cb.widgetGrabber = [this](const QString &target) -> QPixmap {
        if (target == QLatin1String("window"))
            return grab();
        if (target == QLatin1String("editor")) {
            auto *ed = currentEditor();
            return ed ? ed->grab() : QPixmap();
        }
        if (target == QLatin1String("terminal")) {
            auto *d = dock(QStringLiteral("TerminalDock"));
            return d ? d->grab() : QPixmap();
        }
        if (target == QLatin1String("debug")) {
            auto *d = dock(QStringLiteral("DebugDock"));
            return d ? d->grab() : QPixmap();
        }
        if (target == QLatin1String("agent") || target == QLatin1String("chat"))
            return m_chatPanel ? m_chatPanel->grab() : QPixmap();
        if (target == QLatin1String("search")) {
            auto *d = dock(QStringLiteral("SearchDock"));
            return d ? d->grab() : QPixmap();
        }
        if (target == QLatin1String("git")) {
            auto *d = dock(QStringLiteral("GitDock"));
            return d ? d->grab() : QPixmap();
        }
        return grab();
    };

    // ── Introspection ─────────────────────────────────────────────────────
    cb.introspectionHandler = [this](const QString &query) -> QString {
        if (query == QLatin1String("services") && m_services)
            return m_services->keys().join(QLatin1Char('\n'));
        if (query == QLatin1String("plugins") && m_pluginManager) {
            QStringList names;
            for (const auto &lp : m_pluginManager->loadedPlugins())
                names << lp.manifest.name;
            return names.join(QLatin1Char('\n'));
        }
        if (query == QLatin1String("tools") && m_agentPlatform && m_agentPlatform->toolRegistry())
            return m_agentPlatform->toolRegistry()->availableToolNames().join(QLatin1Char('\n'));
        if (query == QLatin1String("config"))
            return QStringLiteral("Workspace: %1\nFolder: %2")
                .arg(QCoreApplication::applicationDirPath(), m_editorMgr->currentFolder());
        return QStringLiteral("Unknown query: %1. Try: services, plugins, tools, config")
            .arg(query);
    };

    // (LuaJIT executor is wired below, before initialize() call)

    // ── Code graph / intelligence ─────────────────────────────────────────
    cb.symbolSearchFn = [this](const QString &query, int maxResults) -> QString {
        auto *sidx = dynamic_cast<SymbolIndex *>(m_services->service(QStringLiteral("symbolIndex")));
        if (!sidx) return {};
        const auto syms = sidx->search(query, maxResults);
        QString result;
        for (const auto &s : syms) {
            static const char *kindNames[] =
                {"class", "function", "struct", "enum", "namespace", "variable", "method"};
            result += QStringLiteral("%1 [%2] %3:%4\n")
                .arg(s.name,
                     QLatin1String(kindNames[static_cast<int>(s.kind)]),
                     s.filePath)
                .arg(s.line);
        }
        return result;
    };

    cb.symbolsInFileFn = [this](const QString &filePath) -> QString {
        auto *sidx = dynamic_cast<SymbolIndex *>(m_services->service(QStringLiteral("symbolIndex")));
        if (!sidx) return {};
        const auto syms = sidx->symbolsInFile(filePath);
        QString result;
        for (const auto &s : syms) {
            static const char *kindNames[] =
                {"class", "function", "struct", "enum", "namespace", "variable", "method"};
            result += QStringLiteral("L%1 %2 [%3]\n")
                .arg(s.line)
                .arg(s.name,
                     QLatin1String(kindNames[static_cast<int>(s.kind)]));
        }
        return result;
    };

    cb.findReferencesFn =
        [this](const QString &filePath, int line, int column) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) return {};
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::referencesResult,
            &loop, [&](const QString &resUri, const QJsonArray &locations) {
                Q_UNUSED(resUri)
                for (const auto &loc : locations) {
                    const QJsonObject obj = loc.toObject();
                    const QJsonObject range = obj[QLatin1String("range")].toObject();
                    const QJsonObject start = range[QLatin1String("start")].toObject();
                    const QString locUri = obj[QLatin1String("uri")].toString();
                    // Convert file URI back to path
                    QString path = QUrl(locUri).toLocalFile();
                    result += QStringLiteral("%1:%2:%3\n")
                        .arg(path)
                        .arg(start[QLatin1String("line")].toInt() + 1)
                        .arg(start[QLatin1String("character")].toInt() + 1);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestReferences(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        return result;
    };

    cb.findDefinitionFn =
        [this](const QString &filePath, int line, int column) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) return {};
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::definitionResult,
            &loop, [&](const QString &resUri, const QJsonArray &locations) {
                Q_UNUSED(resUri)
                for (const auto &loc : locations) {
                    const QJsonObject obj = loc.toObject();
                    const QJsonObject range = obj[QLatin1String("range")].toObject();
                    const QJsonObject start = range[QLatin1String("start")].toObject();
                    const QString locUri = obj[QLatin1String("uri")].toString();
                    QString path = QUrl(locUri).toLocalFile();
                    result += QStringLiteral("%1:%2:%3\n")
                        .arg(path)
                        .arg(start[QLatin1String("line")].toInt() + 1)
                        .arg(start[QLatin1String("character")].toInt() + 1);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestDefinition(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        return result;
    };

    cb.chunkSearchFn = [this](const QString &query, int maxResults) -> QString {
        if (!m_aiServices || !m_aiServices->chunkIndex()) return {};
        const auto chunks = m_aiServices->chunkIndex()->search(query, maxResults);
        QString result;
        for (const auto &c : chunks) {
            result += QStringLiteral("── %1:%2-%3")
                .arg(c.filePath).arg(c.startLine + 1).arg(c.endLine + 1);
            if (!c.symbol.isEmpty())
                result += QStringLiteral(" (%1)").arg(c.symbol);
            result += QLatin1Char('\n');
            result += c.content;
            result += QLatin1Char('\n');
        }
        return result;
    };

    // ── Build & test (via ServiceRegistry) ──────────────────────────────
    cb.buildProjectFn =
        [this](const QString &target, std::atomic<bool> &cancelled) -> BuildProjectTool::BuildResult {
        auto *buildSvc = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (!buildSvc)
            return {false, QStringLiteral("No build system configured."), -1};
        QString output;
        bool success = false;
        int exitCode = -1;
        QEventLoop loop;
        auto connOut = connect(buildSvc, &IBuildSystem::buildOutput,
            &loop, [&](const QString &line, bool isError) {
                Q_UNUSED(isError)
                output += line + QLatin1Char('\n');
            });
        auto connDone = connect(buildSvc, &IBuildSystem::buildFinished,
            &loop, [&](bool ok, int code) {
                success  = ok;
                exitCode = code;
                loop.quit();
            });
        // Safety timeout
        QTimer::singleShot(120000, &loop, &QEventLoop::quit);
        // Cancel check — poll every 200ms so tool responds promptly
        QTimer cancelCheck;
        QObject::connect(&cancelCheck, &QTimer::timeout, &loop, [&]() {
            if (cancelled.load()) loop.quit();
        });
        cancelCheck.start(200);
        buildSvc->build(target);
        loop.exec();
        disconnect(connOut);
        disconnect(connDone);
        if (cancelled.load())
            return {false, QStringLiteral("Build cancelled."), -1};
        return {success, output, exitCode};
    };

    cb.runTestsFn =
        [this](const QString &scope, const QString &filter,
               std::atomic<bool> &cancelled) -> RunTestsTool::TestResult {
        Q_UNUSED(scope)
        auto *buildSvc = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (!buildSvc)
            return {false, 0, 0, 0, QStringLiteral("No build system.")};
        const QString buildDir = buildSvc->buildDirectory();
        if (buildDir.isEmpty())
            return {false, 0, 0, 0, QStringLiteral("No build directory.")};
        QStringList args = {QStringLiteral("--test-dir"), buildDir,
                            QStringLiteral("--output-on-failure")};
        if (!filter.isEmpty())
            args << QStringLiteral("-R") << filter;
        QProcess proc;
        proc.setWorkingDirectory(buildDir);
        proc.start(QStringLiteral("ctest"), args);
        // Poll with cancel check instead of blocking waitForFinished
        constexpr int kTestTimeout = 120000;
        QElapsedTimer elapsed;
        elapsed.start();
        while (!proc.waitForFinished(200)) {
            if (elapsed.elapsed() > kTestTimeout || cancelled.load()) {
                proc.kill();
                proc.waitForFinished(2000);
                break;
            }
        }
        const QString output = QString::fromUtf8(proc.readAllStandardOutput())
                             + QString::fromUtf8(proc.readAllStandardError());
        const int exitCode = proc.exitCode();
        if (cancelled.load()) {
            m_lastTestResult = {false, 0, 0, 0, QStringLiteral("Tests cancelled.")};
            return m_lastTestResult;
        }
        int passed = 0, failed = 0, total = 0;
        QRegularExpression re(QStringLiteral("(\\d+)% tests passed, (\\d+) tests failed out of (\\d+)"));
        auto match = re.match(output);
        if (match.hasMatch()) {
            failed = match.captured(2).toInt();
            total  = match.captured(3).toInt();
            passed = total - failed;
        }
        m_lastTestResult = {exitCode == 0, passed, failed, total, output};
        return m_lastTestResult;
    };

    cb.buildTargetsGetter = [this]() -> QStringList {
        auto *buildSvc = m_services->service<IBuildSystem>(QStringLiteral("buildSystem"));
        if (!buildSvc) return {};
        return buildSvc->targets();
    };

    // ── Code formatting ───────────────────────────────────────────────────
    cb.codeFormatter =
        [this](const QString &filePath, const QString &content,
               const QString &language, int rangeStart, int rangeEnd)
            -> FormatCodeTool::FormatResult {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, {}, {}, QStringLiteral("LSP not available."), {}};
        const QString uri = LspClient::pathToUri(filePath);
        FormatCodeTool::FormatResult result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::formattingResult,
            &loop, [&](const QString &resUri, const QJsonArray &textEdits) {
                Q_UNUSED(resUri)
                if (textEdits.isEmpty()) {
                    result = {true, content, {}, {}, QStringLiteral("LSP")};
                } else {
                    result.ok = true;
                    result.formatterUsed = QStringLiteral("LSP (clangd)");
                    result.formatted = content; // simplified — edits applied by caller
                    QStringList changes;
                    for (const auto &e : textEdits)
                        changes << e.toObject()[QLatin1String("newText")].toString();
                    result.diff = changes.join(QLatin1Char('\n'));
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        Q_UNUSED(language)
        if (rangeStart > 0 && rangeEnd > 0)
            lspClient->requestRangeFormatting(uri, rangeStart - 1, 0, rangeEnd - 1, 0);
        else
            lspClient->requestFormatting(uri);
        loop.exec();
        disconnect(conn);
        return result;
    };

    // ── Refactoring (LSP) ─────────────────────────────────────────────────
    cb.refactorer =
        [this](const QString &operation, const QString &filePath,
               int line, int column, const QString &newName,
               int rangeStartLine, int rangeEndLine) -> RefactorTool::RefactorResult {
        Q_UNUSED(rangeStartLine) Q_UNUSED(rangeEndLine)
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, 0, 0, {}, {}, QStringLiteral("LSP not available.")};
        if (operation != QLatin1String("rename"))
            return {false, 0, 0, {}, {},
                    QStringLiteral("Only 'rename' is currently supported via LSP.")};
        const QString uri = LspClient::pathToUri(filePath);
        RefactorTool::RefactorResult result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::renameResult,
            &loop, [&](const QString &, const QJsonObject &workspaceEdit) {
                applyWorkspaceEdit(workspaceEdit);
                const auto changes = workspaceEdit[QLatin1String("changes")].toObject();
                result.ok = true;
                result.filesChanged = changes.keys().size();
                int edits = 0;
                for (const auto &key : changes.keys())
                    edits += changes[key].toArray().size();
                result.editsApplied = edits;
                result.summary = QStringLiteral("Renamed to '%1': %2 edits across %3 files.")
                    .arg(newName).arg(edits).arg(result.filesChanged);
                loop.quit();
            });
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        lspClient->requestRename(uri, line - 1, column - 1, newName);
        loop.exec();
        disconnect(conn);
        return result;
    };

    // ── Ask user ──────────────────────────────────────────────────────────
    cb.userPrompter =
        [this](const QString &question, const QStringList &options,
               bool allowFreeText) -> AskUserTool::UserResponse {
        if (options.isEmpty() || allowFreeText) {
            bool ok = false;
            const QString answer = QInputDialog::getText(
                this, tr("Agent Question"), question,
                QLineEdit::Normal, QString(), &ok);
            return {ok, answer, -1};
        }
        // Multiple choice via QMessageBox
        QStringList labels = options;
        if (labels.size() <= 3) {
            auto box = std::make_unique<QMessageBox>(this);
            box->setWindowTitle(tr("Agent Question"));
            box->setText(question);
            QList<QPushButton *> buttons;
            for (const QString &opt : labels)
                buttons << box->addButton(opt, QMessageBox::ActionRole);
            box->addButton(QMessageBox::Cancel);
            box->exec();
            int idx = -1;
            for (int i = 0; i < buttons.size(); ++i) {
                if (box->clickedButton() == buttons[i]) { idx = i; break; }
            }
            bool answered = idx >= 0;
            return {answered, answered ? labels[idx] : QString(), idx};
        }
        // Many options → use QInputDialog combo
        bool ok = false;
        const QString chosen = QInputDialog::getItem(
            this, tr("Agent Question"), question, labels, 0, false, &ok);
        int idx = ok ? labels.indexOf(chosen) : -1;
        return {ok, chosen, idx};
    };

    // ── Editor context ────────────────────────────────────────────────────
    cb.editorStateGetter = [this]() -> EditorContextTool::EditorState {
        EditorContextTool::EditorState state;
        auto *ed = currentEditor();
        if (!ed) return state;
        state.activeFilePath = ed->property("filePath").toString();
        const QTextCursor cur = ed->textCursor();
        state.cursorLine   = cur.blockNumber() + 1;
        state.cursorColumn = cur.positionInBlock() + 1;
        state.selectedText = cur.selectedText();
        if (cur.hasSelection()) {
            state.selectionStartLine = cur.document()->findBlock(cur.selectionStart()).blockNumber() + 1;
            state.selectionEndLine   = cur.document()->findBlock(cur.selectionEnd()).blockNumber() + 1;
        }
        state.totalLines = ed->document()->blockCount();
        state.language   = ed->languageId();
        state.isModified = ed->document()->isModified();
        // Viewport
        const QTextCursor topCur = ed->cursorForPosition(QPoint(0, 0));
        state.visibleStartLine = topCur.blockNumber() + 1;
        const QTextCursor botCur = ed->cursorForPosition(
            QPoint(0, ed->viewport()->height() - 1));
        state.visibleEndLine = botCur.blockNumber() + 1;
        // Open files
        for (int i = 0; i < m_editorMgr->tabs()->count(); ++i) {
            auto *tab = m_editorMgr->editorAt(i);
            const QString fp = tab ? tab->property("filePath").toString() : QString();
            if (!fp.isEmpty())
                state.openFiles.append(fp);
        }
        return state;
    };

    // ── Change impact analysis ────────────────────────────────────────────
    cb.changeImpactAnalyzer =
        [this](const QString &filePath, int line, int column,
               const QString &changeType) -> ChangeImpactTool::ImpactResult {
        ChangeImpactTool::ImpactResult result;
        result.ok = false;
        // Use LSP references to estimate impact
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) {
            result.error = QStringLiteral("LSP not available.");
            return result;
        }
        // Find references synchronously
        QStringList refFiles;
        QEventLoop loop;
        const QString uri = LspClient::pathToUri(filePath);
        auto conn = connect(lspClient, &LspClient::referencesResult,
            &loop, [&](const QString &, const QJsonArray &locations) {
                for (const auto &loc : locations) {
                    QString path = QUrl(loc.toObject()[QLatin1String("uri")].toString()).toLocalFile();
                    if (!refFiles.contains(path))
                        refFiles.append(path);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestReferences(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        result.ok = true;
        result.directReferences = refFiles.size();
        result.affectedFiles    = refFiles;
        // Risk scoring
        Q_UNUSED(changeType)
        result.riskScore = qBound(1, refFiles.size(), 10);
        result.summary = QStringLiteral("%1 files reference this symbol.")
            .arg(refFiles.size());
        return result;
    };

    // ── Navigation ────────────────────────────────────────────────────────
    cb.fileOpener =
        [this](const QString &filePath, int line, int column) -> bool {
        navigateToLocation(filePath, line - 1, column > 0 ? column - 1 : 0);
        return true;
    };

    cb.headerSourceSwitcher = [this](const QString &filePath) -> QString {
        const QFileInfo fi(filePath);
        const QString base = fi.completeBaseName();
        const QString dir  = fi.absolutePath();
        static const QStringList headerExts = {
            QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hxx"), QStringLiteral("hh")};
        static const QStringList sourceExts = {
            QStringLiteral("cpp"), QStringLiteral("cc"), QStringLiteral("cxx"), QStringLiteral("c")};
        const QString ext = fi.suffix().toLower();
        const QStringList &searchExts = headerExts.contains(ext) ? sourceExts : headerExts;
        for (const QString &e : searchExts) {
            const QString candidate = dir + QLatin1Char('/') + base + QLatin1Char('.') + e;
            if (QFile::exists(candidate)) return candidate;
        }
        return {};
    };

    // ── LSP rename & usages ───────────────────────────────────────────────
    cb.symbolRenamer =
        [this](const QString &filePath, int line, int column,
               const QString &newName) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return QStringLiteral("Error: LSP not available.");
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::renameResult,
            &loop, [&](const QString &, const QJsonObject &workspaceEdit) {
                applyWorkspaceEdit(workspaceEdit);
                const auto changes = workspaceEdit[QLatin1String("changes")].toObject();
                int edits = 0;
                for (const auto &key : changes.keys())
                    edits += changes[key].toArray().size();
                result = QStringLiteral("%1 edits across %2 files.")
                    .arg(edits).arg(changes.keys().size());
                loop.quit();
            });
        auto errConn = connect(lspClient, &LspClient::serverError,
            &loop, [&](const QString &msg) {
                result = QStringLiteral("Error: %1").arg(msg);
                loop.quit();
            });
        QTimer::singleShot(15000, &loop, &QEventLoop::quit);
        lspClient->requestRename(uri, line - 1, column - 1, newName);
        loop.exec();
        disconnect(conn);
        disconnect(errConn);
        return result;
    };

    cb.usageFinder =
        [this](const QString &filePath, int line, int column) -> QString {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized()) return {};
        const QString uri = LspClient::pathToUri(filePath);
        QString result;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::referencesResult,
            &loop, [&](const QString &, const QJsonArray &locations) {
                for (const auto &loc : locations) {
                    const QJsonObject obj = loc.toObject();
                    const QJsonObject range = obj[QLatin1String("range")].toObject();
                    const QJsonObject start = range[QLatin1String("start")].toObject();
                    QString path = QUrl(obj[QLatin1String("uri")].toString()).toLocalFile();
                    result += QStringLiteral("%1:%2:%3\n")
                        .arg(path)
                        .arg(start[QLatin1String("line")].toInt() + 1)
                        .arg(start[QLatin1String("character")].toInt() + 1);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestReferences(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        return result;
    };

    // ── LuaJIT executor ──────────────────────────────────────────────────
    cb.luaExecutor = [this](const QString &script) -> LuaExecuteTool::LuaResult {
        auto *engine = m_pluginManager ? m_pluginManager->luaEngine() : nullptr;
        if (!engine)
            return {false, {}, {}, QStringLiteral("LuaJIT engine not available."), 0};

        auto adhoc = engine->executeAdhoc(script);
        return {adhoc.ok, adhoc.output, adhoc.returnValue, adhoc.error,
                adhoc.memoryUsedBytes};
    };

    // ── Diff viewer ──────────────────────────────────────────────────────
    cb.diffViewer =
        [this](const QString &left, const QString &right,
               const QString &leftTitle, const QString &rightTitle) -> bool {
        // Display diff in a new dock widget with side-by-side view
        auto diffWidget = std::make_unique<QWidget>();
        auto *layout = new QHBoxLayout(diffWidget.get());

        auto leftEdit = std::make_unique<QPlainTextEdit>();
        leftEdit->setReadOnly(true);
        leftEdit->setPlainText(left);
        leftEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto rightEdit = std::make_unique<QPlainTextEdit>();
        rightEdit->setReadOnly(true);
        rightEdit->setPlainText(right);
        rightEdit->setLineWrapMode(QPlainTextEdit::NoWrap);

        auto *leftGroup = new QGroupBox(leftTitle);
        auto *leftLayout = new QVBoxLayout(leftGroup);
        leftLayout->addWidget(leftEdit.release());

        auto *rightGroup = new QGroupBox(rightTitle);
        auto *rightLayout = new QVBoxLayout(rightGroup);
        rightLayout->addWidget(rightEdit.release());

        layout->addWidget(leftGroup);
        layout->addWidget(rightGroup);

        auto *dock = new QDockWidget(
            tr("Diff: %1 vs %2").arg(leftTitle, rightTitle), this);
        dock->setWidget(diffWidget.release());
        dock->setAttribute(Qt::WA_DeleteOnClose);
        addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->show();
        return true;
    };

    // ── Static analysis callback ─────────────────────────────────────────
    cb.staticAnalyzer =
        [this](const QString &filePath, const QStringList &checkers,
               bool fixMode) -> StaticAnalysisTool::AnalysisResult {
        // Dispatch to the appropriate analyzer based on file extension
        StaticAnalysisTool::AnalysisResult result;
        result.ok = false;

        if (filePath.isEmpty()) {
            result.error = QStringLiteral("No file path provided.");
            return result;
        }

        const QFileInfo fi(filePath);
        const QString suffix = fi.suffix().toLower();

        // Build the command based on language
        QString program;
        QStringList args;

        if (suffix == QLatin1String("cpp") || suffix == QLatin1String("cxx") ||
            suffix == QLatin1String("cc")  || suffix == QLatin1String("c")   ||
            suffix == QLatin1String("h")   || suffix == QLatin1String("hpp")) {
            // clang-tidy
            program = QStringLiteral("clang-tidy");
            args << filePath;
            if (!checkers.isEmpty()) {
                args << QStringLiteral("-checks=%1").arg(checkers.join(QLatin1Char(',')));
            }
            if (fixMode)
                args << QStringLiteral("--fix");
            // Add compile_commands.json path if available
            const QString compileDb = m_editorMgr->currentFolder() + QStringLiteral("/build-llvm/compile_commands.json");
            if (QFileInfo::exists(compileDb))
                args << QStringLiteral("-p") << m_editorMgr->currentFolder() + QStringLiteral("/build-llvm");
            result.analyzerUsed = QStringLiteral("clang-tidy");

        } else if (suffix == QLatin1String("py")) {
            program = QStringLiteral("pylint");
            args << QStringLiteral("--output-format=text") << filePath;
            result.analyzerUsed = QStringLiteral("pylint");

        } else if (suffix == QLatin1String("js") || suffix == QLatin1String("ts") ||
                   suffix == QLatin1String("jsx") || suffix == QLatin1String("tsx")) {
            program = QStringLiteral("eslint");
            args << QStringLiteral("--format=compact") << filePath;
            if (fixMode)
                args << QStringLiteral("--fix");
            result.analyzerUsed = QStringLiteral("eslint");

        } else if (suffix == QLatin1String("rs")) {
            program = QStringLiteral("cargo");
            args << QStringLiteral("clippy") << QStringLiteral("--message-format=short");
            result.analyzerUsed = QStringLiteral("clippy");

        } else if (suffix == QLatin1String("go")) {
            program = QStringLiteral("staticcheck");
            args << filePath;
            result.analyzerUsed = QStringLiteral("staticcheck");

        } else {
            result.error = QStringLiteral("No analyzer available for .%1 files.").arg(suffix);
            return result;
        }

        // Run the analyzer process
        QProcess proc;
        proc.setWorkingDirectory(m_editorMgr->currentFolder());
        proc.start(program, args);
        if (!proc.waitForFinished(55000)) {
            result.error = QStringLiteral("Analyzer timed out or failed to start: %1").arg(program);
            return result;
        }

        result.ok = true;
        const QString output = QString::fromUtf8(proc.readAllStandardOutput())
                             + QString::fromUtf8(proc.readAllStandardError());

        // Parse output into findings (simple line-based parsing)
        const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        static const QRegularExpression findingRe(
            QStringLiteral("^(.+?):(\\d+):(\\d+):\\s*(warning|error|note|info):\\s*(.+)$"));

        for (const QString &line : lines) {
            const QRegularExpressionMatch m = findingRe.match(line);
            if (m.hasMatch()) {
                StaticAnalysisTool::Finding f;
                f.filePath = m.captured(1);
                f.line     = m.captured(2).toInt();
                f.column   = m.captured(3).toInt();
                f.severity = m.captured(4);
                f.message  = m.captured(5);

                // Extract rule ID if present (e.g. [bugprone-...] or (C1234))
                static const QRegularExpression ruleRe(
                    QStringLiteral("\\[([a-zA-Z0-9._-]+)\\]$"));
                const QRegularExpressionMatch ruleMatch = ruleRe.match(f.message);
                if (ruleMatch.hasMatch()) {
                    f.ruleId = ruleMatch.captured(1);
                    f.message = f.message.left(ruleMatch.capturedStart()).trimmed();
                }

                if (f.severity == QLatin1String("error"))
                    result.errorCount++;
                else if (f.severity == QLatin1String("warning"))
                    result.warningCount++;

                result.findings.append(f);
            }
        }

        return result;
    };

    // ── Terminal selection callback ──────────────────────────────────────
    cb.terminalSelectionGetter = [this]() -> QString {
        auto *ts = dynamic_cast<ITerminalService *>(m_services->service(QStringLiteral("terminalService")));
        if (!ts) return {};
        return ts->selectedText();
    };

    // ── Test failure cache callback ─────────────────────────────────────
    cb.testFailureGetter = [this]() -> RunTestsTool::TestResult {
        return m_lastTestResult;
    };

    // ── IDE command execution callbacks ─────────────────────────────────
    cb.commandExecutor = [this](const QString &id) -> bool {
        auto *cmdSvc = m_hostServices->commandService();
        return cmdSvc ? cmdSvc->executeCommand(id) : false;
    };
    cb.commandListGetter = [this]() -> QStringList {
        auto *cmdSvc = m_hostServices->commandService();
        return cmdSvc ? cmdSvc->commandIds() : QStringList{};
    };

    // ── Tree-sitter AST access callbacks ────────────────────────────────
    auto tsHelper = std::make_shared<TreeSitterAgentHelper>();
    cb.tsFileParser = [tsHelper](const QString &fp, int md) {
        return tsHelper->parseFile(fp, md);
    };
    cb.tsQueryRunner = [tsHelper](const QString &fp, const QString &qp) {
        return tsHelper->runQuery(fp, qp);
    };
    cb.tsSymbolExtractor = [tsHelper](const QString &fp) {
        return tsHelper->extractSymbols(fp);
    };
    cb.tsNodeAtPosition = [tsHelper](const QString &fp, int l, int c) {
        return tsHelper->nodeAtPosition(fp, l, c);
    };

    // ── Symbol documentation (LSP hover) ────────────────────────────────
    cb.symbolDocGetter =
        [this](const QString &filePath, int line, int column) -> SymbolDocTool::DocResult {
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, {}, {}, {}, QStringLiteral("LSP not available.")};
        const QString uri = LspClient::pathToUri(filePath);
        SymbolDocTool::DocResult result;
        result.ok = false;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::hoverResult,
            &loop, [&](const QString &, int, int, const QString &markdown) {
                if (markdown.isEmpty()) {
                    result.error = QStringLiteral("No documentation found at this location.");
                } else {
                    result.ok = true;
                    result.documentation = markdown;
                    // Try to extract signature (first code block or first line)
                    const int codeStart = markdown.indexOf(QLatin1String("```"));
                    if (codeStart >= 0) {
                        const int lineEnd = markdown.indexOf(QLatin1Char('\n'), codeStart + 3);
                        const int codeEnd = markdown.indexOf(QLatin1String("```"), lineEnd);
                        if (lineEnd >= 0 && codeEnd > lineEnd)
                            result.signature = markdown.mid(lineEnd + 1, codeEnd - lineEnd - 1).trimmed();
                    }
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestHover(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        if (!result.ok && result.error.isEmpty())
            result.error = QStringLiteral("Hover request timed out.");
        return result;
    };

    // ── Code completion (LSP) ───────────────────────────────────────────
    cb.completionGetter =
        [this](const QString &filePath, int line, int column,
               const QString &prefix) -> CodeCompletionTool::CompletionResult {
        Q_UNUSED(prefix)
        auto *lspClient = m_services->service<LspClient>(QStringLiteral("lspClient"));
        if (!lspClient || !lspClient->isInitialized())
            return {false, {}, false, QStringLiteral("LSP not available.")};
        const QString uri = LspClient::pathToUri(filePath);
        CodeCompletionTool::CompletionResult result;
        result.ok = false;
        QEventLoop loop;
        auto conn = connect(lspClient, &LspClient::completionResult,
            &loop, [&](const QString &, int, int,
                       const QJsonArray &items, bool isIncomplete) {
                result.ok = true;
                result.isIncomplete = isIncomplete;
                static const char *kindNames[] = {
                    "", "text", "method", "function", "constructor", "field",
                    "variable", "class", "interface", "module", "property",
                    "unit", "value", "enum", "keyword", "snippet", "color",
                    "file", "reference", "folder", "enum_member", "constant",
                    "struct", "event", "operator", "type_parameter"
                };
                for (const auto &item : items) {
                    const QJsonObject obj = item.toObject();
                    CodeCompletionTool::CompletionItem ci;
                    ci.label = obj[QLatin1String("label")].toString();
                    const int kind = obj[QLatin1String("kind")].toInt();
                    ci.kind = (kind >= 0 && kind <= 25)
                        ? QString::fromLatin1(kindNames[kind])
                        : QString::number(kind);
                    ci.detail = obj[QLatin1String("detail")].toString();
                    ci.documentation = obj[QLatin1String("documentation")].toString();
                    if (ci.documentation.isEmpty()) {
                        const QJsonObject docObj = obj[QLatin1String("documentation")].toObject();
                        ci.documentation = docObj[QLatin1String("value")].toString();
                    }
                    ci.insertText = obj[QLatin1String("insertText")].toString();
                    if (ci.insertText.isEmpty())
                        ci.insertText = ci.label;
                    result.items.append(ci);
                }
                loop.quit();
            });
        QTimer::singleShot(10000, &loop, &QEventLoop::quit);
        lspClient->requestCompletion(uri, line - 1, column - 1);
        loop.exec();
        disconnect(conn);
        if (!result.ok && result.error.isEmpty())
            result.error = QStringLiteral("Completion request timed out.");
        return result;
    };

    // ── Diagram generation (Mermaid CLI) ────────────────────────────────
    cb.diagramRenderer =
        [this](const QString &markup, const QString &format,
               const QString &outputPath) -> GenerateDiagramTool::DiagramResult {
        Q_UNUSED(this)
        GenerateDiagramTool::DiagramResult result;
        result.ok = false;

        // Determine output file path
        QString outFile = outputPath;
        const QString diagramCache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        QDir().mkpath(diagramCache);
        if (outFile.isEmpty()) {
            outFile = diagramCache + QStringLiteral("/exorcist_diagram.svg");
        }

        // Write markup to a temp input file
        const QString inputFile = diagramCache + QStringLiteral("/exorcist_diagram_input.mmd");
        {
            QFile f(inputFile);
            if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                result.error = QStringLiteral("Failed to write temp input file.");
                return result;
            }
            f.write(markup.toUtf8());
        }

        if (format == QLatin1String("mermaid") || format.isEmpty()) {
            // Use mmdc (Mermaid CLI)
            QProcess proc;
            QStringList args = {
                QStringLiteral("-i"), inputFile,
                QStringLiteral("-o"), outFile
            };
            // Force SVG output unless outputPath ends with .png
            if (outFile.endsWith(QLatin1String(".png"), Qt::CaseInsensitive))
                args << QStringLiteral("-e") << QStringLiteral("png");

            proc.start(QStringLiteral("mmdc"), args);
            if (!proc.waitForFinished(30000)) {
                result.error = QStringLiteral("mmdc (Mermaid CLI) timed out or not found. "
                    "Install with: npm install -g @mermaid-js/mermaid-cli");
                return result;
            }
            if (proc.exitCode() != 0) {
                result.error = QStringLiteral("mmdc failed: %1")
                    .arg(QString::fromUtf8(proc.readAllStandardError()));
                return result;
            }
        } else if (format == QLatin1String("plantuml")) {
            // Use plantuml.jar or plantuml command
            QProcess proc;
            QStringList args = {
                QStringLiteral("-tsvg"),
                QStringLiteral("-o"), QFileInfo(outFile).absolutePath(),
                inputFile
            };
            proc.start(QStringLiteral("plantuml"), args);
            if (!proc.waitForFinished(30000)) {
                result.error = QStringLiteral("PlantUML timed out or not found.");
                return result;
            }
            if (proc.exitCode() != 0) {
                result.error = QStringLiteral("PlantUML failed: %1")
                    .arg(QString::fromUtf8(proc.readAllStandardError()));
                return result;
            }
            // PlantUML outputs next to input file with .svg extension
            const QString plantOut = diagramCache + QStringLiteral("/exorcist_diagram_input.svg");
            if (plantOut != outFile)
                QFile::rename(plantOut, outFile);
        } else {
            result.error = QStringLiteral("Unknown format: %1. Use 'mermaid' or 'plantuml'.").arg(format);
            return result;
        }

        // Read SVG content if output is SVG
        if (outFile.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
            QFile f(outFile);
            if (f.open(QIODevice::ReadOnly))
                result.svgContent = QString::fromUtf8(f.readAll());
        }
        result.pngPath = outFile;
        result.ok = true;
        return result;
    };

    // ── Performance profiling ───────────────────────────────────────────
    cb.profiler =
        [this](const QString &target, int duration,
               const QString &type) -> PerformanceProfileTool::ProfileResult {
        Q_UNUSED(this)
        PerformanceProfileTool::ProfileResult result;
        result.ok = false;

        if (target.isEmpty()) {
            result.error = QStringLiteral("No profiling target specified.");
            return result;
        }

#ifdef Q_OS_WIN
        // Windows: use xperf / WPR or fallback to sampling via tasklist
        Q_UNUSED(duration) Q_UNUSED(type)
        QProcess proc;
        // Use "perf" (WSL) or dotnet-trace, or fallback to basic process stats
        proc.start(QStringLiteral("powershell"), {
            QStringLiteral("-Command"),
            QStringLiteral("Get-Process -Name '%1' -ErrorAction SilentlyContinue | "
                "Select-Object Name, Id, CPU, WorkingSet64, "
                "VirtualMemorySize64, HandleCount, Threads | "
                "Format-List | Out-String").arg(target)
        });
        proc.waitForFinished(15000);
        const QString output = QString::fromUtf8(proc.readAllStandardOutput());
        if (output.trimmed().isEmpty()) {
            result.error = QStringLiteral("Process '%1' not found or no data available.").arg(target);
            return result;
        }
        result.ok = true;
        result.report = output;
        result.totalMs = 0;
        result.totalSamples = 0;
#else
        if (type == QLatin1String("cpu") || type.isEmpty()) {
            QProcess proc;
            proc.start(QStringLiteral("perf"), {
                QStringLiteral("record"), QStringLiteral("-g"),
                QStringLiteral("-p"), target,
                QStringLiteral("--"), QStringLiteral("sleep"),
                QString::number(duration)
            });
            if (!proc.waitForFinished((duration + 5) * 1000)) {
                result.error = QStringLiteral("perf timed out.");
                return result;
            }
            // Get report
            QProcess report;
            report.start(QStringLiteral("perf"), {
                QStringLiteral("report"), QStringLiteral("--stdio"),
                QStringLiteral("--no-children")
            });
            report.waitForFinished(15000);
            result.ok = true;
            result.report = QString::fromUtf8(report.readAllStandardOutput());
            result.totalMs = duration * 1000.0;
        } else if (type == QLatin1String("memory")) {
            QProcess proc;
            proc.start(QStringLiteral("valgrind"), {
                QStringLiteral("--tool=massif"),
                QStringLiteral("--pages-as-heap=yes"),
                target
            });
            proc.waitForFinished(qMax(duration, 10) * 1000);
            result.ok = true;
            result.report = QString::fromUtf8(proc.readAllStandardOutput())
                          + QString::fromUtf8(proc.readAllStandardError());
        } else {
            result.error = QStringLiteral("Unsupported profile type: %1").arg(type);
            return result;
        }
#endif
        return result;
    };

    // ── Secure key storage ────────────────────────────────────────────────
    if (auto *ks = m_services->service<SecureKeyStorage>(QStringLiteral("secureKeyStorage"))) {
        cb.secureKeyStorer  = [ks](const QString &svc, const QString &val) {
            return ks->storeKey(svc, val);
        };
        cb.secureKeyGetter  = [ks](const QString &svc) {
            return ks->retrieveKey(svc);
        };
        cb.secureKeyLister  = [ks]() {
            return ks->services();
        };
        cb.secureKeyDeleter = [ks](const QString &svc) {
            return ks->deleteKey(svc);
        };
    }

    return cb;
}
