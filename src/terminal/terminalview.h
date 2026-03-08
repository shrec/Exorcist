#pragma once

#include <QAbstractScrollArea>
#include <QFont>
#include <QPoint>

class TerminalScreen;
class QTimer;
class QMenu;
class QLineEdit;
class QLabel;

// ── TerminalView ──────────────────────────────────────────────────────────────
// Cell-based terminal renderer.
//
// Layout: QAbstractScrollArea with a scrollback + current screen.
// Rendering uses QPainter with a fixed monospace font.
// Keyboard events are translated to VT sequences and emitted via inputData().
class TerminalView : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit TerminalView(TerminalScreen *screen, QWidget *parent = nullptr);

    void scrollToBottom();

signals:
    void inputData(const QByteArray &data);   // keyboard / paste → PTY
    void sizeChanged(int cols, int rows);      // pixel resize → cell grid

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onDamaged();
    void blinkCursor();
    void findNext();
    void findPrev();

private:
    void    updateScrollbar();
    QPoint  pixelToCell(const QPoint &pos) const;
    bool    isCellSelected(int col, int absRow) const;
    QString selectedText() const;
    void    setFontSize(int pointSize);
    void    showFindBar();
    void    hideFindBar();
    void    updateSearchHighlight();
    int     lineMatchCount(int absRow, const QString &needle) const;

    TerminalScreen *m_screen;
    QFont  m_font;
    QFont  m_fontBold;
    int    m_cellW = 8;
    int    m_cellH = 16;
    int    m_fontSize = 10;

    // Cursor blink
    QTimer *m_blinkTimer;
    bool    m_curBlink = true;   // blink phase
    bool    m_hasFocus = false;

    // Text selection
    QPoint m_selStart, m_selEnd;
    bool   m_selecting = false;

    // Search
    QWidget   *m_findBar     = nullptr;
    QLineEdit *m_findInput   = nullptr;
    QLabel    *m_findCount   = nullptr;
    QString    m_searchNeedle;
    QList<QPair<int, int>> m_searchHits; // (absRow, col) of each match
    int        m_searchCurrent = -1;     // current hit index
};
