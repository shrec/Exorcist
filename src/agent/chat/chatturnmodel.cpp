#include "chatturnmodel.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

void ChatTurnModel::appendPart(const ChatContentPart &part)
{
    parts.append(part);
}

void ChatTurnModel::appendMarkdownDelta(const QString &delta)
{
    for (int i = parts.size() - 1; i >= 0; --i) {
        if (parts[i].type == ChatContentPart::Type::Markdown) {
            parts[i].markdownText += delta;
            return;
        }
    }
    parts.append(ChatContentPart::markdown(delta));
}

void ChatTurnModel::appendThinkingDelta(const QString &delta)
{
    for (int i = parts.size() - 1; i >= 0; --i) {
        if (parts[i].type == ChatContentPart::Type::Thinking) {
            parts[i].thinkingText += delta;
            return;
        }
    }
    parts.append(ChatContentPart::thinking(delta));
}

QString ChatTurnModel::fullMarkdownText() const
{
    QString result;
    for (const auto &p : parts) {
        if (p.type == ChatContentPart::Type::Markdown)
            result += p.markdownText;
    }
    return result;
}

QString ChatTurnModel::fullThinkingText() const
{
    QString result;
    for (const auto &p : parts) {
        if (p.type == ChatContentPart::Type::Thinking)
            result += p.thinkingText;
    }
    return result;
}

ChatContentPart *ChatTurnModel::findToolPart(const QString &callId)
{
    for (auto &p : parts) {
        if (p.type == ChatContentPart::Type::ToolInvocation
            && p.toolCallId == callId)
            return &p;
    }
    return nullptr;
}

// ── Serialization ─────────────────────────────────────────────────────────────

QJsonObject ChatTurnModel::toJson() const
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

    if (!references.isEmpty()) {
        QJsonArray refsArr;
        for (const auto &ref : references) {
            QJsonObject r;
            r[QLatin1String("label")]   = ref.label;
            r[QLatin1String("tooltip")] = ref.tooltip;
            if (!ref.filePath.isEmpty())
                r[QLatin1String("filePath")] = ref.filePath;
            refsArr.append(r);
        }
        o[QLatin1String("references")] = refsArr;
    }

    QJsonArray partsArr;
    for (const auto &p : parts)
        partsArr.append(p.toJson());
    o[QLatin1String("parts")] = partsArr;

    return o;
}

ChatTurnModel ChatTurnModel::fromJson(const QJsonObject &o)
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

    const QJsonArray refsArr = o.value(QLatin1String("references")).toArray();
    for (const auto &v : refsArr) {
        const QJsonObject r = v.toObject();
        TurnReference ref;
        ref.label    = r.value(QLatin1String("label")).toString();
        ref.tooltip  = r.value(QLatin1String("tooltip")).toString();
        ref.filePath = r.value(QLatin1String("filePath")).toString();
        t.references.append(ref);
    }

    const QJsonArray partsArr = o.value(QLatin1String("parts")).toArray();
    for (const auto &v : partsArr)
        t.parts.append(ChatContentPart::fromJson(v.toObject()));

    return t;
}
