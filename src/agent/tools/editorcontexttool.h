#pragma once

#include "../itool.h"

#include <functional>

// ── EditorContextTool ───────────────────────────────────────────────────────
// Gives the agent awareness of what the user is currently looking at.
// Instead of guessing or asking, the agent can read the active file,
// selection, cursor position, visible range, and open tabs.

class EditorContextTool : public ITool
{
public:
    struct EditorState {
        QString activeFilePath;      // full path of focused editor
        int     cursorLine;          // 1-based
        int     cursorColumn;        // 1-based
        QString selectedText;        // empty if no selection
        int     selectionStartLine;  // 0 if no selection
        int     selectionEndLine;    // 0 if no selection
        int     visibleStartLine;    // first visible line in viewport
        int     visibleEndLine;      // last visible line in viewport
        int     totalLines;          // total lines in document
        QStringList openFiles;       // all open tab file paths
        QString language;            // detected language (e.g. "cpp", "python")
        bool    isModified;          // unsaved changes
    };

    // getter(query) → EditorState
    //   query is unused for now but reserved for future filtered queries
    using EditorStateGetter = std::function<EditorState()>;

    explicit EditorContextTool(EditorStateGetter getter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    EditorStateGetter m_getter;
};
