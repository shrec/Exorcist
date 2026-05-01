#pragma once

#include <QFrame>
#include <QList>
#include <QPointer>
#include <QTimer>

#include <functional>

class QLabel;
class QToolButton;
class QHBoxLayout;
class QPropertyAnimation;

/// Non-modal toast notification that slides in from the bottom-right
/// and auto-dismisses after a timeout. Multiple toasts stack vertically
/// above one another, with fade in/out animations.
class NotificationToast : public QFrame
{
    Q_OBJECT

public:
    enum Level { Info, Success, Warning, Error };

    explicit NotificationToast(const QString &message, Level level = Info,
                               int durationMs = 4000, QWidget *parent = nullptr);
    ~NotificationToast() override;

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void positionOnParent();
    void startFadeIn();
    void dismiss();
    static void repositionAll(QWidget *parent);

    QLabel             *m_label;
    QToolButton        *m_closeBtn;
    QHBoxLayout        *m_layout;
    QTimer              m_timer;
    QPropertyAnimation *m_fadeAnim {nullptr};
    bool                m_dismissing {false};

    static QList<QPointer<NotificationToast>> s_visibleToasts;
};
