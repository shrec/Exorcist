#include "dashboardjsbridge.h"

#include <QJsonDocument>

#ifdef EXORCIST_HAS_ULTRALIGHT
#include "../chat/ultralight/ultralightwidget.h"
#endif

#ifdef EXORCIST_HAS_ULTRALIGHT

DashboardJSBridge::DashboardJSBridge(exorcist::UltralightWidget *view,
                                      QObject *parent)
    : QObject(parent), m_view(view)
{
    m_view->registerJSCallback(QStringLiteral("openArtifact"),
        [this](const QJsonValue &v) {
            const auto obj = v.toObject();
            Q_EMIT openArtifactRequested(
                obj.value(QStringLiteral("path")).toString(),
                obj.value(QStringLiteral("type")).toString());
        });
}

#else

DashboardJSBridge::DashboardJSBridge(QObject *parent)
    : QObject(parent)
{
}

#endif

void DashboardJSBridge::pushEvent(const QString &typeName,
                                   const QString &missionId,
                                   qint64 timestamp,
                                   const QJsonObject &payload)
{
    QJsonObject envelope;
    envelope[QStringLiteral("type")]      = typeName;
    envelope[QStringLiteral("missionId")] = missionId;
    envelope[QStringLiteral("timestamp")] = timestamp;
    envelope[QStringLiteral("payload")]   = payload;

    const QByteArray json = QJsonDocument(envelope).toJson(QJsonDocument::Compact);
    const QString escaped = QString::fromUtf8(json)
        .replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
        .replace(QLatin1Char('\''), QStringLiteral("\\'"));

    eval(QStringLiteral("dashboardBridge.handleEvent('%1');").arg(escaped));
}

void DashboardJSBridge::clearDashboard()
{
    eval(QStringLiteral("dashboardBridge.clear();"));
}

void DashboardJSBridge::setTheme(const QJsonObject &themeTokens)
{
    const QByteArray json = QJsonDocument(themeTokens).toJson(QJsonDocument::Compact);
    const QString escaped = QString::fromUtf8(json)
        .replace(QLatin1Char('\\'), QStringLiteral("\\\\"))
        .replace(QLatin1Char('\''), QStringLiteral("\\'"));
    eval(QStringLiteral("dashboardBridge.setTheme('%1');").arg(escaped));
}

void DashboardJSBridge::eval(const QString &js)
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    if (m_view)
        m_view->evaluateScript(js);
#else
    Q_UNUSED(js);
#endif
}
