#include "typescriptlanguageplugin.h"

#include "lsp/lspclient.h"
#include "lsp/processlanguageserver.h"
#include "sdk/icommandservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/iterminalservice.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>

namespace {

constexpr auto kTsMenuId       = "typescript.language";
constexpr auto kTsToolBarId    = "typescript";
constexpr auto kTsStatusItemId = "typescript.language.status";

}

TypeScriptLanguagePlugin::TypeScriptLanguagePlugin(QObject *parent)
    : QObject(parent)
{
}

TypeScriptLanguagePlugin::~TypeScriptLanguagePlugin() = default;

PluginInfo TypeScriptLanguagePlugin::info() const
{
    PluginInfo pi;
    pi.id          = QStringLiteral("org.exorcist.typescript-language");
    pi.name        = QStringLiteral("TypeScript/JavaScript Language Support");
    pi.version     = QStringLiteral("1.0.0");
    pi.description = QStringLiteral("TypeScript and JavaScript support via typescript-language-server.");
    pi.author      = QStringLiteral("Exorcist");
    pi.requestedPermissions = {
        PluginPermission::WorkspaceRead,
        PluginPermission::TerminalExecute,
        PluginPermission::FilesystemRead,
    };
    return pi;
}

bool TypeScriptLanguagePlugin::initializePlugin()
{
    // ── Create and start typescript-language-server ─────────────────────
    m_server = createProcessLanguageServer(
        QStringLiteral("typescript-language-server"),
        tr("TypeScript language server"),
        { QStringLiteral("--stdio") });
    attachLanguageServerToWorkbench(m_server.get());

    // ── Status bar + auto-restart ──────────────────────────────────────
    installLanguageStatusItem(
        QLatin1String(kTsStatusItemId),
        tr("TS: starting"),
        tr("Starting typescript-language-server for the active workspace."));
    setupServerAutoRestart(m_server.get(),
                           QLatin1String(kTsStatusItemId),
                           QStringLiteral("typescript-language-server"));

    // ── Register commands, menu, toolbar ───────────────────────────────
    registerStandardLspCommands(QStringLiteral("typescript"),
                                m_server->client(), m_server.get(), this);
    registerTypeScriptCommands();
    installTypeScriptMenu();
    installStandardLspToolBar(QLatin1String(kTsToolBarId),
                              tr("TypeScript"), QStringLiteral("typescript"), this);

    // ── Show standard panels ──────────────────────────────────────────
    showStandardLanguagePanels();

    // ── Start language server ─────────────────────────────────────────
    startProcessLanguageServer(m_server.get());
    return true;
}

void TypeScriptLanguagePlugin::shutdownPlugin()
{
    stopProcessLanguageServer(m_server.get());
    removeLanguageStatusItem(QLatin1String(kTsStatusItemId));
}

// ── TypeScript-specific commands ──────────────────────────────────────────────

void TypeScriptLanguagePlugin::registerTypeScriptCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(
        QStringLiteral("typescript.npmInstall"),
        tr("npm install"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("npm install"));
        });

    cmds->registerCommand(
        QStringLiteral("typescript.npmBuild"),
        tr("npm run build"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("npm run build"));
        });

    cmds->registerCommand(
        QStringLiteral("typescript.npmTest"),
        tr("npm test"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("npm test"));
        });

    cmds->registerCommand(
        QStringLiteral("typescript.npmStart"),
        tr("npm start"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("npm start"));
        });

    cmds->registerCommand(
        QStringLiteral("typescript.npmRunScript"),
        tr("Run npm Script"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            // List available scripts so the user can choose
            term->runCommand(QStringLiteral("npm run"));
        });

    cmds->registerCommand(
        QStringLiteral("typescript.npxTsc"),
        tr("TypeScript Compile"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("npx tsc --noEmit"));
        });

    cmds->registerCommand(
        QStringLiteral("typescript.npxEslint"),
        tr("Run ESLint"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("npx eslint ."));
        });

    cmds->registerCommand(
        QStringLiteral("typescript.openPackageJson"),
        tr("Open package.json"),
        [this]() {
            const QString root = languageWorkspaceRoot();
            const QString path = root + QStringLiteral("/package.json");
            if (!QFile::exists(path)) {
                showWarning(tr("package.json not found in workspace root."));
                return;
            }
            openFile(path);
        });

    cmds->registerCommand(
        QStringLiteral("typescript.openTsConfig"),
        tr("Open tsconfig.json"),
        [this]() {
            const QString root = languageWorkspaceRoot();
            const QString path = root + QStringLiteral("/tsconfig.json");
            if (!QFile::exists(path)) {
                // Try jsconfig.json
                const QString jsconfig = root + QStringLiteral("/jsconfig.json");
                if (QFile::exists(jsconfig)) {
                    openFile(jsconfig);
                    return;
                }
                showWarning(tr("Neither tsconfig.json nor jsconfig.json found."));
                return;
            }
            openFile(path);
        });
}

void TypeScriptLanguagePlugin::installTypeScriptMenu()
{
    if (!menus())
        return;

    // Standard LSP commands in the TS menu
    installStandardLspMenu(QLatin1String(kTsMenuId),
                           tr("&TypeScript"), QStringLiteral("typescript"), this);

    // TypeScript/npm-specific entries
    if (auto *menu = ensureMenu(QLatin1String(kTsMenuId), tr("&TypeScript")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kTsMenuId), tr("npm &install"),
                   QStringLiteral("typescript.npmInstall"), this);
    addMenuCommand(QLatin1String(kTsMenuId), tr("npm run &build"),
                   QStringLiteral("typescript.npmBuild"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    addMenuCommand(QLatin1String(kTsMenuId), tr("npm &test"),
                   QStringLiteral("typescript.npmTest"), this);
    addMenuCommand(QLatin1String(kTsMenuId), tr("npm &start"),
                   QStringLiteral("typescript.npmStart"), this);
    addMenuCommand(QLatin1String(kTsMenuId), tr("&Run npm Script"),
                   QStringLiteral("typescript.npmRunScript"), this);

    if (auto *menu = ensureMenu(QLatin1String(kTsMenuId), tr("&TypeScript")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kTsMenuId), tr("TypeScript &Compile"),
                   QStringLiteral("typescript.npxTsc"), this);
    addMenuCommand(QLatin1String(kTsMenuId), tr("Run &ESLint"),
                   QStringLiteral("typescript.npxEslint"), this);

    if (auto *menu = ensureMenu(QLatin1String(kTsMenuId), tr("&TypeScript")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kTsMenuId), tr("Open &package.json"),
                   QStringLiteral("typescript.openPackageJson"), this);
    addMenuCommand(QLatin1String(kTsMenuId), tr("Open ts&config.json"),
                   QStringLiteral("typescript.openTsConfig"), this);
}
