#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <memory>

/// ScopedSettings — QSettings-like API with tiered storage.
///
/// Two scopes:
///   - User      : persisted globally via QSettings("Exorcist","Exorcist").
///   - Workspace : persisted in <workspace_root>/.exorcist/settings.json.
///
/// Read order (highest priority first):
///   workspace JSON  →  user QSettings  →  caller-supplied default.
///
/// Keys are flat strings. Slashes ("editor/fontSize") are honored both ways:
///   * QSettings naturally treats "/" as group separator.
///   * Workspace JSON splits on the first "/" into a section + leaf
///     (e.g. "editor/fontSize" stores under {"editor":{"fontSize":...}}).
///     Keys without "/" live under a synthetic "_" section.
///
/// This class is additive: existing QSettings reads continue to work and
/// will see the same user-scope value, but they will NOT see workspace
/// overrides until the caller migrates to ScopedSettings::value().
class ScopedSettings : public QObject
{
    Q_OBJECT

public:
    enum Scope { User, Workspace };
    Q_ENUM(Scope)

    static ScopedSettings &instance();

    /// Set the workspace root. Pass an empty string to clear (no workspace).
    /// Loads <root>/.exorcist/settings.json if present.
    void setWorkspaceRoot(const QString &root);
    QString workspaceRoot() const { return m_workspaceRoot; }
    bool hasWorkspace() const { return !m_workspaceRoot.isEmpty(); }

    /// Absolute path to the workspace JSON file (empty if no workspace open).
    QString workspaceConfigFile() const;

    /// Read a setting. Workspace overlay first, then user QSettings, then default.
    QVariant value(const QString &key, const QVariant &defaultValue = {}) const;

    /// Write a setting to the chosen scope.
    /// Scope::Workspace requires hasWorkspace() — otherwise the write is ignored
    /// and a warning is logged.
    void setValue(const QString &key, const QVariant &val, Scope scope = User);

    /// True if the workspace JSON contains an override for this key.
    bool hasOverride(const QString &key) const;

    /// Remove a workspace override (caller falls back to user value).
    /// No-op if no workspace is open or the key isn't overridden.
    void removeWorkspaceOverride(const QString &key);

    /// Effective scope a value is currently coming from for this key.
    /// Returns Workspace if hasOverride(key), else User.
    Scope effectiveScope(const QString &key) const;

    /// All keys currently overridden in the workspace JSON (flat form).
    QStringList workspaceKeys() const;

signals:
    /// Emitted whenever any value changes (set, remove, external file edit, root change).
    /// Key is empty for bulk events (load / root change).
    void valueChanged(const QString &key);

private:
    explicit ScopedSettings(QObject *parent = nullptr);
    ~ScopedSettings() override = default;
    ScopedSettings(const ScopedSettings &) = delete;
    ScopedSettings &operator=(const ScopedSettings &) = delete;

    void loadWorkspaceFile();
    bool saveWorkspaceFileAtomic() const;
    void rewatch();

    // Split "editor/fontSize" → ("editor","fontSize"); "fontSize" → ("_","fontSize").
    static void splitKey(const QString &key, QString &section, QString &leaf);

    QString     m_workspaceRoot;
    QJsonObject m_workspaceJson;

    std::unique_ptr<QFileSystemWatcher> m_watcher;
};
