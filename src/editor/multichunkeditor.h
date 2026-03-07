#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QUrl>

// ─────────────────────────────────────────────────────────────────────────────
// MultiChunkEditor — applies multiple independent hunks from one AI response.
//
// The AI agent may return multiple separate edits across different regions of
// a file (or across multiple files). This class coordinates applying them
// in bottom-to-top order so line numbers remain valid.
// ─────────────────────────────────────────────────────────────────────────────

struct EditHunk {
    QString filePath;
    int startLine = 0;       // 1-based
    int endLine = 0;         // 1-based, inclusive
    QString oldText;         // original text (for verification)
    QString newText;         // replacement text
    bool applied = false;
    bool accepted = false;
};

class MultiChunkEditor : public QObject
{
    Q_OBJECT

public:
    explicit MultiChunkEditor(QObject *parent = nullptr)
        : QObject(parent) {}

    /// Set the hunks to apply
    void setHunks(const QList<EditHunk> &hunks)
    {
        m_hunks = hunks;
        m_currentIndex = 0;
        emit hunksChanged();
    }

    /// Get all hunks
    QList<EditHunk> hunks() const { return m_hunks; }

    /// Get number of hunks
    int hunkCount() const { return m_hunks.size(); }

    /// Get current hunk index
    int currentIndex() const { return m_currentIndex; }

    /// Navigate to next hunk
    void nextHunk()
    {
        if (m_currentIndex < m_hunks.size() - 1) {
            ++m_currentIndex;
            emit currentHunkChanged(m_currentIndex);
        }
    }

    /// Navigate to previous hunk
    void prevHunk()
    {
        if (m_currentIndex > 0) {
            --m_currentIndex;
            emit currentHunkChanged(m_currentIndex);
        }
    }

    /// Accept a specific hunk
    void acceptHunk(int index)
    {
        if (index < 0 || index >= m_hunks.size()) return;
        m_hunks[index].accepted = true;
        emit hunkAccepted(index);
    }

    /// Reject a specific hunk
    void rejectHunk(int index)
    {
        if (index < 0 || index >= m_hunks.size()) return;
        m_hunks[index].accepted = false;
        emit hunkRejected(index);
    }

    /// Accept all hunks
    void acceptAll()
    {
        for (int i = 0; i < m_hunks.size(); ++i)
            m_hunks[i].accepted = true;
        emit allAccepted();
    }

    /// Reject all hunks
    void rejectAll()
    {
        for (int i = 0; i < m_hunks.size(); ++i)
            m_hunks[i].accepted = false;
        emit allRejected();
    }

    /// Apply all accepted hunks. Returns the modified text.
    /// Hunks are applied bottom-to-top to preserve line numbers.
    static QString applyHunks(const QString &originalText,
                               QList<EditHunk> &hunks)
    {
        // Sort by startLine descending (apply from bottom to top)
        QList<EditHunk *> sorted;
        for (auto &h : hunks) {
            if (h.accepted)
                sorted.append(&h);
        }
        std::sort(sorted.begin(), sorted.end(),
                  [](const EditHunk *a, const EditHunk *b) {
                      return a->startLine > b->startLine;
                  });

        QStringList lines = originalText.split('\n');

        for (EditHunk *hunk : sorted) {
            if (hunk->startLine < 1 || hunk->endLine > lines.size())
                continue;

            const int start = hunk->startLine - 1;
            const int count = hunk->endLine - hunk->startLine + 1;

            // Remove old lines
            for (int i = 0; i < count; ++i)
                lines.removeAt(start);

            // Insert new lines
            const QStringList newLines = hunk->newText.split('\n');
            for (int i = 0; i < newLines.size(); ++i)
                lines.insert(start + i, newLines[i]);

            hunk->applied = true;
        }

        return lines.join('\n');
    }

    /// Parse hunks from a JSON array
    static QList<EditHunk> fromJson(const QJsonArray &arr)
    {
        QList<EditHunk> result;
        for (const auto &val : arr) {
            const QJsonObject obj = val.toObject();
            EditHunk h;
            h.filePath = obj.value(QStringLiteral("file")).toString();
            h.startLine = obj.value(QStringLiteral("startLine")).toInt();
            h.endLine = obj.value(QStringLiteral("endLine")).toInt();
            h.oldText = obj.value(QStringLiteral("oldText")).toString();
            h.newText = obj.value(QStringLiteral("newText")).toString();
            result.append(h);
        }
        return result;
    }

signals:
    void hunksChanged();
    void currentHunkChanged(int index);
    void hunkAccepted(int index);
    void hunkRejected(int index);
    void allAccepted();
    void allRejected();

private:
    QList<EditHunk> m_hunks;
    int m_currentIndex = 0;
};
