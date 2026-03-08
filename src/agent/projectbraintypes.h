#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

/// A workspace rule that constrains AI behavior (e.g. style, safety, scope).
struct ProjectRule {
    QString     id;
    QString     category;   // "style", "safety", "architecture", "testing"
    QString     text;       // rule content
    QString     severity;   // "must", "should", "may"
    QStringList scope;      // glob patterns: ["src/crypto/**", "*.h"]

    QJsonObject toJson() const {
        QJsonObject o;
        o[QLatin1String("id")]       = id;
        o[QLatin1String("category")] = category;
        o[QLatin1String("text")]     = text;
        o[QLatin1String("severity")] = severity;
        o[QLatin1String("scope")]    = QJsonArray::fromStringList(scope);
        return o;
    }

    static ProjectRule fromJson(const QJsonObject &o) {
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
};

/// A persistent fact the AI has learned about the project.
struct MemoryFact {
    QString id;
    QString category;   // "architecture", "build", "tests", "pitfalls", "conventions"
    QString key;        // short identifier
    QString value;      // fact content
    double  confidence = 0.6; // 1.0=user, 0.9=tool_verified, 0.6=inferred
    QString source;     // "user_confirmed" | "agent_inferred" | "tool_verified"
    QString updatedAt;  // ISO 8601

    QJsonObject toJson() const {
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

    static MemoryFact fromJson(const QJsonObject &o) {
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
};

/// Summary of a completed agent session for cross-session knowledge.
struct SessionSummary {
    QString     id;
    QString     title;
    QString     objective;
    QStringList visitedFiles;
    QStringList findings;
    QString     result;  // "success" | "partial" | "failed"
    QString     summary;

    QJsonObject toJson() const {
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

    static SessionSummary fromJson(const QJsonObject &o) {
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
};
