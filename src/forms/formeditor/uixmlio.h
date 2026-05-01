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

#include <QList>
#include <QString>
#include <QWidget>

namespace exo::forms {

class UiXmlIO {
public:
    // Load a .ui file and return a live widget tree.  Caller owns the result.
    // Returns nullptr on parse failure; an error message is written to qWarning().
    static QWidget *load(const QString &path);

    // Serialize a live widget tree to disk in Qt Designer .ui XML format.
    // root must be the top-level form widget (the one that hosts all children).
    // Returns true on success.
    static bool save(const QString &path, QWidget *root,
                     const QList<FormConnection> &connections = {});

    // Phase 2: parse a .ui file's <connections> block into a list.  Returns
    // an empty list if the file has no connections section or fails to open.
    static QList<FormConnection> loadConnections(const QString &path);
};

} // namespace exo::forms
