// widgetpalette.h — left-pane "stencil" of draggable Qt widgets.
//
// The palette is a search-filtered, categorised list.  Users either
//   (a) start a QDrag from a row → drop on the canvas, or
//   (b) double-click a row → palette emits widgetActivated() and the
//       canvas places the widget at its current cursor / centroid.
//
// Categories are emitted as non-selectable header rows (italic, slightly
// dimmer) and collapse all rows in their group when clicked.  This is a
// custom QListWidget subclass rather than a QTreeWidget on purpose: a tree
// adds a useless extra level of indentation for a UX where every entry is
// one click away.  The "headers as rows" pattern matches Figma's component
// panel and keeps the palette dense.
#pragma once

#include <QListWidget>
#include <QString>
#include <QStringList>

class QLineEdit;
class QListWidgetItem;
class QMouseEvent;

namespace exo::forms {

struct PaletteEntry {
    QString className;   // e.g. "QPushButton" — Qt class to instantiate
    QString label;       // e.g. "Push Button" — what we show the user
    QString category;    // grouping (Buttons / Inputs / Display / …)
};

// MIME type used by the drag-and-drop handshake between palette and canvas.
constexpr const char *kFormWidgetMime = "application/x-exorcist-form-widget";

class WidgetPalette : public QWidget {
    Q_OBJECT
public:
    explicit WidgetPalette(QWidget *parent = nullptr);

    // Populate with default widget set.  Called automatically by ctor; exposed
    // so plugins can re-populate later (Phase 2: custom widget registration).
    void rebuild(const QList<PaletteEntry> &entries);

signals:
    // Emitted on double-click when the user wants to insert the widget at
    // the canvas' current focus point rather than dragging it.
    void widgetActivated(const QString &className);

private:
    void buildUi();
    void applyFilter(const QString &needle);

    QLineEdit  *m_search   = nullptr;
    QListWidget *m_list    = nullptr;
    QList<PaletteEntry> m_entries;
};

} // namespace exo::forms
