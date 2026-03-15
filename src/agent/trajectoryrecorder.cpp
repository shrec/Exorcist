#include "trajectoryrecorder.h"

#include <QJsonArray>
#include <QJsonDocument>

// ── TrajectoryStep ──────────────────────────────────────────────────────────

QJsonObject TrajectoryStep::toJson() const
{
    QJsonObject obj;
    switch (type) {
    case Type::UserMessage:      obj[QStringLiteral("type")] = QStringLiteral("user"); break;
    case Type::AssistantMessage: obj[QStringLiteral("type")] = QStringLiteral("assistant"); break;
    case Type::ToolCall:         obj[QStringLiteral("type")] = QStringLiteral("tool_call"); break;
    case Type::ToolResult:       obj[QStringLiteral("type")] = QStringLiteral("tool_result"); break;
    case Type::SystemEvent:      obj[QStringLiteral("type")] = QStringLiteral("system"); break;
    }
    obj[QStringLiteral("timestamp")] = timestamp.toString(Qt::ISODate);
    obj[QStringLiteral("content")] = content;
    if (!toolName.isEmpty())
        obj[QStringLiteral("tool")] = toolName;
    if (!metadata.isEmpty())
        obj[QStringLiteral("metadata")] = metadata;
    return obj;
}

// ── TrajectoryRecorder ──────────────────────────────────────────────────────

TrajectoryRecorder::TrajectoryRecorder(QObject *parent)
    : QObject(parent)
{
}

void TrajectoryRecorder::startRecording(const QString &sessionId)
{
    m_recording = true;
    m_sessionId = sessionId;
    m_steps.clear();
    m_startTime = QDateTime::currentDateTime();
}

void TrajectoryRecorder::stopRecording()
{
    m_recording = false;
}

void TrajectoryRecorder::recordStep(TrajectoryStep::Type type,
                                    const QString &content,
                                    const QString &toolName,
                                    const QJsonObject &metadata)
{
    if (!m_recording) return;
    m_steps.append({type, QDateTime::currentDateTime(),
                    content, toolName, metadata});
    emit stepRecorded(m_steps.size());
}

TrajectoryStep TrajectoryRecorder::step(int index) const
{
    if (index >= 0 && index < m_steps.size())
        return m_steps[index];
    return {};
}

QJsonObject TrajectoryRecorder::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("sessionId")] = m_sessionId;
    obj[QStringLiteral("startTime")] = m_startTime.toString(Qt::ISODate);
    obj[QStringLiteral("stepCount")] = m_steps.size();

    QJsonArray steps;
    for (const auto &s : m_steps)
        steps.append(s.toJson());
    obj[QStringLiteral("steps")] = steps;
    return obj;
}

QString TrajectoryRecorder::toJsonString() const
{
    return QString::fromUtf8(
        QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
}
