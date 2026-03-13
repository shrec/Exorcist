#pragma once

#include <QJsonObject>

#include "../toolpresentation.h"
#include "chatcontentpart.h"

// ── ToolPresentationFormatter ─────────────────────────────────────────────────
//
// Chat-layer convenience functions that bridge core ToolPresentation (provider-
// independent) with ChatContentPart (UI data model).
//
// Core lookup lives in src/agent/toolpresentation.h — use ToolPresentation::
// present() directly when you don't need ChatContentPart dependencies.

namespace ToolPresentationFormatter {

// Re-export core types for backward compat
using ToolPresentation = ::ToolPresentation::Info;

/// Lookup wrapper (delegates to core).
inline ToolPresentation present(const QString &toolName)
{
    return ::ToolPresentation::present(toolName);
}

/// Enrich a ChatContentPart's tool fields with presentation data.
/// Call this when creating a ToolInvocation content part.
inline void enrichToolPart(ChatContentPart &part)
{
    auto p = ::ToolPresentation::present(part.toolName);
    if (part.toolFriendlyTitle.isEmpty())
        part.toolFriendlyTitle = p.friendlyTitle;
    if (part.toolInvocationMsg.isEmpty())
        part.toolInvocationMsg = p.invocationMessage;
    if (part.toolPastTenseMsg.isEmpty())
        part.toolPastTenseMsg = p.pastTenseMessage;
}

/// Create a ChatContentPart from a tool execution result.
/// This is the standard tool-result → transcript-part transformation.
inline ChatContentPart toolResultToPart(const QString &toolName,
                                        const QString &toolCallId,
                                        bool success,
                                        const QString &output,
                                        const QString &input = {})
{
    auto p = ::ToolPresentation::present(toolName);
    auto state = success ? ChatContentPart::ToolState::CompleteSuccess
                         : ChatContentPart::ToolState::CompleteError;

    auto part = ChatContentPart::toolInvocation(
        toolName, toolCallId, p.friendlyTitle, p.invocationMessage, state);
    part.toolPastTenseMsg = p.pastTenseMessage;
    part.toolInput = input;
    part.toolOutput = output;
    return part;
}

/// Extract a human-readable description from tool arguments.
/// Returns empty string if no meaningful description can be built.
inline QString descriptionFromArgs(const QString &toolName, const QJsonObject &args)
{
    // run_in_terminal / run_command: prefer explanation, fall back to command
    if (toolName == QLatin1String("run_in_terminal")
        || toolName == QLatin1String("run_command")) {
        const QString expl = args.value(QLatin1String("explanation")).toString();
        if (!expl.isEmpty()) return expl;
        const QString cmd = args.value(QLatin1String("command")).toString();
        if (!cmd.isEmpty()) {
            const QString trimmed = cmd.length() > 80
                ? cmd.left(77) + QStringLiteral("\u2026") : cmd;
            return QStringLiteral("`%1`").arg(trimmed);
        }
    }

    // read_file
    if (toolName == QLatin1String("read_file")) {
        const QString fp = args.value(QLatin1String("filePath")).toString();
        if (!fp.isEmpty()) {
            // Show just the filename or last path component
            const int sep = fp.lastIndexOf(QLatin1Char('/'));
            const int bsep = fp.lastIndexOf(QLatin1Char('\\'));
            const int idx = qMax(sep, bsep);
            return idx >= 0 ? fp.mid(idx + 1) : fp;
        }
    }

    // create_file
    if (toolName == QLatin1String("create_file")) {
        const QString fp = args.value(QLatin1String("filePath")).toString();
        if (!fp.isEmpty()) {
            const int sep = fp.lastIndexOf(QLatin1Char('/'));
            const int bsep = fp.lastIndexOf(QLatin1Char('\\'));
            const int idx = qMax(sep, bsep);
            return idx >= 0 ? fp.mid(idx + 1) : fp;
        }
    }

    // edit_file / replace_string_in_file
    if (toolName == QLatin1String("replace_string_in_file")
        || toolName == QLatin1String("edit_file")) {
        const QString fp = args.value(QLatin1String("filePath")).toString();
        if (!fp.isEmpty()) {
            const int sep = fp.lastIndexOf(QLatin1Char('/'));
            const int bsep = fp.lastIndexOf(QLatin1Char('\\'));
            const int idx = qMax(sep, bsep);
            return idx >= 0 ? fp.mid(idx + 1) : fp;
        }
    }

    // grep_search / semantic_search
    if (toolName == QLatin1String("grep_search")
        || toolName == QLatin1String("semantic_search")
        || toolName == QLatin1String("codebase_search")) {
        const QString query = args.value(QLatin1String("query")).toString();
        if (!query.isEmpty()) {
            const QString trimmed = query.length() > 60
                ? query.left(57) + QStringLiteral("\u2026") : query;
            return QStringLiteral("\u201C%1\u201D").arg(trimmed);
        }
    }

    // file_search
    if (toolName == QLatin1String("file_search")) {
        const QString query = args.value(QLatin1String("query")).toString();
        if (!query.isEmpty()) return query;
    }

    // list_dir
    if (toolName == QLatin1String("list_dir")) {
        const QString path = args.value(QLatin1String("path")).toString();
        if (!path.isEmpty()) {
            const int sep = path.lastIndexOf(QLatin1Char('/'));
            const int bsep = path.lastIndexOf(QLatin1Char('\\'));
            const int idx = qMax(sep, bsep);
            return idx >= 0 ? path.mid(idx + 1) : path;
        }
    }

    // rename_file / copy_file / delete_file
    if (toolName == QLatin1String("rename_file")
        || toolName == QLatin1String("copy_file")
        || toolName == QLatin1String("delete_file")) {
        const QString fp = args.value(QLatin1String("filePath")).toString();
        if (!fp.isEmpty()) {
            const int sep = fp.lastIndexOf(QLatin1Char('/'));
            const int bsep = fp.lastIndexOf(QLatin1Char('\\'));
            const int idx = qMax(sep, bsep);
            return idx >= 0 ? fp.mid(idx + 1) : fp;
        }
    }

    // open_file
    if (toolName == QLatin1String("open_file")) {
        const QString fp = args.value(QLatin1String("filePath")).toString();
        if (!fp.isEmpty()) {
            const int sep = fp.lastIndexOf(QLatin1Char('/'));
            const int bsep = fp.lastIndexOf(QLatin1Char('\\'));
            const int idx = qMax(sep, bsep);
            return idx >= 0 ? fp.mid(idx + 1) : fp;
        }
    }

    // rename_symbol
    if (toolName == QLatin1String("rename_symbol")) {
        const QString name = args.value(QLatin1String("newName")).toString();
        if (!name.isEmpty()) return name;
    }

    // build_project
    if (toolName == QLatin1String("build_project")) {
        const QString target = args.value(QLatin1String("target")).toString();
        if (!target.isEmpty()) return target;
    }

    // run_lua_tool
    if (toolName == QLatin1String("run_lua_tool")) {
        const QString name = args.value(QLatin1String("name")).toString();
        if (!name.isEmpty()) return name;
    }

    // http_request
    if (toolName == QLatin1String("http_request")) {
        const QString url = args.value(QLatin1String("url")).toString();
        if (!url.isEmpty()) {
            const QString trimmed = url.length() > 60
                ? url.left(57) + QStringLiteral("\u2026") : url;
            return trimmed;
        }
    }

    // database_query
    if (toolName == QLatin1String("database_query")) {
        const QString query = args.value(QLatin1String("query")).toString();
        if (!query.isEmpty()) {
            const QString trimmed = query.length() > 60
                ? query.left(57) + QStringLiteral("\u2026") : query;
            return QStringLiteral("\u201C%1\u201D").arg(trimmed);
        }
    }

    // run_subagent
    if (toolName == QLatin1String("run_subagent")) {
        const QString desc = args.value(QLatin1String("description")).toString();
        if (!desc.isEmpty()) return desc;
    }

    // github_issues / github_pr
    if (toolName == QLatin1String("github_issues")
        || toolName == QLatin1String("github_pr")) {
        const QString repo = args.value(QLatin1String("repo")).toString();
        if (!repo.isEmpty()) return repo;
    }

    return {};
}

} // namespace ToolPresentationFormatter
