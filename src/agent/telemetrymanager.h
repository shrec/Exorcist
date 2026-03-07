#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QMap>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// TelemetryManager — opt-in feature usage analytics.
//
// Stores events locally in memory (and optionally to disk).
// All telemetry is opt-in: collection is disabled by default.
// ─────────────────────────────────────────────────────────────────────────────

class TelemetryManager : public QObject
{
    Q_OBJECT

public:
    explicit TelemetryManager(QObject *parent = nullptr) : QObject(parent) {}

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool on) { m_enabled = on; }

    // Record a feature usage event
    void trackEvent(const QString &category, const QString &action,
                    const QJsonObject &properties = {})
    {
        if (!m_enabled) return;

        QJsonObject event;
        event[QStringLiteral("category")] = category;
        event[QStringLiteral("action")] = action;
        event[QStringLiteral("timestamp")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        if (!properties.isEmpty())
            event[QStringLiteral("properties")] = properties;

        m_events.append(event);
        m_counters[category + QStringLiteral("/") + action]++;
    }

    // Track message feedback (thumbs up/down)
    void trackFeedback(const QString &messageId, bool positive)
    {
        QJsonObject props;
        props[QStringLiteral("messageId")] = messageId;
        props[QStringLiteral("positive")] = positive;
        trackEvent(QStringLiteral("feedback"), positive ? QStringLiteral("thumbsUp") : QStringLiteral("thumbsDown"), props);
    }

    // Get all events as JSON
    QJsonArray events() const { return m_events; }

    // Get event counts
    QMap<QString, int> counters() const { return m_counters; }

    // Export events as JSON string
    QString exportJson() const
    {
        return QString::fromUtf8(QJsonDocument(m_events).toJson(QJsonDocument::Indented));
    }

    // Clear all events
    void clear() { m_events = QJsonArray(); m_counters.clear(); }

    // Privacy summary
    static QString privacyNotice()
    {
        return QStringLiteral(
            "Exorcist IDE collects anonymous usage analytics to improve the product.\n"
            "This includes: feature usage counts, error rates, and session lengths.\n"
            "No personal data, code content, or API keys are ever collected.\n\n"
            "You can opt out at any time in Settings → Privacy → Telemetry.");
    }

private:
    bool m_enabled = false; // opt-in: disabled by default
    QJsonArray m_events;
    QMap<QString, int> m_counters;
};
