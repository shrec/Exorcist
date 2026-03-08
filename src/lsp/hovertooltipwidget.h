#pragma once

#include <QFrame>

class QLabel;

// ── HoverTooltipWidget ───────────────────────────────────────────────────────
//
// A rich tooltip that renders Markdown content as styled HTML.
// Used by LspEditorBridge to show hover information from the language server.
// Automatically hides when the mouse moves away or the user types.

class HoverTooltipWidget : public QFrame
{
    Q_OBJECT

public:
    explicit HoverTooltipWidget(QWidget *parent = nullptr);

    void showTooltip(const QPoint &globalPos, const QString &markdown,
                     QWidget *anchor);
    void hideTooltip();
    bool isTooltipVisible() const;

protected:
    bool event(QEvent *e) override;

private:
    QLabel *m_label;
};
