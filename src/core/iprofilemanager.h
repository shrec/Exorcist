#pragma once

/// IProfileManager — Abstract interface for development environment profiles.
///
/// A profile is a named collection of plugins, settings, and UI layout
/// that configures the IDE for a specific workflow (C++ dev, Rust dev,
/// embedded, security research, etc.).
///
/// Only one profile is active at a time per workspace. Profile activation
/// loads the required plugins, deactivates the rest, and applies settings.
///
/// Profiles can be activated:
/// - Automatically via workspace file detection
/// - Manually via Command Palette
/// - By explicit user selection in settings
///
/// Plugins obtain this via IHostServices::profiles() or
/// ServiceRegistry under key "profileManager".

#include <QString>
#include <QStringList>

class IProfileManager
{
public:
    virtual ~IProfileManager() = default;

    // ── Profile discovery ───────────────────────────────────────────

    /// All known profile IDs (from profiles/ directory).
    virtual QStringList availableProfiles() const = 0;

    /// Human-readable name for a profile ID.
    virtual QString profileName(const QString &profileId) const = 0;

    /// Short description of what this profile is for.
    virtual QString profileDescription(const QString &profileId) const = 0;

    // ── Activation ──────────────────────────────────────────────────

    /// Currently active profile ID. Empty if none (lightweight mode).
    virtual QString activeProfile() const = 0;

    /// Activate a profile by ID. Loads its plugins, applies settings.
    /// Pass empty string to switch to lightweight/no-profile mode.
    virtual bool activateProfile(const QString &profileId) = 0;

    // ── Detection ───────────────────────────────────────────────────

    /// Detect the best profile for a workspace root path.
    /// Returns profile ID or empty string if no match.
    virtual QString detectProfile(const QString &workspacePath) const = 0;

    /// Whether auto-detection is enabled (user can override).
    virtual bool autoDetectEnabled() const = 0;

    /// Enable/disable auto-detection.
    virtual void setAutoDetectEnabled(bool enabled) = 0;
};
