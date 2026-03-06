#include "piecetablebuffer.h"

PieceTableBuffer::PieceTableBuffer(const QString &initial)
    : m_original(initial),
      m_length(initial.size())
{
    if (!initial.isEmpty()) {
        m_pieces.push_back(Piece{false, 0, static_cast<int>(initial.size())});
    }
}

int PieceTableBuffer::length() const
{
    return m_length;
}

QString PieceTableBuffer::slice(int start, int len) const
{
    if (len <= 0 || start >= m_length) {
        return {};
    }

    QString result;
    result.reserve(len);

    int remaining = len;
    int logical = 0;

    for (const Piece &p : m_pieces) {
        if (logical + p.length <= start) {
            logical += p.length;
            continue;
        }

        const int offsetInPiece = (start > logical) ? (start - logical) : 0;
        const int available = p.length - offsetInPiece;
        const int take = qMin(available, remaining);

        const QString &buf = p.inAdd ? m_add : m_original;
        result.append(buf.mid(p.start + offsetInPiece, take));

        remaining -= take;
        if (remaining <= 0) {
            break;
        }

        logical += p.length;
    }

    return result;
}

QString PieceTableBuffer::text() const
{
    return slice(0, m_length);
}

void PieceTableBuffer::insert(int pos, const QString &text)
{
    if (text.isEmpty()) {
        return;
    }

    const int addStart = static_cast<int>(m_add.size());
    m_add.append(text);
    Piece newPiece{true, addStart, static_cast<int>(text.size())};

    if (pos <= 0) {
        m_pieces.prepend(newPiece);
    } else if (pos >= m_length) {
        m_pieces.append(newPiece);
    } else {
        int offsetInPiece = 0;
        int idx = pieceAt(pos, &offsetInPiece);

        if (offsetInPiece == 0) {
            m_pieces.insert(idx, newPiece);
        } else {
            splitPiece(idx, offsetInPiece);
            m_pieces.insert(idx + 1, newPiece);
        }
    }

    m_length += text.size();
}

void PieceTableBuffer::remove(int pos, int len)
{
    if (len <= 0 || pos >= m_length) {
        return;
    }
    len = qMin(len, m_length - pos);

    // Left boundary: split so deletion starts at a piece boundary.
    int leftOffset = 0;
    int leftIdx = pieceAt(pos, &leftOffset);
    if (leftOffset > 0) {
        splitPiece(leftIdx, leftOffset);
        ++leftIdx;
    }

    // Right boundary: split so deletion ends at a piece boundary.
    const int endPos = pos + len;
    int rightOffset = 0;
    int rightIdx = pieceAt(endPos, &rightOffset);
    if (rightOffset > 0 && endPos < m_length) {
        splitPiece(rightIdx, rightOffset);
    }

    // Remove all pieces from leftIdx up to (not including) the piece that
    // starts at or after endPos.
    int logical = 0;
    for (int i = 0; i < leftIdx; ++i) {
        logical += m_pieces[i].length;
    }

    int i = leftIdx;
    int deleted = 0;
    while (i < m_pieces.size() && deleted < len) {
        deleted += m_pieces[i].length;
        m_pieces.remove(i);
    }

    m_length -= len;
}

int PieceTableBuffer::pieceAt(int pos, int *offsetInPiece) const
{
    int logical = 0;
    for (int i = 0; i < m_pieces.size(); ++i) {
        const int end = logical + m_pieces[i].length;
        if (pos < end || i == m_pieces.size() - 1) {
            *offsetInPiece = pos - logical;
            return i;
        }
        logical = end;
    }
    *offsetInPiece = 0;
    return 0;
}

void PieceTableBuffer::splitPiece(int idx, int at)
{
    Q_ASSERT(at > 0 && at < m_pieces[idx].length);
    Piece &p = m_pieces[idx];
    Piece right{p.inAdd, p.start + at, p.length - at};
    p.length = at;
    m_pieces.insert(idx + 1, right);
}
