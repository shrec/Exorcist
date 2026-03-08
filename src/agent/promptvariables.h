#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

struct PromptVar
{
    QString token;        // "file", "selection", "problems", "terminal", "codebase"
    QString description;  // shown in autocomplete popup
};

struct PromptVariable
{
    QString token;
    QString description;
    std::function<QString()> getter;
};

class PromptVariableResolver : public QObject
{
    Q_OBJECT

public:
    explicit PromptVariableResolver(QObject *parent = nullptr) : QObject(parent) {}

    void registerVar(const PromptVar &var, std::function<QString()> getter);
    void registerVariable(const PromptVariable &var);
    QVector<PromptVar> matchingVars(const QString &partial) const;
    QString resolve(const QString &promptText) const;
    QString resolveInPrompt(const QString &promptText, QStringList *unresolved) const;

private:
    QMap<QString, std::function<QString()>> m_getters;
    QVector<PromptVar> m_vars;
};
