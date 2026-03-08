#pragma once

#include <QWidget>
#include <QList>
#include <QString>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QTabWidget;

// ── ProposedEdit ──────────────────────────────────────────────────────────────
// Represents one file proposed for editing.
struct ProposedEdit
{
    QString filePath;       // absolute path
    QString originalText;   // original file content
    QString proposedText;   // AI-proposed content
};

// ── ProposedEditPanel ─────────────────────────────────────────────────────────
// Side-by-side diff view with Accept / Reject buttons for AI-proposed edits.
// Shows one or more files; user can accept or reject per-file or all at once.

class ProposedEditPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProposedEditPanel(QWidget *parent = nullptr);

    void setEdits(const QList<ProposedEdit> &edits);
    void clear();

    bool hasEdits() const { return !m_edits.isEmpty(); }

signals:
    void editAccepted(const QString &filePath, const QString &newText);
    void editRejected(const QString &filePath);
    void allAccepted();
    void allRejected();

private:
    void addFileDiff(const ProposedEdit &edit, int index);
    void highlightDiffLines(QPlainTextEdit *left, QPlainTextEdit *right);

    QTabWidget               *m_tabs;
    QPushButton              *m_acceptAllBtn;
    QPushButton              *m_rejectAllBtn;
    QLabel                   *m_summaryLabel;
    QList<ProposedEdit>       m_edits;
};
