#include "memorysuggestionengine.h"
#include "projectbrainservice.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QUuid>

MemorySuggestionEngine::MemorySuggestionEngine(ProjectBrainService *brain,
                                               QObject *parent)
    : QObject(parent), m_brain(brain)
{
}

void MemorySuggestionEngine::suggestFacts(const QString &userPrompt,
                                          const QString &assistantResponse,
                                          const QStringList &touchedFiles)
{
    if (!m_brain || !m_brain->isLoaded())
        return;

    // Skip trivially short responses
    if (assistantResponse.size() < 80)
        return;

    QList<MemoryFact> candidates;
    extractPitfalls(userPrompt, assistantResponse, candidates);
    extractBuildFacts(assistantResponse, candidates);
    extractArchitectureFacts(userPrompt, assistantResponse, touchedFiles, candidates);

    for (const auto &fact : std::as_const(candidates)) {
        if (!isDuplicate(fact))
            emit factSuggested(fact);
    }
}

// ── Pitfall extractor ────────────────────────────────────────────────────────

void MemorySuggestionEngine::extractPitfalls(const QString &userPrompt,
                                             const QString &response,
                                             QList<MemoryFact> &out)
{
    // Heuristic: if the user prompt contains error/fix/bug/crash keywords and
    // the response mentions a specific cause, capture it.
    static const QRegularExpression rxError(
        QStringLiteral("\\b(error|bug|crash|fix|broke|fail|wrong|issue)\\b"),
        QRegularExpression::CaseInsensitiveOption);

    if (!rxError.match(userPrompt).hasMatch())
        return;

    // Look for sentences in the response that describe the root cause.
    // Pattern: "The problem was..." / "The issue is..." / "Fixed by..."
    static const QRegularExpression rxCause(
        QStringLiteral("(?:the (?:problem|issue|bug|cause|fix) (?:was|is|requires?)|"
                        "fixed by|the solution is|you need to|must be|should be)"
                        "\\s+(.{10,120}?)[.!\\n]"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = rxCause.globalMatch(response);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString sentence = match.captured(0).trimmed();

        MemoryFact fact;
        fact.id         = QUuid::createUuid().toString(QUuid::WithoutBraces);
        fact.category   = QStringLiteral("pitfalls");
        fact.key        = sentence.left(60);
        fact.value      = sentence;
        fact.confidence = 0.6;
        fact.source     = QStringLiteral("agent_inferred");
        fact.updatedAt  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        out.append(fact);
        break; // one pitfall per turn is enough
    }
}

// ── Build fact extractor ─────────────────────────────────────────────────────

void MemorySuggestionEngine::extractBuildFacts(const QString &response,
                                               QList<MemoryFact> &out)
{
    // Look for build/test commands in code blocks: cmake, make, ninja, cargo, npm
    static const QRegularExpression rxBuild(
        QStringLiteral("```(?:bash|sh|shell|cmd|powershell)?\\s*\\n"
                        "((?:cmake|make|ninja|cargo|npm|yarn|pip|python|dotnet|"
                        "gradle|mvn|go)\\s[^`]{5,120})\\n```"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = rxBuild.globalMatch(response);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString cmd = match.captured(1).trimmed();

        MemoryFact fact;
        fact.id         = QUuid::createUuid().toString(QUuid::WithoutBraces);
        fact.category   = QStringLiteral("build");
        fact.key        = QStringLiteral("Build command: %1").arg(cmd.left(50));
        fact.value      = cmd;
        fact.confidence = 0.7;
        fact.source     = QStringLiteral("agent_inferred");
        fact.updatedAt  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        out.append(fact);
        break; // one per turn
    }
}

// ── Architecture extractor ───────────────────────────────────────────────────

void MemorySuggestionEngine::extractArchitectureFacts(const QString &userPrompt,
                                                      const QString &response,
                                                      const QStringList &files,
                                                      QList<MemoryFact> &out)
{
    Q_UNUSED(userPrompt)

    // If multiple files were touched (≥3), note the pattern.
    if (files.size() < 3)
        return;

    // Look for architectural remarks: "depends on", "inherits from",
    // "communicates via", "registered in"
    static const QRegularExpression rxArch(
        QStringLiteral("(?:depends on|inherits from|communicates (?:via|through)|"
                        "registered (?:in|via)|coupled to|decoupled from|"
                        "abstracted (?:by|via))\\s+(.{5,80}?)[.!,\\n]"),
        QRegularExpression::CaseInsensitiveOption);

    auto it = rxArch.globalMatch(response);
    while (it.hasNext()) {
        const auto match = it.next();
        const QString sentence = match.captured(0).trimmed();

        MemoryFact fact;
        fact.id         = QUuid::createUuid().toString(QUuid::WithoutBraces);
        fact.category   = QStringLiteral("architecture");
        fact.key        = sentence.left(60);
        fact.value      = sentence;
        fact.confidence = 0.5;
        fact.source     = QStringLiteral("agent_inferred");
        fact.updatedAt  = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        out.append(fact);
        break;
    }
}

// ── Dedup ────────────────────────────────────────────────────────────────────

bool MemorySuggestionEngine::isDuplicate(const MemoryFact &fact) const
{
    const auto &existing = m_brain->facts();
    for (const auto &f : existing) {
        // Same category + similar key → duplicate
        if (f.category == fact.category && f.key == fact.key)
            return true;
        // Fuzzy: if value overlap > 70%
        if (f.category == fact.category) {
            const auto fWords = f.value.toLower().split(' ', Qt::SkipEmptyParts);
            const auto cWords = fact.value.toLower().split(' ', Qt::SkipEmptyParts);
            if (fWords.isEmpty() || cWords.isEmpty()) continue;
            int overlap = 0;
            for (const auto &w : cWords) {
                if (fWords.contains(w)) ++overlap;
            }
            if (overlap * 10 > cWords.size() * 7) // >70% overlap
                return true;
        }
    }
    return false;
}
