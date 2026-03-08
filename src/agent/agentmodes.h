#pragma once

#include <QString>

#include "itool.h"

namespace AgentModes {

// Index 0
static const char * const AskPrompt =
    "You are a read-only coding assistant inside Exorcist IDE. "
    "Answer questions, explain code, suggest improvements. "
    "Use read_file and search_workspace freely. "
    "NEVER use write_file, replace_string_in_file, apply_patch, or run_command.";

// Index 1
static const char * const EditPrompt =
    "You are a focused code editor inside Exorcist IDE. "
    "Make targeted, minimal edits to the files the user explicitly mentions. "
    "Prefer replace_string_in_file over full rewrites. "
    "Always explain each change briefly before making it.";

// Index 2
static const char * const AgentPrompt =
    "You are an autonomous coding agent inside Exorcist IDE. "
    "Complete tasks end-to-end: explore code, plan changes, implement, verify. "
    "Use all available tools. Think step by step. "
    "Summarise what you did in the final response.";

inline QString systemPrompt(int modeIndex)
{
    switch (modeIndex) {
    case 0: return QString::fromUtf8(AskPrompt);
    case 1: return QString::fromUtf8(EditPrompt);
    case 2: return QString::fromUtf8(AgentPrompt);
    default: return {};
    }
}

inline AgentToolPermission maxPermission(int modeIndex)
{
    switch (modeIndex) {
    case 0: return AgentToolPermission::ReadOnly;
    case 1: return AgentToolPermission::SafeMutate;
    case 2: return AgentToolPermission::Dangerous;
    default: return AgentToolPermission::SafeMutate;
    }
}

inline bool usesAgentLoop(int modeIndex)
{
    return modeIndex == 2;
}

inline QString systemPromptForMode(int modeIndex)
{
    return systemPrompt(modeIndex);
}

inline AgentToolPermission maxPermissionForMode(int modeIndex)
{
    return maxPermission(modeIndex);
}

} // namespace AgentModes
