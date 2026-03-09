#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include "lsptransport.h"

// ── LspClient ─────────────────────────────────────────────────────────────────
//
// High-level Language Server Protocol client built on top of LspTransport.
// Handles request/response correlation, JSON-RPC framing, and emits typed
// signals for each server response / notification.
//
// Lifecycle:
//   initialize(root) → [initialized signal] → didOpen/didChange/requests → shutdown()
//
// All request results arrive asynchronously via signals. Callers match results
// by uri/position or by the signal emitted.

class LspClient : public QObject
{
    Q_OBJECT

public:
    explicit LspClient(LspTransport *transport, QObject *parent = nullptr);

    // ── Lifecycle ─────────────────────────────────────────────────────────
    void initialize(const QString &workspaceRoot);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // ── Document sync ─────────────────────────────────────────────────────
    // languageId: "cpp", "python", "javascript", etc.
    void didOpen(const QString &uri, const QString &languageId,
                 const QString &text, int version = 1);
    void didChange(const QString &uri, const QString &text, int version);
    void didClose(const QString &uri);

    // ── Requests (async — results arrive via signals) ─────────────────────
    void requestCompletion(const QString &uri, int line, int character,
                           int triggerKind = 1, const QString &triggerChar = {});
    void requestHover(const QString &uri, int line, int character);
    void requestSignatureHelp(const QString &uri, int line, int character);
    void requestDefinition(const QString &uri, int line, int character);
    void requestDeclaration(const QString &uri, int line, int character);

    // Format whole document
    void requestFormatting(const QString &uri, int tabSize = 4,
                           bool insertSpaces = true);
    // Format a selection range
    void requestRangeFormatting(const QString &uri,
                                int startLine, int startChar,
                                int endLine,   int endChar,
                                int tabSize = 4, bool insertSpaces = true);

    // Format on type (triggered by \n, }, ; etc.)
    void requestOnTypeFormatting(const QString &uri, int line, int character,
                                 const QString &ch, int tabSize = 4,
                                 bool insertSpaces = true);

    void requestReferences(const QString &uri, int line, int character,
                           bool includeDeclaration = true);
    void requestRename(const QString &uri, int line, int character,
                       const QString &newName);
    void requestDocumentSymbols(const QString &uri);

    // ── Helpers ───────────────────────────────────────────────────────────
    // Convert a local file path to a LSP URI: file:///C:/...
    static QString pathToUri(const QString &path);
    // Infer LSP languageId from file extension
    static QString languageIdForPath(const QString &path);

signals:
    void initialized();

    // Results — uri matches the request uri
    void completionResult(const QString &uri, int line, int character,
                          const QJsonArray &items, bool isIncomplete);
    void hoverResult(const QString &uri, int line, int character,
                     const QString &markdown);
    void signatureHelpResult(const QString &uri, int line, int character,
                             const QJsonObject &result);
    void formattingResult(const QString &uri, const QJsonArray &textEdits);
    void definitionResult(const QString &uri, const QJsonArray &locations);
    void declarationResult(const QString &uri, const QJsonArray &locations);
    void referencesResult(const QString &uri, const QJsonArray &locations);
    void renameResult(const QString &uri, const QJsonObject &workspaceEdit);
    void documentSymbolsResult(const QString &uri, const QJsonArray &symbols);

    // Push notifications from the server (no request needed)
    void diagnosticsPublished(const QString &uri, const QJsonArray &diagnostics);

    void serverError(const QString &message);

private:
    void onMessageReceived(const LspMessage &msg);
    void handleResponse(int id, const QJsonValue &result,
                        const QJsonObject &error);
    void handleNotification(const QString &method, const QJsonObject &params);

    int sendRequest(const QString &method, const QJsonObject &params);
    void sendNotification(const QString &method, const QJsonObject &params);

    LspTransport *m_transport;
    int           m_nextId     = 1;
    bool          m_initialized = false;

    // Maps request id → {method, extra data (uri, line, char)}
    struct PendingRequest
    {
        QString method;
        QString uri;
        int     line      = 0;
        int     character = 0;
    };
    QHash<int, PendingRequest> m_pending;
};
