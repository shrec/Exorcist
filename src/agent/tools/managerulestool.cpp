#include "managerulestool.h"
#include "../projectbrainservice.h"
#include "../projectbraintypes.h"

#include <QJsonArray>
#include <QJsonDocument>

ManageRulesTool::ManageRulesTool(ProjectBrainService *brain)
    : m_brain(brain)
{
}

ToolSpec ManageRulesTool::spec() const
{
    return ToolSpec{
        .name        = QStringLiteral("manage_rules"),
        .description = QStringLiteral(
            "Manage project-level rules that guide AI behavior. Rules persist across sessions "
            "and are injected into system prompts automatically.\n\n"
            "Operations:\n"
            "- add: Add a new rule (category, text, severity, optional scope patterns)\n"
            "- remove: Remove a rule by its ID\n"
            "- list: List all rules, optionally filtered by category\n\n"
            "Categories: style, safety, architecture, naming, testing, documentation, workflow\n"
            "Severities: must (hard rule), should (strong recommendation), prefer (soft preference)\n"
            "Scope: list of glob patterns limiting which files the rule applies to (empty = all files)"),
        .inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("add"),
                        QStringLiteral("remove"),
                        QStringLiteral("list")}},
                    {QStringLiteral("description"), QStringLiteral("Operation to perform")}
                }},
                {QStringLiteral("category"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Rule category: style, safety, architecture, naming, testing, documentation, workflow")}
                }},
                {QStringLiteral("text"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "The rule text — a concise, imperative statement")}
                }},
                {QStringLiteral("severity"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("must"),
                        QStringLiteral("should"),
                        QStringLiteral("prefer")}},
                    {QStringLiteral("description"), QStringLiteral(
                        "Enforcement level: must, should, or prefer")}
                }},
                {QStringLiteral("scope"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("array")},
                    {QStringLiteral("items"), QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")}}},
                    {QStringLiteral("description"), QStringLiteral(
                        "Glob patterns limiting which files this rule applies to (empty = all)")}
                }},
                {QStringLiteral("id"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Rule ID (required for remove)")}
                }}
            }}
        },
        .permission  = AgentToolPermission::SafeMutate,
        .timeoutMs   = 5000,
        .parallelSafe = true,
    };
}

ToolExecResult ManageRulesTool::invoke(const QJsonObject &args)
{
    if (!m_brain || !m_brain->isLoaded()) {
        return {false, {}, {},
                QStringLiteral("Project brain not loaded. Open a workspace first.")};
    }

    const QString op = args.value(QStringLiteral("operation")).toString();
    if (op == QLatin1String("add"))    return doAdd(args);
    if (op == QLatin1String("remove")) return doRemove(args);
    if (op == QLatin1String("list"))   return doList(args);

    return {false, {}, {},
            QStringLiteral("Unknown operation: %1. Use add, remove, or list.").arg(op)};
}

ToolExecResult ManageRulesTool::doAdd(const QJsonObject &args)
{
    const QString text = args.value(QStringLiteral("text")).toString().trimmed();
    if (text.isEmpty()) {
        return {false, {}, {},
                QStringLiteral("Missing required parameter: text")};
    }

    ProjectRule rule;
    rule.category = args.value(QStringLiteral("category")).toString(QStringLiteral("general"));
    rule.text     = text;
    rule.severity = args.value(QStringLiteral("severity")).toString(QStringLiteral("should"));

    const auto scopeArr = args.value(QStringLiteral("scope")).toArray();
    for (const auto &v : scopeArr)
        rule.scope.append(v.toString());

    m_brain->addRule(rule);
    m_brain->save();

    return {true,
            rule.toJson(),
            QStringLiteral("Rule added: [%1/%2] %3 (id: %4)")
                .arg(rule.severity, rule.category, rule.text, rule.id),
            {}};
}

ToolExecResult ManageRulesTool::doRemove(const QJsonObject &args)
{
    const QString id = args.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
        return {false, {}, {},
                QStringLiteral("Missing required parameter: id")};
    }

    m_brain->removeRule(id);
    m_brain->save();

    return {true, {},
            QStringLiteral("Rule removed: %1").arg(id),
            {}};
}

ToolExecResult ManageRulesTool::doList(const QJsonObject &args)
{
    const QString filterCat = args.value(QStringLiteral("category")).toString();

    QJsonArray arr;
    for (const auto &rule : m_brain->rules()) {
        if (!filterCat.isEmpty() && rule.category != filterCat)
            continue;
        arr.append(rule.toJson());
    }

    QJsonObject data;
    data[QStringLiteral("count")] = arr.size();
    data[QStringLiteral("rules")] = arr;

    return {true, data,
            QStringLiteral("%1 rule(s) found.").arg(arr.size()),
            {}};
}
