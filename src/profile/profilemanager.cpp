#include "profilemanager.h"

#include "pluginmanager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>

ProfileManager::ProfileManager(PluginManager *pluginMgr, QObject *parent)
    : QObject(parent)
    , m_pluginMgr(pluginMgr)
{
    QSettings settings;
    m_autoDetect = settings.value(QStringLiteral("profiles/autoDetect"), true).toBool();
}

// ── IProfileManager — Discovery ──────────────────────────────────────────────

QStringList ProfileManager::availableProfiles() const
{
    return m_profiles.keys();
}

QString ProfileManager::profileName(const QString &profileId) const
{
    auto it = m_profiles.constFind(profileId);
    return (it != m_profiles.constEnd()) ? it->name : QString();
}

QString ProfileManager::profileDescription(const QString &profileId) const
{
    auto it = m_profiles.constFind(profileId);
    return (it != m_profiles.constEnd()) ? it->description : QString();
}

// ── IProfileManager — Activation ─────────────────────────────────────────────

QString ProfileManager::activeProfile() const
{
    return m_activeProfileId;
}

bool ProfileManager::activateProfile(const QString &profileId)
{
    // Deactivate to lightweight mode
    if (profileId.isEmpty()) {
        const QString old = m_activeProfileId;
        m_activeProfileId.clear();
        emit profileChanged(QString(), old);
        return true;
    }

    auto it = m_profiles.constFind(profileId);
    if (it == m_profiles.constEnd())
        return false;

    const QString old = m_activeProfileId;
    m_activeProfileId = profileId;
    const auto &profile = *it;

    // Disable plugins explicitly excluded by this profile
    for (const QString &pluginId : profile.disabledPlugins) {
        if (m_pluginMgr)
            m_pluginMgr->setPluginDisabled(pluginId, true);
    }

    // Enable required plugins
    for (const QString &pluginId : profile.requiredPlugins) {
        if (m_pluginMgr)
            m_pluginMgr->setPluginDisabled(pluginId, false);
    }

    // Activate plugins by workspace detection events
    if (m_pluginMgr) {
        for (const QString &pluginId : profile.requiredPlugins)
            m_pluginMgr->activateByEvent(
                QStringLiteral("onProfile:") + profileId);
    }

    // Persist active profile
    QSettings settings;
    settings.setValue(QStringLiteral("profiles/active"), profileId);

    emit profileChanged(profileId, old);
    return true;
}

// ── IProfileManager — Detection ──────────────────────────────────────────────

QString ProfileManager::detectProfile(const QString &workspacePath) const
{
    if (workspacePath.isEmpty())
        return {};

    QString bestId;
    int bestScore = 0;

    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        const int score = scoreWorkspace(it.value(), workspacePath);
        if (score >= it->detectionThreshold && score > bestScore) {
            bestScore = score;
            bestId = it.key();
        }
    }

    return bestId;
}

bool ProfileManager::autoDetectEnabled() const
{
    return m_autoDetect;
}

void ProfileManager::setAutoDetectEnabled(bool enabled)
{
    m_autoDetect = enabled;
    QSettings settings;
    settings.setValue(QStringLiteral("profiles/autoDetect"), enabled);
}

// ── Extended API ─────────────────────────────────────────────────────────────

void ProfileManager::loadProfilesFromDirectory(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return;

    QDirIterator iter(dirPath, {QStringLiteral("*.json")}, QDir::Files);
    while (iter.hasNext()) {
        iter.next();
        QFile file(iter.filePath());
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QJsonParseError parseError;
        const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject())
            continue;

        auto manifest = ProfileManifest::fromJson(doc.object());
        if (manifest.isValid())
            m_profiles.insert(manifest.id, std::move(manifest));
    }
}

void ProfileManager::registerProfile(const ProfileManifest &manifest)
{
    if (manifest.isValid())
        m_profiles.insert(manifest.id, manifest);
}

const ProfileManifest *ProfileManager::manifest(const QString &profileId) const
{
    auto it = m_profiles.constFind(profileId);
    return (it != m_profiles.constEnd()) ? &(*it) : nullptr;
}

// ── Private ──────────────────────────────────────────────────────────────────

int ProfileManager::scoreWorkspace(const ProfileManifest &profile,
                                   const QString &workspacePath) const
{
    int totalScore = 0;
    const QDir root(workspacePath);

    for (const auto &rule : profile.detectionRules) {
        switch (rule.type) {
        case ProfileDetectionRule::FileExists: {
            // Glob match against root directory files
            const QStringList matches = root.entryList(
                {rule.pattern}, QDir::Files | QDir::Dirs);
            if (!matches.isEmpty())
                totalScore += rule.weight;
            break;
        }
        case ProfileDetectionRule::DirectoryExists: {
            if (root.exists(rule.pattern))
                totalScore += rule.weight;
            break;
        }
        case ProfileDetectionRule::FileContent: {
            // pattern = "filename:regex" — check first line of file
            const int colonIdx = rule.pattern.indexOf(QLatin1Char(':'));
            if (colonIdx < 0)
                break;
            const QString fileName = rule.pattern.left(colonIdx);
            const QString regex = rule.pattern.mid(colonIdx + 1);
            const QString filePath = root.filePath(fileName);
            QFile f(filePath);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                break;
            const QString firstLine = QString::fromUtf8(f.readLine(4096));
            const QRegularExpression re(regex);
            if (re.match(firstLine).hasMatch())
                totalScore += rule.weight;
            break;
        }
        }
    }

    return totalScore;
}
