#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

// ── ILanguageContributor ──────────────────────────────────────────────────────
//
// Plugins implement this to provide language support beyond what the
// manifest's LanguageContribution can declare statically.
//
// Static contributions (extensions, filenames, LSP command) are handled
// automatically by ContributionRegistry. This interface is for dynamic
// behavior: custom LSP init options, runtime grammar loading, etc.

class ILanguageContributor
{
public:
    virtual ~ILanguageContributor() = default;

    /// Additional initialization options for the LSP server.
    /// Called after the language is activated and before LSP initialize.
    virtual QJsonObject lspInitializationOptions(const QString &languageId) const
    {
        Q_UNUSED(languageId);
        return {};
    }

    /// Additional LSP server settings (workspace/didChangeConfiguration).
    virtual QJsonObject lspSettings(const QString &languageId) const
    {
        Q_UNUSED(languageId);
        return {};
    }

    /// File patterns that should trigger this language's LSP.
    /// Supplements the manifest's static extensions list.
    virtual QStringList additionalFilePatterns(const QString &languageId) const
    {
        Q_UNUSED(languageId);
        return {};
    }

    /// Called when a document of this language is opened.
    virtual void documentOpened(const QString &languageId,
                                const QString &filePath)
    {
        Q_UNUSED(languageId);
        Q_UNUSED(filePath);
    }

    /// Called when a document of this language is saved.
    virtual void documentSaved(const QString &languageId,
                               const QString &filePath)
    {
        Q_UNUSED(languageId);
        Q_UNUSED(filePath);
    }
};

#define EXORCIST_LANGUAGE_CONTRIBUTOR_IID "org.exorcist.ILanguageContributor/1.0"
Q_DECLARE_INTERFACE(ILanguageContributor, EXORCIST_LANGUAGE_CONTRIBUTOR_IID)
