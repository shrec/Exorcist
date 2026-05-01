#include "imagepreviewwidget.h"

#include <QAction>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSvgRenderer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QPainter>

namespace {

constexpr double kMinZoom    = 0.05;
constexpr double kMaxZoom    = 32.0;
constexpr double kZoomStep   = 1.25;

// VS dark-modern palette — kept local to avoid coupling with chatthemetokens.
constexpr const char *kBackground   = "#1e1e1e";
constexpr const char *kStatusBg     = "#252526";
constexpr const char *kBorder       = "#3c3c3c";
constexpr const char *kForeground   = "#cccccc";
constexpr const char *kStatusFg     = "#9d9d9d";
constexpr const char *kToolbarBg    = "#2d2d30";
constexpr const char *kToolbarHover = "#3e3e42";

QPixmap renderSvg(const QString &filePath)
{
    QSvgRenderer renderer(filePath);
    if (!renderer.isValid())
        return {};
    QSize size = renderer.defaultSize();
    if (!size.isValid() || size.isEmpty())
        size = QSize(512, 512);
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&p);
    return pix;
}

} // namespace

ImagePreviewWidget::ImagePreviewWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ────────────────────────────────────────────────────────────
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setStyleSheet(QStringLiteral(
        "QToolBar { background: %1; border: 0; border-bottom: 1px solid %2; spacing: 2px; padding: 4px; }"
        "QToolButton { color: %3; background: transparent; border: 1px solid transparent; padding: 4px 8px; border-radius: 2px; }"
        "QToolButton:hover { background: %4; border: 1px solid %2; }"
        "QToolButton:pressed { background: %2; }"
    ).arg(kToolbarBg, kBorder, kForeground, kToolbarHover));

    m_zoomInAction = m_toolbar->addAction(tr("Zoom In"));
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    m_zoomInAction->setText(QStringLiteral("+"));
    m_zoomInAction->setToolTip(tr("Zoom In (Ctrl++)"));
    connect(m_zoomInAction, &QAction::triggered, this, &ImagePreviewWidget::zoomIn);

    m_zoomOutAction = m_toolbar->addAction(tr("Zoom Out"));
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    m_zoomOutAction->setText(QStringLiteral("−")); // unicode minus
    m_zoomOutAction->setToolTip(tr("Zoom Out (Ctrl+-)"));
    connect(m_zoomOutAction, &QAction::triggered, this, &ImagePreviewWidget::zoomOut);

    m_resetZoomAction = m_toolbar->addAction(tr("1:1"));
    m_resetZoomAction->setToolTip(tr("Reset Zoom (Ctrl+0)"));
    m_resetZoomAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(m_resetZoomAction, &QAction::triggered, this, &ImagePreviewWidget::resetZoom);

    m_fitAction = m_toolbar->addAction(tr("Fit"));
    m_fitAction->setCheckable(true);
    m_fitAction->setToolTip(tr("Fit to Window"));
    connect(m_fitAction, &QAction::toggled, this, [this](bool on) {
        m_fitToWindow = on;
        if (on)
            fitToWindow();
        else
            applyZoom();
    });

    root->addWidget(m_toolbar);

    // ── Scroll area + image label ──────────────────────────────────────────
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setBackgroundRole(QPalette::Dark);
    m_scrollArea->setAlignment(Qt::AlignCenter);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setStyleSheet(QStringLiteral(
        "QScrollArea { background: #000000; border: 0; }"
        "QScrollArea > QWidget > QWidget { background: #000000; }"
        "QScrollBar:vertical, QScrollBar:horizontal { background: %1; }"
    ).arg(kBackground));

    m_imageLabel = new QLabel(m_scrollArea);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setBackgroundRole(QPalette::Dark);
    m_imageLabel->setStyleSheet(QStringLiteral("background: #000000; color: %1;").arg(kForeground));
    m_imageLabel->setText(tr("(no image)"));
    m_scrollArea->setWidget(m_imageLabel);

    root->addWidget(m_scrollArea, 1);

    // ── Status bar ─────────────────────────────────────────────────────────
    auto *statusBar = new QWidget(this);
    statusBar->setStyleSheet(QStringLiteral(
        "background: %1; border-top: 1px solid %2;"
    ).arg(kStatusBg, kBorder));
    auto *statusLayout = new QHBoxLayout(statusBar);
    statusLayout->setContentsMargins(8, 3, 8, 3);
    statusLayout->setSpacing(8);

    m_statusLabel = new QLabel(statusBar);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(kStatusFg));
    m_statusLabel->setText(tr("No image loaded"));
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch(1);

    root->addWidget(statusBar);

    setStyleSheet(QStringLiteral("ImagePreviewWidget { background: %1; }").arg(kBackground));
}

ImagePreviewWidget::~ImagePreviewWidget() = default;

bool ImagePreviewWidget::loadImage(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isFile()) {
        m_pixmap = QPixmap();
        m_filePath.clear();
        m_fileSize = 0;
        m_imageLabel->setText(tr("File not found: %1").arg(filePath));
        updateStatus();
        return false;
    }

    QPixmap pix;
    const QString ext = fi.suffix().toLower();
    if (ext == QLatin1String("svg")) {
        pix = renderSvg(filePath);
    } else {
        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        const QImage img = reader.read();
        if (!img.isNull())
            pix = QPixmap::fromImage(img);
    }

    if (pix.isNull()) {
        m_pixmap = QPixmap();
        m_filePath.clear();
        m_fileSize = 0;
        m_imageLabel->setText(tr("Failed to load image: %1").arg(fi.fileName()));
        updateStatus();
        return false;
    }

    m_pixmap   = pix;
    m_filePath = filePath;
    m_fileSize = fi.size();
    m_zoom     = 1.0;
    if (m_fitToWindow)
        fitToWindow();
    else
        applyZoom();
    updateStatus();
    return true;
}

void ImagePreviewWidget::zoomIn()
{
    if (m_pixmap.isNull()) return;
    if (m_fitAction && m_fitAction->isChecked())
        m_fitAction->setChecked(false);
    setZoom(m_zoom * kZoomStep);
}

void ImagePreviewWidget::zoomOut()
{
    if (m_pixmap.isNull()) return;
    if (m_fitAction && m_fitAction->isChecked())
        m_fitAction->setChecked(false);
    setZoom(m_zoom / kZoomStep);
}

void ImagePreviewWidget::resetZoom()
{
    if (m_pixmap.isNull()) return;
    if (m_fitAction && m_fitAction->isChecked())
        m_fitAction->setChecked(false);
    setZoom(1.0);
}

void ImagePreviewWidget::fitToWindow()
{
    if (m_pixmap.isNull()) return;
    const QSize viewport = m_scrollArea->viewport()->size();
    if (viewport.isEmpty() || m_pixmap.width() == 0 || m_pixmap.height() == 0)
        return;
    const double sx = double(viewport.width())  / double(m_pixmap.width());
    const double sy = double(viewport.height()) / double(m_pixmap.height());
    const double s  = qMax(kMinZoom, qMin(sx, sy));
    m_zoom = s;
    applyZoom();
    updateStatus();
}

void ImagePreviewWidget::setZoom(double factor)
{
    factor = qBound(kMinZoom, factor, kMaxZoom);
    if (qFuzzyCompare(factor, m_zoom)) return;
    m_zoom = factor;
    applyZoom();
    updateStatus();
}

void ImagePreviewWidget::applyZoom()
{
    if (m_pixmap.isNull()) {
        m_imageLabel->setPixmap(QPixmap());
        m_imageLabel->adjustSize();
        return;
    }
    const QSize target(qMax(1, int(m_pixmap.width()  * m_zoom)),
                       qMax(1, int(m_pixmap.height() * m_zoom)));
    const QPixmap scaled = m_pixmap.scaled(target,
                                           Qt::KeepAspectRatio,
                                           m_zoom < 1.0 ? Qt::SmoothTransformation
                                                        : Qt::FastTransformation);
    m_imageLabel->setPixmap(scaled);
    m_imageLabel->resize(scaled.size());
}

void ImagePreviewWidget::updateStatus()
{
    if (m_pixmap.isNull() || m_filePath.isEmpty()) {
        m_statusLabel->setText(tr("No image loaded"));
        return;
    }
    const QString text = tr("%1 × %2 px   |   %3   |   Zoom: %4%")
        .arg(m_pixmap.width())
        .arg(m_pixmap.height())
        .arg(formatFileSize(m_fileSize))
        .arg(int(m_zoom * 100.0));
    m_statusLabel->setText(text);
}

QString ImagePreviewWidget::formatFileSize(qint64 bytes)
{
    constexpr double kKB = 1024.0;
    constexpr double kMB = 1024.0 * 1024.0;
    constexpr double kGB = 1024.0 * 1024.0 * 1024.0;
    if (bytes < qint64(kKB))
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < qint64(kMB))
        return QStringLiteral("%1 KB").arg(double(bytes) / kKB, 0, 'f', 1);
    if (bytes < qint64(kGB))
        return QStringLiteral("%1 MB").arg(double(bytes) / kMB, 0, 'f', 2);
    return QStringLiteral("%1 GB").arg(double(bytes) / kGB, 0, 'f', 2);
}

void ImagePreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_fitToWindow && !m_pixmap.isNull())
        fitToWindow();
}

void ImagePreviewWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier && !m_pixmap.isNull()) {
        const int delta = event->angleDelta().y();
        if (delta > 0)      zoomIn();
        else if (delta < 0) zoomOut();
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}
