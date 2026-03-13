#pragma once

#include <QMap>
#include <QObject>
#include <QSettings>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// FeatureFlags — enable/disable individual IDE features.
//
// Provides a centralized toggle system for:
//   - Inline chat (Ctrl+I)
//   - Ghost text / inline completions
//   - Code review features
//   - Memory system
//   - Auto-compaction
//   - Inline rename suggestions
//   - MCP protocol
// ─────────────────────────────────────────────────────────────────────────────

class FeatureFlags : public QObject
{
    Q_OBJECT

public:
    explicit FeatureFlags(QObject *parent = nullptr);

    // ── Predefined feature flags ──────────────────────────────────────────

    bool inlineChatEnabled() const { return flag(QStringLiteral("inlineChat")); }
    void setInlineChatEnabled(bool v) { setFlag(QStringLiteral("inlineChat"), v); }

    bool ghostTextEnabled() const { return flag(QStringLiteral("ghostText")); }
    void setGhostTextEnabled(bool v) { setFlag(QStringLiteral("ghostText"), v); }

    bool codeReviewEnabled() const { return flag(QStringLiteral("codeReview")); }
    void setCodeReviewEnabled(bool v) { setFlag(QStringLiteral("codeReview"), v); }

    bool memoryEnabled() const { return flag(QStringLiteral("memory")); }
    void setMemoryEnabled(bool v) { setFlag(QStringLiteral("memory"), v); }

    bool autoCompactionEnabled() const { return flag(QStringLiteral("autoCompaction")); }
    void setAutoCompactionEnabled(bool v) { setFlag(QStringLiteral("autoCompaction"), v); }

    bool renameSuggestionsEnabled() const { return flag(QStringLiteral("renameSuggestions")); }
    void setRenameSuggestionsEnabled(bool v) { setFlag(QStringLiteral("renameSuggestions"), v); }

    bool mcpEnabled() const { return flag(QStringLiteral("mcp")); }
    void setMcpEnabled(bool v) { setFlag(QStringLiteral("mcp"), v); }

    bool telemetryEnabled() const { return flag(QStringLiteral("telemetry"), false); }
    void setTelemetryEnabled(bool v) { setFlag(QStringLiteral("telemetry"), v); }

    // ── Generic flag access ───────────────────────────────────────────────

    bool flag(const QString &name, bool defaultValue = true) const;
    void setFlag(const QString &name, bool value);
    QStringList allFlags() const { return m_flags.keys(); }

signals:
    void flagChanged(const QString &name, bool enabled);

private:
    void loadFromSettings();
    void saveToSettings();

    QMap<QString, bool> m_flags;
};
