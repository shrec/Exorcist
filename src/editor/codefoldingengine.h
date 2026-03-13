#pragma once

#include <QObject>
#include <QSet>
#include <QVector>

class QTextDocument;

// ── CodeFoldingEngine ────────────────────────────────────────────────────────
//
// Detects foldable regions in a QTextDocument and manages fold state.
// Supports brace-based folding ({/}) and indentation-based folding (Python).

class CodeFoldingEngine : public QObject
{
    Q_OBJECT

public:
    struct FoldRegion {
        int startBlock;  // 0-based block number of fold start
        int endBlock;    // 0-based block number of fold end (inclusive)
    };

    explicit CodeFoldingEngine(QObject *parent = nullptr);

    void setDocument(QTextDocument *doc);
    void setLanguageId(const QString &langId);

    /// Rebuild fold regions from the document. Call after major edits.
    void update();

    /// Returns all detected fold regions.
    const QVector<FoldRegion> &regions() const { return m_regions; }

    /// Returns the fold region starting at the given block, or nullptr.
    const FoldRegion *regionAt(int blockNumber) const;

    /// True if the given block is the start of a foldable region.
    bool isFoldable(int blockNumber) const;

    /// True if the region starting at blockNumber is currently collapsed.
    bool isFolded(int blockNumber) const;

    /// Toggle fold state at blockNumber. Returns true if it was toggled.
    bool toggleFold(int blockNumber);

    /// Fold the region starting at blockNumber.
    void fold(int blockNumber);

    /// Unfold the region starting at blockNumber.
    void unfold(int blockNumber);

    /// Unfold all.
    void unfoldAll();

signals:
    void foldStateChanged();

private:
    void detectBraceRegions();
    void detectIndentRegions();

    QTextDocument      *m_document = nullptr;
    QString             m_languageId;
    QVector<FoldRegion>  m_regions;
    QSet<int>            m_foldedBlocks; // block numbers of collapsed regions
};
