#include "terminalscreen.h"

#include <algorithm>

// ── 16-colour ANSI palette (xterm defaults) ───────────────────────────────────
static const QRgb kAnsi16[16] = {
    // Normal (0–7)
    qRgba(  0,   0,   0, 255), qRgba(128,   0,   0, 255),
    qRgba(  0, 128,   0, 255), qRgba(128, 128,   0, 255),
    qRgba(  0,   0, 128, 255), qRgba(128,   0, 128, 255),
    qRgba(  0, 128, 128, 255), qRgba(192, 192, 192, 255),
    // Bright (8–15)
    qRgba(128, 128, 128, 255), qRgba(255,  85,  85, 255),
    qRgba( 85, 255,  85, 255), qRgba(255, 255,  85, 255),
    qRgba( 85,  85, 255, 255), qRgba(255,  85, 255, 255),
    qRgba( 85, 255, 255, 255), qRgba(255, 255, 255, 255),
};

QRgb TerminalScreen::ansi4(int n)
{
    return (n >= 0 && n < 16) ? kAnsi16[n] : kTermDefaultFg;
}

QRgb TerminalScreen::xterm256(int n)
{
    if (n < 0)   return kTermDefaultFg;
    if (n < 16)  return kAnsi16[n];
    if (n < 232) {
        n -= 16;
        int b = n % 6; n /= 6;
        int g = n % 6; n /= 6;
        int r = n;
        auto v = [](int i) -> int { return i ? 55 + i * 40 : 0; };
        return qRgba(v(r), v(g), v(b), 255);
    }
    int l = 8 + (n - 232) * 10;
    return qRgba(l, l, l, 255);
}

// ── Constructor ───────────────────────────────────────────────────────────────

TerminalScreen::TerminalScreen(int cols, int rows, QObject *parent)
    : QObject(parent), m_cols(cols), m_rows(rows)
{
    m_attr.fg = kTermDefaultFg;
    m_attr.bg = kTermDefaultBg;
    m_pri.resize(rows, blankRow());
    m_alt.resize(rows, blankRow());
    m_stBot = rows - 1;
}

// ── Public API ────────────────────────────────────────────────────────────────

void TerminalScreen::resize(int cols, int rows)
{
    if (cols == m_cols && rows == m_rows) return;

    auto resizeGrid = [&](QVector<Row> &grid) {
        for (auto &r : grid) {
            if (cols > (int)r.size()) r.resize(cols, m_attr);
            else                      r.resize(cols);
        }
        while ((int)grid.size() < rows) grid.push_back(blankRow());
        if ((int)grid.size() > rows)    grid.resize(rows);
    };

    m_cols = cols;
    m_rows = rows;
    resizeGrid(m_pri);
    resizeGrid(m_alt);
    m_cur.setX(qMin(m_cur.x(), cols - 1));
    m_cur.setY(qMin(m_cur.y(), rows - 1));
    m_stTop = 0;
    m_stBot = rows - 1;
    emit damaged();
}

TermCell TerminalScreen::cellAt(int col, int row) const
{
    if (col < 0 || col >= m_cols) return TermCell{};
    const auto &grid = m_altOn ? m_alt : m_pri;

    if (row >= 0) {
        if (row >= m_rows) return TermCell{};
        return grid[row][col];
    } else {
        int idx = (int)m_sb.size() + row;
        if (idx < 0 || idx >= (int)m_sb.size()) return TermCell{};
        return m_sb[idx][col];
    }
}

QString TerminalScreen::recentText(int maxLines) const
{
    if (maxLines <= 0)
        return {};

    auto rowToText = [this](const Row &row) {
        QString line;
        line.reserve(row.size());
        for (int c = 0; c < m_cols && c < row.size(); ++c) {
            const char32_t cp = row[c].cp;
            if (cp == U'\0') {
                line += QLatin1Char(' ');
            } else {
                line += QString::fromUcs4(&cp, 1);
            }
        }
        while (line.endsWith(QLatin1Char(' ')))
            line.chop(1);
        return line;
    };

    QStringList all;
    all.reserve(m_sb.size() + m_rows);
    for (const auto &row : m_sb)
        all.append(rowToText(row));

    const auto &grid = m_altOn ? m_alt : m_pri;
    for (int r = 0; r < m_rows && r < grid.size(); ++r)
        all.append(rowToText(grid[r]));

    const int start = qMax(0, all.size() - maxLines);
    return all.mid(start).join(QLatin1Char('\n')).trimmed();
}

void TerminalScreen::feed(const QByteArray &data)
{
    for (unsigned char c : data)
        eat(c);
    emit damaged();
}

QString TerminalScreen::recentOutput(int lines) const
{
    if (lines <= 0)
        return {};

    auto rowToText = [](const Row &row) -> QString {
        QString out;
        out.reserve(row.size());
        for (const TermCell &c : row) {
            const char32_t cp = c.cp ? c.cp : U' ';
            if (cp <= 0x7F)
                out.append(QChar(static_cast<ushort>(cp)));
            else
                out.append(QChar::fromUcs4(cp));
        }
        int end = out.size();
        while (end > 0 && out[end - 1].isSpace())
            --end;
        out.truncate(end);
        return out;
    };

    const auto &grid = m_altOn ? m_alt : m_pri;
    QStringList out;

    for (const Row &r : m_sb)
        out.append(rowToText(r));
    for (const Row &r : grid)
        out.append(rowToText(r));

    if (out.size() > lines)
        out = out.mid(out.size() - lines);
    return out.join(QLatin1Char('\n'));
}

// ── VT parser state machine ───────────────────────────────────────────────────

void TerminalScreen::eat(unsigned char c)
{
    // UTF-8 multi-byte continuation
    if (m_u8n > 0) {
        if ((c & 0xC0) == 0x80) {
            m_u8 = (m_u8 << 6) | (c & 0x3F);
            if (--m_u8n == 0) put(m_u8);
        } else {
            m_u8n = 0; // malformed sequence – skip
        }
        return;
    }

    // ESC resets and transitions from any state
    if (c == 0x1B) {
        m_wasOsc = (m_st == St::Osc);
        m_st     = St::Esc;
        m_priv   = false;
        m_pars.clear();
        m_pbuf.clear();
        return;
    }

    // BEL terminates an OSC string (most common terminator for OSC)
    if (c == 0x07 && m_st == St::Osc) {
        oscExecute();
        m_st = St::Normal;
        return;
    }

    // C0 controls are passed through in all states
    if (c < 0x20 || c == 0x7F) {
        ctrl(c);
        return;
    }

    switch (m_st) {
    case St::Normal:
        if      (c >= 0xC2 && c <= 0xDF) { m_u8 = c & 0x1F; m_u8n = 1; }
        else if (c >= 0xE0 && c <= 0xEF) { m_u8 = c & 0x0F; m_u8n = 2; }
        else if (c >= 0xF0 && c <= 0xF4) { m_u8 = c & 0x07; m_u8n = 3; }
        else if (c < 0x80)               { put((char32_t)c); }
        break;

    case St::Esc:
        m_st = St::Normal;
        switch (c) {
        case '[':  m_st = St::Csi;  break;
        case ']':  m_st = St::Osc;  m_pbuf.clear(); m_wasOsc = false; break;
        case '7':  saveCursor();    break;
        case '8':  restoreCursor(); break;
        case 'D':  lineFeed();      break;  // IND
        case 'M':  revLineFeed();   break;  // RI
        case 'c':                           // RIS (full reset)
            m_pri.fill(blankRow(), m_rows);
            m_alt.fill(blankRow(), m_rows);
            m_cur = m_saved = m_altSaved = {0, 0};
            m_stTop = 0; m_stBot = m_rows - 1;
            m_attr    = TermCell{};
            m_attr.fg = kTermDefaultFg;
            m_attr.bg = kTermDefaultBg;
            m_altOn   = false;
            break;
        case '\\':  // ST (String Terminator)
            if (m_wasOsc) { oscExecute(); m_wasOsc = false; }
            break;
        default: break;
        }
        break;

    case St::Csi:
        if (c == '?')              { m_priv = true; break; }
        if (c >= '0' && c <= '9') { m_pbuf.append((char)c); break; }
        if (c == ';' || c == ':') {
            m_pars.push_back(m_pbuf.isEmpty() ? -1 : m_pbuf.toInt());
            m_pbuf.clear();
            break;
        }
        if (c >= 0x40 && c <= 0x7E) {
            m_pars.push_back(m_pbuf.isEmpty() ? -1 : m_pbuf.toInt());
            m_pbuf.clear();
            csiExecute(c);
            m_st   = St::Normal;
            m_priv = false;
            m_pars.clear();
        }
        break;

    case St::Osc:
        m_pbuf.append((char)c);
        break;
    }
}

void TerminalScreen::ctrl(unsigned char c)
{
    switch (c) {
    case 0x07: emit bell();   break;              // BEL
    case 0x08: bs();          break;              // BS
    case 0x09: ht();          break;              // HT
    case 0x0A:                                    // LF
    case 0x0B:                                    // VT
    case 0x0C: lineFeed();    break;              // FF
    case 0x0D: cr();          break;              // CR
    case 0x9B:                                    // C1 CSI
        m_st = St::Csi; m_priv = false;
        m_pars.clear(); m_pbuf.clear();
        break;
    case 0x9D:                                    // C1 OSC
        m_st = St::Osc; m_pbuf.clear();
        break;
    default: break;
    }
}

int TerminalScreen::param(int i, int def) const
{
    if (i >= (int)m_pars.size()) return def;
    int v = m_pars[i];
    return (v < 0) ? def : v;
}

// ── CSI dispatch ──────────────────────────────────────────────────────────────

void TerminalScreen::csiExecute(unsigned char fin)
{
    auto &grid = m_altOn ? m_alt : m_pri;
    const int stBot = (m_stBot == 0) ? m_rows - 1 : m_stBot;
    const int p0    = param(0, 0);
    const int p1    = param(1, 0);

    // ── DEC private modes (h/l) ───────────────────────────────────────────
    if (m_priv && (fin == 'h' || fin == 'l')) {
        bool on = (fin == 'h');
        for (int v : m_pars) {
            if (v < 0) v = 0;
            switch (v) {
            case 25:   m_curVis = on; break;
            case 1047: switchAlt(on); break;
            case 1048:
                if (on) saveCursor(); else restoreCursor();
                break;
            case 1049:
                if (on)  { saveCursor(); switchAlt(true);  eraseDisplay(2); }
                else     { switchAlt(false); restoreCursor(); }
                break;
            default: break;
            }
        }
        return;
    }

    switch (fin) {
    // ── Cursor movement ───────────────────────────────────────────────────
    case 'A': m_cur.setY(qMax(m_stTop,    m_cur.y() - qMax(1,p0))); m_wrap=false; break; // CUU
    case 'B': m_cur.setY(qMin(stBot,      m_cur.y() + qMax(1,p0))); m_wrap=false; break; // CUD
    case 'C': m_cur.setX(qMin(m_cols-1,   m_cur.x() + qMax(1,p0))); m_wrap=false; break; // CUF
    case 'D': m_cur.setX(qMax(0,          m_cur.x() - qMax(1,p0))); m_wrap=false; break; // CUB
    case 'E': m_cur.setY(qMin(stBot,      m_cur.y() + qMax(1,p0)));  m_cur.setX(0); m_wrap=false; break; // CNL
    case 'F': m_cur.setY(qMax(m_stTop,    m_cur.y() - qMax(1,p0)));  m_cur.setX(0); m_wrap=false; break; // CPL
    case 'G': m_cur.setX(qBound(0, qMax(1,p0)-1, m_cols-1));         m_wrap=false; break; // CHA
    case 'H': case 'f':  // CUP / HVP
        m_cur.setY(qBound(0, qMax(1,p0)-1, m_rows-1));
        m_cur.setX(qBound(0, qMax(1,p1)-1, m_cols-1));
        m_wrap = false;
        break;
    case 'd': m_cur.setY(qBound(0, qMax(1,p0)-1, m_rows-1)); m_wrap=false; break; // VPA

    // ── Erase ─────────────────────────────────────────────────────────────
    case 'J': eraseDisplay(p0); break;
    case 'K': eraseLine(p0);    break;
    case 'X': {  // ECH
        int n = qMin(qMax(1,p0), m_cols - m_cur.x());
        eraseRow(grid[m_cur.y()], m_cur.x(), m_cur.x() + n - 1);
        break;
    }

    // ── Scroll ────────────────────────────────────────────────────────────
    case 'S': scrollUp(qMax(1,p0));   break;
    case 'T': scrollDown(qMax(1,p0)); break;

    // ── Insert / delete ───────────────────────────────────────────────────
    case 'L': {  // IL
        int n = qMax(1,p0);
        for (int i = 0; i < n; ++i) {
            grid.insert(m_cur.y(), blankRow());
            if ((int)grid.size() > m_rows) grid.removeAt(stBot + 1);
        }
        break;
    }
    case 'M': {  // DL
        int n = qMax(1,p0);
        for (int i = 0; i < n; ++i) {
            if (m_cur.y() < (int)grid.size()) grid.removeAt(m_cur.y());
            grid.insert(stBot, blankRow());
        }
        break;
    }
    case 'P': {  // DCH
        int n = qMin(qMax(1,p0), m_cols - m_cur.x());
        auto &row = grid[m_cur.y()];
        row.remove(m_cur.x(), n);
        while ((int)row.size() < m_cols) row.append(m_attr);
        break;
    }
    case '@': {  // ICH
        int n = qMin(qMax(1,p0), m_cols - m_cur.x());
        auto &row = grid[m_cur.y()];
        for (int i = 0; i < n; ++i) row.insert(m_cur.x(), m_attr);
        while ((int)row.size() > m_cols) row.removeLast();
        break;
    }

    // ── SGR ───────────────────────────────────────────────────────────────
    case 'm': applySgr(); break;

    // ── Scroll region ─────────────────────────────────────────────────────
    case 'r': {  // DECSTBM
        int top = qMax(1,p0) - 1;
        int bot = (p1 == 0 ? m_rows : p1) - 1;
        if (top < bot && bot < m_rows) { m_stTop = top; m_stBot = bot; }
        m_cur = {0, m_stTop};
        m_wrap = false;
        break;
    }

    case 's': saveCursor();    break;
    case 'u': restoreCursor(); break;

    // ── Misc ──────────────────────────────────────────────────────────────
    case 'n': // DSR – device status report
        if (p0 == 6) {
            // CPR: cursor position report – we don't have a way to write back
            // (no callback to PTY from screen); ignore.
        }
        break;

    default: break;
    }
}

// ── OSC dispatch ─────────────────────────────────────────────────────────────

void TerminalScreen::oscExecute()
{
    int semi = m_pbuf.indexOf(';');
    if (semi < 0) return;
    int     code = m_pbuf.left(semi).toInt();
    QString text = QString::fromUtf8(m_pbuf.mid(semi + 1));
    if (code == 0 || code == 2) {
        m_title = text;
        emit titleChanged(m_title);
    }
}

// ── SGR ───────────────────────────────────────────────────────────────────────

void TerminalScreen::applySgr()
{
    if (m_pars.isEmpty() || (m_pars.size() == 1 && m_pars[0] <= 0)) {
        m_attr.fg   = kTermDefaultFg;
        m_attr.bg   = kTermDefaultBg;
        m_attr.bold = m_attr.ul = m_attr.rev = false;
        return;
    }

    for (int i = 0; i < (int)m_pars.size(); ++i) {
        int p = (m_pars[i] < 0) ? 0 : m_pars[i];
        switch (p) {
        case 0:
            m_attr.fg = kTermDefaultFg; m_attr.bg = kTermDefaultBg;
            m_attr.bold = m_attr.ul = m_attr.rev = false;
            break;
        case 1:  m_attr.bold = true;  break;
        case 4:  m_attr.ul   = true;  break;
        case 7:  m_attr.rev  = true;  break;
        case 22: m_attr.bold = false; break;
        case 24: m_attr.ul   = false; break;
        case 27: m_attr.rev  = false; break;

        // Standard fg
        case 30: case 31: case 32: case 33:
        case 34: case 35: case 36: case 37:
            m_attr.fg = ansi4(p - 30 + (m_attr.bold ? 8 : 0)); break;
        case 39: m_attr.fg = kTermDefaultFg; break;

        // Standard bg
        case 40: case 41: case 42: case 43:
        case 44: case 45: case 46: case 47:
            m_attr.bg = ansi4(p - 40); break;
        case 49: m_attr.bg = kTermDefaultBg; break;

        // Bright fg
        case 90: case 91: case 92: case 93:
        case 94: case 95: case 96: case 97:
            m_attr.fg = ansi4(p - 90 + 8); break;

        // Bright bg
        case 100: case 101: case 102: case 103:
        case 104: case 105: case 106: case 107:
            m_attr.bg = ansi4(p - 100 + 8); break;

        // 256-colour / true-colour fg and bg
        case 38: case 48: {
            bool isFg = (p == 38);
            int sub   = (i + 1 < (int)m_pars.size()) ? m_pars[i + 1] : -1;
            if (sub == 5 && i + 2 < (int)m_pars.size()) {
                QRgb c = xterm256(m_pars[i + 2]);
                if (isFg) m_attr.fg = c; else m_attr.bg = c;
                i += 2;
            } else if (sub == 2 && i + 4 < (int)m_pars.size()) {
                int r = qBound(0, m_pars[i+2], 255);
                int g = qBound(0, m_pars[i+3], 255);
                int b = qBound(0, m_pars[i+4], 255);
                QRgb c = qRgba(r, g, b, 255);
                if (isFg) m_attr.fg = c; else m_attr.bg = c;
                i += 4;
            }
            break;
        }
        default: break;
        }
    }
}

// ── Screen operations ─────────────────────────────────────────────────────────

void TerminalScreen::put(char32_t cp)
{
    auto &grid = m_altOn ? m_alt : m_pri;
    const int stBot = (m_stBot == 0) ? m_rows - 1 : m_stBot;

    if (m_wrap) {
        cr();
        if (m_cur.y() == stBot) scrollUp(1);
        else                    m_cur.setY(m_cur.y() + 1);
        m_wrap = false;
    }

    if (m_cur.x() < m_cols && m_cur.y() < m_rows) {
        TermCell cell  = m_attr;
        cell.cp        = cp;
        grid[m_cur.y()][m_cur.x()] = cell;
    }

    if (m_cur.x() >= m_cols - 1) m_wrap = true;
    else                          m_cur.setX(m_cur.x() + 1);
}

void TerminalScreen::lineFeed()
{
    const int stBot = (m_stBot == 0) ? m_rows - 1 : m_stBot;
    if (m_cur.y() == stBot) scrollUp(1);
    else                    m_cur.setY(qMin(m_rows - 1, m_cur.y() + 1));
}

void TerminalScreen::revLineFeed()
{
    if (m_cur.y() == m_stTop) scrollDown(1);
    else                      m_cur.setY(qMax(0, m_cur.y() - 1));
}

void TerminalScreen::cr()  { m_cur.setX(0); m_wrap = false; }
void TerminalScreen::bs()  { if (m_cur.x() > 0) { m_cur.setX(m_cur.x() - 1); m_wrap = false; } }
void TerminalScreen::ht()
{
    int next = ((m_cur.x() / 8) + 1) * 8;
    m_cur.setX(qMin(next, m_cols - 1));
}

void TerminalScreen::scrollUp(int n)
{
    auto &grid  = m_altOn ? m_alt : m_pri;
    const int stBot = (m_stBot == 0) ? m_rows - 1 : m_stBot;

    for (int i = 0; i < n; ++i) {
        // Preserve to scrollback only when on primary screen with full region
        if (!m_altOn && m_stTop == 0 && stBot == m_rows - 1) {
            if ((int)m_sb.size() >= kSbMax) m_sb.removeFirst();
            m_sb.push_back(grid[m_stTop]);
        }
        grid.removeAt(m_stTop);
        grid.insert(stBot, blankRow());
    }
}

void TerminalScreen::scrollDown(int n)
{
    auto &grid  = m_altOn ? m_alt : m_pri;
    const int stBot = (m_stBot == 0) ? m_rows - 1 : m_stBot;

    for (int i = 0; i < n; ++i) {
        if (stBot < (int)grid.size()) grid.removeAt(stBot);
        grid.insert(m_stTop, blankRow());
    }
}

void TerminalScreen::eraseDisplay(int mode)
{
    auto &grid = m_altOn ? m_alt : m_pri;
    switch (mode) {
    case 0:
        eraseRow(grid[m_cur.y()], m_cur.x(), m_cols - 1);
        for (int r = m_cur.y() + 1; r < m_rows; ++r) grid[r] = blankRow();
        break;
    case 1:
        for (int r = 0; r < m_cur.y(); ++r) grid[r] = blankRow();
        eraseRow(grid[m_cur.y()], 0, m_cur.x());
        break;
    case 2: case 3:
        for (auto &r : grid) r = blankRow();
        break;
    default: break;
    }
}

void TerminalScreen::eraseLine(int mode)
{
    auto &grid = m_altOn ? m_alt : m_pri;
    switch (mode) {
    case 0: eraseRow(grid[m_cur.y()], m_cur.x(), m_cols - 1); break;
    case 1: eraseRow(grid[m_cur.y()], 0, m_cur.x());          break;
    case 2: grid[m_cur.y()] = blankRow();                      break;
    default: break;
    }
}

void TerminalScreen::eraseRow(Row &r, int from, int to)
{
    TermCell blank;
    blank.fg = m_attr.fg;
    blank.bg = m_attr.bg;
    for (int c = from; c <= to && c < (int)r.size(); ++c)
        r[c] = blank;
}

TerminalScreen::Row TerminalScreen::blankRow() const
{
    TermCell blank;
    blank.fg = kTermDefaultFg;
    blank.bg = kTermDefaultBg;
    return Row(m_cols, blank);
}

void TerminalScreen::switchAlt(bool on)
{
    if (on == m_altOn) return;
    m_altOn = on;
}

void TerminalScreen::saveCursor()
{
    if (m_altOn) m_altSaved = m_cur;
    else         m_saved    = m_cur;
}

void TerminalScreen::restoreCursor()
{
    m_cur = m_altOn ? m_altSaved : m_saved;
    m_cur.setX(qBound(0, m_cur.x(), m_cols - 1));
    m_cur.setY(qBound(0, m_cur.y(), m_rows - 1));
    m_wrap = false;
}
