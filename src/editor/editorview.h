#pragma once

#include <QPlainTextEdit>

class LineNumberArea;
class FindBar;

class EditorView : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit EditorView(QWidget *parent = nullptr);
    void setLargeFilePreview(const QString &text, bool isPartial);
    void appendLargeFileChunk(const QString &text, bool isFinal);
    bool isLargeFilePreview() const;

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    void showFindBar();
    void hideFindBar();

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

    LineNumberArea *m_lineNumberArea;
    FindBar        *m_findBar;
    bool m_isLargeFilePreview;
    bool m_isLoadingChunk;
};
