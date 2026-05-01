#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QSettings>
#include <QTimer>
#include <QTranslator>

#include "crashhandler.h"
#include "logger.h"
#include "mainwindow.h"
#include "startupprofiler.h"
#include "ui/thememanager.h"

namespace
{
void loadTranslator(QApplication &app)
{
    auto *translator = new QTranslator(&app);
    const QString overrideLang = qEnvironmentVariable("EXORCIST_LANG");
    const QString localeName = overrideLang.isEmpty() ? QLocale::system().name() : overrideLang;

    const QString basePath = QCoreApplication::applicationDirPath() + "/translations";
    const QString fileName = QString("exorcist_%1.qm").arg(localeName);
    if (!translator->load(fileName, basePath)) {
        const QString shortLocale = localeName.section('_', 0, 0);
        const QString fallbackName = QString("exorcist_%1.qm").arg(shortLocale);
        (void)translator->load(fallbackName, basePath);
    }

    if (!translator->isEmpty()) {
        app.installTranslator(translator);
    }
}
} // namespace

int main(int argc, char *argv[])
{
    StartupProfiler::instance().begin();

    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("Exorcist"));
    app.setApplicationName(QStringLiteral("Exorcist"));
    CrashHandler::install();
    Logger::install();
    qInfo() << "Exorcist starting";

    app.setWindowIcon(QIcon(":/icons/icon.png"));
    app.setFont(QFont(QStringLiteral("Segoe UI"), 10));

    // Determine initial theme from persisted setting (default: dark).
    const bool darkInitial = QSettings(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"))
                                 .value(QStringLiteral("theme/dark"), true)
                                 .toBool();
    Exorcist::applyTheme(app, darkInitial);
    loadTranslator(app);
    StartupProfiler::instance().mark(QStringLiteral("app init + theme"));

    MainWindow window;
    StartupProfiler::instance().mark(QStringLiteral("MainWindow constructed"));
    window.showMaximized();

    // Defer heavy init (plugins, last folder) to after first paint.
    QTimer::singleShot(0, &window, &MainWindow::deferredInit);

    // Log startup profile after the event loop processes the initial
    // show/paint events (singleShot 0 fires once the queue is drained).
    QTimer::singleShot(0, []() {
        StartupProfiler::instance().mark(QStringLiteral("first paint"));
        StartupProfiler::instance().finish();
    });

    const int ret = app.exec();
    Logger::uninstall();
    return ret;
}
