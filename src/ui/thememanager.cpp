#include "thememanager.h"

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QSettings>
#include <QString>
#include <QStringLiteral>
#include <QStyleFactory>

namespace Exorcist
{

namespace
{

constexpr const char *kThemeGroup = "theme";
constexpr const char *kThemeDarkKey = "theme/dark";
constexpr const char *kOrg = "Exorcist";
constexpr const char *kApp = "Exorcist";

/* ── Palettes ──────────────────────────────────────────────────────────── */

QPalette darkPalette()
{
    QPalette p;
    // VS2022 dark theme colors
    p.setColor(QPalette::Window, QColor(30, 30, 30));        // #1e1e1e
    p.setColor(QPalette::WindowText, QColor(212, 212, 212)); // #d4d4d4
    p.setColor(QPalette::Base, QColor(30, 30, 30));          // #1e1e1e
    p.setColor(QPalette::AlternateBase, QColor(37, 37, 38)); // #252526
    p.setColor(QPalette::ToolTipBase, QColor(45, 45, 48));   // #2d2d30
    p.setColor(QPalette::ToolTipText, QColor(212, 212, 212));
    p.setColor(QPalette::Text, QColor(212, 212, 212));       // #d4d4d4
    p.setColor(QPalette::Button, QColor(45, 45, 48));        // #2d2d30
    p.setColor(QPalette::ButtonText, QColor(212, 212, 212));
    p.setColor(QPalette::BrightText, QColor(255, 255, 255));
    p.setColor(QPalette::Highlight, QColor(0, 122, 204));    // #007acc
    p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    p.setColor(QPalette::Mid, QColor(60, 60, 60));           // #3c3c3c
    p.setColor(QPalette::Dark, QColor(37, 37, 38));          // #252526
    p.setColor(QPalette::Light, QColor(62, 62, 66));         // #3e3e42
    p.setColor(QPalette::Midlight, QColor(51, 51, 51));
    p.setColor(QPalette::Shadow, QColor(0, 0, 0));
    p.setColor(QPalette::PlaceholderText, QColor(128, 128, 128));
    p.setColor(QPalette::Link, QColor(0, 122, 204));
    p.setColor(QPalette::LinkVisited, QColor(0, 122, 204));

    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(92, 92, 92));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(92, 92, 92));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(92, 92, 92));
    return p;
}

QPalette lightPalette()
{
    QPalette p;
    // VS2022 light theme — symmetric swap of the dark palette above.
    // bg #1e1e1e -> #ffffff, text #d4d4d4 -> #1f1f1f, accent #007acc -> #005a9e,
    // borders #3c3c3c -> #d4d4d4, hover #3e3e42 -> #e8e8e8, sel #094771 -> #cce8ff
    p.setColor(QPalette::Window, QColor(0xff, 0xff, 0xff));    // #ffffff
    p.setColor(QPalette::WindowText, QColor(0x1f, 0x1f, 0x1f)); // #1f1f1f
    p.setColor(QPalette::Base, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::AlternateBase, QColor(0xf3, 0xf3, 0xf3));
    p.setColor(QPalette::ToolTipBase, QColor(0xf5, 0xf5, 0xf5));
    p.setColor(QPalette::ToolTipText, QColor(0x1f, 0x1f, 0x1f));
    p.setColor(QPalette::Text, QColor(0x1f, 0x1f, 0x1f));
    p.setColor(QPalette::Button, QColor(0xf5, 0xf5, 0xf5));
    p.setColor(QPalette::ButtonText, QColor(0x1f, 0x1f, 0x1f));
    p.setColor(QPalette::BrightText, QColor(0, 0, 0));
    p.setColor(QPalette::Highlight, QColor(0x00, 0x5a, 0x9e)); // #005a9e
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::Mid, QColor(0xd4, 0xd4, 0xd4));        // #d4d4d4 borders
    p.setColor(QPalette::Dark, QColor(0xc8, 0xc8, 0xc8));
    p.setColor(QPalette::Light, QColor(0xe8, 0xe8, 0xe8));      // hover
    p.setColor(QPalette::Midlight, QColor(0xee, 0xee, 0xee));
    p.setColor(QPalette::Shadow, QColor(0xa0, 0xa0, 0xa0));
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Link, QColor(0x00, 0x5a, 0x9e));
    p.setColor(QPalette::LinkVisited, QColor(0x00, 0x5a, 0x9e));

    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0xa0, 0xa0, 0xa0));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0xa0, 0xa0, 0xa0));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0xa0, 0xa0, 0xa0));
    return p;
}

/* ── Stylesheets ───────────────────────────────────────────────────────── */
//
// The two stylesheets are the same skeleton with a swapped color set. We keep
// them as separate functions (rather than a token-substitution map) because:
//   (1) the originals are already heavily commented and easy to diff against
//       the VS reference;
//   (2) Qt parses literal strings faster than runtime-built QString concats;
//   (3) light/dark have a couple of asymmetries (status bar fg, scrollbar
//       trough) that read more clearly inline than as conditionals.
//

QString darkStylesheet()
{
    return QStringLiteral(
        /* ── Top-level windows ──────────────────────────────── */
        // Without these explicit rules, newly opened QDialog instances
        // (NewQtClassWizard, KitManagerDialog, AboutDialog, plugin wizards)
        // fall back to a Fusion default white background even when the
        // qApp palette is dark, because the global stylesheet bypasses
        // the palette for un-named widgets.
        "QDialog {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "}"
        "QMainWindow {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "}"

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
        "QSplitter::handle:vertical { height: 1px; }");
}

QString lightStylesheet()
{
    // Color swap reference (dark -> light):
    //   bg     #1e1e1e -> #ffffff
    //   panel  #252526 -> #f3f3f3
    //   bar    #2d2d30 -> #f5f5f5
    //   text   #d4d4d4 -> #1f1f1f
    //   muted  #9d9d9d -> #6a6a6a
    //   accent #007acc -> #005a9e
    //   border #3c3c3c -> #d4d4d4
    //   hover  #3e3e40/#3e3e42 -> #e8e8e8
    //   sel    #094771 -> #cce8ff (selection text becomes #1f1f1f, not white)
    //   tree-hover #2a2d2e -> #eef0f2
    //   line-edit bg #3c3c3c -> #ffffff (with #d4d4d4 border)
    //   pressed #1b1b1c -> #d0d0d0
    //   sel-bg #264f78 -> #cce8ff
    //   scrollbar trough #1e1e1e -> #f5f5f5
    //   scrollbar handle #5a5a5a -> #c0c0c0 (hover #a0a0a0)
    //   tab unselected text #9d9d9d -> #6a6a6a
    return QStringLiteral(
        /* ── Top-level windows ──────────────────────────────── */
        "QDialog {"
        "  background-color: #ffffff;"
        "  color: #1f1f1f;"
        "}"
        "QMainWindow {"
        "  background-color: #ffffff;"
        "  color: #1f1f1f;"
        "}"

        /* ── Scrollbars ─────────────────────────────────────── */
        "QScrollBar:vertical {"
        "  background: #f5f5f5; width: 12px; margin: 0; }"
        "QScrollBar::handle:vertical {"
        "  background: #c0c0c0; min-height: 20px; border-radius: 3px; margin: 2px; }"
        "QScrollBar::handle:vertical:hover { background: #a0a0a0; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal {"
        "  background: #f5f5f5; height: 12px; margin: 0; }"
        "QScrollBar::handle:horizontal {"
        "  background: #c0c0c0; min-width: 20px; border-radius: 3px; margin: 2px; }"
        "QScrollBar::handle:horizontal:hover { background: #a0a0a0; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }"

        /* ── Menu bar ───────────────────────────────────────── */
        "QMenuBar {"
        "  background: #f5f5f5; color: #1f1f1f;"
        "  border-bottom: 1px solid #d4d4d4;"
        "  padding: 0; spacing: 0;"
        "}"
        "QMenuBar::item {"
        "  background: transparent; padding: 4px 8px; color: #1f1f1f;"
        "}"
        "QMenuBar::item:selected {"
        "  background: #e8e8e8;"
        "}"
        "QMenuBar::item:pressed {"
        "  background: #cce8ff;"
        "}"

        /* ── Menus (drop-down) ──────────────────────────────── */
        "QMenu {"
        "  background: #f3f3f3; color: #1f1f1f;"
        "  border: 1px solid #d4d4d4;"
        "  padding: 2px 0;"
        "}"
        "QMenu::item {"
        "  padding: 4px 28px 4px 24px;"
        "}"
        "QMenu::item:selected {"
        "  background: #cce8ff; color: #1f1f1f;"
        "}"
        "QMenu::separator {"
        "  height: 1px; background: #d4d4d4; margin: 2px 8px;"
        "}"
        "QMenu::icon {"
        "  padding-left: 4px;"
        "}"

        /* ── Toolbars (DockToolBar via object name) ─────────── */
        "QToolBar[objectName^=\"exdock-toolbar\"] {"
        "  background: #f5f5f5;"
        "  border-bottom: 1px solid #d4d4d4;"
        "  spacing: 1px;"
        "  padding: 1px 4px;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton {"
        "  background: transparent;"
        "  border: 1px solid transparent;"
        "  border-radius: 2px;"
        "  color: #1f1f1f;"
        "  padding: 2px 5px;"
        "  font-size: 12px;"
        "  min-height: 20px;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:hover {"
        "  background: #e8e8e8;"
        "  border: 1px solid #c0c0c0;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:pressed {"
        "  background: #d0d0d0;"
        "  border: 1px solid #005a9e;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:checked {"
        "  background: #cce8ff;"
        "  border: 1px solid #005a9e;"
        "}"
        "QToolBar[objectName^=\"exdock-toolbar\"] QToolButton:disabled {"
        "  color: #a0a0a0;"
        "}"

        /* ── Toolbar separators ─────────────────────────────── */
        "QToolBar[objectName^=\"exdock-toolbar\"]::separator {"
        "  width: 1px; background: #d4d4d4; margin: 3px 4px;"
        "}"

        /* ── Toolbar combo boxes (e.g. build config) ────────── */
        "QToolBar QComboBox {"
        "  background: #ffffff; color: #1f1f1f;"
        "  border: 1px solid #c0c0c0;"
        "  padding: 1px 6px; font-size: 12px;"
        "  border-radius: 0px; min-height: 20px;"
        "}"
        "QToolBar QComboBox:hover {"
        "  border: 1px solid #005a9e;"
        "}"
        "QToolBar QComboBox::drop-down {"
        "  border: none; width: 16px;"
        "}"
        "QToolBar QComboBox QAbstractItemView {"
        "  background: #ffffff; color: #1f1f1f;"
        "  selection-background-color: #cce8ff;"
        "  selection-color: #1f1f1f;"
        "  border: 1px solid #c0c0c0;"
        "}"

        /* ── Toolbar line edits (e.g. search) ───────────────── */
        "QToolBar QLineEdit {"
        "  background: #ffffff; color: #1f1f1f;"
        "  border: 1px solid #c0c0c0;"
        "  padding: 2px 4px; font-size: 12px;"
        "  selection-background-color: #cce8ff;"
        "  selection-color: #1f1f1f;"
        "}"
        "QToolBar QLineEdit:focus {"
        "  border: 1px solid #005a9e;"
        "}"

        /* ── Status bar ─────────────────────────────────────── */
        "QStatusBar {"
        "  background: #005a9e; color: white;"
        "  border: none; font-size: 12px;"
        "}"
        "QStatusBar::item { border: none; }"
        "QStatusBar QLabel { color: white; background: transparent; }"

        /* ── Tab widgets ────────────────────────────────────── */
        "QTabWidget::pane {"
        "  border: 1px solid #d4d4d4; background: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  background: #f5f5f5; color: #6a6a6a;"
        "  padding: 5px 12px;"
        "  border: none;"
        "  border-bottom: 2px solid transparent;"
        "}"
        "QTabBar::tab:selected {"
        "  color: #1f1f1f;"
        "  border-bottom: 2px solid #005a9e;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  color: #1f1f1f;"
        "  background: #e8e8e8;"
        "}"

        /* ── Tree / List / Table views ──────────────────────── */
        "QTreeView, QListView, QTableView {"
        "  background: #ffffff; color: #1f1f1f;"
        "  border: none;"
        "  selection-background-color: #cce8ff;"
        "  selection-color: #1f1f1f;"
        "  outline: none;"
        "}"
        "QTreeView::item:hover, QListView::item:hover {"
        "  background: #eef0f2;"
        "}"
        "QTreeView::item:selected, QListView::item:selected {"
        "  background: #cce8ff; color: #1f1f1f;"
        "}"
        "QTreeView::branch {"
        "  background: transparent;"
        "}"
        "QHeaderView::section {"
        "  background: #f5f5f5; color: #1f1f1f;"
        "  border: none; border-right: 1px solid #d4d4d4;"
        "  padding: 3px 6px; font-size: 11px;"
        "}"

        /* ── Group boxes / frames ───────────────────────────── */
        "QGroupBox {"
        "  color: #1f1f1f;"
        "  border: 1px solid #d4d4d4;"
        "  margin-top: 8px; padding-top: 8px;"
        "}"
        "QGroupBox::title {"
        "  subcontrol-origin: margin; left: 8px;"
        "}"

        /* ── Line edits ─────────────────────────────────────── */
        "QLineEdit {"
        "  background: #ffffff; color: #1f1f1f;"
        "  border: 1px solid #c0c0c0;"
        "  padding: 3px 6px;"
        "  selection-background-color: #cce8ff;"
        "  selection-color: #1f1f1f;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #005a9e;"
        "}"

        /* ── Push buttons ───────────────────────────────────── */
        "QPushButton {"
        "  background: #f5f5f5; color: #1f1f1f;"
        "  border: 1px solid #c0c0c0;"
        "  padding: 4px 14px;"
        "  min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "  background: #e8e8e8; border: 1px solid #005a9e;"
        "}"
        "QPushButton:pressed {"
        "  background: #d0d0d0;"
        "}"
        "QPushButton:disabled {"
        "  background: #f3f3f3; color: #a0a0a0; border: 1px solid #d4d4d4;"
        "}"

        /* ── Check/Radio boxes ──────────────────────────────── */
        "QCheckBox, QRadioButton {"
        "  color: #1f1f1f; spacing: 6px;"
        "}"
        "QCheckBox::indicator, QRadioButton::indicator {"
        "  width: 14px; height: 14px;"
        "}"

        /* ── Tooltips ───────────────────────────────────────── */
        "QToolTip {"
        "  background: #f5f5f5; color: #1f1f1f;"
        "  border: 1px solid #d4d4d4;"
        "  padding: 4px; font-size: 12px;"
        "}"

        /* ── Splitters ──────────────────────────────────────── */
        "QSplitter::handle {"
        "  background: #d4d4d4;"
        "}"
        "QSplitter::handle:hover {"
        "  background: #005a9e;"
        "}"
        "QSplitter::handle:horizontal { width: 1px; }"
        "QSplitter::handle:vertical { height: 1px; }");
}

} // namespace

/* ── ThemeManager ──────────────────────────────────────────────────────── */

ThemeManager::ThemeManager()
{
    QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
    m_dark = s.value(QString::fromLatin1(kThemeDarkKey), true).toBool();
    m_loaded = true;
}

ThemeManager &ThemeManager::instance()
{
    static ThemeManager s;
    return s;
}

QString ThemeManager::stylesheet(bool dark)
{
    return dark ? darkStylesheet() : lightStylesheet();
}

void ThemeManager::apply(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    app.setPalette(m_dark ? darkPalette() : lightPalette());
    app.setStyleSheet(stylesheet(m_dark));
}

void ThemeManager::setDark(bool dark)
{
    if (m_loaded && m_dark == dark) {
        return;
    }
    m_dark = dark;
    m_loaded = true;

    QSettings s(QString::fromLatin1(kOrg), QString::fromLatin1(kApp));
    s.setValue(QString::fromLatin1(kThemeDarkKey), m_dark);

    if (auto *qa = qobject_cast<QApplication *>(QApplication::instance())) {
        apply(*qa);
    }
    Q_EMIT themeChanged(m_dark);
}

/* ── Free helpers ──────────────────────────────────────────────────────── */

void applyTheme(QApplication &app, bool dark)
{
    auto &mgr = ThemeManager::instance();
    // Force the requested mode without firing themeChanged at startup. We set
    // the internal flag through setDark() only if it differs from persisted.
    if (mgr.isDark() != dark) {
        mgr.setDark(dark); // also persists + emits, which is fine on startup
    } else {
        mgr.apply(app);
    }
    Q_UNUSED(kThemeGroup);
}

void setDarkTheme(bool dark)
{
    ThemeManager::instance().setDark(dark);
}

} // namespace Exorcist
