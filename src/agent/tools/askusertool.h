#pragma once

#include "../itool.h"

#include <functional>

// ── AskUserTool ─────────────────────────────────────────────────────────────
// Lets the agent pause and ask the user a question mid-task. Critical for
// agentic loops where the agent needs clarification, confirmation before
// destructive actions, or a choice between approaches.
//
// The host shows a dialog/inline prompt and blocks until the user responds.

class AskUserTool : public ITool
{
public:
    struct UserResponse {
        bool    answered;      // false if user dismissed/cancelled
        QString response;      // free text or selected option label
        int     selectedIndex; // -1 if free text, otherwise 0-based option index
    };

    // prompter(question, options, allowFreeText) → UserResponse
    //   question: the question to display
    //   options: list of choices (empty = free text only)
    //   allowFreeText: whether to show a text input alongside options
    using UserPrompter = std::function<UserResponse(
        const QString &question,
        const QStringList &options,
        bool allowFreeText)>;

    explicit AskUserTool(UserPrompter prompter);

    ToolSpec spec() const override;
    ToolExecResult invoke(const QJsonObject &args) override;

private:
    UserPrompter m_prompter;
};
