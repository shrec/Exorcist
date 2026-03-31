#include "workspacesettings.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>

WorkspaceSettings::WorkspaceSettings(QObject *parent)
    : QObject(parent)
    , m_watcher(std::make_unique<QFileSystemWatcher>(this))
{
    connect(m_watcher.get(), &QFileSystemWatcher::fileChanged,
            this, [this](const QString &path) {
        if (path == settingsFilePath()) {
            reload();
            // Re-add the watch — some editors replace files (atomic save)
            if (!m_watcher->files().contains(path) && QFileInfo::exists(path))
                m_watcher->addPath(path);
        }
    });
}

void WorkspaceSettings::setWorkspaceRoot(const QString &root)
{
    // Remove old watch
    if (!m_root.isEmpty()) {
        const QString old = settingsFilePath();
        if (m_watcher->files().contains(old))
            m_watcher->removePath(old);
    }

    m_root = root;
    m_wsJson = {};
    m_globalCache.clear();

    if (!root.isEmpty())
        loadWorkspaceFile();

    emit settingsChanged();
}

QVariant WorkspaceSettings::value(const QString &group, const QString &key,
                                  const QVariant &defaultValue) const
{
    // Workspace layer takes priority (constFind avoids double-lookup)
    const auto git = m_wsJson.constFind(group);
    if (git != m_wsJson.constEnd()) {
        const QJsonObject grp = git->toObject();
        const auto kit = grp.constFind(key);
        if (kit != grp.constEnd())
            return kit->toVariant();
    }

    // Check in-memory cache of global QSettings
    const QString cacheKey = group + QLatin1Char('/') + key;
    const auto cit = m_globalCache.constFind(cacheKey);
    if (cit != m_globalCache.constEnd())
        return *cit;

    // Fall back to global QSettings (expensive — cache the result)
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(group);
    const QVariant v = s.value(key, defaultValue);
    s.endGroup();

    m_globalCache.insert(cacheKey, v);
    return v;
}

void WorkspaceSettings::setWorkspaceValue(const QString &group, const QString &key,
                                          const QVariant &value)
{
    QJsonObject grp = m_wsJson.value(group).toObject();
    grp.insert(key, QJsonValue::fromVariant(value));
    m_wsJson.insert(group, grp);
    m_globalCache.remove(group + QLatin1Char('/') + key);
    saveWorkspaceFile();
    emit settingsChanged();
}

void WorkspaceSettings::removeWorkspaceValue(const QString &group, const QString &key)
{
    if (!m_wsJson.contains(group))
        return;
    QJsonObject grp = m_wsJson.value(group).toObject();
    if (!grp.contains(key))
        return;
    grp.remove(key);
    if (grp.isEmpty())
        m_wsJson.remove(group);
    else
        m_wsJson.insert(group, grp);
    saveWorkspaceFile();
    emit settingsChanged();
}

bool WorkspaceSettings::hasWorkspaceOverride(const QString &group, const QString &key) const
{
    if (!m_wsJson.contains(group))
        return false;
    return m_wsJson.value(group).toObject().contains(key);
}

void WorkspaceSettings::reload()
{
    m_globalCache.clear();
    loadWorkspaceFile();
    emit settingsChanged();
}

// ── Convenience typed accessors ──────────────────────────────────────────────

QString WorkspaceSettings::fontFamily() const
{
    return value(QStringLiteral("editor"), QStringLiteral("fontFamily"),
                 QStringLiteral("Consolas")).toString();
}

int WorkspaceSettings::fontSize() const
{
    return value(QStringLiteral("editor"), QStringLiteral("fontSize"), 11).toInt();
}

int WorkspaceSettings::tabSize() const
{
    return value(QStringLiteral("editor"), QStringLiteral("tabSize"), 4).toInt();
}

bool WorkspaceSettings::wordWrap() const
{
    return value(QStringLiteral("editor"), QStringLiteral("wordWrap"), false).toBool();
}

bool WorkspaceSettings::showLineNumbers() const
{
    return value(QStringLiteral("editor"), QStringLiteral("showLineNumbers"), true).toBool();
}

bool WorkspaceSettings::showMinimap() const
{
    return value(QStringLiteral("editor"), QStringLiteral("showMinimap"), false).toBool();
}

// ── Private ──────────────────────────────────────────────────────────────────

void WorkspaceSettings::loadWorkspaceFile()
{
    const QString path = settingsFilePath();
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isObject())
        m_wsJson = doc.object();

    // Watch for external changes
    if (!m_watcher->files().contains(path))
        m_watcher->addPath(path);
}

void WorkspaceSettings::saveWorkspaceFile() const
{
    if (m_root.isEmpty())
        return;

    const QString dirPath = m_root + QStringLiteral("/.exorcist");
    QDir().mkpath(dirPath);

    const QString path = settingsFilePath();
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;

    file.write(QJsonDocument(m_wsJson).toJson(QJsonDocument::Indented));
}

QString WorkspaceSettings::settingsFilePath() const
{
    if (m_root.isEmpty())
        return {};
    return m_root + QStringLiteral("/.exorcist/settings.json");
}
