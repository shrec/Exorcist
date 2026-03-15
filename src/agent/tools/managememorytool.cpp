#include "managememorytool.h"
#include "../projectbrainservice.h"
#include "../projectbraintypes.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

ManageMemoryTool::ManageMemoryTool(ProjectBrainService *brain)
    : m_brain(brain)
{
}

ToolSpec ManageMemoryTool::spec() const
{
    return ToolSpec{
        .name        = QStringLiteral("manage_memory"),
        .description = QStringLiteral(
            "Manage project-level memory: persistent facts and markdown notes that are "
            "injected into system prompts automatically across sessions.\n\n"
            "Operations:\n"
            "- add_fact: Store a key-value fact (architecture pattern, dependency, convention)\n"
            "- update_fact: Update an existing fact by ID\n"
            "- forget_fact: Remove a fact by ID\n"
            "- list_facts: List all facts, optionally filtered by category or query\n"
            "- read_note: Read a markdown note (architecture.md, build.md, pitfalls.md)\n"
            "- write_note: Write or overwrite a markdown note\n"
            "- list_notes: List all available notes\n\n"
            "Fact categories: architecture, dependency, pattern, convention, tooling, performance, bug\n"
            "Reserved notes: architecture.md, build.md, pitfalls.md (auto-injected into prompts)"),
        .inputSchema = QJsonObject{
            {QStringLiteral("type"), QStringLiteral("object")},
            {QStringLiteral("required"), QJsonArray{QStringLiteral("operation")}},
            {QStringLiteral("properties"), QJsonObject{
                {QStringLiteral("operation"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("enum"), QJsonArray{
                        QStringLiteral("add_fact"),
                        QStringLiteral("update_fact"),
                        QStringLiteral("forget_fact"),
                        QStringLiteral("list_facts"),
                        QStringLiteral("read_note"),
                        QStringLiteral("write_note"),
                        QStringLiteral("list_notes")}},
                    {QStringLiteral("description"), QStringLiteral("Operation to perform")}
                }},
                {QStringLiteral("category"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Fact category: architecture, dependency, pattern, convention, tooling, performance, bug")}
                }},
                {QStringLiteral("key"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Fact key — a short identifier (e.g. 'build_system', 'main_language')")}
                }},
                {QStringLiteral("value"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Fact value — the information to store")}
                }},
                {QStringLiteral("confidence"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("number")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Confidence level 0.0-1.0 (default 0.8)")}
                }},
                {QStringLiteral("source"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Where this fact was learned from (e.g. 'CMakeLists.txt', 'user')")}
                }},
                {QStringLiteral("query"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Search query for filtering facts")}
                }},
                {QStringLiteral("id"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Fact ID (required for update_fact, forget_fact)")}
                }},
                {QStringLiteral("name"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Note filename (e.g. 'architecture.md', 'build.md', 'pitfalls.md')")}
                }},
                {QStringLiteral("content"), QJsonObject{
                    {QStringLiteral("type"), QStringLiteral("string")},
                    {QStringLiteral("description"), QStringLiteral(
                        "Markdown content for write_note")}
                }}
            }}
        },
        .permission  = AgentToolPermission::SafeMutate,
        .timeoutMs   = 5000,
        .parallelSafe = true,
    };
}

ToolExecResult ManageMemoryTool::invoke(const QJsonObject &args)
{
    if (!m_brain || !m_brain->isLoaded()) {
        return {false, {}, {},
                QStringLiteral("Project brain not loaded. Open a workspace first.")};
    }

    const QString op = args.value(QStringLiteral("operation")).toString();
    if (op == QLatin1String("add_fact"))    return doAddFact(args);
    if (op == QLatin1String("update_fact")) return doUpdateFact(args);
    if (op == QLatin1String("forget_fact")) return doForgetFact(args);
    if (op == QLatin1String("list_facts"))  return doListFacts(args);
    if (op == QLatin1String("read_note"))   return doReadNote(args);
    if (op == QLatin1String("write_note"))  return doWriteNote(args);
    if (op == QLatin1String("list_notes"))  return doListNotes();

    return {false, {}, {},
            QStringLiteral("Unknown operation: %1").arg(op)};
}

ToolExecResult ManageMemoryTool::doAddFact(const QJsonObject &args)
{
    const QString key = args.value(QStringLiteral("key")).toString().trimmed();
    const QString value = args.value(QStringLiteral("value")).toString().trimmed();
    if (key.isEmpty() || value.isEmpty()) {
        return {false, {}, {},
                QStringLiteral("Missing required parameters: key and value")};
    }

    MemoryFact fact;
    fact.category   = args.value(QStringLiteral("category")).toString(QStringLiteral("general"));
    fact.key        = key;
    fact.value      = value;
    fact.confidence = args.value(QStringLiteral("confidence")).toDouble(0.8);
    fact.source     = args.value(QStringLiteral("source")).toString(QStringLiteral("agent"));

    m_brain->addFact(fact);
    m_brain->save();

    return {true,
            fact.toJson(),
            QStringLiteral("Fact stored: %1 = %2 (id: %3)")
                .arg(fact.key, fact.value, fact.id),
            {}};
}

ToolExecResult ManageMemoryTool::doUpdateFact(const QJsonObject &args)
{
    const QString id = args.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
        return {false, {}, {},
                QStringLiteral("Missing required parameter: id")};
    }

    // Find existing fact
    const auto &facts = m_brain->facts();
    const MemoryFact *existing = nullptr;
    for (const auto &f : facts) {
        if (f.id == id) {
            existing = &f;
            break;
        }
    }
    if (!existing) {
        return {false, {}, {},
                QStringLiteral("Fact not found: %1").arg(id)};
    }

    MemoryFact updated = *existing;
    if (args.contains(QStringLiteral("key")))
        updated.key = args.value(QStringLiteral("key")).toString();
    if (args.contains(QStringLiteral("value")))
        updated.value = args.value(QStringLiteral("value")).toString();
    if (args.contains(QStringLiteral("category")))
        updated.category = args.value(QStringLiteral("category")).toString();
    if (args.contains(QStringLiteral("confidence")))
        updated.confidence = args.value(QStringLiteral("confidence")).toDouble();
    if (args.contains(QStringLiteral("source")))
        updated.source = args.value(QStringLiteral("source")).toString();

    m_brain->updateFact(updated);
    m_brain->save();

    return {true,
            updated.toJson(),
            QStringLiteral("Fact updated: %1 = %2").arg(updated.key, updated.value),
            {}};
}

ToolExecResult ManageMemoryTool::doForgetFact(const QJsonObject &args)
{
    const QString id = args.value(QStringLiteral("id")).toString().trimmed();
    if (id.isEmpty()) {
        return {false, {}, {},
                QStringLiteral("Missing required parameter: id")};
    }

    m_brain->forgetFact(id);
    m_brain->save();

    return {true, {},
            QStringLiteral("Fact removed: %1").arg(id),
            {}};
}

ToolExecResult ManageMemoryTool::doListFacts(const QJsonObject &args)
{
    const QString query = args.value(QStringLiteral("query")).toString();
    const QString filterCat = args.value(QStringLiteral("category")).toString();

    QList<MemoryFact> matching;
    if (query.isEmpty() && filterCat.isEmpty()) {
        matching = m_brain->facts();
    } else {
        matching = m_brain->relevantFacts(query, {});
    }

    QJsonArray arr;
    for (const auto &fact : matching) {
        if (!filterCat.isEmpty() && fact.category != filterCat)
            continue;
        arr.append(fact.toJson());
    }

    QJsonObject data;
    data[QStringLiteral("count")] = arr.size();
    data[QStringLiteral("facts")] = arr;

    return {true, data,
            QStringLiteral("%1 fact(s) found.").arg(arr.size()),
            {}};
}

ToolExecResult ManageMemoryTool::doReadNote(const QJsonObject &args)
{
    const QString name = args.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty()) {
        return {false, {}, {},
                QStringLiteral("Missing required parameter: name")};
    }

    const QString content = m_brain->readNote(name);
    if (content.isEmpty()) {
        return {true, {},
                QStringLiteral("Note '%1' is empty or does not exist.").arg(name),
                {}};
    }

    QJsonObject data;
    data[QStringLiteral("name")]    = name;
    data[QStringLiteral("content")] = content;

    return {true, data, content, {}};
}

ToolExecResult ManageMemoryTool::doWriteNote(const QJsonObject &args)
{
    const QString name = args.value(QStringLiteral("name")).toString().trimmed();
    const QString content = args.value(QStringLiteral("content")).toString();
    if (name.isEmpty()) {
        return {false, {}, {},
                QStringLiteral("Missing required parameter: name")};
    }

    if (!m_brain->writeNote(name, content)) {
        return {false, {}, {},
                QStringLiteral("Failed to write note '%1'.").arg(name)};
    }
    m_brain->save();

    return {true, {},
            QStringLiteral("Note '%1' written (%2 chars).").arg(name).arg(content.size()),
            {}};
}

ToolExecResult ManageMemoryTool::doListNotes()
{
    const QString notesDir = m_brain->root() + QStringLiteral("/.exorcist/notes");
    QJsonArray arr;

    QDir dir(notesDir);
    if (dir.exists()) {
        const auto entries = dir.entryList({QStringLiteral("*.md")}, QDir::Files, QDir::Name);
        for (const auto &name : entries) {
            QJsonObject entry;
            entry[QStringLiteral("name")] = name;
            QFileInfo fi(dir.filePath(name));
            entry[QStringLiteral("size")] = fi.size();
            arr.append(entry);
        }
    }

    QJsonObject data;
    data[QStringLiteral("count")] = arr.size();
    data[QStringLiteral("notes")] = arr;

    return {true, data,
            QStringLiteral("%1 note(s) available.").arg(arr.size()),
            {}};
}
