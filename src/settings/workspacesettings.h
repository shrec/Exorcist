#pragma once

#include <QFileSystemWatcher>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariant>

class QSettings;

/// Hierarchical settings manager: global (QSettings) → workspace (.exorcist/settings.json).
///
/// Workspace settings override global settings for any key present.
/// Settings are organized in groups (e.g. "editor", "indexer").
///
/// File format (.exorcist/settings.json):
/// {
///     "editor": { "tabSize": 2, "fontFamily": "Fira Code" },
///     "indexer": { "maxFileSizeKB": 512 }
/// }
class WorkspaceSettings : public QObject
{
    Q_OBJECT

public:
    explicit WorkspaceSettings(QObject *parent = nullptr);

    /// Set workspace root directory. Loads .exorcist/settings.json if present.
    void setWorkspaceRoot(const QString &root);

    /// Current workspace root (empty if none).
    QString workspaceRoot() const { return m_root; }

    /// Read a setting with group/key resolution: workspace → global → defaultValue.
    QVariant value(const QString &group, const QString &key,
                   const QVariant &defaultValue = {}) const;

    /// Write a setting to the workspace layer (.exorcist/settings.json).
    /// Creates .exorcist/ directory if needed.
    void setWorkspaceValue(const QString &group, const QString &key,
                           const QVariant &value);

    /// Remove a key from the workspace layer (reverts to global).
    void removeWorkspaceValue(const QString &group, const QString &key);

    /// Check if a workspace override exists for this group/key.
    bool hasWorkspaceOverride(const QString &group, const QString &key) const;

    /// Get the full workspace JSON (for inspection/export).
    QJsonObject workspaceJson() const { return m_wsJson; }

    /// Reload workspace settings from disk.
    void reload();

    // ── Convenience typed accessors ──────────────────────────────────────

    QString fontFamily() const;
    int     fontSize() const;
    int     tabSize() const;
    bool    wordWrap() const;
    bool    showLineNumbers() const;
    bool    showMinimap() const;

signals:
    /// Emitted when any workspace setting changes (load, set, remove, file change).
    void settingsChanged();

private:
    void loadWorkspaceFile();
    void saveWorkspaceFile() const;
    QString settingsFilePath() const;

    QString     m_root;
    QJsonObject m_wsJson;

    std::unique_ptr<QFileSystemWatcher> m_watcher;
};
