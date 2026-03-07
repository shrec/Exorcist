#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// TrajectoryRecorder — records multi-step agent reasoning to JSON.
//
// Captures each turn of an agent conversation: user messages, tool calls,
// tool results, assistant responses. Can be exported and replayed.
// ─────────────────────────────────────────────────────────────────────────────

struct TrajectoryStep {
    enum class Type { UserMessage, AssistantMessage, ToolCall, ToolResult, SystemEvent };

    Type type;
    QDateTime timestamp;
    QString content;       // message text or tool call JSON
    QString toolName;      // for ToolCall/ToolResult
    QJsonObject metadata;  // additional data (token count, model, etc.)

    QJsonObject toJson() const {
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
};

class TrajectoryRecorder : public QObject
{
    Q_OBJECT

public:
    explicit TrajectoryRecorder(QObject *parent = nullptr)
        : QObject(parent) {}

    /// Start a new trajectory recording
    void startRecording(const QString &sessionId)
    {
        m_recording = true;
        m_sessionId = sessionId;
        m_steps.clear();
        m_startTime = QDateTime::currentDateTime();
    }

    /// Stop recording
    void stopRecording()
    {
        m_recording = false;
    }

    /// Check if recording
    bool isRecording() const { return m_recording; }

    /// Record a step
    void recordStep(TrajectoryStep::Type type,
                    const QString &content,
                    const QString &toolName = {},
                    const QJsonObject &metadata = {})
    {
        if (!m_recording) return;
        m_steps.append({type, QDateTime::currentDateTime(),
                        content, toolName, metadata});
        emit stepRecorded(m_steps.size());
    }

    /// Get step count
    int stepCount() const { return m_steps.size(); }

    /// Get a specific step
    TrajectoryStep step(int index) const
    {
        if (index >= 0 && index < m_steps.size())
            return m_steps[index];
        return {};
    }

    /// Export as JSON
    QJsonObject toJson() const
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

    /// Export as formatted JSON string
    QString toJsonString() const
    {
        return QString::fromUtf8(
            QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    }

    /// Get all steps
    QList<TrajectoryStep> steps() const { return m_steps; }

signals:
    void stepRecorded(int totalSteps);

private:
    bool m_recording = false;
    QString m_sessionId;
    QDateTime m_startTime;
    QList<TrajectoryStep> m_steps;
};
