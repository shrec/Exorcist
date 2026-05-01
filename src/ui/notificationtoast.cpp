#include "notificationtoast.h"

#include <QEasingCurve>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QToolButton>

QList<QPointer<NotificationToast>> NotificationToast::s_visibleToasts;

NotificationToast::NotificationToast(const QString &message, Level level,
                                     int durationMs, QWidget *parent)
    : QFrame(parent)
    , m_label(new QLabel(message, this))
    , m_closeBtn(new QToolButton(this))
    , m_layout(nullptr)
{
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground, false);
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
        "border-radius: 8px; padding: 10px 14px; }"
        "QLabel { color: %3; font-size: 13px; }"
        "QToolButton { color: %3; border: none; font-size: 14px; }"
        "QToolButton#actionBtn { background: #0078d4; color: #ffffff; "
        "border-radius: 3px; padding: 2px 8px; font-size: 12px; }")
        .arg(bg, border, fg));

    // Subtle drop-shadow for VS-like depth.
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setBlurRadius(16);
    shadow->setOffset(0, 2);
    setGraphicsEffect(shadow);

    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(4, 4, 4, 4);
    m_layout->setSpacing(8);

    m_label->setWordWrap(true);
    m_label->setMaximumWidth(380);
    m_layout->addWidget(m_label, 1);

    m_closeBtn->setText(QStringLiteral("✕"));
    m_closeBtn->setFixedSize(20, 20);
    connect(m_closeBtn, &QToolButton::clicked, this, [this]() { dismiss(); });
    m_layout->addWidget(m_closeBtn);

    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this]() { dismiss(); });
    if (durationMs > 0)
        m_timer.start(durationMs);

    // Track this toast in the stacking list.
    s_visibleToasts.append(QPointer<NotificationToast>(this));

    // Watch parent resize so we can re-anchor.
    if (parent)
        parent->installEventFilter(this);

    adjustSize();
    positionOnParent();

    // Start invisible, then animate in.
    setWindowOpacity(0.0);
    QFrame::show();
    raise();

    startFadeIn();

    // Reposition any other toasts on the same parent so they stack correctly
    // above this newly-added one.
    repositionAll(parent);
}

NotificationToast::~NotificationToast()
{
    QWidget *p = parentWidget();

    // Drop ourselves and any expired entries from the visible list.
    s_visibleToasts.erase(
        std::remove_if(s_visibleToasts.begin(), s_visibleToasts.end(),
                       [this](const QPointer<NotificationToast> &t) {
                           return t.isNull() || t.data() == this;
                       }),
        s_visibleToasts.end());

    if (p)
        repositionAll(p);
}

void NotificationToast::startFadeIn()
{
    if (m_fadeAnim) {
        m_fadeAnim->stop();
        m_fadeAnim->deleteLater();
    }
    m_fadeAnim = new QPropertyAnimation(this, "windowOpacity", this);
    m_fadeAnim->setDuration(200);
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_fadeAnim->start(QAbstractAnimation::DeleteWhenStopped);
}

void NotificationToast::dismiss()
{
    if (m_dismissing) return;
    m_dismissing = true;
    m_timer.stop();

    // Remove from the stacking list eagerly so siblings can shift down
    // immediately rather than waiting for actual destruction.
    s_visibleToasts.erase(
        std::remove_if(s_visibleToasts.begin(), s_visibleToasts.end(),
                       [this](const QPointer<NotificationToast> &t) {
                           return t.isNull() || t.data() == this;
                       }),
        s_visibleToasts.end());

    if (parentWidget())
        repositionAll(parentWidget());

    if (m_fadeAnim) {
        m_fadeAnim->stop();
        m_fadeAnim->deleteLater();
        m_fadeAnim = nullptr;
    }

    auto *anim = new QPropertyAnimation(this, "windowOpacity", this);
    anim->setDuration(200);
    anim->setStartValue(windowOpacity());
    anim->setEndValue(0.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QPropertyAnimation::finished, this, &QWidget::close);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

bool NotificationToast::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        repositionAll(parentWidget());
    }
    return QFrame::eventFilter(watched, event);
}

void NotificationToast::positionOnParent()
{
    QWidget *p = parentWidget();
    if (!p) return;

    constexpr int kMargin  = 16;
    constexpr int kSpacing = 8;

    const QPoint parentGlobal = p->mapToGlobal(QPoint(0, 0));
    const int baseX = parentGlobal.x() + p->width()  - width()  - kMargin;
    const int baseY = parentGlobal.y() + p->height() - height() - kMargin;

    // Stack: count how many *active* toasts on the same parent appear before
    // this one in the list (newer toasts go above older ones).
    int offsetY = 0;
    for (const auto &t : s_visibleToasts) {
        if (t.isNull() || t.data() == this) continue;
        if (t->parentWidget() != p) continue;
        if (t->m_dismissing) continue;
        offsetY += t->height() + kSpacing;
    }

    move(baseX, baseY - offsetY);
}

void NotificationToast::repositionAll(QWidget *parent)
{
    if (!parent) return;
    for (const auto &t : s_visibleToasts) {
        if (t.isNull()) continue;
        if (t->parentWidget() != parent) continue;
        if (t->m_dismissing) continue;
        t->positionOnParent();
    }
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
        dismiss();
    });
    // Insert before the close button
    m_layout->insertWidget(m_layout->count() - 1, btn);
    adjustSize();
    positionOnParent();
    repositionAll(parentWidget());
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
