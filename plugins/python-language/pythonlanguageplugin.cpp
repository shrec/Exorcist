#include "pythonlanguageplugin.h"

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

constexpr auto kPythonMenuId       = "python.language";
constexpr auto kPythonToolBarId    = "python";
constexpr auto kPythonStatusItemId = "python.language.status";

}

PythonLanguagePlugin::PythonLanguagePlugin(QObject *parent)
    : QObject(parent)
{
}

PythonLanguagePlugin::~PythonLanguagePlugin() = default;

PluginInfo PythonLanguagePlugin::info() const
{
    PluginInfo pi;
    pi.id          = QStringLiteral("org.exorcist.python-language");
    pi.name        = QStringLiteral("Python Language Support");
    pi.version     = QStringLiteral("1.0.0");
    pi.description = QStringLiteral("Process-based Python LSP integration for .py files.");
    pi.author      = QStringLiteral("Exorcist");
    pi.requestedPermissions = {
        PluginPermission::WorkspaceRead,
        PluginPermission::TerminalExecute,
        PluginPermission::FilesystemRead,
    };
    return pi;
}

bool PythonLanguagePlugin::initializePlugin()
{
    // ── Create and start pylsp ──────────────────────────────────────────
    m_server = createProcessLanguageServer(
        QStringLiteral("pylsp"),
        tr("Python language server"));
    attachLanguageServerToWorkbench(m_server.get());

    // ── Status bar + auto-restart ──────────────────────────────────────
    installLanguageStatusItem(
        QLatin1String(kPythonStatusItemId),
        tr("Python: starting"),
        tr("Starting pylsp for the active workspace."));
    setupServerAutoRestart(m_server.get(),
                           QLatin1String(kPythonStatusItemId),
                           QStringLiteral("pylsp"));

    // ── Register commands, menu, toolbar ───────────────────────────────
    registerStandardLspCommands(QStringLiteral("python"),
                                m_server->client(), m_server.get(), this);
    registerPythonCommands();
    installPythonMenu();
    installStandardLspToolBar(QLatin1String(kPythonToolBarId),
                              tr("Python"), QStringLiteral("python"), this);

    // ── Show standard panels ──────────────────────────────────────────
    showStandardLanguagePanels();

    // ── Start language server ─────────────────────────────────────────
    startProcessLanguageServer(m_server.get());
    return true;
}

void PythonLanguagePlugin::shutdownPlugin()
{
    stopProcessLanguageServer(m_server.get());
    removeLanguageStatusItem(QLatin1String(kPythonStatusItemId));
}

// ── Python-specific commands ──────────────────────────────────────────────────

void PythonLanguagePlugin::registerPythonCommands()
{
    auto *cmds = commands();
    if (!cmds)
        return;

    cmds->registerCommand(
        QStringLiteral("python.runScript"),
        tr("Run Python Script"),
        [this]() {
            auto *term = terminal();
            auto *ed = editor();
            if (!term || !ed) {
                showWarning(tr("Terminal or editor service is not available."));
                return;
            }
            const QString path = ed->activeFilePath();
            if (path.isEmpty() || !path.endsWith(QStringLiteral(".py"))) {
                showWarning(tr("No Python file is open."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("python \"%1\"").arg(QDir::toNativeSeparators(path)));
        });

    cmds->registerCommand(
        QStringLiteral("python.runModule"),
        tr("Run as Module"),
        [this]() {
            auto *term = terminal();
            auto *ed = editor();
            if (!term || !ed) {
                showWarning(tr("Terminal or editor service is not available."));
                return;
            }
            const QString path = ed->activeFilePath();
            if (path.isEmpty() || !path.endsWith(QStringLiteral(".py"))) {
                showWarning(tr("No Python file is open."));
                return;
            }
            // Convert path to module notation relative to workspace root
            const QString root = languageWorkspaceRoot();
            QString rel = QDir(root).relativeFilePath(path);
            rel.replace(QChar('/'), QChar('.'));
            if (rel.endsWith(QStringLiteral(".py")))
                rel.chop(3);
            showTerminalPanel();
            term->runCommand(QStringLiteral("python -m %1").arg(rel));
        });

    cmds->registerCommand(
        QStringLiteral("python.pipInstall"),
        tr("Install Requirements"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            const QString root = languageWorkspaceRoot();
            const QString reqFile = root + QStringLiteral("/requirements.txt");
            if (!QFile::exists(reqFile)) {
                showWarning(tr("requirements.txt not found in workspace root."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("pip install -r \"%1\"")
                                 .arg(QDir::toNativeSeparators(reqFile)));
        });

    cmds->registerCommand(
        QStringLiteral("python.pipFreeze"),
        tr("Pip Freeze"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("pip freeze"));
        });

    cmds->registerCommand(
        QStringLiteral("python.pytest"),
        tr("Run pytest"),
        [this]() {
            auto *term = terminal();
            if (!term) {
                showWarning(tr("Terminal service is not available."));
                return;
            }
            showTerminalPanel();
            term->runCommand(QStringLiteral("python -m pytest -v"));
        });

    cmds->registerCommand(
        QStringLiteral("python.openRequirements"),
        tr("Open requirements.txt"),
        [this]() {
            const QString root = languageWorkspaceRoot();
            const QString path = root + QStringLiteral("/requirements.txt");
            if (!QFile::exists(path)) {
                // Try pyproject.toml as fallback
                const QString pyproject = root + QStringLiteral("/pyproject.toml");
                if (QFile::exists(pyproject)) {
                    openFile(pyproject);
                    return;
                }
                showWarning(tr("Neither requirements.txt nor pyproject.toml found."));
                return;
            }
            openFile(path);
        });
}

void PythonLanguagePlugin::installPythonMenu()
{
    if (!menus())
        return;

    // Standard LSP commands in the Python menu
    installStandardLspMenu(QLatin1String(kPythonMenuId),
                           tr("&Python"), QStringLiteral("python"), this);

    // Python-specific entries
    if (auto *menu = ensureMenu(QLatin1String(kPythonMenuId), tr("&Python")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kPythonMenuId), tr("&Run Script"),
                   QStringLiteral("python.runScript"), this,
                   QKeySequence(Qt::CTRL | Qt::Key_F5));
    addMenuCommand(QLatin1String(kPythonMenuId), tr("Run as &Module"),
                   QStringLiteral("python.runModule"), this);
    addMenuCommand(QLatin1String(kPythonMenuId), tr("Run p&ytest"),
                   QStringLiteral("python.pytest"), this);

    if (auto *menu = ensureMenu(QLatin1String(kPythonMenuId), tr("&Python")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kPythonMenuId), tr("&Install Requirements"),
                   QStringLiteral("python.pipInstall"), this);
    addMenuCommand(QLatin1String(kPythonMenuId), tr("Pip &Freeze"),
                   QStringLiteral("python.pipFreeze"), this);

    if (auto *menu = ensureMenu(QLatin1String(kPythonMenuId), tr("&Python")))
        menu->addSeparator();
    addMenuCommand(QLatin1String(kPythonMenuId), tr("Open &requirements.txt"),
                   QStringLiteral("python.openRequirements"), this);
}
