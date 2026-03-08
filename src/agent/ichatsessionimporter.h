#pragma once

#include <QPair>
#include <QString>
#include <QVector>

// ── IChatSessionImporter ─────────────────────────────────────────────────────
//
// Optional interface that a provider plugin can implement to support importing
// remote/vendor-specific chat transcripts into the IDE's session store.
//
// Each provider has its own transcript format (e.g. Copilot conversation
// history, Claude's API log).  This interface normalises that into the
// universal {role, text} pair sequence that ChatSessionService can persist.
//
// Usage:
//   auto *importer = qobject_cast<IChatSessionImporter *>(plugin);
//   if (importer && importer->canImport(remoteData))
//       auto messages = importer->importTranscript(remoteData);

class IChatSessionImporter
{
public:
    virtual ~IChatSessionImporter() = default;

    /// Unique identifier for this importer (typically the provider ID).
    virtual QString importerId() const = 0;

    /// Human-readable label (e.g. "Import from Copilot").
    virtual QString importerLabel() const = 0;

    /// Return true if the raw data can be handled by this importer.
    virtual bool canImport(const QByteArray &rawData) const = 0;

    /// Parse vendor-specific transcript data into normalised messages.
    /// Each pair is { role ("user" | "assistant" | "system" | "tool"),
    ///                text content }.
    virtual QVector<QPair<QString, QString>> importTranscript(
        const QByteArray &rawData) const = 0;
};

#define EXORCIST_CHAT_SESSION_IMPORTER_IID "org.exorcist.IChatSessionImporter/1.0"
Q_DECLARE_INTERFACE(IChatSessionImporter, EXORCIST_CHAT_SESSION_IMPORTER_IID)
