#pragma once

#include <QInputDialog>
#include <QObject>
#include <QString>
#include <QStringList>

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
    explicit RenameIntegration(QObject *parent = nullptr) : QObject(parent) {}

    // Build AI prompt for rename suggestion
    QString buildRenamePrompt(const QString &oldName, const QString &code,
                               const QString &languageId) const
    {
        return QStringLiteral(
            "Suggest a better name for the variable/function '%1' "
            "in the following %2 code. The new name should be:\n"
            "- More descriptive and self-documenting\n"
            "- Following %2 naming conventions\n"
            "- Concise but clear\n\n"
            "```%2\n%3\n```\n\n"
            "Reply with ONLY the suggested new name, nothing else.")
            .arg(oldName, languageId, code.left(2000));
    }

    // Parse AI response for a clean name
    static QString parseSuggestion(const QString &response)
    {
        QString name = response.trimmed();
        // Remove backticks, quotes
        name.remove(QLatin1Char('`'));
        name.remove(QLatin1Char('"'));
        name.remove(QLatin1Char('\''));
        // Take first word/identifier
        static const QRegularExpression rx(QStringLiteral(R"(^(\w+))"));
        const auto match = rx.match(name);
        return match.hasMatch() ? match.captured(1) : name;
    }

    // Show rename dialog with AI suggestion pre-filled
    static QString showRenameDialog(QWidget *parent, const QString &oldName,
                                     const QString &suggestion)
    {
        bool ok = false;
        const QString newName = QInputDialog::getText(
            parent, QObject::tr("Rename Symbol"),
            QObject::tr("Rename '%1' to:").arg(oldName),
            QLineEdit::Normal, suggestion, &ok);
        return ok ? newName : QString();
    }

signals:
    void renameRequested(const QString &filePath, int line, int column,
                         const QString &newName);
    void renameSuggested(const QString &oldName, const QString &suggestion);
};
