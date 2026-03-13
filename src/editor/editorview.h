#pragma once

#include <QJsonArray>
#include <QList>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <QString>

#include <memory>

class LineNumberArea;
class FindBar;
class MultiCursorEngine;
class PieceTableBuffer;
class VimHandler;

// Diagnostic severity — matches LSP DiagnosticSeverity values
enum class DiagSeverity { Error = 1, Warning = 2, Info = 3, Hint = 4 };

struct DiagnosticMark
{
    int          line;
    int          startChar;
    int          endChar;
    DiagSeverity severity;
    QString      message;
};

class EditorView : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit EditorView(QWidget *parent = nullptr);
    ~EditorView() override;
    void setLargeFilePreview(const QString &text, bool isPartial);
    void appendLargeFileChunk(const QString &text, bool isFinal);
    bool isLargeFilePreview() const;

    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    void showFindBar();
    void hideFindBar();

    void setLanguageId(const QString &id) { m_languageId = id; }
    QString languageId() const { return m_languageId; }

    // ── Ghost text (inline completion) ────────────────────────────────────
    void setGhostText(const QString &text);
    void clearGhostText();
    bool hasGhostText() const { return !m_ghostText.isEmpty(); }
    void acceptGhostTextWord();   // accept next word (Ctrl+→)

    // ── LSP integration ───────────────────────────────────────────────────
    void setDiagnostics(const QJsonArray &lspDiagnostics);
    void applyTextEdits(const QJsonArray &edits);

    const QList<DiagnosticMark> &diagnosticMarks() const { return m_diagMarks; }

    // ── Inline review annotations ─────────────────────────────────────────
    struct ReviewAnnotation {
        int     line;       // 1-based line number
        QString comment;    // review comment text
    };
    void setReviewAnnotations(const QList<ReviewAnnotation> &annotations);
    void clearReviewAnnotations();
    const QList<ReviewAnnotation> &reviewAnnotations() const { return m_reviewAnnotations; }
    void nextReviewAnnotation();
    void prevReviewAnnotation();
    void applyReviewAnnotation(int index);   // accept suggestion at index
    void discardReviewAnnotation(int index); // dismiss annotation at index
    int  currentReviewIndex() const { return m_currentReviewIdx; }

    // ── Next Edit Suggestion (NES) arrow ──────────────────────────────────
    void setNesLine(int line);       // set 0-based line for arrow indicator
    void clearNesLine();
    int  nesLine() const { return m_nesLine; }

    // ── Inline diff gutter ────────────────────────────────────────────────
    enum class DiffType { Added, Modified, Deleted };
    struct DiffRange {
        int      startLine;  // 0-based
        int      lineCount;
        DiffType type;
    };
    void setDiffRanges(const QList<DiffRange> &ranges);
    void clearDiffRanges();

    // Blame annotations
    struct BlameInfo {
        QString author;
        QString hash;     // short hash
        QString summary;
    };
    void setBlameData(const QHash<int, BlameInfo> &blameByLine); // 1-based line keys
    void clearBlameData();
    bool isBlameVisible() const { return m_showBlame; }
    void setBlameVisible(bool visible);

    void setMinimapVisible(bool visible);
    bool isMinimapVisible() const { return m_minimapVisible; }

    // ── Indent guides ─────────────────────────────────────────────────────
    void setIndentGuidesVisible(bool visible);
    bool isIndentGuidesVisible() const { return m_showIndentGuides; }

    // ── PieceTableBuffer (shadow backing store) ───────────────────────────
    PieceTableBuffer *buffer() const;
    QString bufferText() const;
    QString bufferSlice(int start, int length) const;

signals:
    void requestMoreData();
    void ghostTextAccepted();
    void ghostTextPartiallyAccepted();  // word accepted, ghost text shortened
    void ghostTextDismissed();
    void manualCompletionRequested();    // Alt+\ manual trigger
    void goToDefinitionRequested();
    void goToDeclarationRequested();
    void findReferencesRequested();
    void renameSymbolRequested();
    void formatDocumentRequested();
    void openIncludeRequested(const QString &includePath);
    // Emitted when the user triggers Ctrl+I inline chat
    void inlineChatRequested(const QString &selectedText, const QString &languageId);

    // AI context menu actions
    void aiExplainRequested(const QString &selectedText, const QString &filePath, const QString &languageId);
    void aiReviewRequested(const QString &selectedText, const QString &filePath, const QString &languageId);
    void aiFixRequested(const QString &selectedText, const QString &filePath, const QString &languageId);
    void aiTestRequested(const QString &selectedText, const QString &filePath, const QString &languageId);
    void aiDocRequested(const QString &selectedText, const QString &filePath, const QString &languageId);
    void reviewAnnotationApplied(int line, const QString &comment);
    void reviewAnnotationDiscarded(int line, const QString &comment);

    /// Emitted when user clicks the gutter to toggle a breakpoint (1-based line).
    void breakpointToggled(const QString &filePath, int line, bool added);

public:
    // ── Breakpoint gutter ─────────────────────────────────────────────────

    /// Toggle a breakpoint on the given 1-based line.
    void toggleBreakpoint(int line);

    /// Set breakpoints from external source (e.g. debug adapter).
    void setBreakpointLines(const QSet<int> &lines);

    /// Get current breakpoint lines (1-based).
    const QSet<int> &breakpointLines() const { return m_breakpointLines; }

    /// Set the line where the debugger is currently stopped (1-based, 0=none).
    void setCurrentDebugLine(int line);
    int  currentDebugLine() const { return m_currentDebugLine; }

    /// Called by LineNumberArea on mouse click.
    void lineNumberAreaMousePress(QMouseEvent *event);

    // ── Multi-cursor ──────────────────────────────────────────────────────
    MultiCursorEngine *multiCursorEngine() const;
    VimHandler *vimHandler() const;
    bool hasMultipleCursors() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void highlightMatchingBrackets();
    void handleScroll(int value);
    void repositionFindBar();
    void refreshDiagnosticSelections();
    void acceptGhostText();
    void acceptNextWord();
    QString includePathUnderCursor() const;
    static QChar matchingBracket(QChar ch);
    int scanForMatchingBracket(int pos, QChar open, QChar close, bool forward) const;
    void paintMultiCursors(QPainter &painter);
    void syncPrimaryCursorToEngine();

    LineNumberArea          *m_lineNumberArea;
    FindBar                 *m_findBar;
    bool                     m_isLargeFilePreview;
    bool                     m_isLoadingChunk;
    QList<DiagnosticMark>    m_diagMarks;

    // Ghost text (inline completion)
    QString                  m_ghostText;
    int                      m_ghostBlock  = -1;  // block number where ghost was placed
    int                      m_ghostColumn = -1;  // column in that block

    QString                  m_languageId;

    // Inline review annotations
    QList<ReviewAnnotation>  m_reviewAnnotations;
    QList<QTextEdit::ExtraSelection> m_reviewSelections;
    int                      m_currentReviewIdx = -1;

    // Next Edit Suggestion (NES) gutter arrow
    int                      m_nesLine = -1;   // 0-based line for arrow indicator

    // Inline diff gutter
    QList<DiffRange>         m_diffRanges;
    QHash<int, BlameInfo>    m_blameData;    // 1-based line → blame info
    bool                     m_showBlame = false;

    class MinimapWidget     *m_minimap = nullptr;
    bool                     m_minimapVisible = true;
    bool                     m_showIndentGuides = true;

    // Breakpoints
    QSet<int>                m_breakpointLines;   // 1-based lines
    int                      m_currentDebugLine = 0; // 1-based (0=none)

    // Bracket matching
    QList<QTextEdit::ExtraSelection> m_bracketSelections;

    // Shadow buffer kept in sync with QTextDocument via contentsChanged
    std::unique_ptr<PieceTableBuffer> m_buffer;
    bool                     m_bufferSyncing = false;  // guard re-entrant syncs

    // Multi-cursor engine
    std::unique_ptr<MultiCursorEngine> m_multiCursor;

    // Vim emulation
    std::unique_ptr<VimHandler> m_vimHandler;
    bool                     m_altDragging = false;
    QPoint                   m_altDragStart;  // viewport coords where Alt+drag began
};
