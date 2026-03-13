#include "agentmodes.h"

namespace AgentModes {

QString systemPrompt(int modeIndex)
{
    switch (modeIndex) {
    case 0: return QString::fromUtf8(AskPrompt);
    case 1: return QString::fromUtf8(EditPrompt);
    case 2: return QString::fromUtf8(AgentPrompt);
    default: return {};
    }
}

AgentToolPermission maxPermission(int modeIndex)
{
    switch (modeIndex) {
    case 0: return AgentToolPermission::ReadOnly;
    case 1: return AgentToolPermission::SafeMutate;
    case 2: return AgentToolPermission::Dangerous;
    default: return AgentToolPermission::SafeMutate;
    }
}

bool usesAgentLoop(int modeIndex)
{
    return modeIndex == 2;
}

QString systemPromptForMode(int modeIndex)
{
    return systemPrompt(modeIndex);
}

AgentToolPermission maxPermissionForMode(int modeIndex)
{
    return maxPermission(modeIndex);
}

} // namespace AgentModes
