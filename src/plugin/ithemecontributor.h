#pragma once

#include <QJsonObject>
#include <QString>

// ── IThemeContributor ─────────────────────────────────────────────────────────
//
// Plugins implement this to provide theme customization beyond what the
// static ThemeContribution can declare.
//
// Static themes (JSON files) are loaded automatically by ContributionRegistry.
// This interface enables dynamic theme generation, runtime palette overrides,
// and syntax color schemes.

class IThemeContributor
{
public:
    virtual ~IThemeContributor() = default;

    /// Return the full theme palette as a JSON object.
    /// Structure follows Exorcist theme format:
    ///   { "colors": { "editor.background": "#1e1e1e", ... },
    ///     "tokenColors": [ { "scope": "comment", "settings": { "foreground": "#6A9955" } }, ... ] }
    virtual QJsonObject themeData(const QString &themeId) const = 0;

    /// Called when this theme is activated.
    virtual void themeActivated(const QString &themeId)
    {
        Q_UNUSED(themeId);
    }

    /// Called when this theme is deactivated.
    virtual void themeDeactivated(const QString &themeId)
    {
        Q_UNUSED(themeId);
    }
};

#define EXORCIST_THEME_CONTRIBUTOR_IID "org.exorcist.IThemeContributor/1.0"
Q_DECLARE_INTERFACE(IThemeContributor, EXORCIST_THEME_CONTRIBUTOR_IID)
