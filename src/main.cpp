#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QLocale>
#include <QPalette>
#include <QStyleFactory>
#include <QTranslator>

#include "mainwindow.h"

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
        translator->load(fallbackName, basePath);
    }

    if (!translator->isEmpty()) {
        app.installTranslator(translator);
    }
}
} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/icon.png"));
    applyDarkTheme(app);
    loadTranslator(app);

    MainWindow window;
    window.show();

    return app.exec();
}
