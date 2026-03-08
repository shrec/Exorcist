#include "promptvariables.h"

#include <QRegularExpression>

void PromptVariableResolver::registerVar(const PromptVar &var,
                                         std::function<QString()> getter)
{
    if (var.token.isEmpty() || !getter)
        return;
    if (!m_getters.contains(var.token))
        m_vars.append(var);
    m_getters[var.token] = std::move(getter);
}

void PromptVariableResolver::registerVariable(const PromptVariable &var)
{
    registerVar({var.token, var.description}, var.getter);
}

QVector<PromptVar> PromptVariableResolver::matchingVars(const QString &partial) const
{
    QVector<PromptVar> result;
    for (const auto &var : m_vars) {
        if (var.token.startsWith(partial, Qt::CaseInsensitive))
            result.append(var);
    }
    return result;
}

QString PromptVariableResolver::resolve(const QString &promptText) const
{
    return resolveInPrompt(promptText, nullptr);
}

QString PromptVariableResolver::resolveInPrompt(const QString &promptText,
                                                QStringList *unresolved) const
{
    static const QRegularExpression re(QStringLiteral("#([A-Za-z0-9_]+)"));
    QString output;
    int last = 0;

    auto it = re.globalMatch(promptText);
    while (it.hasNext()) {
        const auto m = it.next();
        output += promptText.mid(last, m.capturedStart() - last);

        const QString token = m.captured(1);
        auto getterIt = m_getters.find(token);
        if (getterIt != m_getters.end()) {
            QString payload = getterIt.value()();
            if (payload.endsWith(QLatin1Char('\n')))
                payload.chop(1);
            output += QStringLiteral("[Context: #%1]\n```%2\n```\n[End context]\n")
                          .arg(token, payload);
        } else {
            if (unresolved && !unresolved->contains(token))
                unresolved->append(token);
            output += m.captured(0);
        }

        last = m.capturedEnd();
    }

    output += promptText.mid(last);
    return output;
}
