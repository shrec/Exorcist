#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVariant>

// ── ChatContentPart ───────────────────────────────────────────────────────────
//
// A tagged-union type representing one atomic piece of response content.
// Mirrors VS Code ChatResponsePart hierarchy:
//   Markdown | Thinking | ToolInvocation | WorkspaceEdit | Followup
//   | Confirmation | Warning | Error | Progress | FileTree | CommandButton
//
// Each variant holds just the data — rendering is done by the corresponding
// widget (ChatMarkdownWidget, ChatThinkingWidget, etc.)

struct ChatContentPart
{
    enum class Type
    {
        Markdown,
        Thinking,
        ToolInvocation,
        WorkspaceEdit,
        Followup,
        Confirmation,
        Warning,
        Error,
        Progress,
        FileTree,
        CommandButton,
    };

    Type type = Type::Markdown;

    // ── Markdown ──────────────────────────────────────────────────────────
    QString markdownText;         // raw Markdown source
    QString renderedHtml;         // pre-rendered HTML (optional cache)

    // ── Thinking ──────────────────────────────────────────────────────────
    QString thinkingText;         // reasoning content
    bool    thinkingCollapsed = false;

    // ── ToolInvocation ────────────────────────────────────────────────────
    enum class ToolState
    {
        Queued,
        Streaming,
        ConfirmationNeeded,
        CompleteSuccess,
        CompleteError,
    };

    QString   toolName;
    QString   toolCallId;
    QString   toolFriendlyTitle;   // human-readable title
    QString   toolInvocationMsg;   // present-tense message
    QString   toolPastTenseMsg;    // past-tense message
    QString   toolOriginMsg;       // secondary info
    QString   toolInput;           // JSON args or formatted input
    QString   toolOutput;          // result text
    ToolState toolState = ToolState::Queued;
    bool      toolCollapsedIO = true;

    // ── WorkspaceEdit ─────────────────────────────────────────────────────
    struct EditedFile {
        QString filePath;
        int     insertions = 0;
        int     deletions  = 0;
        QString diffPreview;       // optional unified diff excerpt
        enum class Action { Modified, Created, Deleted, Renamed };
        Action action = Action::Modified;
        enum class State { Pending, Applied, Kept, Undone };
        State state = State::Applied;
    };
    QList<EditedFile> editedFiles;

    // ── Followup ──────────────────────────────────────────────────────────
    struct FollowupItem {
        QString message;           // message to send on click
        QString label;             // display text
        QString tooltip;           // optional tooltip
        QString iconName;          // optional icon
    };
    QList<FollowupItem> followups;

    // ── Confirmation ──────────────────────────────────────────────────────
    QString confirmationTitle;
    QString confirmationMessage;
    // Result stored when user responds
    enum class ConfirmResult { Pending, Accepted, Rejected };
    ConfirmResult confirmResult = ConfirmResult::Pending;

    // ── Warning / Error ───────────────────────────────────────────────────
    QString warningText;
    QString errorText;

    // ── Progress ──────────────────────────────────────────────────────────
    QString progressLabel;

    // ── FileTree ──────────────────────────────────────────────────────────
    QStringList fileTreePaths;

    // ── CommandButton ─────────────────────────────────────────────────────
    QString buttonLabel;
    QString buttonCommand;

    // ── Factory helpers ───────────────────────────────────────────────────

    static ChatContentPart markdown(const QString &md)
    {
        ChatContentPart p;
        p.type = Type::Markdown;
        p.markdownText = md;
        return p;
    }

    static ChatContentPart thinking(const QString &text)
    {
        ChatContentPart p;
        p.type = Type::Thinking;
        p.thinkingText = text;
        return p;
    }

    static ChatContentPart toolInvocation(const QString &name,
                                          const QString &callId,
                                          const QString &friendlyTitle,
                                          const QString &invocationMsg,
                                          ToolState state = ToolState::Queued)
    {
        ChatContentPart p;
        p.type = Type::ToolInvocation;
        p.toolName = name;
        p.toolCallId = callId;
        p.toolFriendlyTitle = friendlyTitle;
        p.toolInvocationMsg = invocationMsg;
        p.toolState = state;
        return p;
    }

    static ChatContentPart workspaceEdit(const QList<EditedFile> &files)
    {
        ChatContentPart p;
        p.type = Type::WorkspaceEdit;
        p.editedFiles = files;
        return p;
    }

    static ChatContentPart followup(const QList<FollowupItem> &items)
    {
        ChatContentPart p;
        p.type = Type::Followup;
        p.followups = items;
        return p;
    }

    static ChatContentPart warning(const QString &text)
    {
        ChatContentPart p;
        p.type = Type::Warning;
        p.warningText = text;
        return p;
    }

    static ChatContentPart error(const QString &text)
    {
        ChatContentPart p;
        p.type = Type::Error;
        p.errorText = text;
        return p;
    }

    static ChatContentPart progress(const QString &label)
    {
        ChatContentPart p;
        p.type = Type::Progress;
        p.progressLabel = label;
        return p;
    }

    static ChatContentPart confirmation(const QString &title, const QString &msg)
    {
        ChatContentPart p;
        p.type = Type::Confirmation;
        p.confirmationTitle = title;
        p.confirmationMessage = msg;
        return p;
    }

    // ── Serialization ─────────────────────────────────────────────────────

    QJsonObject toJson() const
    {
        QJsonObject o;
        static const char *typeNames[] = {
            "markdown", "thinking", "toolInvocation", "workspaceEdit",
            "followup", "confirmation", "warning", "error",
            "progress", "fileTree", "commandButton"
        };
        o[QLatin1String("type")] = QLatin1String(typeNames[static_cast<int>(type)]);

        switch (type) {
        case Type::Markdown:
            o[QLatin1String("text")] = markdownText;
            break;
        case Type::Thinking:
            o[QLatin1String("text")] = thinkingText;
            o[QLatin1String("collapsed")] = thinkingCollapsed;
            break;
        case Type::ToolInvocation: {
            o[QLatin1String("name")] = toolName;
            o[QLatin1String("callId")] = toolCallId;
            o[QLatin1String("title")] = toolFriendlyTitle;
            o[QLatin1String("invocationMsg")] = toolInvocationMsg;
            o[QLatin1String("pastTenseMsg")] = toolPastTenseMsg;
            if (!toolInput.isEmpty())
                o[QLatin1String("input")] = toolInput;
            if (!toolOutput.isEmpty())
                o[QLatin1String("output")] = toolOutput;
            o[QLatin1String("state")] = static_cast<int>(toolState);
            break;
        }
        case Type::WorkspaceEdit: {
            QJsonArray files;
            for (const auto &ef : editedFiles) {
                QJsonObject fo;
                fo[QLatin1String("path")] = ef.filePath;
                fo[QLatin1String("ins")] = ef.insertions;
                fo[QLatin1String("del")] = ef.deletions;
                fo[QLatin1String("action")] = static_cast<int>(ef.action);
                fo[QLatin1String("state")] = static_cast<int>(ef.state);
                if (!ef.diffPreview.isEmpty())
                    fo[QLatin1String("diff")] = ef.diffPreview;
                files.append(fo);
            }
            o[QLatin1String("files")] = files;
            break;
        }
        case Type::Followup: {
            QJsonArray items;
            for (const auto &fu : followups) {
                QJsonObject fo;
                fo[QLatin1String("message")] = fu.message;
                fo[QLatin1String("label")] = fu.label;
                if (!fu.tooltip.isEmpty())
                    fo[QLatin1String("tooltip")] = fu.tooltip;
                items.append(fo);
            }
            o[QLatin1String("items")] = items;
            break;
        }
        case Type::Confirmation:
            o[QLatin1String("title")] = confirmationTitle;
            o[QLatin1String("message")] = confirmationMessage;
            o[QLatin1String("result")] = static_cast<int>(confirmResult);
            break;
        case Type::Warning:
            o[QLatin1String("text")] = warningText;
            break;
        case Type::Error:
            o[QLatin1String("text")] = errorText;
            break;
        case Type::Progress:
            o[QLatin1String("label")] = progressLabel;
            break;
        case Type::FileTree:
            o[QLatin1String("paths")] = QJsonArray::fromStringList(fileTreePaths);
            break;
        case Type::CommandButton:
            o[QLatin1String("label")] = buttonLabel;
            o[QLatin1String("command")] = buttonCommand;
            break;
        }
        return o;
    }

    static ChatContentPart fromJson(const QJsonObject &o)
    {
        ChatContentPart p;
        const QString t = o.value(QLatin1String("type")).toString();
        static const QHash<QString, Type> typeMap = {
            {QStringLiteral("markdown"),       Type::Markdown},
            {QStringLiteral("thinking"),       Type::Thinking},
            {QStringLiteral("toolInvocation"), Type::ToolInvocation},
            {QStringLiteral("workspaceEdit"),  Type::WorkspaceEdit},
            {QStringLiteral("followup"),       Type::Followup},
            {QStringLiteral("confirmation"),   Type::Confirmation},
            {QStringLiteral("warning"),        Type::Warning},
            {QStringLiteral("error"),          Type::Error},
            {QStringLiteral("progress"),       Type::Progress},
            {QStringLiteral("fileTree"),       Type::FileTree},
            {QStringLiteral("commandButton"),  Type::CommandButton},
        };
        p.type = typeMap.value(t, Type::Markdown);

        switch (p.type) {
        case Type::Markdown:
            p.markdownText = o.value(QLatin1String("text")).toString();
            break;
        case Type::Thinking:
            p.thinkingText = o.value(QLatin1String("text")).toString();
            p.thinkingCollapsed = o.value(QLatin1String("collapsed")).toBool(true);
            break;
        case Type::ToolInvocation:
            p.toolName = o.value(QLatin1String("name")).toString();
            p.toolCallId = o.value(QLatin1String("callId")).toString();
            p.toolFriendlyTitle = o.value(QLatin1String("title")).toString();
            p.toolInvocationMsg = o.value(QLatin1String("invocationMsg")).toString();
            p.toolPastTenseMsg = o.value(QLatin1String("pastTenseMsg")).toString();
            p.toolInput = o.value(QLatin1String("input")).toString();
            p.toolOutput = o.value(QLatin1String("output")).toString();
            p.toolState = static_cast<ToolState>(
                o.value(QLatin1String("state")).toInt(
                    static_cast<int>(ToolState::CompleteSuccess)));
            break;
        case Type::WorkspaceEdit: {
            const QJsonArray files = o.value(QLatin1String("files")).toArray();
            for (const auto &v : files) {
                const QJsonObject fo = v.toObject();
                EditedFile ef;
                ef.filePath = fo.value(QLatin1String("path")).toString();
                ef.insertions = fo.value(QLatin1String("ins")).toInt();
                ef.deletions = fo.value(QLatin1String("del")).toInt();
                ef.action = static_cast<EditedFile::Action>(
                    fo.value(QLatin1String("action")).toInt());
                ef.state = static_cast<EditedFile::State>(
                    fo.value(QLatin1String("state")).toInt());
                ef.diffPreview = fo.value(QLatin1String("diff")).toString();
                p.editedFiles.append(ef);
            }
            break;
        }
        case Type::Followup: {
            const QJsonArray items = o.value(QLatin1String("items")).toArray();
            for (const auto &v : items) {
                const QJsonObject fo = v.toObject();
                FollowupItem fu;
                fu.message = fo.value(QLatin1String("message")).toString();
                fu.label = fo.value(QLatin1String("label")).toString();
                fu.tooltip = fo.value(QLatin1String("tooltip")).toString();
                p.followups.append(fu);
            }
            break;
        }
        case Type::Confirmation:
            p.confirmationTitle = o.value(QLatin1String("title")).toString();
            p.confirmationMessage = o.value(QLatin1String("message")).toString();
            p.confirmResult = static_cast<ConfirmResult>(
                o.value(QLatin1String("result")).toInt());
            break;
        case Type::Warning:
            p.warningText = o.value(QLatin1String("text")).toString();
            break;
        case Type::Error:
            p.errorText = o.value(QLatin1String("text")).toString();
            break;
        case Type::Progress:
            p.progressLabel = o.value(QLatin1String("label")).toString();
            break;
        case Type::FileTree: {
            const QJsonArray arr = o.value(QLatin1String("paths")).toArray();
            for (const auto &v : arr)
                p.fileTreePaths.append(v.toString());
            break;
        }
        case Type::CommandButton:
            p.buttonLabel = o.value(QLatin1String("label")).toString();
            p.buttonCommand = o.value(QLatin1String("command")).toString();
            break;
        }
        return p;
    }
};
