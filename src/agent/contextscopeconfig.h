#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QMap>

// ─────────────────────────────────────────────────────────────────────────────
// ContextScopeConfig — configure what gets auto-included in context.
//
// Lets users control which context types are automatically sent with
// each request (active file, selection, diagnostics, terminal, git).
// ─────────────────────────────────────────────────────────────────────────────

class ContextScopeConfig : public QObject
{
    Q_OBJECT

public:
    explicit ContextScopeConfig(QObject *parent = nullptr) : QObject(parent)
    {
        loadSettings();
    }

    enum Scope {
        ActiveFile,
        Selection,
        OpenFiles,
        Diagnostics,
        Terminal,
        GitDiff,
        GitStatus,
        WorkspaceInfo,
    };

    bool isScopeEnabled(Scope scope) const
    {
        return m_scopes.value(scopeKey(scope), true);
    }

    void setScopeEnabled(Scope scope, bool enabled)
    {
        m_scopes[scopeKey(scope)] = enabled;
        saveSettings();
        emit configChanged();
    }

    QStringList enabledScopes() const
    {
        QStringList result;
        for (auto it = m_scopes.begin(); it != m_scopes.end(); ++it) {
            if (it.value()) result << it.key();
        }
        return result;
    }

    // Custom instruction file paths
    void setInstructionFilePaths(const QStringList &paths)
    {
        m_instructionPaths = paths;
        QSettings settings;
        settings.setValue(QStringLiteral("Context/instructionPaths"), paths);
        emit configChanged();
    }

    QStringList instructionFilePaths() const { return m_instructionPaths; }

signals:
    void configChanged();

private:
    static QString scopeKey(Scope scope)
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

    void loadSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("Context/Scopes"));
        for (const auto &key : settings.childKeys())
            m_scopes[key] = settings.value(key, true).toBool();
        settings.endGroup();
        m_instructionPaths = settings.value(
            QStringLiteral("Context/instructionPaths")).toStringList();
    }

    void saveSettings()
    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("Context/Scopes"));
        for (auto it = m_scopes.begin(); it != m_scopes.end(); ++it)
            settings.setValue(it.key(), it.value());
        settings.endGroup();
    }

    QMap<QString, bool> m_scopes;
    QStringList m_instructionPaths;
};
