#include "contextbuilder.h"
#include "../core/ifilesystem.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>

ContextBuilder::ContextBuilder(QObject *parent)
    : QObject(parent)
{
}

// ── Public API ────────────────────────────────────────────────────────────────

ContextSnapshot ContextBuilder::buildContext()
{
    ContextSnapshot ctx;
    ctx.workspaceRoot = m_workspaceRoot;

    // Inject custom instructions as highest-priority Custom item
    if (!m_customInstructions.isEmpty()) {
        ContextItem ci;
        ci.type     = ContextItem::Type::Custom;
        ci.label    = QStringLiteral("Custom instructions");
        ci.content  = m_customInstructions;
        ci.priority = 20;
        ctx.items.append(ci);
    }

    addOpenFilesContext(ctx);
    addDiagnosticsContext(ctx);
    addGitContext(ctx);
    addTerminalContext(ctx);
    addWorkspaceInfoContext(ctx);

    // Apply pinned flags based on m_pinnedTypes
    for (auto &item : ctx.items) {
        if (m_pinnedTypes.contains(item.type))
            item.pinned = true;
    }

    pruneContext(ctx, m_maxContextChars);

    return ctx;
}

ContextSnapshot ContextBuilder::buildContext(const QString &userPrompt,
                                             const QString &activeFilePath,
                                             const QString &selectedText,
                                             const QString &languageId)
{
    Q_UNUSED(userPrompt)

    ContextSnapshot ctx;
    ctx.workspaceRoot  = m_workspaceRoot;
    ctx.activeFilePath = activeFilePath;
    ctx.languageId     = languageId;
    ctx.selectedText   = selectedText;

    // Inject custom instructions as highest-priority Custom item
    if (!m_customInstructions.isEmpty()) {
        ContextItem ci;
        ci.type     = ContextItem::Type::Custom;
        ci.label    = QStringLiteral("Custom instructions");
        ci.content  = m_customInstructions;
        ci.priority = 20;
        ctx.items.append(ci);
    }

    // Add contextual items
    if (!activeFilePath.isEmpty() && isContextTypeEnabled(ContextItem::Type::ActiveFile))
        addFileContext(ctx, activeFilePath);

    if (!selectedText.isEmpty() && isContextTypeEnabled(ContextItem::Type::Selection))
        addSelectionContext(ctx, selectedText);

    if (isContextTypeEnabled(ContextItem::Type::OpenFiles))
        addOpenFilesContext(ctx);
    if (isContextTypeEnabled(ContextItem::Type::Diagnostics))
        addDiagnosticsContext(ctx);
    if (isContextTypeEnabled(ContextItem::Type::GitDiff))
        addGitContext(ctx);
    if (isContextTypeEnabled(ContextItem::Type::WorkspaceInfo))
        addTerminalContext(ctx);
    if (isContextTypeEnabled(ContextItem::Type::WorkspaceInfo))
        addWorkspaceInfoContext(ctx);

    // Apply pinned flags based on m_pinnedTypes
    for (auto &item : ctx.items) {
        if (m_pinnedTypes.contains(item.type))
            item.pinned = true;
    }

    pruneContext(ctx, m_maxContextChars);

    return ctx;
}

QFuture<ContextSnapshot> ContextBuilder::buildContextAsync(
    const QString &userPrompt,
    const QString &activeFilePath,
    const QString &selectedText,
    const QString &languageId)
{
    return QtConcurrent::run([this, userPrompt, activeFilePath,
                              selectedText, languageId]() {
        return buildContext(userPrompt, activeFilePath,
                            selectedText, languageId);
    });
}

// ── Internal helpers ──────────────────────────────────────────────────────────

void ContextBuilder::addFileContext(ContextSnapshot &ctx, const QString &filePath)
{
    if (!m_fs || filePath.isEmpty())
        return;

    if (!m_fs->exists(filePath))
        return;

    QString error;
    const QString content = m_fs->readTextFile(filePath, &error);
    if (!error.isEmpty())
        return;

    // Only include files up to ~100KB to avoid blowing context
    if (content.size() > 100 * 1024)
        return;

    ContextItem item;
    item.type     = ContextItem::Type::ActiveFile;
    item.label    = QStringLiteral("Active file: %1").arg(
        QDir(m_workspaceRoot).relativeFilePath(filePath));
    item.content  = content;
    item.filePath = filePath;
    item.priority = 10;
    ctx.items.append(item);
}

void ContextBuilder::addSelectionContext(ContextSnapshot &ctx,
                                         const QString &selectedText)
{
    if (selectedText.isEmpty())
        return;

    ContextItem item;
    item.type     = ContextItem::Type::Selection;
    item.label    = QStringLiteral("Selected text");
    item.content  = selectedText;
    item.priority = 15; // Higher priority than file content
    ctx.items.append(item);
}

void ContextBuilder::addOpenFilesContext(ContextSnapshot &ctx)
{
    if (!m_openFilesGetter)
        return;

    const QStringList files = m_openFilesGetter();
    if (files.isEmpty())
        return;

    QString listing;
    for (const QString &f : files) {
        const QString rel = m_workspaceRoot.isEmpty()
            ? f
            : QDir(m_workspaceRoot).relativeFilePath(f);
        listing += rel + QLatin1Char('\n');
    }

    ContextItem item;
    item.type     = ContextItem::Type::OpenFiles;
    item.label    = QStringLiteral("Open files");
    item.content  = listing;
    item.priority = 3;
    ctx.items.append(item);
}

void ContextBuilder::addDiagnosticsContext(ContextSnapshot &ctx)
{
    if (!m_diagnosticsGetter)
        return;

    const QList<AgentDiagnostic> diags = m_diagnosticsGetter();
    if (diags.isEmpty())
        return;

    QString text;
    for (const auto &d : diags) {
        QString sev;
        switch (d.severity) {
        case AgentDiagnostic::Severity::Error:   sev = QStringLiteral("error"); break;
        case AgentDiagnostic::Severity::Warning: sev = QStringLiteral("warning"); break;
        case AgentDiagnostic::Severity::Info:    sev = QStringLiteral("info"); break;
        }
        text += QStringLiteral("%1:%2: %3: %4\n")
                    .arg(d.filePath).arg(d.line).arg(sev, d.message);
    }

    ContextItem item;
    item.type     = ContextItem::Type::Diagnostics;
    item.label    = QStringLiteral("Diagnostics (%1)").arg(diags.size());
    item.content  = text;
    item.priority = 8;
    ctx.items.append(item);
}

void ContextBuilder::addGitContext(ContextSnapshot &ctx)
{
    if (m_gitStatusGetter) {
        const QString status = m_gitStatusGetter();
        if (!status.isEmpty()) {
            ContextItem item;
            item.type     = ContextItem::Type::GitStatus;
            item.label    = QStringLiteral("Git status");
            item.content  = status;
            item.priority = 5;
            ctx.items.append(item);
        }
    }

    if (m_gitDiffGetter) {
        const QString diff = m_gitDiffGetter();
        if (!diff.isEmpty() && diff.size() < 50 * 1024) { // truncate large diffs
            ContextItem item;
            item.type     = ContextItem::Type::GitDiff;
            item.label    = QStringLiteral("Git diff");
            item.content  = diff;
            item.priority = 4;
            ctx.items.append(item);
        }
    }
}

void ContextBuilder::addTerminalContext(ContextSnapshot &ctx)
{
    if (!m_terminalOutputGetter)
        return;

    const QString output = m_terminalOutputGetter();
    if (output.isEmpty())
        return;

    ContextItem item;
    item.type     = ContextItem::Type::Custom;
    item.label    = QStringLiteral("Terminal output");
    item.content  = output;
    item.priority = 6;
    ctx.items.append(item);
}

void ContextBuilder::addWorkspaceInfoContext(ContextSnapshot &ctx)
{
    if (m_workspaceRoot.isEmpty())
        return;

    QDir root(m_workspaceRoot);
    if (!root.exists())
        return;

    // Build depth-limited directory tree (max depth 3, max 300 entries)
    QStringList tree;
    std::function<void(const QDir &, int, int)> collectTree =
        [&tree, &collectTree, &root](const QDir &dir, int depth, int maxDepth) {
        if (depth > maxDepth || tree.size() >= 300)
            return;
        const auto entries = dir.entryInfoList(
            QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
        for (const auto &entry : entries) {
            if (tree.size() >= 300)
                break;
            const QString name = entry.fileName();
            // Skip build artifacts, hidden dirs, and common noise
            if (name.startsWith(QLatin1Char('.'))
                || name == QLatin1String("build")
                || name.startsWith(QLatin1String("build-"))
                || name == QLatin1String("node_modules")
                || name == QLatin1String("__pycache__"))
                continue;

            const QString rel = root.relativeFilePath(entry.absoluteFilePath());
            if (entry.isDir()) {
                tree << rel + QLatin1Char('/');
                collectTree(QDir(entry.absoluteFilePath()), depth + 1, maxDepth);
            } else {
                tree << rel;
            }
        }
    };

    collectTree(root, 0, 3);

    if (tree.isEmpty())
        return;

    ContextItem item;
    item.type     = ContextItem::Type::WorkspaceInfo;
    item.label    = QStringLiteral("@workspace");
    item.content  = QStringLiteral("Project structure:\n") + tree.join(QLatin1Char('\n'));
    item.priority = 2; // low priority — pruned first
    ctx.items.append(item);
}

// ── Pruning ───────────────────────────────────────────────────────────────────

void ContextBuilder::pruneContext(ContextSnapshot &ctx, int maxChars)
{
    if (maxChars <= 0)
        return;

    // Use UTF-8 byte size / 4 as token estimate (more accurate than char count)
    auto estimateTokens = [](const QString &s) -> int {
        return s.toUtf8().size() / 4;
    };

    auto totalTokens = [&ctx, &estimateTokens]() {
        int total = 0;
        for (const auto &item : std::as_const(ctx.items))
            total += estimateTokens(item.content);
        return total;
    };

    const int maxTokens = maxChars / 4; // convert char budget to token budget
    int total = totalTokens();
    if (total <= maxTokens)
        return;

    // Sort: pinned first, then by priority ascending (lowest = first to remove),
    // then by token count descending (largest removed first at same priority)
    std::sort(ctx.items.begin(), ctx.items.end(),
              [&estimateTokens](const ContextItem &a, const ContextItem &b) {
                  if (a.pinned != b.pinned) return b.pinned;
                  if (a.priority != b.priority) return a.priority < b.priority;
                  return estimateTokens(a.content) > estimateTokens(b.content);
              });

    // Remove lowest-priority unpinned items until we fit
    while (total > maxTokens && !ctx.items.isEmpty()) {
        if (ctx.items.first().pinned)
            break;  // all remaining items are pinned
        total -= estimateTokens(ctx.items.first().content);
        ctx.items.removeFirst();
    }

    // If still over budget after removing items, truncate the largest remaining item
    if (total > maxTokens && !ctx.items.isEmpty()) {
        int largestIdx = 0;
        int largestTokens = 0;
        for (int i = 0; i < ctx.items.size(); ++i) {
            const int t = estimateTokens(ctx.items[i].content);
            if (t > largestTokens) {
                largestTokens = t;
                largestIdx = i;
            }
        }
        const int excessTokens = total - maxTokens;
        const int excessChars = excessTokens * 4;
        auto &item = ctx.items[largestIdx];
        if (item.content.size() > excessChars)
            item.content.truncate(item.content.size() - excessChars);
        item.content += QStringLiteral("\n... [truncated]");
    }

    // Re-sort by priority descending (highest priority first) for consistent output
    std::sort(ctx.items.begin(), ctx.items.end(),
              [](const ContextItem &a, const ContextItem &b) {
                  return a.priority > b.priority;
              });
}
