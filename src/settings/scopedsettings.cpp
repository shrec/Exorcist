#include "scopedsettings.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QSaveFile>
#include <QSettings>

namespace {
constexpr auto kFlatSection = "_";   // Synthetic section for keys without "/"
constexpr auto kCfgDir      = "/.exorcist";
constexpr auto kCfgFile     = "/.exorcist/settings.json";
} // namespace

ScopedSettings &ScopedSettings::instance()
{
    static ScopedSettings inst;
    return inst;
}

ScopedSettings::ScopedSettings(QObject *parent)
    : QObject(parent)
    , m_watcher(std::make_unique<QFileSystemWatcher>(this))
{
    connect(m_watcher.get(), &QFileSystemWatcher::fileChanged, this,
            [this](const QString &path) {
                if (path == workspaceConfigFile()) {
                    loadWorkspaceFile();
                    rewatch();
                    emit valueChanged({}); // bulk
                }
            });
}

void ScopedSettings::splitKey(const QString &key, QString &section, QString &leaf)
{
    const int slash = key.indexOf(QLatin1Char('/'));
    if (slash <= 0) {
        section = QString::fromLatin1(kFlatSection);
        leaf    = key;
    } else {
        section = key.left(slash);
        leaf    = key.mid(slash + 1);
    }
}

QString ScopedSettings::workspaceConfigFile() const
{
    if (m_workspaceRoot.isEmpty())
        return {};
    return m_workspaceRoot + QLatin1String(kCfgFile);
}

void ScopedSettings::setWorkspaceRoot(const QString &root)
{
    if (m_workspaceRoot == root)
        return;

    if (!m_workspaceRoot.isEmpty()) {
        const auto files = m_watcher->files();
        if (!files.isEmpty())
            m_watcher->removePaths(files);
    }

    m_workspaceRoot = root;
    m_workspaceJson = {};

    if (!root.isEmpty())
        loadWorkspaceFile();

    rewatch();
    emit valueChanged({});
}

QVariant ScopedSettings::value(const QString &key, const QVariant &defaultValue) const
{
    // 1) Workspace overlay
    QString section, leaf;
    splitKey(key, section, leaf);
    const auto sit = m_workspaceJson.constFind(section);
    if (sit != m_workspaceJson.constEnd()) {
        const QJsonObject obj = sit->toObject();
        const auto kit = obj.constFind(leaf);
        if (kit != obj.constEnd())
            return kit->toVariant();
    }

    // 2) User QSettings
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    return s.value(key, defaultValue);
}

void ScopedSettings::setValue(const QString &key, const QVariant &val, Scope scope)
{
    if (scope == Workspace) {
        if (!hasWorkspace()) {
            qWarning() << "ScopedSettings::setValue(Workspace) ignored — no workspace open. Key:" << key;
            return;
        }
        QString section, leaf;
        splitKey(key, section, leaf);
        QJsonObject obj = m_workspaceJson.value(section).toObject();
        obj.insert(leaf, QJsonValue::fromVariant(val));
        m_workspaceJson.insert(section, obj);
        if (saveWorkspaceFileAtomic())
            emit valueChanged(key);
        return;
    }

    // User
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.setValue(key, val);
    emit valueChanged(key);
}

bool ScopedSettings::hasOverride(const QString &key) const
{
    QString section, leaf;
    splitKey(key, section, leaf);
    const auto sit = m_workspaceJson.constFind(section);
    if (sit == m_workspaceJson.constEnd())
        return false;
    return sit->toObject().contains(leaf);
}

void ScopedSettings::removeWorkspaceOverride(const QString &key)
{
    if (!hasWorkspace())
        return;
    QString section, leaf;
    splitKey(key, section, leaf);
    if (!m_workspaceJson.contains(section))
        return;
    QJsonObject obj = m_workspaceJson.value(section).toObject();
    if (!obj.contains(leaf))
        return;
    obj.remove(leaf);
    if (obj.isEmpty())
        m_workspaceJson.remove(section);
    else
        m_workspaceJson.insert(section, obj);
    if (saveWorkspaceFileAtomic())
        emit valueChanged(key);
}

ScopedSettings::Scope ScopedSettings::effectiveScope(const QString &key) const
{
    return hasOverride(key) ? Workspace : User;
}

QStringList ScopedSettings::workspaceKeys() const
{
    QStringList out;
    for (auto it = m_workspaceJson.constBegin(); it != m_workspaceJson.constEnd(); ++it) {
        const QString section = it.key();
        const QJsonObject obj = it.value().toObject();
        for (auto kit = obj.constBegin(); kit != obj.constEnd(); ++kit) {
            if (section == QLatin1String(kFlatSection))
                out << kit.key();
            else
                out << section + QLatin1Char('/') + kit.key();
        }
    }
    return out;
}

void ScopedSettings::loadWorkspaceFile()
{
    m_workspaceJson = {};
    const QString path = workspaceConfigFile();
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "ScopedSettings: failed to parse" << path << ":" << err.errorString();
        return;
    }
    if (doc.isObject())
        m_workspaceJson = doc.object();
}

bool ScopedSettings::saveWorkspaceFileAtomic() const
{
    if (m_workspaceRoot.isEmpty())
        return false;

    const QString dirPath = m_workspaceRoot + QLatin1String(kCfgDir);
    if (!QDir().mkpath(dirPath)) {
        qWarning() << "ScopedSettings: failed to create" << dirPath;
        return false;
    }

    const QString path = workspaceConfigFile();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "ScopedSettings: cannot open" << path << "for writing";
        return false;
    }
    file.write(QJsonDocument(m_workspaceJson).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        qWarning() << "ScopedSettings: commit failed for" << path;
        return false;
    }
    return true;
}

void ScopedSettings::rewatch()
{
    const auto cur = m_watcher->files();
    if (!cur.isEmpty())
        m_watcher->removePaths(cur);

    const QString path = workspaceConfigFile();
    if (!path.isEmpty() && QFileInfo::exists(path))
        m_watcher->addPath(path);
}
