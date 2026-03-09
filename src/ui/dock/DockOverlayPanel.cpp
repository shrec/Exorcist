#include "DockOverlayPanel.h"
#include "ExDockWidget.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

namespace exdock {

DockOverlayPanel::DockOverlayPanel(QWidget *mainWindow, QWidget *parent)
    : QFrame(parent)
    , m_mainWindow(mainWindow)
{
    setObjectName(QStringLiteral("exdock-overlay-panel"));
    setFrameShape(QFrame::NoFrame);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    setMinimumSize(150, 100);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_hideTimer.setSingleShot(true);
    m_hideTimer.setInterval(350);
    connect(&m_hideTimer, &QTimer::timeout, this, &DockOverlayPanel::hideOverlay);

    // Install event filter on the application for global click detection
    qApp->installEventFilter(this);
}

DockOverlayPanel::~DockOverlayPanel()
{
    // Detach content if any
    if (m_activeDock && m_activeDock->contentWidget()) {
        m_activeDock->contentWidget()->setParent(m_activeDock);
    }
}

void DockOverlayPanel::showForDock(ExDockWidget *dock, SideBarArea side,
                                    const QRect &sideBarGeometry)
{
    if (m_activeDock && m_activeDock != dock)
        hideOverlay();

    cancelHide();
    m_activeDock = dock;
    m_activeSide = side;

    // Clean up previous overlay title bar (prevents widget leak)
    if (m_overlayTitleBar) {
        m_overlayTitleBar->deleteLater();
        m_overlayTitleBar = nullptr;
    }
    delete layout(); // Qt requires synchronous layout delete before replacement

    auto *mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(1, 1, 1, 1);
    mainLay->setSpacing(0);

    // Title bar
    auto *titleBar = new QWidget(this);
    titleBar->setObjectName(QStringLiteral("exdock-overlay-titlebar"));
    auto *titleLay = new QHBoxLayout(titleBar);
    titleLay->setContentsMargins(6, 2, 2, 2);
    titleLay->setSpacing(2);

    auto *titleLabel = new QLabel(dock->title(), titleBar);
    titleLabel->setObjectName(QStringLiteral("exdock-overlay-title"));
    titleLabel->setTextFormat(Qt::PlainText);
    titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titleLay->addWidget(titleLabel, 1);

    // Pin button — restore to docked layout
    auto *pinBtn = new QToolButton(titleBar);
    pinBtn->setObjectName(QStringLiteral("exdock-overlay-pin"));
    pinBtn->setFixedSize(20, 20);
    pinBtn->setToolTip(tr("Pin to dock area"));
    pinBtn->setText(QStringLiteral("\U0001F4CC")); // 📌
    connect(pinBtn, &QToolButton::clicked, this, [this]() {
        if (m_activeDock)
            emit pinRequested(m_activeDock);
    });
    titleLay->addWidget(pinBtn);

    // Close button
    auto *closeBtn = new QToolButton(titleBar);
    closeBtn->setObjectName(QStringLiteral("exdock-overlay-close"));
    closeBtn->setFixedSize(20, 20);
    closeBtn->setText(QStringLiteral("\u00D7")); // ×
    connect(closeBtn, &QToolButton::clicked, this, [this]() {
        if (m_activeDock)
            emit closeRequested(m_activeDock);
    });
    titleLay->addWidget(closeBtn);

    titleBar->setFixedHeight(24);
    m_overlayTitleBar = titleBar;
    mainLay->addWidget(titleBar);

    // Reparent the dock widget's content into the overlay
    if (auto *content = dock->contentWidget()) {
        content->setParent(this);
        mainLay->addWidget(content, 1);
        content->show();
    }

    // Position the overlay
    const QRect geo = computeGeometry(side, sideBarGeometry);
    setGeometry(geo);
    show();
    raise();
}

void DockOverlayPanel::hideOverlay()
{
    if (!m_activeDock)
        return;

    // Don't hide if mouse is inside overlay or focus is inside it
    if (underMouse() || isAncestorOf(QApplication::focusWidget())) {
        scheduleHide(); // retry later
        return;
    }

    forceHideOverlay();
}

void DockOverlayPanel::forceHideOverlay()
{
    if (!m_activeDock)
        return;

    cancelHide();

    // Reparent content back to the dock widget
    if (auto *content = m_activeDock->contentWidget()) {
        content->setParent(m_activeDock);
        m_activeDock->layout()->addWidget(content);
    }

    m_activeDock = nullptr;
    m_activeSide = SideBarArea::None;
    hide();

    emit overlayHidden();
}

bool DockOverlayPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (m_activeDock && event->type() == QEvent::MouseButtonPress) {
        auto *w = qobject_cast<QWidget *>(watched);
        if (w && !isAncestorOf(w) && w != this) {
            // Click landed outside the overlay — schedule auto-hide
            // unless the click is on the sidebar tab itself
            scheduleHide();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void DockOverlayPanel::scheduleHide()
{
    m_hideTimer.start();
}

void DockOverlayPanel::cancelHide()
{
    m_hideTimer.stop();
}

QRect DockOverlayPanel::computeGeometry(SideBarArea side,
                                         const QRect &sideBarGeo) const
{
    const QRect mainGeo = m_mainWindow->geometry();

    // Use the dock widget's preferred size if available, clamped to window
    int prefW = 350;
    int prefH = 400;
    if (m_activeDock) {
        const QSize pref = m_activeDock->preferredSize();
        prefW = qBound(150, pref.width(), mainGeo.width() / 2);
        prefH = qBound(100, pref.height(), mainGeo.height() / 2);
    }

    switch (side) {
    case SideBarArea::Left:
        return QRect(sideBarGeo.right() + 1,
                     sideBarGeo.top(),
                     prefW,
                     sideBarGeo.height());

    case SideBarArea::Right:
        return QRect(sideBarGeo.left() - prefW,
                     sideBarGeo.top(),
                     prefW,
                     sideBarGeo.height());

    case SideBarArea::Bottom:
        return QRect(sideBarGeo.left(),
                     sideBarGeo.top() - prefH,
                     sideBarGeo.width(),
                     prefH);

    case SideBarArea::Top:
        return QRect(sideBarGeo.left(),
                     sideBarGeo.bottom() + 1,
                     sideBarGeo.width(),
                     prefH);

    default:
        return QRect(mainGeo.left() + 34, mainGeo.top() + 30,
                     prefW, prefH);
    }
}

} // namespace exdock
