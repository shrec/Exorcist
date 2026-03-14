#pragma once

#include <QMap>
#include <QObject>
#include <QString>

// ── LanguageProfile ──────────────────────────────────────────────────────────
// Per-language editor configuration. Settings stored in QSettings under
// "LanguageProfiles/<languageId>/".
// When no profile exists for a language, the global editor settings apply.

struct LanguageProfileData
{
    int     tabSize        = -1;    // -1 = use global
    bool    useTabs        = false; // false = spaces (default)
    bool    useTabsSet     = false; // whether useTabs was explicitly set
    bool    trimWhitespace = false;
    bool    trimSet        = false;
    bool    formatOnSave   = false;
    bool    formatOnSaveSet = false;
    QString eolMode;               // empty = use global, "lf", "crlf", "cr"
    QString formatter;             // empty = use LSP default
};

class LanguageProfileManager : public QObject
{
    Q_OBJECT
public:
    explicit LanguageProfileManager(QObject *parent = nullptr);

    QStringList configuredLanguages() const;
    LanguageProfileData profile(const QString &languageId) const;
    void setProfile(const QString &languageId, const LanguageProfileData &data);
    void removeProfile(const QString &languageId);

    int     tabSize(const QString &languageId, int globalDefault) const;
    bool    useTabs(const QString &languageId, bool globalDefault) const;
    bool    trimTrailingWhitespace(const QString &languageId) const;
    bool    formatOnSave(const QString &languageId) const;
    QString eolMode(const QString &languageId) const;

    void load();
    void save() const;

signals:
    void profileChanged(const QString &languageId);

private:
    QMap<QString, LanguageProfileData> m_profiles;
};
