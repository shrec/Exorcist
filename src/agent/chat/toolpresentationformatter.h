#pragma once

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

} // namespace ToolPresentationFormatter
