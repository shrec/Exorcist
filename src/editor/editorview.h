#pragma once

#include <QPlainTextEdit>

class LineNumberArea;

class EditorView : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit EditorView(QWidget *parent = nullptr);

    int lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();

    LineNumberArea *m_lineNumberArea;
};
