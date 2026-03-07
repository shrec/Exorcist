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
    explicit FeatureFlags(QObject *parent = nullptr)
        : QObject(parent)
    {
        loadFromSettings();
    }

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

    bool flag(const QString &name, bool defaultValue = true) const
    {
        return m_flags.value(name, defaultValue);
    }

    void setFlag(const QString &name, bool value)
    {
        if (m_flags.value(name, !value) == value) return;
        m_flags[name] = value;
        saveToSettings();
        emit flagChanged(name, value);
    }

    QStringList allFlags() const { return m_flags.keys(); }

signals:
    void flagChanged(const QString &name, bool enabled);

private:
    void loadFromSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("FeatureFlags"));

        // Defaults: everything on except telemetry
        m_flags[QStringLiteral("inlineChat")] = settings.value(QStringLiteral("inlineChat"), true).toBool();
        m_flags[QStringLiteral("ghostText")] = settings.value(QStringLiteral("ghostText"), true).toBool();
        m_flags[QStringLiteral("codeReview")] = settings.value(QStringLiteral("codeReview"), true).toBool();
        m_flags[QStringLiteral("memory")] = settings.value(QStringLiteral("memory"), true).toBool();
        m_flags[QStringLiteral("autoCompaction")] = settings.value(QStringLiteral("autoCompaction"), true).toBool();
        m_flags[QStringLiteral("renameSuggestions")] = settings.value(QStringLiteral("renameSuggestions"), true).toBool();
        m_flags[QStringLiteral("mcp")] = settings.value(QStringLiteral("mcp"), true).toBool();
        m_flags[QStringLiteral("telemetry")] = settings.value(QStringLiteral("telemetry"), false).toBool();

        settings.endGroup();
    }

    void saveToSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("FeatureFlags"));
        for (auto it = m_flags.constBegin(); it != m_flags.constEnd(); ++it)
            settings.setValue(it.key(), it.value());
        settings.endGroup();
    }

    QMap<QString, bool> m_flags;
};
