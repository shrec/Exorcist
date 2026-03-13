#include "sshconnectionmanager.h"
#include "sshsession.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QUuid>

SshConnectionManager::SshConnectionManager(QObject *parent)
    : QObject(parent)
{
    loadProfiles();
}

SshConnectionManager::~SshConnectionManager()
{
    disconnectAll();
}

// ── Profile management ────────────────────────────────────────────────────────

void SshConnectionManager::addProfile(const SshProfile &profile)
{
    SshProfile p = profile;
    if (p.id.isEmpty())
        p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_profiles.append(p);
    saveProfiles();
    emit profilesChanged();
}

void SshConnectionManager::updateProfile(const SshProfile &profile)
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == profile.id) {
            m_profiles[i] = profile;
            saveProfiles();
            emit profilesChanged();
            return;
        }
    }
}

void SshConnectionManager::removeProfile(const QString &id)
{
    disconnect(id);
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == id) {
            m_profiles.removeAt(i);
            saveProfiles();
            emit profilesChanged();
            return;
        }
    }
}

SshProfile SshConnectionManager::profile(const QString &id) const
{
    for (const auto &p : m_profiles) {
        if (p.id == id)
            return p;
    }
    return {};
}

bool SshConnectionManager::loadProfiles()
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("ssh"));
    const QByteArray data = s.value(QStringLiteral("profiles")).toByteArray();
    s.endGroup();

    if (data.isEmpty())
        return false;

    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError)
        return false;

    m_profiles.clear();
    const auto arr = doc.array();
    for (const auto &v : arr)
        m_profiles.append(SshProfile::fromJson(v.toObject()));

    emit profilesChanged();
    return true;
}

bool SshConnectionManager::saveProfiles() const
{
    QJsonArray arr;
    for (const auto &p : m_profiles)
        arr.append(p.toJson());

    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("ssh"));
    s.setValue(QStringLiteral("profiles"),
              QJsonDocument(arr).toJson(QJsonDocument::Compact));
    s.endGroup();
    return true;
}

// ── Session management ────────────────────────────────────────────────────────

SshSession *SshConnectionManager::connect(const QString &profileId)
{
    if (m_sessions.contains(profileId))
        return m_sessions.value(profileId);

    const SshProfile prof = profile(profileId);
    if (prof.id.isEmpty())
        return nullptr;

    auto *session = new SshSession(prof, this);
    QObject::connect(session, &SshSession::connectionEstablished,
                     this, [this, profileId]() {
        emit connected(profileId);
    });
    QObject::connect(session, &SshSession::connectionLost,
                     this, [this, profileId](const QString &err) {
        m_sessions.remove(profileId);
        emit disconnected(profileId);
        if (!err.isEmpty())
            emit connectionError(profileId, err);
    });

    if (!session->connectToHost()) {
        const QString err = session->lastError();
        session->deleteLater();
        emit connectionError(profileId, err);
        return nullptr;
    }

    m_sessions.insert(profileId, session);
    return session;
}

SshSession *SshConnectionManager::activeSession(const QString &profileId) const
{
    return m_sessions.value(profileId, nullptr);
}

void SshConnectionManager::disconnect(const QString &profileId)
{
    if (auto *s = m_sessions.take(profileId)) {
        s->disconnect();
        s->disconnectFromHost();
        s->deleteLater();
        emit disconnected(profileId);
    }
}

void SshConnectionManager::disconnectAll()
{
    const auto ids = m_sessions.keys();
    for (const auto &id : ids)
        disconnect(id);
}

bool SshConnectionManager::isConnected(const QString &profileId) const
{
    auto *s = m_sessions.value(profileId, nullptr);
    return s && s->isConnected();
}
