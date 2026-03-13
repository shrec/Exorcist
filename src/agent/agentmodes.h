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

QString systemPrompt(int modeIndex);
AgentToolPermission maxPermission(int modeIndex);
bool usesAgentLoop(int modeIndex);
QString systemPromptForMode(int modeIndex);
AgentToolPermission maxPermissionForMode(int modeIndex);

} // namespace AgentModes
