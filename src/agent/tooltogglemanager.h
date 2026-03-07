#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QMap>

// ─────────────────────────────────────────────────────────────────────────────
// ToolToggleManager — enable/disable tools from the agent's toolbox.
//
// Persists per-tool toggle state via QSettings. When a tool is disabled,
// the agent won't see it in its available tools list.
// ─────────────────────────────────────────────────────────────────────────────

class ToolToggleManager : public QObject
{
    Q_OBJECT

public:
    explicit ToolToggleManager(QObject *parent = nullptr) : QObject(parent)
    {
        loadSettings();
    }

    bool isToolEnabled(const QString &toolName) const
    {
        return m_enabled.value(toolName, true);
    }

    void setToolEnabled(const QString &toolName, bool enabled)
    {
        m_enabled[toolName] = enabled;
        saveSettings();
        emit toolToggled(toolName, enabled);
    }

    QStringList enabledTools(const QStringList &allTools) const
    {
        QStringList result;
        for (const auto &t : allTools) {
            if (isToolEnabled(t))
                result << t;
        }
        return result;
    }

    QStringList disabledTools() const
    {
        QStringList result;
        for (auto it = m_enabled.begin(); it != m_enabled.end(); ++it) {
            if (!it.value())
                result << it.key();
        }
        return result;
    }

    void enableAll()
    {
        m_enabled.clear();
        saveSettings();
        emit allToolsChanged();
    }

signals:
    void toolToggled(const QString &toolName, bool enabled);
    void allToolsChanged();

private:
    void loadSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("ToolToggles"));
        for (const auto &key : settings.childKeys())
            m_enabled[key] = settings.value(key, true).toBool();
        settings.endGroup();
    }

    void saveSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("ToolToggles"));
        settings.remove(QString()); // clear group
        for (auto it = m_enabled.begin(); it != m_enabled.end(); ++it)
            settings.setValue(it.key(), it.value());
        settings.endGroup();
    }

    QMap<QString, bool> m_enabled;
};
