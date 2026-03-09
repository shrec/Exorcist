#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QPalette>
#include <QStyleFactory>
#include <QTimer>
#include <QTranslator>

#include "logger.h"
#include "mainwindow.h"
#include "startupprofiler.h"

namespace
{
void applyDarkTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor(30, 30, 30));
    palette.setColor(QPalette::WindowText, QColor(220, 220, 220));
    palette.setColor(QPalette::Base, QColor(20, 20, 20));
    palette.setColor(QPalette::AlternateBase, QColor(35, 35, 35));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(0, 0, 0));
    palette.setColor(QPalette::Text, QColor(220, 220, 220));
    palette.setColor(QPalette::Button, QColor(40, 40, 40));
    palette.setColor(QPalette::ButtonText, QColor(220, 220, 220));
    palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, QColor(0, 0, 0));

    app.setPalette(palette);

    // Style scrollbars — Fusion QPalette alone renders them nearly invisible
    app.setStyleSheet(QStringLiteral(
        "QScrollBar:vertical {"
        "  background: #1e1e1e; width: 12px; margin: 0; }"
        "QScrollBar::handle:vertical {"
        "  background: #5a5a5a; min-height: 20px; border-radius: 3px; margin: 2px; }"
        "QScrollBar::handle:vertical:hover { background: #787878; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal {"
        "  background: #1e1e1e; height: 12px; margin: 0; }"
        "QScrollBar::handle:horizontal {"
        "  background: #5a5a5a; min-width: 20px; border-radius: 3px; margin: 2px; }"
        "QScrollBar::handle:horizontal:hover { background: #787878; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }"
    ));
}

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
    Logger::install();
    qInfo() << "Exorcist starting";

    app.setWindowIcon(QIcon(":/icons/icon.png"));
    app.setFont(QFont(QStringLiteral("Segoe UI"), 10));
    applyDarkTheme(app);
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
