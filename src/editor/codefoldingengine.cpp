#include "codefoldingengine.h"

#include <QTextBlock>
#include <QTextDocument>

#include <algorithm>

CodeFoldingEngine::CodeFoldingEngine(QObject *parent)
    : QObject(parent)
{
}

void CodeFoldingEngine::setDocument(QTextDocument *doc)
{
    m_document = doc;
    m_regions.clear();
    m_foldedBlocks.clear();
}

void CodeFoldingEngine::setLanguageId(const QString &langId)
{
    m_languageId = langId;
}

void CodeFoldingEngine::update()
{
    if (!m_document)
        return;

    m_regions.clear();

    // Python / YAML use indentation-based folding
    static const QStringList indentLangs = {
        QStringLiteral("python"), QStringLiteral("yaml"),
        QStringLiteral("coffeescript"), QStringLiteral("nim")
    };

    if (indentLangs.contains(m_languageId))
        detectIndentRegions();
    else
        detectBraceRegions();
}

const CodeFoldingEngine::FoldRegion *CodeFoldingEngine::regionAt(int blockNumber) const
{
    for (const auto &r : m_regions) {
        if (r.startBlock == blockNumber)
            return &r;
    }
    return nullptr;
}

bool CodeFoldingEngine::isFoldable(int blockNumber) const
{
    return regionAt(blockNumber) != nullptr;
}

bool CodeFoldingEngine::isFolded(int blockNumber) const
{
    return m_foldedBlocks.contains(blockNumber);
}

bool CodeFoldingEngine::toggleFold(int blockNumber)
{
    if (!isFoldable(blockNumber))
        return false;

    if (isFolded(blockNumber))
        unfold(blockNumber);
    else
        fold(blockNumber);
    return true;
}

void CodeFoldingEngine::fold(int blockNumber)
{
    if (!m_document)
        return;

    const auto *region = regionAt(blockNumber);
    if (!region)
        return;

    m_foldedBlocks.insert(blockNumber);

    // Hide all blocks in the folded range (excluding the start line)
    for (int i = region->startBlock + 1; i <= region->endBlock; ++i) {
        QTextBlock block = m_document->findBlockByNumber(i);
        if (block.isValid())
            block.setVisible(false);
    }

    emit foldStateChanged();
}

void CodeFoldingEngine::unfold(int blockNumber)
{
    if (!m_document)
        return;

    const auto *region = regionAt(blockNumber);
    if (!region)
        return;

    m_foldedBlocks.remove(blockNumber);

    // Show all blocks in the unfolded range
    for (int i = region->startBlock + 1; i <= region->endBlock; ++i) {
        QTextBlock block = m_document->findBlockByNumber(i);
        if (block.isValid()) {
            // Only show if not hidden by a parent fold
            bool hiddenByParent = false;
            for (int fb : m_foldedBlocks) {
                const auto *pr = regionAt(fb);
                if (pr && fb != blockNumber &&
                    i > pr->startBlock && i <= pr->endBlock) {
                    hiddenByParent = true;
                    break;
                }
            }
            if (!hiddenByParent)
                block.setVisible(true);
        }
    }

    emit foldStateChanged();
}

void CodeFoldingEngine::unfoldAll()
{
    if (!m_document)
        return;

    m_foldedBlocks.clear();

    QTextBlock block = m_document->begin();
    while (block.isValid()) {
        block.setVisible(true);
        block = block.next();
    }

    emit foldStateChanged();
}

// ── Brace-based detection ─────────────────────────────────────────────────

void CodeFoldingEngine::detectBraceRegions()
{
    // Simple stack-based brace matching; skips braces inside strings/comments
    struct OpenBrace {
        int blockNumber;
    };

    QVector<OpenBrace> stack;
    QTextBlock block = m_document->begin();

    while (block.isValid()) {
        const QString text = block.text();
        bool inString = false;
        QChar stringChar;

        for (int i = 0; i < text.length(); ++i) {
            const QChar ch = text[i];

            // Simple string/char literal skip
            if (!inString && (ch == QLatin1Char('"') || ch == QLatin1Char('\''))) {
                inString = true;
                stringChar = ch;
                continue;
            }
            if (inString) {
                if (ch == stringChar && (i == 0 || text[i - 1] != QLatin1Char('\\')))
                    inString = false;
                continue;
            }

            // Skip single-line comments
            if (ch == QLatin1Char('/') && i + 1 < text.length()) {
                if (text[i + 1] == QLatin1Char('/'))
                    break;  // rest of line is comment
            }

            if (ch == QLatin1Char('{')) {
                stack.append({block.blockNumber()});
            } else if (ch == QLatin1Char('}') && !stack.isEmpty()) {
                const auto open = stack.takeLast();
                if (block.blockNumber() > open.blockNumber) {
                    m_regions.append({open.blockNumber, block.blockNumber()});
                }
            }
        }
        block = block.next();
    }

    // Sort by start block for consistent ordering
    std::sort(m_regions.begin(), m_regions.end(),
              [](const FoldRegion &a, const FoldRegion &b) {
                  return a.startBlock < b.startBlock;
              });
}

// ── Indentation-based detection ───────────────────────────────────────────

void CodeFoldingEngine::detectIndentRegions()
{
    // A fold region starts when indent increases relative to its predecessor,
    // and ends at the last line of that indentation scope.

    auto indentLevel = [](const QString &line) -> int {
        int spaces = 0;
        for (int i = 0; i < line.length(); ++i) {
            if (line[i] == QLatin1Char(' '))
                ++spaces;
            else if (line[i] == QLatin1Char('\t'))
                spaces += 4;
            else
                break;
        }
        return spaces;
    };

    auto isBlank = [](const QString &line) -> bool {
        return line.trimmed().isEmpty();
    };

    const int blockCount = m_document->blockCount();

    // Pre-compute indent levels
    QVector<int> levels(blockCount, 0);
    QVector<bool> blanks(blockCount, true);

    QTextBlock block = m_document->begin();
    for (int i = 0; i < blockCount && block.isValid(); ++i, block = block.next()) {
        const QString text = block.text();
        blanks[i] = isBlank(text);
        levels[i] = blanks[i] ? -1 : indentLevel(text);
    }

    // For each non-blank line, find the extent of its indentation scope
    for (int i = 0; i < blockCount; ++i) {
        if (blanks[i])
            continue;

        const int baseLevel = levels[i];

        // Check if next non-blank line has deeper indent
        int nextNonBlank = -1;
        for (int j = i + 1; j < blockCount; ++j) {
            if (!blanks[j]) {
                nextNonBlank = j;
                break;
            }
        }

        if (nextNonBlank < 0 || levels[nextNonBlank] <= baseLevel)
            continue;

        // Find the last line in the deeper scope
        int endBlock = nextNonBlank;
        for (int j = nextNonBlank + 1; j < blockCount; ++j) {
            if (!blanks[j]) {
                if (levels[j] <= baseLevel)
                    break;
                endBlock = j;
            }
        }

        if (endBlock > i)
            m_regions.append({i, endBlock});
    }
}
