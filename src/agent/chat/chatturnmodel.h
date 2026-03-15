#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUuid>

#include "chatcontentpart.h"

struct ChatTurnModel
{
    // ── Identity ──────────────────────────────────────────────────────────
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QDateTime timestamp = QDateTime::currentDateTime();

    // ── Request side ──────────────────────────────────────────────────────
    QString userMessage;
    QString slashCommand;
    QStringList attachmentNames;
    QString modelId;
    QString providerId;
    int     mode = 0;

    // ── Context references ────────────────────────────────────────────────
    struct TurnReference {
        QString label;
        QString tooltip;
        QString filePath;
    };
    QList<TurnReference> references;

    // ── Response side ─────────────────────────────────────────────────────
    QList<ChatContentPart> parts;

    // ── State ─────────────────────────────────────────────────────────────
    enum class State { Streaming, Complete, Error, Cancelled };
    State state = State::Streaming;

    // ── Feedback ──────────────────────────────────────────────────────────
    enum class Feedback { None, Helpful, Unhelpful };
    Feedback feedback = Feedback::None;

    // ── Helpers ───────────────────────────────────────────────────────────
    void appendPart(const ChatContentPart &part);
    void appendMarkdownDelta(const QString &delta);
    void appendThinkingDelta(const QString &delta);
    QString fullMarkdownText() const;
    QString fullThinkingText() const;
    ChatContentPart *findToolPart(const QString &callId);

    // ── Serialization ─────────────────────────────────────────────────────
    QJsonObject toJson() const;
    static ChatTurnModel fromJson(const QJsonObject &o);
};
