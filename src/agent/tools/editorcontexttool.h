#pragma once

#include "../itool.h"

#include <QJsonArray>
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

    explicit EditorContextTool(EditorStateGetter getter)
        : m_getter(std::move(getter)) {}

    ToolSpec spec() const override
    {
        ToolSpec s;
        s.name        = QStringLiteral("get_editor_context");
        s.description = QStringLiteral(
            "Get the current state of the user's editor. Returns:\n"
            "  - Active file path and language\n"
            "  - Cursor position (line:column)\n"
            "  - Selected text and selection range\n"
            "  - Visible line range (what's on screen)\n"
            "  - List of all open tabs\n"
            "  - Whether file has unsaved changes\n\n"
            "Use this to understand what the user is working on before acting. "
            "Much better than asking the user which file they mean.");
        s.permission  = AgentToolPermission::ReadOnly;
        s.timeoutMs   = 5000;
        s.inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("includeSelection"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("Include selected text in response. Default: true. "
                                    "Set false if you only need cursor position.")}
                }},
                {QStringLiteral("includeOpenFiles"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("boolean")},
                    {QStringLiteral("description"),
                     QStringLiteral("Include list of all open tabs. Default: true.")}
                }}
            }},
            {QStringLiteral("required"), QJsonArray{}}
        };
        return s;
    }

    ToolExecResult invoke(const QJsonObject &args) override
    {
        const EditorState state = m_getter();

        if (state.activeFilePath.isEmpty())
            return {true, {}, QStringLiteral("No file is currently open."), {}};

        const bool includeSelection = args[QLatin1String("includeSelection")].toBool(true);
        const bool includeOpenFiles = args[QLatin1String("includeOpenFiles")].toBool(true);

        QJsonObject data;
        data[QLatin1String("activeFile")]  = state.activeFilePath;
        data[QLatin1String("language")]    = state.language;
        data[QLatin1String("cursorLine")]  = state.cursorLine;
        data[QLatin1String("cursorColumn")] = state.cursorColumn;
        data[QLatin1String("totalLines")]  = state.totalLines;
        data[QLatin1String("isModified")]  = state.isModified;
        data[QLatin1String("visibleStartLine")] = state.visibleStartLine;
        data[QLatin1String("visibleEndLine")]   = state.visibleEndLine;

        if (includeSelection && !state.selectedText.isEmpty()) {
            QString sel = state.selectedText;
            if (sel.size() > 10000) {
                sel.truncate(10000);
                sel += QStringLiteral("\n... (truncated)");
            }
            data[QLatin1String("selectedText")] = sel;
            data[QLatin1String("selectionStartLine")] = state.selectionStartLine;
            data[QLatin1String("selectionEndLine")]   = state.selectionEndLine;
        }

        if (includeOpenFiles && !state.openFiles.isEmpty()) {
            QJsonArray files;
            for (const QString &f : state.openFiles)
                files.append(f);
            data[QLatin1String("openFiles")] = files;
        }

        // Human-readable summary
        QString text = QStringLiteral("%1:%2:%3")
                            .arg(state.activeFilePath)
                            .arg(state.cursorLine)
                            .arg(state.cursorColumn);
        if (state.isModified)
            text += QStringLiteral(" [modified]");
        if (!state.selectedText.isEmpty())
            text += QStringLiteral(" [%1 chars selected, lines %2-%3]")
                        .arg(state.selectedText.size())
                        .arg(state.selectionStartLine)
                        .arg(state.selectionEndLine);
        text += QStringLiteral(" (%1, %2 lines)")
                    .arg(state.language)
                    .arg(state.totalLines);
        if (includeOpenFiles)
            text += QStringLiteral("\nOpen tabs: %1").arg(state.openFiles.size());

        return {true, data, text, {}};
    }

private:
    EditorStateGetter m_getter;
};
