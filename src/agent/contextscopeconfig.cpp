#include "contextscopeconfig.h"

ContextScopeConfig::ContextScopeConfig(QObject *parent) : QObject(parent)
{
    loadSettings();
}

bool ContextScopeConfig::isScopeEnabled(Scope scope) const
{
    return m_scopes.value(scopeKey(scope), true);
}

void ContextScopeConfig::setScopeEnabled(Scope scope, bool enabled)
{
    m_scopes[scopeKey(scope)] = enabled;
    saveSettings();
    emit configChanged();
}

QStringList ContextScopeConfig::enabledScopes() const
{
    QStringList result;
    for (auto it = m_scopes.begin(); it != m_scopes.end(); ++it) {
        if (it.value()) result << it.key();
    }
    return result;
}

void ContextScopeConfig::setInstructionFilePaths(const QStringList &paths)
{
    m_instructionPaths = paths;
    QSettings settings;
    settings.setValue(QStringLiteral("Context/instructionPaths"), paths);
    emit configChanged();
}

QString ContextScopeConfig::scopeKey(Scope scope)
{
    static const QMap<Scope, QString> keys = {
        {ActiveFile, QStringLiteral("activeFile")},
        {Selection, QStringLiteral("selection")},
        {OpenFiles, QStringLiteral("openFiles")},
        {Diagnostics, QStringLiteral("diagnostics")},
        {Terminal, QStringLiteral("terminal")},
        {GitDiff, QStringLiteral("gitDiff")},
        {GitStatus, QStringLiteral("gitStatus")},
        {WorkspaceInfo, QStringLiteral("workspaceInfo")},
    };
    return keys.value(scope, QStringLiteral("unknown"));
}

void ContextScopeConfig::loadSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("Context/Scopes"));
    for (const auto &key : settings.childKeys())
        m_scopes[key] = settings.value(key, true).toBool();
    settings.endGroup();
    m_instructionPaths = settings.value(
        QStringLiteral("Context/instructionPaths")).toStringList();
}

void ContextScopeConfig::saveSettings()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("Context/Scopes"));
    for (auto it = m_scopes.begin(); it != m_scopes.end(); ++it)
        settings.setValue(it.key(), it.value());
    settings.endGroup();
}
