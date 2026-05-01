#pragma once

#include "git/gitservice.h"

#include <QPointer>
#include <QString>
#include <QWidget>

class QPlainTextEdit;

/// Transparent overlay that paints inline git-blame annotations after the
/// current line of the editor — GitLens-style.
///
/// The overlay is parented to the editor's viewport and forwards mouse events
/// (it uses Qt::WA_TransparentForMouseEvents but installs an event filter on
/// the editor to deliver clicks/hovers on the annotation glyph itself).
///
/// Cross-DLL safety: the overlay never touches EditorView API. It uses only
/// QPlainTextEdit base APIs (textCursor, viewport, blockBoundingGeometry,
/// document) which are guaranteed available across DLL boundaries.
class InlineBlameOverlay : public QWidget
{
    Q_OBJECT

public:
    explicit InlineBlameOverlay(QPlainTextEdit *editor,
                                GitService *gitService,
                                QWidget *parent = nullptr);

    /// Path of the file shown in the editor (used as blame cache key).
    void setFilePath(const QString &path);
    QString filePath() const { return m_filePath; }

    /// Globally enable/disable the overlay (per QSettings toggle).
    void setEnabled(bool on);
    bool isEnabledOverlay() const { return m_enabled; }

    /// "currentLine" → only annotate the line that has the cursor.
    /// "allLines"    → annotate every visible line (heavier; cached).
    enum Mode { CurrentLineOnly, AllVisibleLines };
    void setMode(Mode m);
    Mode mode() const { return m_mode; }

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onBlameLineReady(const QString &filePath, int line,
                          const GitService::BlameLineInfo &info);
    void onCursorOrViewportChanged();

private:
    void requestBlameForCurrentLine();
    void repositionToViewport();
    QString formatAnnotation(const GitService::BlameLineInfo &info) const;
    static QString relativeTime(qint64 unixSeconds);
    QRect annotationRectForLine(int line, const QString &text) const;
    int lineUnderPoint(const QPoint &viewportPos) const;
    void showCommitPopover(const GitService::BlameLineInfo &info,
                           const QPoint &globalPos);

    QPointer<QPlainTextEdit> m_editor;
    QPointer<GitService>     m_git;
    QString                  m_filePath;
    bool                     m_enabled = true;
    Mode                     m_mode    = CurrentLineOnly;

    int  m_lastRequestedLine = -1; // 1-based
    QRect m_lastAnnotationRect; // viewport coords; for click hit-testing
    GitService::BlameLineInfo m_lastShownInfo;
};
