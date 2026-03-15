#include "renameintegration.h"

#include <QInputDialog>
#include <QLineEdit>
#include <QRegularExpression>
#include <QWidget>

RenameIntegration::RenameIntegration(QObject *parent) : QObject(parent) {}

QString RenameIntegration::buildRenamePrompt(const QString &oldName, const QString &code,
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

QString RenameIntegration::parseSuggestion(const QString &response)
{
    QString name = response.trimmed();
    name.remove(QLatin1Char('`'));
    name.remove(QLatin1Char('"'));
    name.remove(QLatin1Char('\''));
    static const QRegularExpression rx(QStringLiteral(R"(^(\w+))"));
    const auto match = rx.match(name);
    return match.hasMatch() ? match.captured(1) : name;
}

QString RenameIntegration::showRenameDialog(QWidget *parent, const QString &oldName,
                                             const QString &suggestion)
{
    bool ok = false;
    const QString newName = QInputDialog::getText(
        parent, QObject::tr("Rename Symbol"),
        QObject::tr("Rename '%1' to:").arg(oldName),
        QLineEdit::Normal, suggestion, &ok);
    return ok ? newName : QString();
}
