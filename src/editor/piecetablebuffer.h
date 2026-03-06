#pragma once

#include "textbuffer.h"

#include <QString>
#include <QVector>

// Piece table implementation of TextBuffer.
//
// Two immutable buffers:
//   original  — initial file content, never modified.
//   add       — append-only buffer for all inserted text.
//
// A piece descriptor refers to a contiguous run in one of the two buffers.
// Edits split or replace pieces without touching stored text.

class PieceTableBuffer : public TextBuffer
{
public:
    explicit PieceTableBuffer(const QString &initial = {});

    int length() const override;
    QString slice(int start, int length) const override;

    void insert(int pos, const QString &text);
    void remove(int pos, int length);

    // Convenience: return full text (re-assembles all pieces).
    QString text() const;

private:
    struct Piece {
        bool inAdd;     // true = add buffer, false = original buffer
        int  start;     // offset in the buffer
        int  length;    // number of QChar units
    };

    // Find which piece contains logical offset `pos` and the offset within it.
    // Returns index into m_pieces; sets *offsetInPiece.
    int pieceAt(int pos, int *offsetInPiece) const;

    // Split piece at index `idx` so that piece[idx] ends at `at` chars.
    void splitPiece(int idx, int at);

    QString m_original;
    QString m_add;
    QVector<Piece> m_pieces;
    int m_length = 0;
};
