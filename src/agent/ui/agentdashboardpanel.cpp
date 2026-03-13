#include "agentdashboardpanel.h"
#include "dashboardjsbridge.h"

#include <QVBoxLayout>
#include <QFile>

#ifdef EXORCIST_HAS_ULTRALIGHT
#include "../chat/ultralight/ultralightwidget.h"
#endif

#ifndef EXORCIST_HAS_ULTRALIGHT
#include <QLabel>
#include <QListWidget>
#endif

AgentDashboardPanel::AgentDashboardPanel(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

void AgentDashboardPanel::buildUi()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

#ifdef EXORCIST_HAS_ULTRALIGHT
    // ── Ultralight HTML path ─────────────────────────────────────────────
    m_ultralightView = new exorcist::UltralightWidget(this);
    m_ultralightView->setReadyProbe(QStringLiteral("dashboardBridge"));
    m_bridge = new DashboardJSBridge(m_ultralightView, this);

    connect(m_bridge, &DashboardJSBridge::openArtifactRequested,
            this, [this](const QString &path, const QString &) {
                Q_EMIT openFileRequested(path);
            });

    {
        auto loadRes = [](const QString &path) -> QString {
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) {
                qWarning("DashboardPanel: failed to load resource %s", qPrintable(path));
                return {};
            }
            return QString::fromUtf8(f.readAll());
        };

        const QString css  = loadRes(QStringLiteral(":/dashboard/dashboard.css"));
        const QString js   = loadRes(QStringLiteral(":/dashboard/dashboard.js"));
        QString html       = loadRes(QStringLiteral(":/dashboard/dashboard.html"));

        html.replace(QLatin1String("%STYLE%"), css);
        html.replace(QLatin1String("%SCRIPT%"), js);

        // loadHTML uses about:blank + JS injection (no temp files needed)
        m_ultralightView->loadHTML(html);
    }

    m_layout->addWidget(m_ultralightView);

#else
    // ── QWidget fallback path ────────────────────────────────────────────
    m_bridge = new DashboardJSBridge(this);

    setStyleSheet(QStringLiteral(
        "QWidget { background: #1e1e1e; color: #cccccc; }"
        "QLabel { padding: 4px 8px; }"
        "QListWidget { background: #252526; border: none; color: #cccccc; "
        "   font-family: 'Cascadia Code', Consolas, monospace; font-size: 12px; }"));

    m_titleLabel = new QLabel(tr("Agent Dashboard"), this);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "font-weight: bold; font-size: 14px; padding: 8px;"));
    m_layout->addWidget(m_titleLabel);

    m_statusLabel = new QLabel(tr("idle"), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "color: #666666; font-size: 11px; padding: 2px 8px;"));
    m_layout->addWidget(m_statusLabel);

    auto *stepsLabel = new QLabel(tr("Steps"), this);
    stepsLabel->setStyleSheet(QStringLiteral(
        "font-size: 11px; font-weight: bold; color: #999999; "
        "text-transform: uppercase; padding: 6px 8px 2px;"));
    m_layout->addWidget(stepsLabel);

    m_stepsList = new QListWidget(this);
    m_layout->addWidget(m_stepsList, 1);

    auto *logsLabel = new QLabel(tr("Activity Log"), this);
    logsLabel->setStyleSheet(QStringLiteral(
        "font-size: 11px; font-weight: bold; color: #999999; "
        "text-transform: uppercase; padding: 6px 8px 2px;"));
    m_layout->addWidget(logsLabel);

    m_logsList = new QListWidget(this);
    m_layout->addWidget(m_logsList, 2);
#endif
}

void AgentDashboardPanel::handleEvent(const AgentUIEvent &event)
{
    const QString typeName = eventTypeName(event.type);

#ifdef EXORCIST_HAS_ULTRALIGHT
    m_bridge->pushEvent(typeName, event.missionId, event.timestamp, event.payload);
#else
    // QWidget fallback — minimal rendering
    if (!m_stepsList || !m_logsList)
        return;
    const QString msg = event.payload.value(QStringLiteral("message")).toString();

    switch (event.type) {
    case AgentUIEventType::MissionCreated:
        m_titleLabel->setText(
            event.payload.value(QStringLiteral("title")).toString());
        m_statusLabel->setText(QStringLiteral("created"));
        m_stepsList->clear();
        m_logsList->clear();
        break;

    case AgentUIEventType::MissionUpdated:
        m_statusLabel->setText(
            event.payload.value(QStringLiteral("status")).toString());
        break;

    case AgentUIEventType::MissionCompleted:
        m_statusLabel->setText(
            event.payload.value(QStringLiteral("success")).toBool()
                ? QStringLiteral("completed") : QStringLiteral("failed"));
        break;

    case AgentUIEventType::StepAdded:
    {
        const QString stepId = event.payload.value(QStringLiteral("stepId")).toString();
        const QString label = event.payload.value(QStringLiteral("label")).toString();
        auto *item = new QListWidgetItem(QStringLiteral("[pending] %1").arg(label));
        item->setData(Qt::UserRole, stepId);
        item->setData(Qt::UserRole + 1, label);
        m_stepsList->addItem(item);
        break;
    }

    case AgentUIEventType::StepUpdated: {
        const QString stepId = event.payload.value(QStringLiteral("stepId")).toString();
        const QString status = event.payload.value(QStringLiteral("status")).toString();
        for (int i = 0; i < m_stepsList->count(); ++i) {
            auto *item = m_stepsList->item(i);
            if (item->data(Qt::UserRole).toString() == stepId) {
                const QString label = item->data(Qt::UserRole + 1).toString();
                const QString display = label.isEmpty() ? stepId : label;
                item->setText(QStringLiteral("[%1] %2").arg(status, display));
                break;
            }
        }
        break;
    }

    case AgentUIEventType::LogAdded:
        m_logsList->addItem(QStringLiteral("[%1] %2")
            .arg(event.payload.value(QStringLiteral("level")).toString(), msg));
        m_logsList->scrollToBottom();
        break;

    case AgentUIEventType::MetricUpdated:
    case AgentUIEventType::ArtifactAdded:
    case AgentUIEventType::PanelCreated:
    case AgentUIEventType::PanelUpdated:
    case AgentUIEventType::PanelRemoved:
    case AgentUIEventType::CustomEvent:
        break;
    }
#endif
}

void AgentDashboardPanel::clearDashboard()
{
#ifdef EXORCIST_HAS_ULTRALIGHT
    m_bridge->clearDashboard();
#else
    m_titleLabel->setText(tr("Agent Dashboard"));
    m_statusLabel->setText(QStringLiteral("idle"));
    m_stepsList->clear();
    m_logsList->clear();
#endif
}

bool AgentDashboardPanel::isActive() const
{
    return isVisible();
}

QString AgentDashboardPanel::eventTypeName(AgentUIEventType type) const
{
    switch (type) {
    case AgentUIEventType::MissionCreated:   return QStringLiteral("MissionCreated");
    case AgentUIEventType::MissionUpdated:   return QStringLiteral("MissionUpdated");
    case AgentUIEventType::MissionCompleted: return QStringLiteral("MissionCompleted");
    case AgentUIEventType::StepAdded:        return QStringLiteral("StepAdded");
    case AgentUIEventType::StepUpdated:      return QStringLiteral("StepUpdated");
    case AgentUIEventType::MetricUpdated:    return QStringLiteral("MetricUpdated");
    case AgentUIEventType::LogAdded:         return QStringLiteral("LogAdded");
    case AgentUIEventType::PanelCreated:     return QStringLiteral("PanelCreated");
    case AgentUIEventType::PanelUpdated:     return QStringLiteral("PanelUpdated");
    case AgentUIEventType::PanelRemoved:     return QStringLiteral("PanelRemoved");
    case AgentUIEventType::ArtifactAdded:    return QStringLiteral("ArtifactAdded");
    case AgentUIEventType::CustomEvent:      return QStringLiteral("CustomEvent");
    }
    return QStringLiteral("Unknown");
}
