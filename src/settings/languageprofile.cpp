#include "languageprofile.h"

#include <QSettings>

LanguageProfileManager::LanguageProfileManager(QObject *parent)
    : QObject(parent)
{
    load();
}

QStringList LanguageProfileManager::configuredLanguages() const
{
    return m_profiles.keys();
}

LanguageProfileData LanguageProfileManager::profile(const QString &languageId) const
{
    return m_profiles.value(languageId);
}

void LanguageProfileManager::setProfile(const QString &languageId,
                                        const LanguageProfileData &data)
{
    m_profiles[languageId] = data;
    save();
    emit profileChanged(languageId);
}

void LanguageProfileManager::removeProfile(const QString &languageId)
{
    if (m_profiles.remove(languageId)) {
        QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
        s.remove(QStringLiteral("LanguageProfiles/%1").arg(languageId));
        emit profileChanged(languageId);
    }
}

int LanguageProfileManager::tabSize(const QString &languageId, int globalDefault) const
{
    auto it = m_profiles.constFind(languageId);
    if (it != m_profiles.constEnd() && it->tabSize > 0)
        return it->tabSize;
    return globalDefault;
}

bool LanguageProfileManager::useTabs(const QString &languageId, bool globalDefault) const
{
    auto it = m_profiles.constFind(languageId);
    if (it != m_profiles.constEnd() && it->useTabsSet)
        return it->useTabs;
    return globalDefault;
}

bool LanguageProfileManager::trimTrailingWhitespace(const QString &languageId) const
{
    auto it = m_profiles.constFind(languageId);
    if (it != m_profiles.constEnd() && it->trimSet)
        return it->trimWhitespace;
    return false;
}

bool LanguageProfileManager::formatOnSave(const QString &languageId) const
{
    auto it = m_profiles.constFind(languageId);
    if (it != m_profiles.constEnd() && it->formatOnSaveSet)
        return it->formatOnSave;
    return false;
}

QString LanguageProfileManager::eolMode(const QString &languageId) const
{
    auto it = m_profiles.constFind(languageId);
    if (it != m_profiles.constEnd() && !it->eolMode.isEmpty())
        return it->eolMode;
    return {};
}

void LanguageProfileManager::load()
{
    m_profiles.clear();
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("LanguageProfiles"));
    const QStringList langs = s.childGroups();
    for (const QString &lang : langs) {
        s.beginGroup(lang);
        LanguageProfileData d;
        if (s.contains(QStringLiteral("tabSize")))
            d.tabSize = s.value(QStringLiteral("tabSize")).toInt();
        if (s.contains(QStringLiteral("useTabs"))) {
            d.useTabs = s.value(QStringLiteral("useTabs")).toBool();
            d.useTabsSet = true;
        }
        if (s.contains(QStringLiteral("trimWhitespace"))) {
            d.trimWhitespace = s.value(QStringLiteral("trimWhitespace")).toBool();
            d.trimSet = true;
        }
        if (s.contains(QStringLiteral("formatOnSave"))) {
            d.formatOnSave = s.value(QStringLiteral("formatOnSave")).toBool();
            d.formatOnSaveSet = true;
        }
        d.eolMode   = s.value(QStringLiteral("eolMode")).toString();
        d.formatter = s.value(QStringLiteral("formatter")).toString();
        m_profiles[lang] = d;
        s.endGroup();
    }
    s.endGroup();
}

void LanguageProfileManager::save() const
{
    QSettings s(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    s.beginGroup(QStringLiteral("LanguageProfiles"));
    // Clear old profiles
    for (const QString &g : s.childGroups())
        s.remove(g);

    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        s.beginGroup(it.key());
        const auto &d = it.value();
        if (d.tabSize > 0)
            s.setValue(QStringLiteral("tabSize"), d.tabSize);
        if (d.useTabsSet)
            s.setValue(QStringLiteral("useTabs"), d.useTabs);
        if (d.trimSet)
            s.setValue(QStringLiteral("trimWhitespace"), d.trimWhitespace);
        if (d.formatOnSaveSet)
            s.setValue(QStringLiteral("formatOnSave"), d.formatOnSave);
        if (!d.eolMode.isEmpty())
            s.setValue(QStringLiteral("eolMode"), d.eolMode);
        if (!d.formatter.isEmpty())
            s.setValue(QStringLiteral("formatter"), d.formatter);
        s.endGroup();
    }
    s.endGroup();
}
