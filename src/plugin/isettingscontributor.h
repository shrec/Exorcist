#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariant>

// ── ISettingsContributor ──────────────────────────────────────────────────────
//
// Plugins implement this to react to settings changes and provide
// custom settings validation/UI beyond what SettingContribution declares.
//
// Static settings (key, type, default) are declared in the manifest.
// The IDE persists them in QSettings under "plugins/<pluginId>/".
// This interface provides dynamic behavior.

class ISettingsContributor
{
public:
    virtual ~ISettingsContributor() = default;

    /// Called when a setting value changes.
    virtual void settingChanged(const QString &key, const QVariant &newValue) = 0;

    /// Validate a setting before it's applied. Return empty string if valid,
    /// or an error message if invalid.
    virtual QString validateSetting(const QString &key,
                                    const QVariant &value) const
    {
        Q_UNUSED(key);
        Q_UNUSED(value);
        return {};
    }

    /// Return all current setting values for this plugin.
    /// Used for settings export/import.
    virtual QJsonObject currentSettings() const { return {}; }
};

#define EXORCIST_SETTINGS_CONTRIBUTOR_IID "org.exorcist.ISettingsContributor/1.0"
Q_DECLARE_INTERFACE(ISettingsContributor, EXORCIST_SETTINGS_CONTRIBUTOR_IID)
