#pragma once

#include <QMap>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────────────────
// LanguageCompletionConfig — per-language completion enable/disable.
//
// Controls which languages have inline ghost text completion enabled.
// Settings are persisted via QSettings.
// ─────────────────────────────────────────────────────────────────────────────

class LanguageCompletionConfig : public QObject
{
    Q_OBJECT

public:
    explicit LanguageCompletionConfig(QObject *parent = nullptr)
        : QObject(parent)
    {
        loadSettings();
    }

    /// Check if completions are enabled for a language
    bool isEnabled(const QString &languageId) const
    {
        return m_enabled.value(languageId.toLower(), m_globalEnabled);
    }

    /// Enable/disable for a specific language
    void setEnabled(const QString &languageId, bool enabled)
    {
        m_enabled[languageId.toLower()] = enabled;
        saveSettings();
        emit configChanged(languageId);
    }

    /// Enable/disable globally
    void setGlobalEnabled(bool enabled)
    {
        m_globalEnabled = enabled;
        saveSettings();
        emit globalChanged(enabled);
    }

    bool globalEnabled() const { return m_globalEnabled; }

    /// Get all language-specific overrides
    QMap<QString, bool> overrides() const { return m_enabled; }

    /// Remove a per-language override (falls back to global)
    void removeOverride(const QString &languageId)
    {
        m_enabled.remove(languageId.toLower());
        saveSettings();
        emit configChanged(languageId);
    }

    /// Get list of all known language IDs with overrides
    QStringList configuredLanguages() const { return m_enabled.keys(); }

signals:
    void configChanged(const QString &languageId);
    void globalChanged(bool enabled);

private:
    void loadSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("InlineCompletion"));
        m_globalEnabled = settings.value(QStringLiteral("enabled"), true).toBool();

        settings.beginGroup(QStringLiteral("languages"));
        const auto keys = settings.childKeys();
        for (const QString &key : keys) {
            m_enabled[key] = settings.value(key).toBool();
        }
        settings.endGroup();
        settings.endGroup();
    }

    void saveSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("InlineCompletion"));
        settings.setValue(QStringLiteral("enabled"), m_globalEnabled);

        settings.beginGroup(QStringLiteral("languages"));
        // Clear old entries
        settings.remove(QString());
        for (auto it = m_enabled.constBegin(); it != m_enabled.constEnd(); ++it)
            settings.setValue(it.key(), it.value());
        settings.endGroup();
        settings.endGroup();
    }

    bool m_globalEnabled = true;
    QMap<QString, bool> m_enabled;  // languageId -> enabled
};
