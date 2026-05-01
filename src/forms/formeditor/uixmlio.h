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
// The serialization is intentionally a clean subset of Designer's format:
// it round-trips through QUiLoader, but we don't bother emitting the
// Designer-only tags (custom widgets database, tab order, slot connections).
// Phase 2 will add those.
#pragma once

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
    static bool save(const QString &path, QWidget *root);
};

} // namespace exo::forms
