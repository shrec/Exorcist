#include "featureflags.h"

FeatureFlags::FeatureFlags(QObject *parent)
    : QObject(parent)
{
    loadFromSettings();
}

bool FeatureFlags::flag(const QString &name, bool defaultValue) const
{
    return m_flags.value(name, defaultValue);
}

void FeatureFlags::setFlag(const QString &name, bool value)
{
    if (m_flags.value(name, !value) == value) return;
    m_flags[name] = value;
    saveToSettings();
    emit flagChanged(name, value);
}

void FeatureFlags::loadFromSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("FeatureFlags"));

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

void FeatureFlags::saveToSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("FeatureFlags"));
    for (auto it = m_flags.constBegin(); it != m_flags.constEnd(); ++it)
        settings.setValue(it.key(), it.value());
    settings.endGroup();
}
