#include "braincontextbuilder.h"
#include "projectbrainservice.h"

BrainContextBuilder::BrainContextBuilder(ProjectBrainService *brain, QObject *parent)
    : QObject(parent), m_brain(brain)
{
}

AgentBrainContext BrainContextBuilder::buildForTask(const QString &task,
                                                    const QStringList &activeFiles) const
{
    AgentBrainContext ctx;
    if (!m_brain || !m_brain->isLoaded())
        return ctx;

    ctx.relevantRules = m_brain->relevantRules(activeFiles);
    ctx.relevantFacts = m_brain->relevantFacts(task, activeFiles);
    ctx.relatedSessions = m_brain->recentSummaries(3);

    // Load optional notes
    ctx.architectureNotes = m_brain->readNote(QStringLiteral("architecture.md"));
    ctx.buildNotes        = m_brain->readNote(QStringLiteral("build.md"));
    ctx.pitfallsNotes     = m_brain->readNote(QStringLiteral("pitfalls.md"));

    return ctx;
}

QString BrainContextBuilder::toPromptText(const AgentBrainContext &ctx) const
{
    QStringList parts;

    // Rules section
    if (!ctx.relevantRules.isEmpty()) {
        QStringList ruleLines;
        ruleLines << QStringLiteral("<project_rules>");
        for (const auto &rule : ctx.relevantRules) {
            ruleLines << QStringLiteral("- [%1/%2] %3")
                .arg(rule.severity, rule.category, rule.text);
        }
        ruleLines << QStringLiteral("</project_rules>");
        parts << ruleLines.join('\n');
    }

    // Memory facts section
    if (!ctx.relevantFacts.isEmpty()) {
        QStringList factLines;
        factLines << QStringLiteral("<project_memory>");
        for (const auto &fact : ctx.relevantFacts) {
            factLines << QStringLiteral("- %1: %2").arg(fact.key, fact.value);
        }
        factLines << QStringLiteral("</project_memory>");
        parts << factLines.join('\n');
    }

    // Recent sessions section
    if (!ctx.relatedSessions.isEmpty()) {
        QStringList sessLines;
        sessLines << QStringLiteral("<recent_sessions>");
        for (const auto &sess : ctx.relatedSessions) {
            if (!sess.summary.isEmpty()) {
                sessLines << QStringLiteral("- %1: %2").arg(sess.title, sess.summary);
            }
        }
        sessLines << QStringLiteral("</recent_sessions>");
        if (sessLines.size() > 2) // has actual content
            parts << sessLines.join('\n');
    }

    // Notes sections
    if (!ctx.architectureNotes.isEmpty()) {
        parts << QStringLiteral("<architecture_notes>\n%1\n</architecture_notes>")
            .arg(ctx.architectureNotes.left(4000)); // cap at 4k chars
    }
    if (!ctx.buildNotes.isEmpty()) {
        parts << QStringLiteral("<build_notes>\n%1\n</build_notes>")
            .arg(ctx.buildNotes.left(2000));
    }
    if (!ctx.pitfallsNotes.isEmpty()) {
        parts << QStringLiteral("<pitfall_notes>\n%1\n</pitfall_notes>")
            .arg(ctx.pitfallsNotes.left(2000));
    }

    return parts.join(QStringLiteral("\n\n"));
}
