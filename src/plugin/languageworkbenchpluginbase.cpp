#include "languageworkbenchpluginbase.h"

#include "core/istatusbarmanager.h"
#include "lsp/lspclient.h"
#include "lsp/processlanguageserver.h"
#include "sdk/icommandservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/ilspservice.h"
#include "sdk/inotificationservice.h"

#include <QCoreApplication>
#include <QDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr const char *kProjectDockId = "ProjectDock";
constexpr const char *kProblemsDockId = "ProblemsDock";
constexpr const char *kSearchDockId = "SearchDock";
constexpr const char *kTerminalDockId = "TerminalDock";
constexpr const char *kGitDockId = "GitDock";

}

QString LanguageWorkbenchPluginBase::languageWorkspaceRoot() const
{
    return workspaceRoot();
}

void LanguageWorkbenchPluginBase::showProjectTree() const
{
    showPanel(QString::fromLatin1(kProjectDockId));
}

void LanguageWorkbenchPluginBase::showProblemsPanel() const
{
    showPanel(QString::fromLatin1(kProblemsDockId));
}

void LanguageWorkbenchPluginBase::showSearchPanel() const
{
    showPanel(QString::fromLatin1(kSearchDockId));
}

void LanguageWorkbenchPluginBase::showTerminalPanel() const
{
    showPanel(QString::fromLatin1(kTerminalDockId));
}

void LanguageWorkbenchPluginBase::showGitPanel() const
{
    showPanel(QString::fromLatin1(kGitDockId));
}

void LanguageWorkbenchPluginBase::showStandardLanguagePanels() const
{
    showProjectTree();
    showProblemsPanel();
}

void LanguageWorkbenchPluginBase::registerLanguageServiceObjects(QObject *client,
                                                                 QObject *serviceObject) const
{
    if (client)
        registerService(QStringLiteral("lspClient"), client);
    if (serviceObject)
        registerService(QStringLiteral("lspService"), serviceObject);
}

void LanguageWorkbenchPluginBase::attachLanguageServerToWorkbench(ProcessLanguageServer *server) const
{
    if (!server)
        return;

    registerLanguageServiceObjects(server->client(), server->service());

    QObject::connect(server, &ProcessLanguageServer::statusMessage,
                     [this](const QString &message, int timeoutMs) {
                         showStatusMessage(message, timeoutMs);
                     });
}

std::unique_ptr<ProcessLanguageServer> LanguageWorkbenchPluginBase::createProcessLanguageServer(
    const QString &program,
    const QString &displayName,
    const QStringList &baseArguments) const
{
    return std::make_unique<ProcessLanguageServer>(
        ProcessLanguageServer::Config{ program, displayName, baseArguments });
}

void LanguageWorkbenchPluginBase::startProcessLanguageServer(ProcessLanguageServer *server,
                                                             const QStringList &extraArgs) const
{
    if (!server)
        return;

    const QString root = languageWorkspaceRoot().isEmpty()
        ? server->workspaceRoot()
        : languageWorkspaceRoot();
    server->start(root, extraArgs);
}

void LanguageWorkbenchPluginBase::stopProcessLanguageServer(ProcessLanguageServer *server) const
{
    if (server)
        server->stop();
}

// ── Standard LSP Command Registration ─────────────────────────────────────────

namespace {

const char *kTrContext = "LanguagePlugin";

QString tr(const char *text)
{
    return QCoreApplication::translate(kTrContext, text);
}

void showWorkspaceSymbolDialog(LspClient *client, IEditorService *ed)
{
    if (!client->isInitialized())
        return;

    auto *dlg = new QDialog(nullptr, Qt::Dialog);
    dlg->setWindowTitle(tr("Go to Symbol in Workspace"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(540, 420);

    auto *layout = new QVBoxLayout(dlg);
    auto *queryEdit = new QLineEdit(dlg);
    queryEdit->setPlaceholderText(tr("Type symbol name to search..."));
    auto *resultList = new QListWidget(dlg);
    layout->addWidget(queryEdit);
    layout->addWidget(resultList);

    auto locations = std::make_shared<QVector<QPair<QString, int>>>();

    auto conn = QObject::connect(client, &LspClient::workspaceSymbolsResult, dlg,
        [resultList, locations](const QJsonArray &symbols) {
            resultList->clear();
            locations->clear();
            for (const QJsonValue &v : symbols) {
                const QJsonObject sym = v.toObject();
                const QString name = sym[QLatin1String("name")].toString();
                const QJsonObject loc = sym[QLatin1String("location")].toObject();
                const QString uri = loc[QLatin1String("uri")].toString();
                const QString path = QUrl(uri).toLocalFile();
                const int line = loc[QLatin1String("range")].toObject()
                                   [QLatin1String("start")].toObject()
                                   [QLatin1String("line")].toInt();
                const QString label = name + QLatin1String("  \u2014  ")
                                    + QFileInfo(path).fileName();
                resultList->addItem(label);
                locations->append({path, line});
            }
        });
    QObject::connect(dlg, &QDialog::destroyed, dlg, [conn]() {
        QObject::disconnect(conn);
    });

    auto *debounce = new QTimer(dlg);
    debounce->setSingleShot(true);
    debounce->setInterval(250);
    QObject::connect(queryEdit, &QLineEdit::textChanged, debounce,
                     [debounce]() { debounce->start(); });
    QObject::connect(debounce, &QTimer::timeout, dlg, [client, queryEdit]() {
        client->requestWorkspaceSymbols(queryEdit->text());
    });

    QObject::connect(resultList, &QListWidget::itemDoubleClicked, dlg,
        [dlg, resultList, locations, ed](QListWidgetItem *item) {
            const int idx = resultList->row(item);
            if (idx >= 0 && idx < locations->size() && ed) {
                const auto &[path, line] = (*locations)[idx];
                ed->openFile(path, line + 1, 0);
            }
            dlg->accept();
        });

    client->requestWorkspaceSymbols({});
    queryEdit->setFocus();
    dlg->show();
}

} // anonymous namespace

void LanguageWorkbenchPluginBase::registerStandardLspCommands(
    const QString &prefix,
    LspClient *client,
    ProcessLanguageServer *server,
    QObject *owner)
{
    auto *cmds = commands();
    if (!cmds || !client)
        return;

    auto *ed = editor();

    cmds->registerCommand(
        prefix + QStringLiteral(".goToDefinition"),
        tr("Go to Definition"),
        [client, ed]() {
            if (!ed || !client->isInitialized()) return;
            client->requestDefinition(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn());
        });

    cmds->registerCommand(
        prefix + QStringLiteral(".goToDeclaration"),
        tr("Go to Declaration"),
        [client, ed]() {
            if (!ed || !client->isInitialized()) return;
            client->requestDeclaration(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn());
        });

    cmds->registerCommand(
        prefix + QStringLiteral(".findReferences"),
        tr("Find All References"),
        [client, ed]() {
            if (!ed || !client->isInitialized()) return;
            client->requestReferences(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn());
        });

    cmds->registerCommand(
        prefix + QStringLiteral(".renameSymbol"),
        tr("Rename Symbol"),
        [client, ed]() {
            if (!ed || !client->isInitialized()) return;
            bool ok = false;
            const QString newName = QInputDialog::getText(
                nullptr, tr("Rename Symbol"), tr("New name:"),
                QLineEdit::Normal,
                ed->selectedText().isEmpty() ? QString() : ed->selectedText(),
                &ok);
            if (!ok || newName.trimmed().isEmpty()) return;
            client->requestRename(
                LspClient::pathToUri(ed->activeFilePath()),
                ed->cursorLine() - 1, ed->cursorColumn(),
                newName.trimmed());
        });

    cmds->registerCommand(
        prefix + QStringLiteral(".formatDocument"),
        tr("Format Document"),
        [client, ed]() {
            if (!ed || !client->isInitialized()) return;
            client->requestFormatting(
                LspClient::pathToUri(ed->activeFilePath()));
        });

    cmds->registerCommand(
        prefix + QStringLiteral(".gotoWorkspaceSymbol"),
        tr("Go to Symbol in Workspace"),
        [client, ed]() {
            showWorkspaceSymbolDialog(client, ed);
        });

    cmds->registerCommand(
        prefix + QStringLiteral(".restartLanguageServer"),
        tr("Restart Language Server"),
        [this, server]() {
            if (!server) return;
            stopProcessLanguageServer(server);
            showStatusMessage(tr("Restarting language server..."), 3000);
            const QString root = languageWorkspaceRoot();
            QTimer::singleShot(500, server, [server, root]() {
                server->start(root);
            });
        });

    cmds->registerCommand(
        prefix + QStringLiteral(".openProblems"),
        tr("Open Problems"),
        [this]() { showProblemsPanel(); });

    cmds->registerCommand(
        prefix + QStringLiteral(".openSearch"),
        tr("Open Search"),
        [this]() { showSearchPanel(); });

    cmds->registerCommand(
        prefix + QStringLiteral(".openTerminal"),
        tr("Open Terminal"),
        [this]() { showTerminalPanel(); });

    cmds->registerCommand(
        prefix + QStringLiteral(".openGit"),
        tr("Open Git"),
        [this]() { showGitPanel(); });

    cmds->registerCommand(
        prefix + QStringLiteral(".openOutput"),
        tr("Open Output"),
        [this]() { showPanel(QStringLiteral("OutputDock")); });

    cmds->registerCommand(
        prefix + QStringLiteral(".openProject"),
        tr("Open Project Explorer"),
        [this]() { showProjectTree(); });
}

void LanguageWorkbenchPluginBase::installStandardLspMenu(
    const QString &menuId,
    const QString &menuTitle,
    const QString &prefix,
    QObject *owner)
{
    if (!menus())
        return;

    ensureMenu(menuId, menuTitle);

    addMenuCommand(menuId, tr("Go to &Definition"),
                   prefix + QStringLiteral(".goToDefinition"), owner,
                   QKeySequence(Qt::Key_F12));
    addMenuCommand(menuId, tr("Go to De&claration"),
                   prefix + QStringLiteral(".goToDeclaration"), owner,
                   QKeySequence(Qt::CTRL | Qt::Key_F12));
    addMenuCommand(menuId, tr("Find &References"),
                   prefix + QStringLiteral(".findReferences"), owner,
                   QKeySequence(Qt::SHIFT | Qt::Key_F12));
    addMenuCommand(menuId, tr("&Rename Symbol"),
                   prefix + QStringLiteral(".renameSymbol"), owner,
                   QKeySequence(Qt::Key_F2));
    addMenuCommand(menuId, tr("&Format Document"),
                   prefix + QStringLiteral(".formatDocument"), owner,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F));
    addMenuCommand(menuId, tr("Go to Symbol in &Workspace"),
                   prefix + QStringLiteral(".gotoWorkspaceSymbol"), owner,
                   QKeySequence(Qt::CTRL | Qt::Key_T));

    if (auto *menu = ensureMenu(menuId, menuTitle))
        menu->addSeparator();
    addMenuCommand(menuId, tr("&Open Problems"),
                   prefix + QStringLiteral(".openProblems"), owner);
    addMenuCommand(menuId, tr("Open &Search"),
                   prefix + QStringLiteral(".openSearch"), owner);
    addMenuCommand(menuId, tr("Open &Terminal"),
                   prefix + QStringLiteral(".openTerminal"), owner);
    addMenuCommand(menuId, tr("Open &Git"),
                   prefix + QStringLiteral(".openGit"), owner);
    addMenuCommand(menuId, tr("Open &Output"),
                   prefix + QStringLiteral(".openOutput"), owner);
    addMenuCommand(menuId, tr("Open Project &Explorer"),
                   prefix + QStringLiteral(".openProject"), owner);

    if (auto *menu = ensureMenu(menuId, menuTitle))
        menu->addSeparator();
    addMenuCommand(menuId, tr("Restart &Language Server"),
                   prefix + QStringLiteral(".restartLanguageServer"), owner);
}

void LanguageWorkbenchPluginBase::installStandardLspToolBar(
    const QString &toolBarId,
    const QString &toolBarTitle,
    const QString &prefix,
    QObject *owner)
{
    createToolBar(toolBarId, toolBarTitle);
    addToolBarCommands(toolBarId, {
        { tr("Definition"),  prefix + QStringLiteral(".goToDefinition"),
          QKeySequence(Qt::Key_F12), false, QStringLiteral("definition") },
        { tr("References"),  prefix + QStringLiteral(".findReferences"),
          QKeySequence(Qt::SHIFT | Qt::Key_F12), false, QStringLiteral("references") },
        { tr("Rename"),      prefix + QStringLiteral(".renameSymbol"),
          QKeySequence(Qt::Key_F2), true, QStringLiteral("rename") },
        { tr("Format"),      prefix + QStringLiteral(".formatDocument"),
          QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), false, QStringLiteral("format") },
        { tr("Symbols"),     prefix + QStringLiteral(".gotoWorkspaceSymbol"),
          QKeySequence(Qt::CTRL | Qt::Key_T), true, QStringLiteral("search") },
        { tr("Restart LSP"), prefix + QStringLiteral(".restartLanguageServer"),
          {}, true, QStringLiteral("restart") },
    }, owner);
}

// ── Status Bar ────────────────────────────────────────────────────────────────

void LanguageWorkbenchPluginBase::installLanguageStatusItem(
    const QString &itemId,
    const QString &initialText,
    const QString &tooltip)
{
    auto *sb = statusBar();
    if (!sb)
        return;

    auto label = std::make_unique<QLabel>(initialText);
    if (!tooltip.isEmpty())
        label->setToolTip(tooltip);
    sb->addWidget(itemId, label.release(), IStatusBarManager::Right, 70);
}

void LanguageWorkbenchPluginBase::updateLanguageStatus(
    const QString &itemId,
    const QString &text,
    const QString &tooltip)
{
    auto *sb = statusBar();
    if (!sb)
        return;

    sb->setText(itemId, text);
    if (!tooltip.isEmpty())
        sb->setTooltip(itemId, tooltip);
}

void LanguageWorkbenchPluginBase::removeLanguageStatusItem(const QString &itemId)
{
    auto *sb = statusBar();
    if (sb)
        sb->removeWidget(itemId);
}

// ── Auto-restart on crash ─────────────────────────────────────────────────────

void LanguageWorkbenchPluginBase::setupServerAutoRestart(
    ProcessLanguageServer *server,
    const QString &statusItemId,
    const QString &displayName,
    int maxRestarts)
{
    if (!server)
        return;

    auto *restartCount = new int(0);

    QObject::connect(server, &ProcessLanguageServer::serverReady, server,
        [this, restartCount, statusItemId, displayName]() {
            *restartCount = 0;
            updateLanguageStatus(statusItemId,
                displayName + QStringLiteral(": ") + tr("ready"),
                tr("Language server is running and ready."));
            showStatusMessage(displayName + QStringLiteral(" ") + tr("ready"), 3000);
        });

    QObject::connect(server, &ProcessLanguageServer::serverStopped, server,
        [this, statusItemId, displayName]() {
            updateLanguageStatus(statusItemId,
                displayName + QStringLiteral(": ") + tr("unavailable"),
                tr("Language server is not installed or not on PATH."));
        });

    QObject::connect(server, &ProcessLanguageServer::serverCrashed, server,
        [this, server, restartCount, statusItemId, displayName, maxRestarts]() {
            if (*restartCount < maxRestarts) {
                ++(*restartCount);
                updateLanguageStatus(statusItemId,
                    displayName + QStringLiteral(": ") + tr("restarting"),
                    tr("Language server crashed. Restart %1 of %2.")
                        .arg(*restartCount).arg(maxRestarts));
                showStatusMessage(
                    tr("%1 crashed \u2014 restarting (%2/%3)...")
                        .arg(displayName).arg(*restartCount).arg(maxRestarts),
                    4000);
                const QString root = languageWorkspaceRoot();
                QTimer::singleShot(3000, server, [server, root]() {
                    server->start(root);
                });
            } else {
                updateLanguageStatus(statusItemId,
                    displayName + QStringLiteral(": ") + tr("degraded"),
                    tr("Language server crashed repeatedly. Features unavailable."));
                if (auto *notif = notifications())
                    notif->warning(
                        tr("%1 crashed repeatedly. Language features unavailable.")
                            .arg(displayName));
            }
        });
}
