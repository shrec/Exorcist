#include "startpageplugin.h"

#include "welcomewidget.h"

#include "sdk/icommandservice.h"

#include <QSettings>

StartPagePlugin::StartPagePlugin(QObject *parent)
    : QObject(parent)
{
}

PluginInfo StartPagePlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.start-page"),
        QStringLiteral("Start Page"),
        QStringLiteral("1.0.0"),
        QStringLiteral("Welcome page shown when no workspace is open."),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {}
    };
}

bool StartPagePlugin::initializePlugin()
{
    // Construct WelcomeWidget owned by this plugin.  Parent stays nullptr
    // here — MainWindow reparents it into its central stack when it queries
    // the "welcomeWidget" service in deferredInit.
    m_welcome = new WelcomeWidget(nullptr);

    // Register as a service so MainWindow can pick it up.
    registerService(QStringLiteral("welcomeWidget"), m_welcome);

    // Route widget signals through the command service.  This way the
    // welcome page does not need to know about MainWindow's slots —
    // whichever plugin / container owns the corresponding command id
    // handles the click.
    connect(m_welcome, &WelcomeWidget::openFolderBrowseRequested, this, [this]() {
        executeCommand(QStringLiteral("file.openFolder"));
    });
    connect(m_welcome, &WelcomeWidget::openFolderRequested, this,
            [this](const QString &path) {
        // The folder path is bound via a command-with-arg pattern.  The
        // container exposes "file.openFolder.path" for this case;
        // executeCommand cannot pass args directly through the simple
        // ICommandService API, so we set a one-shot QSettings key the
        // container reads, then fire the bare command.  TODO: extend
        // ICommandService to carry a QVariantList of args.
        QSettings(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"))
            .setValue(QStringLiteral("startPage/openFolderPath"), path);
        executeCommand(QStringLiteral("file.openFolderPath"));
    });
    connect(m_welcome, &WelcomeWidget::newProjectRequested, this, [this]() {
        executeCommand(QStringLiteral("file.newProject"));
    });
    connect(m_welcome, &WelcomeWidget::newFileRequested, this, [this]() {
        executeCommand(QStringLiteral("file.newTab"));
    });
    connect(m_welcome, &WelcomeWidget::openFileRequested, this, [this]() {
        executeCommand(QStringLiteral("file.openFile"));
    });

    // Plugin-owned commands — refresh and show.  Tracked so they
    // unregister automatically on shutdown (rule L7).
    registerCommand(QStringLiteral("startPage.refresh"),
                    tr("Refresh Recent Folders"),
                    [this]() {
        if (m_welcome) m_welcome->refreshRecent();
    });
    registerCommand(QStringLiteral("startPage.show"),
                    tr("Show Start Page"),
                    [this]() {
        executeCommand(QStringLiteral("ide.showWelcome"));
    });

    return true;
}

void StartPagePlugin::onWorkspaceClosed()
{
    // Refresh the recent folder list so the just-closed workspace appears
    // at the top of the list when the welcome page comes back.
    if (m_welcome)
        m_welcome->refreshRecent();
}
