#pragma once

#include <QObject>
#include <QPoint>
#include <QRgb>
#include <QString>
#include <QVector>
#include <cstdint>

// Default palette (VS Code Dark+ style)
static constexpr QRgb kTermDefaultFg = 0xFFCCCCCC;
static constexpr QRgb kTermDefaultBg = 0xFF1E1E1E;

// ── TermCell ──────────────────────────────────────────────────────────────────
struct TermCell {
    char32_t cp   = U' ';
    QRgb     fg   = kTermDefaultFg;
    QRgb     bg   = kTermDefaultBg;
    bool     bold = false;
    bool     ul   = false;   // underline
    bool     rev  = false;   // reverse video
};

// ── TerminalScreen ────────────────────────────────────────────────────────────
// Cell-based terminal screen buffer with integrated VT100/VT220/xterm parser.
//
// Thread model: feed() MUST be called from a single thread (typically via
// Qt::QueuedConnection from the PTY reader thread → called on UI thread).
// All other methods are UI-thread only.
//
// Rows:
//   row <  0 → scrollback  (−1 = most recent scrollback row)
//   row >= 0 → visible screen
class TerminalScreen : public QObject
{
    Q_OBJECT

public:
    explicit TerminalScreen(int cols = 80, int rows = 24, QObject *parent = nullptr);

    void resize(int cols, int rows);
    void feed(const QByteArray &data);

    int     cols()          const { return m_cols; }
    int     rows()          const { return m_rows; }
    int     scrollbackLines() const { return (int)m_sb.size(); }

    TermCell cellAt(int col, int row) const;   // row < 0 = scrollback
    QPoint   cursorPos()     const { return m_cur; }
    bool     cursorVisible() const { return m_curVis; }
    QString  title()         const { return m_title; }

signals:
    void damaged();
    void titleChanged(const QString &t);
    void bell();

private:
    enum class St { Normal, Esc, Csi, Osc };

    void eat(unsigned char c);
    void ctrl(unsigned char c);
    void csiExecute(unsigned char fin);
    void oscExecute();
    int  param(int i, int def = 0) const;

    using Row = QVector<TermCell>;

    void  put(char32_t cp);
    void  lineFeed();
    void  revLineFeed();
    void  cr();
    void  bs();
    void  ht();
    void  scrollUp(int n = 1);
    void  scrollDown(int n = 1);
    void  eraseDisplay(int mode);
    void  eraseLine(int mode);
    void  eraseRow(Row &r, int from, int to);
    Row   blankRow() const;
    void  switchAlt(bool on);
    void  saveCursor();
    void  restoreCursor();
    void  applySgr();

    static QRgb ansi4(int n);
    static QRgb xterm256(int n);

    int          m_cols, m_rows;
    QVector<Row> m_pri;        // primary screen
    QVector<Row> m_alt;        // alternate screen (vim, less)
    bool         m_altOn = false;
    QVector<Row> m_sb;         // scrollback (oldest first)
    static constexpr int kSbMax = 10000;

    QPoint m_cur      = {0, 0};
    QPoint m_saved    = {0, 0};
    QPoint m_altSaved = {0, 0};
    bool   m_curVis   = true;
    bool   m_wrap     = false;  // pending wrap (next char triggers newline first)
    int    m_stTop    = 0;      // scroll region top (inclusive)
    int    m_stBot    = 0;      // scroll region bottom (inclusive; 0 = rows−1)

    TermCell m_attr;            // current drawing attributes

    // VT parser
    St           m_st     = St::Normal;
    bool         m_priv   = false;  // DEC private flag ('?')
    bool         m_wasOsc = false;  // true when ESC seen during Osc (for ST = ESC \)
    QVector<int> m_pars;
    QByteArray   m_pbuf;

    // UTF-8 decoder
    char32_t m_u8  = 0;
    int      m_u8n = 0;

    QString m_title;
};
