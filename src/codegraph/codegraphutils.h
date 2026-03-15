#pragma once

#include <QString>
#include <QStringList>

// ── CodeGraph Utilities ──────────────────────────────────────────────────────
// Shared helpers used by multiple language indexers.

namespace CodeGraphUtils {

// Scan forward from startIdx looking for '{' and find its matching '}'.
// Returns the end line index, or -1 if no match (e.g. forward declaration).
int findBraceBody(const QStringList &lines, int startIdx, int lineCount);

} // namespace CodeGraphUtils
