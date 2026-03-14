#pragma once

#include "../itool.h"

class IFileSystem;

// ── ApplyPatchTool ────────────────────────────────────────────────────────────
// Applies a text replacement (patch) to a file.
// The agent specifies the exact old text and new text.
// This is safer than full file writes because it verifies the old text exists.

class ApplyPatchTool : public ITool
{
public:
    explicit ApplyPatchTool(IFileSystem *fs);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    IFileSystem *m_fs;
};
