#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

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
    explicit ToolToggleManager(QObject *parent = nullptr);

    bool isToolEnabled(const QString &toolName) const;
    void setToolEnabled(const QString &toolName, bool enabled);
    QStringList enabledTools(const QStringList &allTools) const;
    QStringList disabledTools() const;
    void enableAll();

signals:
    void toolToggled(const QString &toolName, bool enabled);
    void allToolsChanged();

private:
    void loadSettings();
    void saveSettings();

    QMap<QString, bool> m_enabled;
};
