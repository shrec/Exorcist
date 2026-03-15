#include "tooltogglemanager.h"

#include <QSettings>

ToolToggleManager::ToolToggleManager(QObject *parent) : QObject(parent)
{
    loadSettings();
}

bool ToolToggleManager::isToolEnabled(const QString &toolName) const
{
    return m_enabled.value(toolName, true);
}

void ToolToggleManager::setToolEnabled(const QString &toolName, bool enabled)
{
    m_enabled[toolName] = enabled;
    saveSettings();
    emit toolToggled(toolName, enabled);
}

QStringList ToolToggleManager::enabledTools(const QStringList &allTools) const
{
    QStringList result;
    for (const auto &t : allTools) {
        if (isToolEnabled(t))
            result << t;
    }
    return result;
}

QStringList ToolToggleManager::disabledTools() const
{
    QStringList result;
    for (auto it = m_enabled.begin(); it != m_enabled.end(); ++it) {
        if (!it.value())
            result << it.key();
    }
    return result;
}

void ToolToggleManager::enableAll()
{
    m_enabled.clear();
    saveSettings();
    emit allToolsChanged();
}

void ToolToggleManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("ToolToggles"));
    for (const auto &key : settings.childKeys())
        m_enabled[key] = settings.value(key, true).toBool();
    settings.endGroup();
}

void ToolToggleManager::saveSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("ToolToggles"));
    settings.remove(QString());
    for (auto it = m_enabled.begin(); it != m_enabled.end(); ++it)
        settings.setValue(it.key(), it.value());
    settings.endGroup();
}
