#include "projectbraintypes.h"

// ── ProjectRule ───────────────────────────────────────────────────────────

QJsonObject ProjectRule::toJson() const
{
    QJsonObject o;
    o[QLatin1String("id")]       = id;
    o[QLatin1String("category")] = category;
    o[QLatin1String("text")]     = text;
    o[QLatin1String("severity")] = severity;
    o[QLatin1String("scope")]    = QJsonArray::fromStringList(scope);
    return o;
}

ProjectRule ProjectRule::fromJson(const QJsonObject &o)
{
    ProjectRule r;
    r.id       = o.value(QLatin1String("id")).toString();
    r.category = o.value(QLatin1String("category")).toString();
    r.text     = o.value(QLatin1String("text")).toString();
    r.severity = o.value(QLatin1String("severity")).toString();
    const auto arr = o.value(QLatin1String("scope")).toArray();
    for (const auto &v : arr)
        r.scope.append(v.toString());
    return r;
}

// ── MemoryFact ────────────────────────────────────────────────────────────

QJsonObject MemoryFact::toJson() const
{
    QJsonObject o;
    o[QLatin1String("id")]         = id;
    o[QLatin1String("category")]   = category;
    o[QLatin1String("key")]        = key;
    o[QLatin1String("value")]      = value;
    o[QLatin1String("confidence")] = confidence;
    o[QLatin1String("source")]     = source;
    o[QLatin1String("updatedAt")]  = updatedAt;
    return o;
}

MemoryFact MemoryFact::fromJson(const QJsonObject &o)
{
    MemoryFact f;
    f.id         = o.value(QLatin1String("id")).toString();
    f.category   = o.value(QLatin1String("category")).toString();
    f.key        = o.value(QLatin1String("key")).toString();
    f.value      = o.value(QLatin1String("value")).toString();
    f.confidence = o.value(QLatin1String("confidence")).toDouble(0.6);
    f.source     = o.value(QLatin1String("source")).toString();
    f.updatedAt  = o.value(QLatin1String("updatedAt")).toString();
    return f;
}

// ── SessionSummary ────────────────────────────────────────────────────────

QJsonObject SessionSummary::toJson() const
{
    QJsonObject o;
    o[QLatin1String("id")]           = id;
    o[QLatin1String("title")]        = title;
    o[QLatin1String("objective")]    = objective;
    o[QLatin1String("visitedFiles")] = QJsonArray::fromStringList(visitedFiles);
    o[QLatin1String("findings")]     = QJsonArray::fromStringList(findings);
    o[QLatin1String("result")]       = result;
    o[QLatin1String("summary")]      = summary;
    return o;
}

SessionSummary SessionSummary::fromJson(const QJsonObject &o)
{
    SessionSummary s;
    s.id        = o.value(QLatin1String("id")).toString();
    s.title     = o.value(QLatin1String("title")).toString();
    s.objective = o.value(QLatin1String("objective")).toString();
    const auto vf = o.value(QLatin1String("visitedFiles")).toArray();
    for (const auto &v : vf) s.visitedFiles.append(v.toString());
    const auto fn = o.value(QLatin1String("findings")).toArray();
    for (const auto &v : fn) s.findings.append(v.toString());
    s.result  = o.value(QLatin1String("result")).toString();
    s.summary = o.value(QLatin1String("summary")).toString();
    return s;
}
