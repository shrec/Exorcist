#include "rustlanguageplugin.h"

#include "lsp/lspclient.h"
#include "lsp/processlanguageserver.h"
#include "sdk/icommandservice.h"
#include "sdk/ieditorservice.h"
#include "sdk/iterminalservice.h"

#include <QDir>
#include <QFile>
#include <QMenu>

namespace {

constexpr auto kRustMenuId       = "rust.language";
constexpr auto kRustToolBarId    = "rust";
constexpr auto kRustStatusItemId = "rust.language.status";

}

RustLanguagePlugin::RustLanguagePlugin(QObject *parent)
    : QObject(parent)
{
}

RustLanguagePlugin::~RustLanguagePlugin() = default;

PluginInfo RustLanguagePlugin::info() const
{
    PluginInfo pi;
    pi.id          = QStringLiteral("org.exorcist.rust-language");
    pi.name        = QStringLiteral("Rust Language Support");
    pi.version     = QStringLiteral("1.0.0");
    pi.description = QStringLiteral("rust-analyzer integration for Cargo and Rust workspaces.");
    pi.author      = QStringLiteral("Exorcist");
    pi.requestedPermissions = {
        PluginPermission::WorkspaceRead,
        PluginPermission::TerminalExecute,
        PluginPermission::FilesystemRead,
    };
    return pi;
}

bool RustLanguagePlugin::initializePlugin()
{
    // ── Create and start rust-analyzer ──────────────────────────────────
    m_server = createProcessLanguageServer(
        QStringLiteral("rust-analyzer"),
        tr("Rust language server"));
    attachLanguageServerToWorkbench(m_server.get());

    // ── Status bar + auto-restart ──────────────────────────────────────
    installLanguageStatusItem(
        QLatin1String(kRustStatusItemId),
        tr("Rust: starting"),
        tr("Starting rust-analyzer for the active workspace."));
    setupServerAutoRestart(m_server.get(),
                           QLatin1String(kRustStatusItemId),
                           QStringLiteral("rust-analyzer"));

    // ── Register commands, menu, toolbar ───────────────────────────────
    registerStandardLspCommands(QStringLiteral("rust"),
                                m_server->client(), m_server.get(), this);
    registerRustCommands();
    installRustMenu();
    installStandardLspToolBar(QLatin1String(kRustToolBarId),
                              tr("Rust"), QStringLiteral("rust"), this);

    // ── Show standard panels ──────────────────────────────────────────
    showStandardLanguagePanels();

    // ── Start language server ─────────────────────────────────────────
    startProcessLanguageServer(m_server.get());
    return true;
}

void RustLanguagePlugin::shutdownPlugin()
{
    stopProcessLanguageServer(m_server.get());
    removeLanguageStatusItem(QLatin1String(kRustStatusItemId));
}

// ── Rust-specific commands ────────────────────────────────────────────────────

void RustLanguagePlugin::registerRustCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(
        QStringLiteral("rust.cargoBuild"),
        tr("Cargo Build"),
        [this]() {
            executeCommand(QStringLiteral("build.build"));
            showStatusMessage(tr("Running cargo build..."), 3000);
        });

    cmds->registerCommand(
        QStringLiteral("rust.cargoRun"),
        tr("Cargo Run"),
        [this]() {
            executeCommand(QStringLiteral("build.run"));
            showStatusMessage(tr("Running cargo run..."), 3000);
        });

    cmds->registerCommand(
        QStringLiteral("rust.cargoTest"),
        tr("Cargo Test"),
        [this]() {
            executeCommand(QStringLiteral("testing.runAll"));
            showStatusMessage(tr("Running cargo test..."), 3000);
        });

    cmds->registerCommand(
        QStringLiteral("rust.cargoCheck"),
        tr("Cargo Check"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("cargo check"));
        });

    cmds->registerCommand(
        QStringLiteral("rust.cargoClippy"),
        tr("Cargo Clippy"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("cargo clippy"));
        });

    cmds->registerCommand(
        QStringLiteral("rust.cargoClean"),
        tr("Cargo Clean"),
        [this]() {
            executeCommand(QStringLiteral("build.clean"));
            showStatusMessage(tr("Running cargo clean..."), 3000);
        });

    cmds->registerCommand(
        QStringLiteral("rust.openCargoToml"),
        tr("Open Cargo.toml"),
        [this]() {
            const QString root = languageWorkspaceRoot();
            const QString path = root + QStringLiteral("/Cargo.toml");
            if (!QFile::exists(path)) {
                showWarning(tr("Cargo.toml not found in workspace root."));
                return;
            }
            openFile(path);
        });
}

void RustLanguagePlugin::installRustMenu()
{
    if (!menus())
        return;

    // Standard LSP commands in the Rust menu
    installStandardLspMenu(QLatin1String(kRustMenuId),
                           tr("&Rust"), QStringLiteral("rust"), this);

    // Rust-specific entries
    if (auto *menu = ensureMenu(QLatin1String(kRustMenuId), tr("&Rust")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kRustMenuId), tr("Cargo &Build"),
                   QStringLiteral("rust.cargoBuild"), this,
                   QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_B));
    addMenuCommand(QLatin1String(kRustMenuId), tr("Cargo &Run"),
                   QStringLiteral("rust.cargoRun"), this,
                   QKeySequence(Qt::CTRL | Qt::Key_F5));
    addMenuCommand(QLatin1String(kRustMenuId), tr("Cargo &Test"),
                   QStringLiteral("rust.cargoTest"), this);
    addMenuCommand(QLatin1String(kRustMenuId), tr("Cargo C&heck"),
                   QStringLiteral("rust.cargoCheck"), this);
    addMenuCommand(QLatin1String(kRustMenuId), tr("Cargo Cli&ppy"),
                   QStringLiteral("rust.cargoClippy"), this);
    addMenuCommand(QLatin1String(kRustMenuId), tr("Cargo C&lean"),
                   QStringLiteral("rust.cargoClean"), this);

    if (auto *menu = ensureMenu(QLatin1String(kRustMenuId), tr("&Rust")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kRustMenuId), tr("Open &Cargo.toml"),
                   QStringLiteral("rust.openCargoToml"), this);
}
