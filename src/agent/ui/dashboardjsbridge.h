#pragma once

#include <QObject>
#include <QJsonObject>
#include <QVector>

#ifdef EXORCIST_HAS_ULTRALIGHT
namespace exorcist { class UltralightWidget; }
#endif

// ── DashboardJSBridge — C++ ↔ JS bridge for agent dashboard ─────────────────
//
// Mirrors ChatJSBridge pattern. Pushes structured AgentUIEvents to the
// dashboard's JavaScript as JSON strings. Receives user actions (artifact
// clicks) back from JS.
//
// Events pushed before the JS bridge is ready are queued and flushed
// automatically once the view reports DOM readiness.

class DashboardJSBridge : public QObject
{
    Q_OBJECT
public:
#ifdef EXORCIST_HAS_ULTRALIGHT
    explicit DashboardJSBridge(exorcist::UltralightWidget *view,
                                QObject *parent = nullptr);
#else
    explicit DashboardJSBridge(QObject *parent = nullptr);
#endif

    // ── C++ → JS ─────────────────────────────────────────────────────────

    /// Push a structured event to the JS dashboard renderer.
    void pushEvent(const QString &typeName,
                   const QString &missionId,
                   qint64 timestamp,
                   const QJsonObject &payload);

    /// Clear the entire dashboard.
    void clearDashboard();

    /// Update theme CSS custom properties.
    void setTheme(const QJsonObject &themeTokens);

signals:
    // ── JS → C++ ─────────────────────────────────────────────────────────
    void openArtifactRequested(const QString &path, const QString &type);

private:
    void eval(const QString &js);

#ifdef EXORCIST_HAS_ULTRALIGHT
    exorcist::UltralightWidget *m_view = nullptr;
#endif
};
