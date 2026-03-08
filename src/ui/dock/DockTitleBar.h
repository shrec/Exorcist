#pragma once

#include <QWidget>

class QLabel;
class QToolButton;

namespace exdock {

class DockArea;

/// Title bar for a DockArea. Contains title text + pin / maximise / close
/// buttons. Supports double-click to float and drag to relocate.
class DockTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit DockTitleBar(DockArea *area, QWidget *parent = nullptr);

    void setTitle(const QString &title);
    QString title() const;

signals:
    void pinRequested();
    void closeRequested();
    void floatRequested();
    void dragStarted(const QPoint &globalPos);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    DockArea    *m_area;
    QLabel      *m_titleLabel;
    QToolButton *m_pinBtn;
    QToolButton *m_closeBtn;
    QPoint       m_dragStart;
    bool         m_dragging = false;
};

} // namespace exdock
