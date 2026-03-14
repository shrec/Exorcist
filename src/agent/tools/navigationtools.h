#pragma once

#include "../itool.h"

#include <functional>

// ── OpenFileTool ─────────────────────────────────────────────────────────────
// Opens a file in the editor, optionally at a specific line and column.

class OpenFileTool : public ITool
{
public:
    // opener(filePath, line, column) → success
    using FileOpener = std::function<bool(const QString &, int, int)>;

    explicit OpenFileTool(FileOpener opener);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    FileOpener m_opener;
};

// ── SwitchHeaderSourceTool ───────────────────────────────────────────────────
// Toggles between C/C++ header and source file (.h ↔ .cpp).

class SwitchHeaderSourceTool : public ITool
{
public:
    // switcher(currentFilePath) → corresponding file path (empty if not found)
    using HeaderSourceSwitcher = std::function<QString(const QString &)>;

    explicit SwitchHeaderSourceTool(HeaderSourceSwitcher switcher);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    static QString findCounterpart(const QString &path);

    HeaderSourceSwitcher m_switcher;
};
