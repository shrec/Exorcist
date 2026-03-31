#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QIcon>
#include <QLocale>
#include <QPalette>
#include <QStyleFactory>
#include <QTimer>
#include <QTranslator>

#include "crashhandler.h"
#include "logger.h"
#include "mainwindow.h"
#include "startupprofiler.h"

namespace
{
void applyDarkTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    // VS2022 dark theme colors
    palette.setColor(QPalette::Window, QColor(30, 30, 30));        // #1e1e1e
    palette.setColor(QPalette::WindowText, QColor(212, 212, 212)); // #d4d4d4
    palette.setColor(QPalette::Base, QColor(30, 30, 30));          // #1e1e1e
    palette.setColor(QPalette::AlternateBase, QColor(37, 37, 38)); // #252526
    palette.setColor(QPalette::ToolTipBase, QColor(45, 45, 48));   // #2d2d30
    palette.setColor(QPalette::ToolTipText, QColor(212, 212, 212));
    palette.setColor(QPalette::Text, QColor(212, 212, 212));       // #d4d4d4
    palette.setColor(QPalette::Button, QColor(45, 45, 48));        // #2d2d30
    palette.setColor(QPalette::ButtonText, QColor(212, 212, 212));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Highlight, QColor(0, 122, 204));    // #007acc
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Mid, QColor(60, 60, 60));           // #3c3c3c
    palette.setColor(QPalette::Dark, QColor(37, 37, 38));          // #252526
    palette.setColor(QPalette::Light, QColor(62, 62, 66));         // #3e3e42
    palette.setColor(QPalette::Midlight, QColor(51, 51, 51));
    palette.setColor(QPalette::Shadow, QColor(0, 0, 0));
    palette.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));
    palette.setColor(QPalette::Link, QColor(0, 122, 204));
    palette.setColor(QPalette::LinkVisited, QColor(0, 122, 204));

    // Disabled state
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(92, 92, 92));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(92, 92, 92));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(92, 92, 92));

    app.setPalette(palette);

    // Global application stylesheet — VS2022 dark theme
    app.setStyleSheet(QStringLiteral(
        /* ── Scrollbars ─────────────────────────────────────── */
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

        /* ── Menu bar ───────────────────────────────────────── */
        "QMenuBar {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border-bottom: 1px solid #3c3c3c;"
        "  padding: 0; spacing: 0;"
        "}"
        "QMenuBar::item {"
        "  background: transparent; padding: 4px 8px; color: #d4d4d4;"
        "}"
        "QMenuBar::item:selected {"
        "  background: #3e3e40;"
        "}"
        "QMenuBar::item:pressed {"
        "  background: #094771;"
        "}"

        /* ── Menus (drop-down) ──────────────────────────────── */
        "QMenu {"
        "  background: #252526; color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  padding: 2px 0;"
        "}"
        "QMenu::item {"
        "  padding: 4px 28px 4px 24px;"
        "}"
        "QMenu::item:selected {"
        "  background: #094771;"
        "}"
        "QMenu::separator {"
        "  height: 1px; background: #3c3c3c; margin: 2px 8px;"
        "}"
        "QMenu::icon {"
        "  padding-left: 4px;"
        "}"

        /* ── Toolbars (DockToolBar via object name) ─────────── */
        "QToolBar[objectName^=\"exdock-toolbar\"] {"
        "  background: #2d2d30;"
        "  border-bottom: 1px solid #3c3c3c;"
        "  spacing: 1px;"
        "  padding: 1px 4px;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 2px;"
        "  color: #d4d4d4;"
        "  padding: 2px 5px;"
        "  font-size: 12px;"
        "  min-height: 20px;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:hover {"
        "  background: #3e3e40;"
        "  border: 1px solid #555558;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:pressed {"
        "  background: #1b1b1c;"
        "  border: 1px solid #007acc;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:checked {"
        "  background: #094771;"
        "  border: 1px solid #007acc;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:disabled {"
        "  color: #5c5c5c;"
        "}"

        /* ── Toolbar separators ─────────────────────────────── */
        "QToolBar[objectName^=\"exdock-toolbar\"]::separator {"
        "  width: 1px; background: #3c3c3c; margin: 3px 4px;"
        "}"

        /* ── Toolbar combo boxes (e.g. build config) ────────── */
        "QToolBar QComboBox {"
        "  background: #3f3f46; color: #d4d4d4;"
        "  border: 1px solid #555558;"
        "  padding: 1px 6px; font-size: 12px;"
        "  border-radius: 0px; min-height: 20px;"
        "}"
        "QToolBar QComboBox:hover {"
        "  border: 1px solid #007acc;"
        "}"
        "QToolBar QComboBox::drop-down {"
        "  border: none; width: 16px;"
        "}"
        "QToolBar QComboBox QAbstractItemView {"
        "  background: #252526; color: #d4d4d4;"
        "  selection-background-color: #094771;"
        "  border: 1px solid #555558;"
        "}"

        /* ── Toolbar line edits (e.g. search) ───────────────── */
        "QToolBar QLineEdit {"
        "  background: #3c3c3c; color: #d4d4d4;"
        "  border: 1px solid #555558;"
        "  padding: 2px 4px; font-size: 12px;"
        "  selection-background-color: #264f78;"
        "}"
        "QToolBar QLineEdit:focus {"
        "  border: 1px solid #007acc;"
        "}"

        /* ── Status bar ─────────────────────────────────────── */
        "QStatusBar {"
        "  background: #007acc; color: white;"
        "  border: none; font-size: 12px;"
        "}"
        "QStatusBar::item { border: none; }"
        "QStatusBar QLabel { color: white; background: transparent; }"

        /* ── Tab widgets ────────────────────────────────────── */
        "QTabWidget::pane {"
        "  border: 1px solid #3c3c3c; background: #1e1e1e;"
        "}"
        "QTabBar::tab {"
        "  background: #2d2d30; color: #9d9d9d;"
        "  padding: 5px 12px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "}"
        "QTabBar::tab:selected {"
        "  color: #d4d4d4;"
        "  border-bottom: 2px solid #007acc;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  color: #d4d4d4;"
        "  background: #3e3e40;"
        "}"

        /* ── Tree / List / Table views ──────────────────────── */
        "QTreeView, QListView, QTableView {"
        "  background: #1e1e1e; color: #d4d4d4;"
        "  border: none;"
        "  selection-background-color: #094771;"
        "  selection-color: white;"
        "  outline: none;"
        "}"
        "QTreeView::item:hover, QListView::item:hover {"
        "  background: #2a2d2e;"
        "}"
        "QTreeView::item:selected, QListView::item:selected {"
        "  background: #094771;"
        "}"
        "QTreeView::branch {"
        "  background: transparent;"
        "}"
        "QHeaderView::section {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border: none; border-right: 1px solid #3c3c3c;"
        "  padding: 3px 6px; font-size: 11px;"
        "}"

        /* ── Group boxes / frames ───────────────────────────── */
        "QGroupBox {"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  margin-top: 8px; padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin; left: 8px;"
        "}"

        /* ── Line edits ─────────────────────────────────────── */
        "QLineEdit {"
        "  background: #3c3c3c; color: #d4d4d4;"
        "  border: 1px solid #555558;"
        "  padding: 3px 6px;"
        "  selection-background-color: #264f78;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #007acc;"
        "}"

        /* ── Push buttons ───────────────────────────────────── */
        "QPushButton {"
        "  background: #3f3f46; color: #d4d4d4;"
        "  border: 1px solid #555558;"
        "  padding: 4px 14px;"
        "  min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "  background: #4e4e52; border: 1px solid #007acc;"
        "}"
        "QPushButton:pressed {"
        "  background: #1b1b1c;"
        "}"
        "QPushButton:disabled {"
        "  background: #2d2d30; color: #5c5c5c; border: 1px solid #3c3c3c;"
        "}"

        /* ── Check/Radio boxes ──────────────────────────────── */
        "QCheckBox, QRadioButton {"
        "  color: #d4d4d4; spacing: 6px;"
        "}"
        "QCheckBox::indicator, QRadioButton::indicator {"
        "  width: 14px; height: 14px;"
        "}"

        /* ── Tooltips ───────────────────────────────────────── */
        "QToolTip {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  padding: 4px; font-size: 12px;"
        "}"

        /* ── Splitters ──────────────────────────────────────── */
        "QSplitter::handle {"
        "  background: #3c3c3c;"
        "}"
        "QSplitter::handle:hover {"
        "  background: #007acc;"
        "}"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }"
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
    CrashHandler::install();
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
