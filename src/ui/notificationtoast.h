#pragma once

#include <QFrame>
#include <QTimer>

#include <functional>

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

    /// Add an action button to the toast. Call before show().
    void addAction(const QString &label, std::function<void()> callback);

    /// Show a notification on the given parent widget.
    static void show(QWidget *parent, const QString &message,
                     Level level = Info, int durationMs = 4000);

    /// Show a notification with a single action button.
    static NotificationToast *showWithAction(QWidget *parent,
                                             const QString &message,
                                             const QString &actionLabel,
                                             std::function<void()> callback,
                                             Level level = Info,
                                             int durationMs = 8000);

private:
    void positionOnParent();

    QLabel      *m_label;
    QToolButton *m_closeBtn;
    QHBoxLayout *m_layout;
    QTimer       m_timer;
};
