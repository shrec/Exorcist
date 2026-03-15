#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
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
    explicit TelemetryManager(QObject *parent = nullptr);

    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool on) { m_enabled = on; }

    void trackEvent(const QString &category, const QString &action,
                    const QJsonObject &properties = {});
    void trackFeedback(const QString &messageId, bool positive);

    QJsonArray events() const { return m_events; }
    QMap<QString, int> counters() const { return m_counters; }

    QString exportJson() const;
    void clear();

    static QString privacyNotice();

private:
    bool m_enabled = false; // opt-in: disabled by default
    QJsonArray m_events;
    QMap<QString, int> m_counters;
};
