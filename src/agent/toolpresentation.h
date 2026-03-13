#pragma once

#include <QString>

// ── ToolPresentation (core) ───────────────────────────────────────────────────
//
// Provider-independent tool display metadata.  Lives in the agent core layer
// so any surface (chat panel, inline chat, terminal chat, quick chat) can
// render tool invocations uniformly without depending on a specific UI module.
//
// The `present()` function maps a raw tool name (the string emitted by the
// AI model) to human-friendly presentation strings.

namespace ToolPresentation {

struct Info
{
    QString friendlyTitle;       // e.g. "Search Files"
    QString invocationMessage;   // present tense: "Searching files…"
    QString pastTenseMessage;    // past tense: "Searched files"
    QString icon;                // unicode icon character
};

/// Look up presentation metadata for a tool by name.
/// Returns sensible defaults for unknown tools.
Info present(const QString &toolName);

} // namespace ToolPresentation
