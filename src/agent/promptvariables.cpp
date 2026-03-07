#include "promptvariables.h"

#include <QRegularExpression>

PromptVariableResolver::PromptVariableResolver(QObject *parent)
    : QObject(parent)
{
}

void PromptVariableResolver::registerVariable(const PromptVariable &var)
{
    if (var.name.trimmed().isEmpty() || !var.resolver)
        return;

    PromptVariable copy = var;
    copy.name = copy.name.trimmed().toLower();
    m_variables.insert(copy.name, copy);
}

bool PromptVariableResolver::hasVariable(const QString &name) const
{
    return m_variables.contains(name.trimmed().toLower());
}

QStringList PromptVariableResolver::variableNames() const
{
    QStringList names = m_variables.keys();
    names.sort(Qt::CaseInsensitive);
    return names;
}

QStringList PromptVariableResolver::suggestions(const QString &prefix) const
{
    const QString normalized = prefix.trimmed().toLower();
    QStringList out;
    for (auto it = m_variables.constBegin(); it != m_variables.constEnd(); ++it) {
        if (normalized.isEmpty() || it.key().startsWith(normalized))
            out.append(QStringLiteral("#") + it.key());
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

QString PromptVariableResolver::resolveInPrompt(const QString &text,
                                                QStringList *unresolved) const
{
    QString out = text;
    static const QRegularExpression re(QStringLiteral("#([A-Za-z_][A-Za-z0-9_]*)"));

    QList<QPair<int, int>> ranges;
    QList<QString> replacements;

    const auto matches = re.globalMatch(text);
    auto it = matches;
    while (it.hasNext()) {
        const auto m = it.next();
        const QString varName = m.captured(1).toLower();
        const auto found = m_variables.constFind(varName);
        if (found == m_variables.constEnd()) {
            if (unresolved)
                unresolved->append(varName);
            continue;
        }

        QString value = found->resolver();
        if (value.trimmed().isEmpty())
            value = QStringLiteral("(empty %1)").arg(varName);

        ranges.append({m.capturedStart(0), m.capturedLength(0)});
        replacements.append(value);
    }

    for (int i = ranges.size() - 1; i >= 0; --i) {
        out.replace(ranges[i].first, ranges[i].second, replacements[i]);
    }

    if (unresolved) {
        unresolved->removeDuplicates();
        unresolved->sort(Qt::CaseInsensitive);
    }
    return out;
}
