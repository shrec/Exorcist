#pragma once

#include <QHash>
#include <QJsonArray>
#include <QList>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>

class CodeFoldingEngine;
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

    // ── Inlay Hints (parameter names, type annotations) ───────────────────
    struct InlayHint {
        int     line;       // 0-based
        int     character;  // 0-based column
        QString label;
        bool    paddingLeft  = false;
        bool    paddingRight = false;
    };
    void setInlayHints(const QList<InlayHint> &hints);
    void clearInlayHints();
    bool isInlayHintsVisible() const { return m_showInlayHints; }
    void setInlayHintsVisible(bool visible);
    int firstVisibleBlockNumber() const;

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

    // ── Code folding ──────────────────────────────────────────────────────
    CodeFoldingEngine *foldingEngine() const;
    void updateFoldRegions();

    // ── Indent guides ─────────────────────────────────────────────────────
    void setIndentGuidesVisible(bool visible);
    bool isIndentGuidesVisible() const { return m_showIndentGuides; }

    // ── Whitespace markers (Render Whitespace: all) ───────────────────────
    /// Toggle visualization of spaces (·) and tabs (→) as dim-gray glyph
    /// overlays. Does NOT modify document text — purely a viewport
    /// decoration painted after the base paint pass, so it does not
    /// interfere with selection, cursor, or LSP operations.
    void setWhitespaceVisible(bool visible);
    bool isWhitespaceVisible() const { return m_showWhitespace; }

    // ── PieceTableBuffer (shadow backing store) ───────────────────────────
    PieceTableBuffer *buffer() const;
    QString bufferText() const;
    QString bufferSlice(int start, int length) const;

    // ── TODO/FIXME/HACK/NOTE markers ──────────────────────────────────────
    /// Read-only access to the current marker map (1-based line → keyword).
    /// Other UIs (e.g. minimap, problem panel) can iterate this to surface
    /// scan results without re-parsing the document.
    QHash<int, QString> todoMarkers() const { return m_todoMarkers; }

    // ── Debug variable hover tooltip ──────────────────────────────────────
    /// Snapshot of variable name → display value, populated by some external
    /// component (a debug bootstrap or similar) on every "stopped" event.
    /// When non-empty, hovering an identifier in the editor will show a
    /// tooltip with its current value, similar to VS Code.
    /// TODO: a debug-aware bootstrap should call this from
    /// IDebugAdapter::variablesUpdated (see src/bootstrap/* and
    /// src/debug/idebugadapter.h). Kept self-contained here to avoid
    /// cross-DLL coupling in this layer.
    void setLocalsSnapshot(const QHash<QString, QString> &snapshot);
    void clearLocalsSnapshot();
    bool hasLocalsSnapshot() const { return !m_debugLocals.isEmpty(); }

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

    /// Emitted when the user changes a breakpoint's condition via the
    /// gutter context menu. An empty `condition` means the condition was
    /// cleared (the breakpoint becomes unconditional). Listeners (e.g.
    /// PostPluginBootstrap) should re-add the breakpoint via the debug
    /// adapter so GDB's `-break-insert -c "<expr>"` is updated.
    void breakpointConditionChanged(const QString &filePath, int line,
                                    const QString &condition);

    /// Emitted when the user toggles the enabled state of a breakpoint via
    /// the gutter context menu. Listeners should call into the debug
    /// adapter (e.g. GDB `-break-enable` / `-break-disable`).
    void breakpointEnabledChanged(const QString &filePath, int line, bool enabled);

public:
    // ── Breakpoint gutter ─────────────────────────────────────────────────

    /// Toggle a breakpoint on the given 1-based line.
    void toggleBreakpoint(int line);

    /// Set breakpoints from external source (e.g. debug adapter).
    void setBreakpointLines(const QSet<int> &lines);

    /// Get current breakpoint lines (1-based).
    const QSet<int> &breakpointLines() const { return m_breakpointLines; }

    /// Per-line breakpoint condition (1-based line). Empty/missing means
    /// unconditional. Stored locally so the gutter context menu can
    /// pre-fill the dialog with the existing expression.
    QString breakpointCondition(int line) const { return m_breakpointConditions.value(line); }
    void    setBreakpointCondition(int line, const QString &condition);
    const QHash<int, QString> &breakpointConditions() const { return m_breakpointConditions; }

    /// Per-line enabled flag. A breakpoint that exists in m_breakpointLines
    /// but is "disabled" still shows in the gutter (greyed) but does not
    /// trigger. Default = enabled.
    bool isBreakpointEnabled(int line) const { return !m_disabledBreakpoints.contains(line); }
    void setBreakpointEnabled(int line, bool enabled);

    /// Set the line where the debugger is currently stopped (1-based, 0=none).
    void setCurrentDebugLine(int line);
    int  currentDebugLine() const { return m_currentDebugLine; }

    /// Called by LineNumberArea on mouse press (left-click toggle).
    void lineNumberAreaMousePress(QMouseEvent *event);

    /// Called by LineNumberArea on right-click — opens a context menu for
    /// breakpoint management on the clicked gutter line.
    void lineNumberAreaContextMenu(QContextMenuEvent *event);

private:
    /// Resolve a viewport-y coordinate (in the gutter widget's local frame)
    /// to the 1-based document line, or 0 if the click is below the last block.
    int  lineFromGutterY(int y) const;

    /// Show the breakpoint context menu at globalPos for a given 1-based line.
    void showBreakpointContextMenu(int line, const QPoint &globalPos);

    /// Open the BreakpointConditionDialog for a given 1-based line and
    /// commit the result (emit breakpointConditionChanged on change).
    void editBreakpointCondition(int line);

public:

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
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

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
    QString identifierAt(const QPoint &viewportPos) const;
    bool maybeShowDebugValueTooltip(const QPoint &viewportPos, const QPoint &globalPos);
    static QChar matchingBracket(QChar ch);
    int scanForMatchingBracket(int pos, QChar open, QChar close, bool forward) const;
    void paintMultiCursors(QPainter &painter);
    void syncPrimaryCursorToEngine();

    LineNumberArea          *m_lineNumberArea;
    FindBar                 *m_findBar;
    bool                     m_isLargeFilePreview;
    bool                     m_isLoadingChunk;
    QList<DiagnosticMark>    m_diagMarks;

    // Inlay hints
    QList<InlayHint>         m_inlayHints;
    bool                     m_showInlayHints = true;

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
    bool                     m_showWhitespace = false;

    // Breakpoints
    QSet<int>                m_breakpointLines;       // 1-based lines
    QHash<int, QString>      m_breakpointConditions;  // 1-based line → condition expr
    QSet<int>                m_disabledBreakpoints;   // 1-based lines that are disabled
    int                      m_currentDebugLine = 0;  // 1-based (0=none)

    // Bracket matching
    QList<QTextEdit::ExtraSelection> m_bracketSelections;

    // Debug variable hover tooltip
    QHash<QString, QString> m_debugLocals;       // identifier → formatted value
    QString                 m_lastTooltipIdent;  // last-shown identifier (cache)

    // TODO/FIXME/HACK/NOTE markers (1-based line → keyword)
    QHash<int, QString>     m_todoMarkers;
    QTimer                  m_todoRescanTimer;   // debounced rescan on edits
    void                    rescanTodoMarkers();

    // ── Inline color swatches (hex literals) ──────────────────────────────
    // Cache of hex color codes per visible block. Maps block number
    // (0-based) → list of (column, hex-string) tuples. Recomputed on
    // contentsChange (debounced 250ms) so paintEvent stays cheap.
    QHash<int, QList<QPair<int, QString>>> m_colorSwatches;
    QTimer                                  m_colorRescanTimer;
    void                                    rescanColorSwatches();
    /// Hit-test a viewport click against m_colorSwatches. Returns true if
    /// the click landed on a swatch and the user picked a new color (which
    /// has already been applied to the document).
    bool                                    handleColorSwatchClick(const QPoint &viewportPos);

    // Shadow buffer kept in sync with QTextDocument via contentsChanged
    std::unique_ptr<PieceTableBuffer> m_buffer;
    bool                     m_bufferSyncing = false;  // guard re-entrant syncs

    // Code folding
    std::unique_ptr<CodeFoldingEngine> m_foldingEngine;
    QTimer                             m_foldRecomputeTimer;  // debounced rebuild on contentsChange

    // Multi-cursor engine
    std::unique_ptr<MultiCursorEngine> m_multiCursor;

    // Vim emulation
    std::unique_ptr<VimHandler> m_vimHandler;
    bool                     m_altDragging = false;
    QPoint                   m_altDragStart;  // viewport coords where Alt+drag began
};
