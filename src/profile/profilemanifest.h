#pragma once

/// ProfileManifest — Data structures for profile.json files.
///
/// A profile.json defines a development environment template:
/// - Which plugins to activate
/// - File detection rules for auto-activation
/// - Default settings overrides
/// - UI layout preferences (dock visibility defaults)
///
/// Profiles are loaded from the profiles/ directory at IDE startup.
/// Each profile is a single JSON file, e.g., profiles/cpp-native.json.

#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

/// Rule for auto-detecting a workspace type.
struct ProfileDetectionRule
{
    enum Type {
        FileExists,      // workspace contains a file matching glob
        FileContent,     // a file's first line matches regex
        DirectoryExists, // workspace contains a directory
    };
    Type    type = FileExists;
    QString pattern;   // glob or regex depending on type
    int     weight = 1; // higher weight = stronger match
};

/// Which docks should be visible by default in this profile.
struct ProfileDockDefault
{
    QString dockId;
    bool    visible = false;
};

/// A settings override applied when this profile is active.
struct ProfileSetting
{
    QString  key;
    QVariant value;
};

/// The complete profile manifest, parsed from profile.json.
struct ProfileManifest
{
    // ── Identity ──────────────────────────────────────────────────────
    QString     id;           // "cpp-native"
    QString     name;         // "C++ Native Development"
    QString     description;  // "CMake/Ninja/Clang C++ projects"
    QString     icon;         // optional icon resource
    QString     category;     // "native", "web", "embedded", etc.

    // ── Plugin selection ──────────────────────────────────────────────
    /// Plugin IDs that this profile requires (activated on profile switch).
    QStringList requiredPlugins;

    /// Plugin IDs that are optional (user can enable/disable within profile).
    QStringList optionalPlugins;

    /// Plugin IDs that should be explicitly disabled in this profile.
    QStringList disabledPlugins;

    // ── Workspace detection ───────────────────────────────────────────
    /// Rules for auto-detecting this profile from workspace files.
    QList<ProfileDetectionRule> detectionRules;

    /// Minimum total weight from matched rules to auto-activate.
    int detectionThreshold = 1;

    // ── UI defaults ───────────────────────────────────────────────────
    /// Default dock visibility overrides.
    QList<ProfileDockDefault> dockDefaults;

    // ── Settings ──────────────────────────────────────────────────────
    /// Settings overrides applied when this profile is active.
    QList<ProfileSetting> settings;

    // ── Helpers ───────────────────────────────────────────────────────
    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }

    /// Parse from JSON object. Returns invalid manifest on failure.
    static ProfileManifest fromJson(const QJsonObject &obj);
};
