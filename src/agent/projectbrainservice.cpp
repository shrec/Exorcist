#include "projectbrainservice.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUuid>

ProjectBrainService::ProjectBrainService(QObject *parent)
    : QObject(parent)
{
}

// ── Persistence ──────────────────────────────────────────────────────────────

bool ProjectBrainService::load(const QString &projectRoot)
{
    if (projectRoot.isEmpty())
        return false;

    m_root = projectRoot;
    m_rules.clear();
    m_facts.clear();
    m_summaries.clear();

    const QString brainDir = m_root + QStringLiteral("/.exorcist");
    if (!QDir(brainDir).exists())
        return true; // No data yet — fresh workspace.

    // Load rules
    QJsonDocument rulesDoc;
    if (loadJson(brainDir + QStringLiteral("/rules.json"), rulesDoc)) {
        const auto arr = rulesDoc.array();
        for (const auto &v : arr)
            m_rules.append(ProjectRule::fromJson(v.toObject()));
    }

    // Load facts (memory)
    QJsonDocument memDoc;
    if (loadJson(brainDir + QStringLiteral("/memory.json"), memDoc)) {
        const auto arr = memDoc.array();
        for (const auto &v : arr)
            m_facts.append(MemoryFact::fromJson(v.toObject()));
    }

    // Load session summaries
    QJsonDocument sessDoc;
    if (loadJson(brainDir + QStringLiteral("/sessions.json"), sessDoc)) {
        const auto arr = sessDoc.array();
        for (const auto &v : arr)
            m_summaries.append(SessionSummary::fromJson(v.toObject()));
    }

    return true;
}

bool ProjectBrainService::save() const
{
    if (m_root.isEmpty())
        return false;

    const QString brainDir = m_root + QStringLiteral("/.exorcist");
    if (!ensureDirectoryExists(brainDir))
        return false;
    if (!ensureDirectoryExists(brainDir + QStringLiteral("/notes")))
        return false;

    // Save rules
    {
        QJsonArray arr;
        for (const auto &r : m_rules) arr.append(r.toJson());
        if (!saveJson(brainDir + QStringLiteral("/rules.json"), QJsonDocument(arr)))
            return false;
    }

    // Save facts
    {
        QJsonArray arr;
        for (const auto &f : m_facts) arr.append(f.toJson());
        if (!saveJson(brainDir + QStringLiteral("/memory.json"), QJsonDocument(arr)))
            return false;
    }

    // Save session summaries
    {
        QJsonArray arr;
        for (const auto &s : m_summaries) arr.append(s.toJson());
        if (!saveJson(brainDir + QStringLiteral("/sessions.json"), QJsonDocument(arr)))
            return false;
    }

    return true;
}

// ── Rules ────────────────────────────────────────────────────────────────────

void ProjectBrainService::addRule(const ProjectRule &rule)
{
    ProjectRule r = rule;
    if (r.id.isEmpty())
        r.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_rules.append(r);
    emit rulesChanged();
}

void ProjectBrainService::removeRule(const QString &id)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == id) {
            m_rules.removeAt(i);
            emit rulesChanged();
            return;
        }
    }
}

QList<ProjectRule> ProjectBrainService::relevantRules(const QStringList &activeFiles) const
{
    QList<ProjectRule> result;
    for (const auto &rule : m_rules) {
        if (rule.scope.isEmpty() || scopeMatches(rule.scope, activeFiles))
            result.append(rule);
    }
    return result;
}

// ── Facts ────────────────────────────────────────────────────────────────────

void ProjectBrainService::addFact(const MemoryFact &fact)
{
    MemoryFact f = fact;
    if (f.id.isEmpty())
        f.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (f.updatedAt.isEmpty())
        f.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_facts.append(f);
    emit factsChanged();
}

void ProjectBrainService::updateFact(const MemoryFact &fact)
{
    for (int i = 0; i < m_facts.size(); ++i) {
        if (m_facts[i].id == fact.id) {
            m_facts[i] = fact;
            m_facts[i].updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
            emit factsChanged();
            return;
        }
    }
}

void ProjectBrainService::forgetFact(const QString &id)
{
    for (int i = 0; i < m_facts.size(); ++i) {
        if (m_facts[i].id == id) {
            m_facts.removeAt(i);
            emit factsChanged();
            return;
        }
    }
}

QList<MemoryFact> ProjectBrainService::relevantFacts(const QString &query,
                                                     const QStringList &activeFiles) const
{
    QList<MemoryFact> result;
    if (query.isEmpty() && activeFiles.isEmpty())
        return m_facts; // return all if no filter

    const QStringList queryWords = query.toLower().split(
        QRegularExpression(QStringLiteral("\\W+")), Qt::SkipEmptyParts);

    for (const auto &fact : m_facts) {
        // Score: how many query words appear in key+value+category
        const QString haystack = (fact.key + ' ' + fact.value + ' ' + fact.category).toLower();
        int hits = 0;
        for (const auto &w : queryWords) {
            if (haystack.contains(w))
                ++hits;
        }
        // Include if ≥50% of query words match, or if any active file relates
        if (!queryWords.isEmpty() && hits * 2 >= queryWords.size()) {
            result.append(fact);
        } else {
            // Check if fact category mentions an active file path component
            for (const auto &fp : activeFiles) {
                const QString baseName = fp.section('/', -1).section('.', 0, 0).toLower();
                if (!baseName.isEmpty() && haystack.contains(baseName)) {
                    result.append(fact);
                    break;
                }
            }
        }
    }

    return result;
}

// ── Session Summaries ────────────────────────────────────────────────────────

void ProjectBrainService::addSessionSummary(const SessionSummary &summary)
{
    SessionSummary s = summary;
    if (s.id.isEmpty())
        s.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_summaries.append(s);

    // Keep at most 50 summaries
    while (m_summaries.size() > 50)
        m_summaries.removeFirst();

    emit summariesChanged();
}

QList<SessionSummary> ProjectBrainService::recentSummaries(int n) const
{
    if (n >= m_summaries.size())
        return m_summaries;
    return m_summaries.mid(m_summaries.size() - n);
}

// ── Notes ────────────────────────────────────────────────────────────────────

QString ProjectBrainService::readNote(const QString &name) const
{
    if (m_root.isEmpty() || name.isEmpty())
        return {};

    // Sanitize name to prevent path traversal
    const QString safeName = QFileInfo(name).fileName();
    const QString path = m_root + QStringLiteral("/.exorcist/notes/") + safeName;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(file.readAll());
}

bool ProjectBrainService::writeNote(const QString &name, const QString &content)
{
    if (m_root.isEmpty() || name.isEmpty())
        return false;

    const QString safeName = QFileInfo(name).fileName();
    const QString notesDir = m_root + QStringLiteral("/.exorcist/notes");
    if (!ensureDirectoryExists(notesDir))
        return false;

    const QString path = notesDir + '/' + safeName;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(content.toUtf8());
    return true;
}

// ── Helpers ──────────────────────────────────────────────────────────────────

bool ProjectBrainService::loadJson(const QString &path, QJsonDocument &out) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonParseError error;
    out = QJsonDocument::fromJson(file.readAll(), &error);
    return error.error == QJsonParseError::NoError;
}

bool ProjectBrainService::saveJson(const QString &path, const QJsonDocument &doc) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool ProjectBrainService::ensureDirectoryExists(const QString &dirPath) const
{
    QDir dir(dirPath);
    if (dir.exists())
        return true;
    return QDir().mkpath(dirPath);
}

bool ProjectBrainService::scopeMatches(const QStringList &scope,
                                       const QStringList &files) const
{
    for (const auto &pattern : scope) {
        const QRegularExpression rx(
            QRegularExpression::wildcardToRegularExpression(pattern));
        for (const auto &file : files) {
            // Match against relative path from workspace root
            QString relPath = file;
            if (relPath.startsWith(m_root))
                relPath = relPath.mid(m_root.size() + 1);
            relPath.replace('\\', '/');
            if (rx.match(relPath).hasMatch())
                return true;
        }
    }
    return false;
}
