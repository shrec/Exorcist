#pragma once

#include <QJsonArray>
#include <QList>
#include <QPlainTextEdit>
#include <QString>

class LineNumberArea;
class FindBar;

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
    void setLargeFilePreview(const QString &text, bool isPartial);
    void appendLargeFileChunk(const QString &text, bool isFinal);
    bool isLargeFilePreview() const;

    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    void showFindBar();
    void hideFindBar();

    // ── LSP integration ───────────────────────────────────────────────────
    // Apply diagnostics received from a language server.
    // Renders wavy underlines in the text and dots in the gutter.
    void setDiagnostics(const QJsonArray &lspDiagnostics);

    // Apply an array of LSP TextEdit objects (from textDocument/formatting).
    // Edits are applied bottom-to-top so positions stay valid.
    void applyTextEdits(const QJsonArray &edits);

    const QList<DiagnosticMark> &diagnosticMarks() const { return m_diagMarks; }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

signals:
    void requestMoreData();

private:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void handleScroll(int value);
    void repositionFindBar();
    void refreshDiagnosticSelections();

    LineNumberArea          *m_lineNumberArea;
    FindBar                 *m_findBar;
    bool                     m_isLargeFilePreview;
    bool                     m_isLoadingChunk;
    QList<DiagnosticMark>    m_diagMarks;
};
