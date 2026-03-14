#pragma once

#include "../itool.h"

// ── ClipboardTool ────────────────────────────────────────────────────────────
// Read from and write to the system clipboard.

class ClipboardTool : public ITool
{
public:
    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;
};
