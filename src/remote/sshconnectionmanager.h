#pragma once

#include <QObject>
#include <QList>
#include <QString>

#include "sshprofile.h"

class SshSession;

/// Manages SSH connection profiles and active sessions.
///
/// Profiles are persisted to `.exorcist/ssh_profiles.json` in the workspace.
/// Active sessions are tracked and cleaned up on shutdown.
class SshConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit SshConnectionManager(QObject *parent = nullptr);
    ~SshConnectionManager() override;

    // ── Profile management ────────────────────────────────────────────────

    const QList<SshProfile> &profiles() const { return m_profiles; }
    void addProfile(const SshProfile &profile);
    void updateProfile(const SshProfile &profile);
    void removeProfile(const QString &id);
    SshProfile profile(const QString &id) const;

    /// Load profiles from .exorcist/ssh_profiles.json.
    bool loadProfiles(const QString &workspaceRoot);

    /// Save profiles to .exorcist/ssh_profiles.json.
    bool saveProfiles() const;

    // ── Session management ────────────────────────────────────────────────

    /// Create and connect a new SSH session for the given profile.
    /// Ownership is retained by the manager. Returns nullptr on failure.
    SshSession *connect(const QString &profileId);

    /// Get an existing active session for a profile (or nullptr).
    SshSession *activeSession(const QString &profileId) const;

    /// Disconnect and destroy the session for a profile.
    void disconnect(const QString &profileId);

    /// Disconnect all active sessions.
    void disconnectAll();

    /// True if there is an active connected session for profileId.
    bool isConnected(const QString &profileId) const;

signals:
    void profilesChanged();
    void connected(const QString &profileId);
    void disconnected(const QString &profileId);
    void connectionError(const QString &profileId, const QString &error);

private:
    QList<SshProfile> m_profiles;
    QMap<QString, SshSession *> m_sessions; // profileId → SshSession
    QString m_workspaceRoot;
};
