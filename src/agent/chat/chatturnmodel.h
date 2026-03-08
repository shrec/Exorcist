#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUuid>

#include "chatcontentpart.h"

// ── ChatTurnModel ─────────────────────────────────────────────────────────────
//
// Represents one request/response pair in the chat transcript.
// A turn starts with a user message and accumulates response content parts
// as the model streams back its answer.
//
// Mirrors VS Code ChatModel's request+response structure.

struct ChatTurnModel
{
    // ── Identity ──────────────────────────────────────────────────────────
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDateTime timestamp = QDateTime::currentDateTime();

    // ── Request side ──────────────────────────────────────────────────────
    QString userMessage;
    QString slashCommand;         // original slash command (e.g. "/explain"), empty if none
    QStringList attachmentNames;  // display names of attached files/selections
    QString modelId;              // which model answered this turn
    QString providerId;           // which provider
    int     mode = 0;             // 0=Ask, 1=Edit, 2=Agent

    // ── Response side ─────────────────────────────────────────────────────
    QList<ChatContentPart> parts; // ordered content parts

    // ── State ─────────────────────────────────────────────────────────────
    enum class State { Streaming, Complete, Error, Cancelled };
    State state = State::Streaming;

    // ── Feedback ──────────────────────────────────────────────────────────
    enum class Feedback { None, Helpful, Unhelpful };
    Feedback feedback = Feedback::None;

    // ── Helpers ───────────────────────────────────────────────────────────

    void appendPart(const ChatContentPart &part)
    {
        parts.append(part);
    }

    // Find the last markdown part and append text to it (for streaming).
    // Creates a new markdown part if none exists.
    void appendMarkdownDelta(const QString &delta)
    {
        for (int i = parts.size() - 1; i >= 0; --i) {
            if (parts[i].type == ChatContentPart::Type::Markdown) {
                parts[i].markdownText += delta;
                return;
            }
        }
        parts.append(ChatContentPart::markdown(delta));
    }

    // Find the last thinking part and append text to it (for streaming).
    void appendThinkingDelta(const QString &delta)
    {
        for (int i = parts.size() - 1; i >= 0; --i) {
            if (parts[i].type == ChatContentPart::Type::Thinking) {
                parts[i].thinkingText += delta;
                return;
            }
        }
        parts.append(ChatContentPart::thinking(delta));
    }

    // Get the full markdown text across all markdown parts.
    QString fullMarkdownText() const
    {
        QString result;
        for (const auto &p : parts) {
            if (p.type == ChatContentPart::Type::Markdown)
                result += p.markdownText;
        }
        return result;
    }

    // Get the full thinking text across all thinking parts.
    QString fullThinkingText() const
    {
        QString result;
        for (const auto &p : parts) {
            if (p.type == ChatContentPart::Type::Thinking)
                result += p.thinkingText;
        }
        return result;
    }

    // Find a tool invocation part by call ID.
    ChatContentPart *findToolPart(const QString &callId)
    {
        for (auto &p : parts) {
            if (p.type == ChatContentPart::Type::ToolInvocation
                && p.toolCallId == callId)
                return &p;
        }
        return nullptr;
    }

    // ── Serialization ─────────────────────────────────────────────────────

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QLatin1String("id")] = id;
        o[QLatin1String("ts")] = timestamp.toUTC().toString(Qt::ISODate);
        o[QLatin1String("userMessage")] = userMessage;
        if (!slashCommand.isEmpty())
            o[QLatin1String("slashCommand")] = slashCommand;
        if (!attachmentNames.isEmpty())
            o[QLatin1String("attachments")] = QJsonArray::fromStringList(attachmentNames);
        if (!modelId.isEmpty())
            o[QLatin1String("modelId")] = modelId;
        if (!providerId.isEmpty())
            o[QLatin1String("providerId")] = providerId;
        o[QLatin1String("mode")] = mode;
        o[QLatin1String("state")] = static_cast<int>(state);
        o[QLatin1String("feedback")] = static_cast<int>(feedback);

        QJsonArray partsArr;
        for (const auto &p : parts)
            partsArr.append(p.toJson());
        o[QLatin1String("parts")] = partsArr;

        return o;
    }

    static ChatTurnModel fromJson(const QJsonObject &o)
    {
        ChatTurnModel t;
        t.id = o.value(QLatin1String("id")).toString();
        const QString ts = o.value(QLatin1String("ts")).toString();
        t.timestamp = QDateTime::fromString(ts, Qt::ISODate);
        if (!t.timestamp.isValid())
            t.timestamp = QDateTime::currentDateTime();
        t.userMessage = o.value(QLatin1String("userMessage")).toString();
        t.slashCommand = o.value(QLatin1String("slashCommand")).toString();
        const QJsonArray attachArr = o.value(QLatin1String("attachments")).toArray();
        for (const auto &v : attachArr)
            t.attachmentNames.append(v.toString());
        t.modelId = o.value(QLatin1String("modelId")).toString();
        t.providerId = o.value(QLatin1String("providerId")).toString();
        t.mode = o.value(QLatin1String("mode")).toInt();
        t.state = static_cast<State>(
            o.value(QLatin1String("state")).toInt(static_cast<int>(State::Complete)));
        t.feedback = static_cast<Feedback>(
            o.value(QLatin1String("feedback")).toInt());

        const QJsonArray partsArr = o.value(QLatin1String("parts")).toArray();
        for (const auto &v : partsArr)
            t.parts.append(ChatContentPart::fromJson(v.toObject()));

        return t;
    }
};
