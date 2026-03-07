#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>

struct PromptVariable
{
    QString name;
    QString description;
    std::function<QString()> resolver;
};

class PromptVariableResolver : public QObject
{
    Q_OBJECT

public:
    explicit PromptVariableResolver(QObject *parent = nullptr);

    void registerVariable(const PromptVariable &var);
    bool hasVariable(const QString &name) const;

    QStringList variableNames() const;
    QStringList suggestions(const QString &prefix) const;

    QString resolveInPrompt(const QString &text,
                            QStringList *unresolved = nullptr) const;

private:
    QHash<QString, PromptVariable> m_variables;
};
