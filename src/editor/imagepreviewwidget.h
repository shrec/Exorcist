#pragma once

#include <QWidget>

class QLabel;
class QScrollArea;
class QToolBar;
class QAction;

/// Image preview widget shown in place of the text editor for image files.
///
/// Supports raster (PNG, JPG, JPEG, BMP, GIF) and vector (SVG) formats.
/// Provides zoom in/out, reset (1:1), fit-to-window, plus a status bar with
/// image dimensions and file size.  The widget is intentionally a plain
/// QWidget (not an EditorView) — EditorManager::currentEditor() guards
/// against non-editor tabs via qobject_cast, so this composes safely.
class ImagePreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImagePreviewWidget(QWidget *parent = nullptr);
    ~ImagePreviewWidget() override;

    /// Load an image from disk.  Returns false on failure (unsupported,
    /// missing, corrupt).  On success the image is shown at 1:1 zoom and
    /// the status bar is populated with dimensions and file size.
    bool loadImage(const QString &filePath);

    /// Path of the currently loaded image (empty if none).
    QString filePath() const { return m_filePath; }

public slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToWindow();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    void setZoom(double factor);
    void applyZoom();
    void updateStatus();
    static QString formatFileSize(qint64 bytes);

    QToolBar    *m_toolbar    = nullptr;
    QScrollArea *m_scrollArea = nullptr;
    QLabel      *m_imageLabel = nullptr;
    QLabel      *m_statusLabel = nullptr;

    QAction *m_zoomInAction    = nullptr;
    QAction *m_zoomOutAction   = nullptr;
    QAction *m_resetZoomAction = nullptr;
    QAction *m_fitAction       = nullptr;

    QPixmap  m_pixmap;
    QString  m_filePath;
    double   m_zoom        = 1.0;
    bool     m_fitToWindow = false;
    qint64   m_fileSize    = 0;
};
