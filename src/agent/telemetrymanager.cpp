#include "telemetrymanager.h"

#include <QDateTime>
#include <QJsonDocument>

TelemetryManager::TelemetryManager(QObject *parent)
    : QObject(parent)
{
}

void TelemetryManager::trackEvent(const QString &category, const QString &action,
                                  const QJsonObject &properties)
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

void TelemetryManager::trackFeedback(const QString &messageId, bool positive)
{
    QJsonObject props;
    props[QStringLiteral("messageId")] = messageId;
    props[QStringLiteral("positive")] = positive;
    trackEvent(QStringLiteral("feedback"),
               positive ? QStringLiteral("thumbsUp") : QStringLiteral("thumbsDown"),
               props);
}

QString TelemetryManager::exportJson() const
{
    return QString::fromUtf8(QJsonDocument(m_events).toJson(QJsonDocument::Indented));
}

void TelemetryManager::clear()
{
    m_events = QJsonArray();
    m_counters.clear();
}

QString TelemetryManager::privacyNotice()
{
    return QStringLiteral(
        "Exorcist IDE collects anonymous usage analytics to improve the product.\n"
        "This includes: feature usage counts, error rates, and session lengths.\n"
        "No personal data, code content, or API keys are ever collected.\n\n"
        "You can opt out at any time in Settings → Privacy → Telemetry.");
}
