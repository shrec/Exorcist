#pragma once

#include <QWidget>

class QPlainTextEdit;

class MinimapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MinimapWidget(QPlainTextEdit *editor, QWidget *parent = nullptr);

    void updateContent();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void scrollToY(int y);

    QPlainTextEdit *m_editor = nullptr;
    QPixmap         m_cache;
    bool            m_dirty = true;
};
