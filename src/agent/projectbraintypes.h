#pragma once

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

/// A workspace rule that constrains AI behavior (e.g. style, safety, scope).
struct ProjectRule {
    QString     id;
    QString     category;
    QString     text;
    QString     severity;
    QStringList scope;

    QJsonObject toJson() const;
    static ProjectRule fromJson(const QJsonObject &o);
};

struct MemoryFact {
    QString id;
    QString category;
    QString key;
    QString value;
    double  confidence = 0.6;
    QString source;
    QString updatedAt;

    QJsonObject toJson() const;
    static MemoryFact fromJson(const QJsonObject &o);
};

struct SessionSummary {
    QString     id;
    QString     title;
    QString     objective;
    QStringList visitedFiles;
    QStringList findings;
    QString     result;
    QString     summary;

    QJsonObject toJson() const;
    static SessionSummary fromJson(const QJsonObject &o);
};
