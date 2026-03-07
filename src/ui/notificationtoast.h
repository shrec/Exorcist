#pragma once

#include <QFrame>
#include <QTimer>

class QLabel;
class QToolButton;
class QHBoxLayout;

/// Non-modal toast notification that slides in from the bottom-right
/// and auto-dismisses after a timeout.
class NotificationToast : public QFrame
{
    Q_OBJECT

public:
    enum Level { Info, Success, Warning, Error };

    explicit NotificationToast(const QString &message, Level level = Info,
                               int durationMs = 4000, QWidget *parent = nullptr);

    /// Show a notification on the given parent widget.
    static void show(QWidget *parent, const QString &message,
                     Level level = Info, int durationMs = 4000);

private:
    void positionOnParent();

    QLabel      *m_label;
    QToolButton *m_closeBtn;
    QTimer       m_timer;
};
