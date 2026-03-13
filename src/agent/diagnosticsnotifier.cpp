#include "diagnosticsnotifier.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

// ── Construction ─────────────────────────────────────────────────────────────

DiagnosticsNotifier::DiagnosticsNotifier(QObject *parent)
    : QObject(parent)
{
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(1500); // 1.5s debounce — avoids spamming during typing
    connect(&m_debounceTimer, &QTimer::timeout, this, &DiagnosticsNotifier::flush);
}

// ── Public API ───────────────────────────────────────────────────────────────

void DiagnosticsNotifier::setEnabled(bool enabled)
{
    m_enabled = enabled;
    if (!enabled) {
        m_debounceTimer.stop();
        m_pendingChanges.clear();
        m_pendingText.clear();
        m_dirty = false;
    }
}

bool DiagnosticsNotifier::isEnabled() const
{
    return m_enabled;
}

bool DiagnosticsNotifier::hasPendingNotification() const
{
    return !m_pendingText.isEmpty();
}

QString DiagnosticsNotifier::consumeNotification()
{
    QString text;
    text.swap(m_pendingText);
    return text;
}

// ── Diagnostic update from LSP ───────────────────────────────────────────────

void DiagnosticsNotifier::onDiagnosticsPublished(const QString &uri,
                                                  const QJsonArray &diagnostics)
{
    if (!m_enabled)
        return;

    const QString filePath = QUrl(uri).toLocalFile();
    if (filePath.isEmpty())
        return;

    // Parse diagnostics
    int newErrors = 0, newWarnings = 0;
    QList<AgentDiagnostic> diags;
    for (const QJsonValue &val : diagnostics) {
        const QJsonObject d = val.toObject();
        AgentDiagnostic ag;
        ag.filePath = filePath;
        ag.message  = d[QLatin1String("message")].toString();

        const QJsonObject range = d[QLatin1String("range")].toObject();
        ag.line   = range[QLatin1String("start")].toObject()[QLatin1String("line")].toInt();
        ag.column = range[QLatin1String("start")].toObject()[QLatin1String("character")].toInt();

        const int severity = d[QLatin1String("severity")].toInt(1);
        switch (severity) {
        case 1: ag.severity = AgentDiagnostic::Severity::Error;   ++newErrors; break;
        case 2: ag.severity = AgentDiagnostic::Severity::Warning; ++newWarnings; break;
        default: ag.severity = AgentDiagnostic::Severity::Info; break;
        }
        diags.append(ag);
    }

    // Diff against snapshot
    const FileDiagState &prev = m_snapshot[filePath];
    const int resolvedErrors   = qMax(0, prev.errorCount - newErrors);
    const int resolvedWarnings = qMax(0, prev.warningCount - newWarnings);

    const bool changed = (newErrors != prev.errorCount) ||
                         (newWarnings != prev.warningCount);

    if (!changed)
        return; // No meaningful change

    // Update snapshot
    FileDiagState &state = m_snapshot[filePath];
    state.errorCount   = newErrors;
    state.warningCount = newWarnings;
    state.diagnostics  = diags;

    // Queue the change
    Change c;
    c.filePath          = filePath;
    c.newErrors         = newErrors;
    c.newWarnings       = newWarnings;
    c.resolvedErrors    = resolvedErrors;
    c.resolvedWarnings  = resolvedWarnings;
    c.currentDiags      = diags;
    m_pendingChanges.append(c);
    m_dirty = true;

    // Restart debounce timer
    m_debounceTimer.start();
}

// ── Flush — build the notification text ──────────────────────────────────────

void DiagnosticsNotifier::flush()
{
    if (!m_dirty || m_pendingChanges.isEmpty())
        return;

    QStringList lines;
    lines << QStringLiteral("[Diagnostics Update]");

    int totalErrors = 0, totalWarnings = 0;
    for (auto it = m_snapshot.cbegin(); it != m_snapshot.cend(); ++it) {
        totalErrors   += it.value().errorCount;
        totalWarnings += it.value().warningCount;
    }

    lines << QStringLiteral("Workspace: %1 error(s), %2 warning(s)")
                 .arg(totalErrors).arg(totalWarnings);

    // Show details for changed files
    for (const Change &c : std::as_const(m_pendingChanges)) {
        const QString fileName = QFileInfo(c.filePath).fileName();

        if (c.newErrors == 0 && c.newWarnings == 0) {
            lines << QStringLiteral("  %1: all issues resolved").arg(fileName);
            continue;
        }

        lines << QStringLiteral("  %1: %2 error(s), %3 warning(s)")
                     .arg(fileName).arg(c.newErrors).arg(c.newWarnings);

        // Show up to 5 error messages per file
        int shown = 0;
        for (const auto &d : c.currentDiags) {
            if (d.severity != AgentDiagnostic::Severity::Error)
                continue;
            if (++shown > 5) {
                lines << QStringLiteral("    ... and %1 more error(s)")
                             .arg(c.newErrors - 5);
                break;
            }
            lines << QStringLiteral("    L%1: %2").arg(d.line + 1).arg(d.message);
        }
    }

    m_pendingText = lines.join('\n');
    m_pendingChanges.clear();
    m_dirty = false;
}
