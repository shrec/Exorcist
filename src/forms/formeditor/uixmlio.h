// uixmlio.h — Qt .ui (Designer XML) file I/O for the custom form editor.
//
// Two top-level operations:
//
//   load(path)  →  parses a .ui file via QUiLoader (Qt6::UiTools) and returns a
//                  fully-instantiated QWidget tree that can be parented onto
//                  the form canvas.  We deliberately reuse QUiLoader for the
//                  parsing path because it understands every property tag,
//                  layout tag, container hint, and palette role that Designer
//                  emits.  Writing our own parser would be a multi-month
//                  project for zero user-visible gain.
//
//   save(path, root) → emits a Designer-compatible XML document via
//                      QXmlStreamWriter.  We walk the live widget tree, write
//                      a <widget> element per node, and only emit properties
//                      that differ from the Qt default (the inspector tracks
//                      "modified" properties — that's the source of truth).
//
// Phase 2 additions:
//   - The save() overload accepts a list of FormConnection records and emits
//     them in the standard <connections><connection>...</connection></...>
//     block.
//   - loadConnections() parses the <connections> block out of an existing
//     .ui file and returns the list — used by the editor on file load to
//     repopulate the SignalSlotEditor.
#pragma once

#include "signalsloteditor.h"   // FormConnection

#include <QHash>
#include <QList>
#include <QString>
#include <QWidget>

namespace exo::forms {

// Phase 3: A single <customwidget> entry as it appears in Designer's .ui XML.
//   <customwidget>
//     <class>MyButton</class>
//     <extends>QPushButton</extends>
//     <header>mybutton.h</header>
//     <header location="global">…</header>   ← optional location attr
//   </customwidget>
//
// We carry these around as a value type so the editor can map them onto its
// runtime promotion table (FormCanvas::PromotionInfo) and so the writer can
// dump them back out without hand-rolled XML.
struct UiCustomWidget {
    QString promotedName;       // <class>
    QString baseClass;          // <extends>
    QString headerFile;         // <header>
    bool    globalInclude = false;
};

// Per-widget promotion mapping keyed by objectName — the only widget
// identifier that round-trips reliably through .ui XML.  The save side
// uses this to rewrite <widget class="..."> attributes; the load side
// reconstructs FormCanvas's runtime PromotionInfo table from it.
using UiPromotionMap = QHash<QString, UiCustomWidget>;

class UiXmlIO {
public:
    // Load a .ui file and return a live widget tree.  Caller owns the result.
    // Returns nullptr on parse failure; an error message is written to qWarning().
    static QWidget *load(const QString &path);

    // Serialize a live widget tree to disk in Qt Designer .ui XML format.
    // root must be the top-level form widget (the one that hosts all children).
    // Returns true on success.  promotions, when non-empty, drives both the
    // <customwidgets> section and the <widget class="…"> rewriting (a widget
    // whose objectName is a key in promotions is emitted with its promoted
    // class name, not its runtime base class).
    static bool save(const QString &path, QWidget *root,
                     const QList<FormConnection> &connections = {},
                     const UiPromotionMap &promotions = {});

    // Phase 2: parse a .ui file's <connections> block into a list.  Returns
    // an empty list if the file has no connections section or fails to open.
    static QList<FormConnection> loadConnections(const QString &path);

    // Phase 3: parse a .ui file's <customwidgets> block.  The returned hash
    // is keyed by promoted class name (NOT by object name) — that's how the
    // .ui file represents promotions; the editor cross-references the live
    // widget tree to map class names back to specific objectNames.  Returns
    // an empty map if the file lacks the section.
    static QHash<QString, UiCustomWidget> loadCustomWidgets(const QString &path);

    // Phase 3: scan the .ui file's <widget> tree and return a map of
    // objectName → declared class attribute.  Used to identify which live
    // widgets in the loaded tree correspond to promoted entries (since
    // QUiLoader instantiates them as their base class when the custom DLL
    // isn't available, the live metaObject can't tell us).
    static QHash<QString, QString> loadWidgetClassByName(const QString &path);
};

} // namespace exo::forms
