#pragma once

#include "core/iprofilemanager.h"
#include "profile/profilemanifest.h"

#include <QHash>
#include <QObject>

class PluginManager;

/// Concrete implementation of IProfileManager.
///
/// Loads profile.json files from the profiles/ directory, scores workspaces
/// against detection rules, and activates/deactivates plugin sets.
///
/// On profile switch:
/// 1. Deactivates plugins not in the new profile
/// 2. Activates plugins required by the new profile
/// 3. Applies settings overrides
/// 4. Applies dock visibility defaults
/// 5. Emits profileChanged()
class ProfileManager : public QObject, public IProfileManager
{
    Q_OBJECT

public:
    explicit ProfileManager(PluginManager *pluginMgr, QObject *parent = nullptr);

    // ── IProfileManager ─────────────────────────────────────────────

    QStringList availableProfiles() const override;
    QString profileName(const QString &profileId) const override;
    QString profileDescription(const QString &profileId) const override;

    QString activeProfile() const override;
    bool activateProfile(const QString &profileId) override;

    QString detectProfile(const QString &workspacePath) const override;
    bool autoDetectEnabled() const override;
    void setAutoDetectEnabled(bool enabled) override;

    // ── Extended API ────────────────────────────────────────────────

    /// Load all profile.json files from a directory.
    void loadProfilesFromDirectory(const QString &dirPath);

    /// Register a single profile manifest (for programmatic use).
    void registerProfile(const ProfileManifest &manifest);

    /// Get the full manifest for a profile.
    const ProfileManifest *manifest(const QString &profileId) const;

signals:
    /// Emitted when the active profile changes.
    void profileChanged(const QString &newProfileId,
                        const QString &oldProfileId);

private:
    int scoreWorkspace(const ProfileManifest &profile,
                       const QString &workspacePath) const;

    PluginManager *m_pluginMgr;
    QHash<QString, ProfileManifest> m_profiles;
    QString m_activeProfileId;
    bool m_autoDetect = true;
};
