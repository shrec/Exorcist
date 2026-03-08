#include "notificationtoast.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>

NotificationToast::NotificationToast(const QString &message, Level level,
                                     int durationMs, QWidget *parent)
    : QFrame(parent)
    , m_label(new QLabel(message, this))
    , m_closeBtn(new QToolButton(this))
    , m_layout(nullptr)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setFrameShape(QFrame::StyledPanel);

    QString bg, fg, border;
    switch (level) {
    case Success: bg = QStringLiteral("#2d4a2d"); fg = QStringLiteral("#6fbf73"); border = QStringLiteral("#4caf50"); break;
    case Warning: bg = QStringLiteral("#4a3d1a"); fg = QStringLiteral("#ffb74d"); border = QStringLiteral("#ff9800"); break;
    case Error:   bg = QStringLiteral("#4a1a1a"); fg = QStringLiteral("#f44747"); border = QStringLiteral("#f44747"); break;
    default:      bg = QStringLiteral("#252526"); fg = QStringLiteral("#d4d4d4"); border = QStringLiteral("#007acc"); break;
    }

    setStyleSheet(QStringLiteral(
        "NotificationToast { background: %1; border: 1px solid %2; "
        "border-radius: 6px; padding: 8px 12px; }"
        "QLabel { color: %3; font-size: 13px; }"
        "QToolButton { color: %3; border: none; font-size: 14px; }"
        "QToolButton#actionBtn { background: #0078d4; color: #ffffff; "
        "border-radius: 3px; padding: 2px 8px; font-size: 12px; }")
        .arg(bg, border, fg));

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(8);

    m_label->setWordWrap(true);
    m_label->setMaximumWidth(400);
    m_layout->addWidget(m_label, 1);

    m_closeBtn->setText(QStringLiteral("\u2715"));
    m_closeBtn->setFixedSize(20, 20);
    connect(m_closeBtn, &QToolButton::clicked, this, &QWidget::close);
    m_layout->addWidget(m_closeBtn);

    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &QWidget::close);
    if (durationMs > 0)
        m_timer.start(durationMs);

    adjustSize();
    positionOnParent();
    QFrame::show();
    raise();
}

void NotificationToast::positionOnParent()
{
    QWidget *p = parentWidget();
    if (!p) return;

    const QPoint parentGlobal = p->mapToGlobal(QPoint(0, 0));
    const int x = parentGlobal.x() + p->width() - width() - 16;
    const int y = parentGlobal.y() + p->height() - height() - 16;
    move(x, y);
}

void NotificationToast::show(QWidget *parent, const QString &message,
                              Level level, int durationMs)
{
    // The toast deletes itself when closed (WA_DeleteOnClose)
    new NotificationToast(message, level, durationMs, parent);
}

void NotificationToast::addAction(const QString &label,
                                  std::function<void()> callback)
{
    auto *btn = new QToolButton(this);
    btn->setObjectName(QStringLiteral("actionBtn"));
    btn->setText(label);
    connect(btn, &QToolButton::clicked, this, [this, callback]() {
        if (callback) callback();
        close();
    });
    // Insert before the close button
    m_layout->insertWidget(m_layout->count() - 1, btn);
    adjustSize();
    positionOnParent();
}

NotificationToast *NotificationToast::showWithAction(QWidget *parent,
                                                     const QString &message,
                                                     const QString &actionLabel,
                                                     std::function<void()> callback,
                                                     Level level,
                                                     int durationMs)
{
    auto *toast = new NotificationToast(message, level, durationMs, parent);
    toast->addAction(actionLabel, std::move(callback));
    return toast;
}
