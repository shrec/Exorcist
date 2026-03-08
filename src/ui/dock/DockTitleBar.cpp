#include "DockTitleBar.h"
#include "DockArea.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>

namespace exdock {

static QToolButton *makeTitleBtn(const QString &text, const QString &tooltip,
                                 QWidget *parent)
{
    auto *btn = new QToolButton(parent);
    btn->setText(text);
    btn->setToolTip(tooltip);
    btn->setFixedSize(20, 20);
    btn->setAutoRaise(true);
    return btn;
}

DockTitleBar::DockTitleBar(DockArea *area, QWidget *parent)
    : QWidget(parent), m_area(area)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(6, 0, 2, 0);
    lay->setSpacing(1);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("exdock-title-label"));
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    lay->addWidget(m_titleLabel, 1);

    m_pinBtn  = makeTitleBtn(QStringLiteral("\U0001F4CC"), tr("Pin / Unpin"), this);
    m_pinBtn->setObjectName(QStringLiteral("exdock-pin-btn"));
    lay->addWidget(m_pinBtn);

    m_closeBtn = makeTitleBtn(QStringLiteral("\u00D7"), tr("Close"), this);
    m_closeBtn->setObjectName(QStringLiteral("exdock-close-btn"));
    lay->addWidget(m_closeBtn);

    setFixedHeight(26);
    setObjectName(QStringLiteral("exdock-title-bar"));

    connect(m_pinBtn,   &QToolButton::clicked, this, &DockTitleBar::pinRequested);
    connect(m_closeBtn, &QToolButton::clicked, this, &DockTitleBar::closeRequested);
}

void DockTitleBar::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

QString DockTitleBar::title() const
{
    return m_titleLabel->text();
}

// ── Mouse handling ───────────────────────────────────────────────────────

void DockTitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStart = event->globalPosition().toPoint();
        m_dragging  = false;
    }
    QWidget::mousePressEvent(event);
}

void DockTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && !m_dragging) {
        if ((event->globalPosition().toPoint() - m_dragStart).manhattanLength()
                > QApplication::startDragDistance()) {
            m_dragging = true;
            emit dragStarted(event->globalPosition().toPoint());
        }
    }
    QWidget::mouseMoveEvent(event);
}

void DockTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QWidget::mouseReleaseEvent(event);
}

void DockTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit floatRequested();
    QWidget::mouseDoubleClickEvent(event);
}

void DockTitleBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), palette().window());
}

} // namespace exdock
