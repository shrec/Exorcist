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
    explicit ContextScopeConfig(QObject *parent = nullptr);

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

    bool isScopeEnabled(Scope scope) const;
    void setScopeEnabled(Scope scope, bool enabled);
    QStringList enabledScopes() const;

    void setInstructionFilePaths(const QStringList &paths);
    QStringList instructionFilePaths() const { return m_instructionPaths; }

signals:
    void configChanged();

private:
    static QString scopeKey(Scope scope);
    void loadSettings();
    void saveSettings();

    QMap<QString, bool> m_scopes;
    QStringList m_instructionPaths;
};
