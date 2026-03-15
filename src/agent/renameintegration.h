#pragma once

#include <QObject>
#include <QString>

class QWidget;

// ─────────────────────────────────────────────────────────────────────────────
// RenameIntegration — AI-assisted rename via LSP.
//
// When user triggers rename on a symbol, this asks the AI for a better name
// suggestion, then applies via LSP rename.
// ─────────────────────────────────────────────────────────────────────────────

class RenameIntegration : public QObject
{
    Q_OBJECT

public:
    explicit RenameIntegration(QObject *parent = nullptr);

    QString buildRenamePrompt(const QString &oldName, const QString &code,
                               const QString &languageId) const;
    static QString parseSuggestion(const QString &response);
    static QString showRenameDialog(QWidget *parent, const QString &oldName,
                                     const QString &suggestion);

signals:
    void renameRequested(const QString &filePath, int line, int column,
                         const QString &newName);
    void renameSuggested(const QString &oldName, const QString &suggestion);
};
