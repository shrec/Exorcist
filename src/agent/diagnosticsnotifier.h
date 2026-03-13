#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include "../aiinterface.h"

class AgentController;

// ── DiagnosticsNotifier ──────────────────────────────────────────────────────
//
// Listens for LSP diagnostic changes and pushes them into the active
// agent conversation as context. This gives the agent real-time awareness
// of compile errors and warnings without requiring explicit get_errors calls.
//
// Flow:
//   1. LspClient emits diagnosticsPublished(uri, diags)
//   2. DiagnosticsNotifier receives, debounces, diffs against last snapshot
//   3. If significant changes detected, queues a notification
//   4. AgentController picks up pending notifications before next model request
//
// The notifier is NOT a tool — it's infrastructure that augments the agent's
// context with real-time diagnostic information.

class DiagnosticsNotifier : public QObject
{
    Q_OBJECT

public:
    explicit DiagnosticsNotifier(QObject *parent = nullptr);

    /// Feed a diagnostic update from the LSP.
    /// Call this from the slot connected to LspClient::diagnosticsPublished.
    void onDiagnosticsPublished(const QString &uri, const QJsonArray &diagnostics);

    /// Returns true if there are pending diagnostic notifications.
    bool hasPendingNotification() const;

    /// Consume and return the pending notification text.
    /// After calling this, hasPendingNotification() returns false until
    /// new changes arrive.
    QString consumeNotification();

    /// Set whether notifications are active.
    /// Disable during non-agentic operations to save work.
    void setEnabled(bool enabled);
    bool isEnabled() const;

private slots:
    void flush();

private:
    struct FileDiagState {
        int errorCount   = 0;
        int warningCount = 0;
        QList<AgentDiagnostic> diagnostics;
    };

    QHash<QString, FileDiagState> m_snapshot;
    QString m_pendingText;
    QTimer  m_debounceTimer;
    bool    m_enabled   = false;
    bool    m_dirty     = false;

    // Accumulated changes since last flush
    struct Change {
        QString filePath;
        int     newErrors;
        int     newWarnings;
        int     resolvedErrors;
        int     resolvedWarnings;
        QList<AgentDiagnostic> currentDiags;
    };
    QList<Change> m_pendingChanges;
};
