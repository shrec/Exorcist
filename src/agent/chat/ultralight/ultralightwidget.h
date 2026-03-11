#pragma once
// ── UltralightWidget ─────────────────────────────────────────────────────────
//
// QWidget hosting an Ultralight HTML view with CPU rendering.
// BitmapSurface pixels are copied to a QImage and drawn via QPainter.
// Mouse/keyboard events are forwarded from Qt to Ultralight.
// JS↔C++ bridge via evaluateScript() and registerJSCallback().

#ifdef EXORCIST_HAS_ULTRALIGHT

#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>
#include <QMap>

#include <Ultralight/Ultralight.h>
#include <Ultralight/Listener.h>

namespace exorcist {

class UltralightWidget : public QWidget, public ultralight::LoadListener,
                         public ultralight::ViewListener
{
    Q_OBJECT
public:
    explicit UltralightWidget(QWidget *parent = nullptr);
    ~UltralightWidget() override;

    /// Load raw HTML content into the view.
    void loadHTML(const QString &html, const QString &baseUrl = {});

    /// Load a URL (file:// or qrc:// translated to file path).
    void loadURL(const QString &url);

    /// Evaluate JavaScript in the view's context.
    void evaluateScript(const QString &js);

    /// Register a C++ callback callable from JS via window.exorcist.sendToHost(type, payload).
    /// @param type   Message type string.
    /// @param callback  Receives the JSON payload.
    void registerJSCallback(const QString &type,
                            std::function<void(const QJsonValue &)> callback);

    /// True after the initial HTML has loaded and DOMContentLoaded fired.
    bool isDomReady() const { return m_domReady; }

signals:
    /// Emitted when the JS side sends a message to C++.
    void jsMessage(const QString &type, const QJsonObject &payload);

    /// Emitted once the DOM is ready after loadHTML/loadURL.
    void domReady();

protected:
    // Qt event overrides
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

    // Ultralight::LoadListener
    void OnDOMReady(ultralight::View *caller,
                    uint64_t frame_id,
                    bool is_main_frame,
                    const ultralight::String &url) override;

private:
    void initView();
    void tick();
    void updateBitmap();

    // Input helpers
    static ultralight::MouseEvent::Button qtButtonToUL(Qt::MouseButton btn);
    static ultralight::KeyEvent qtKeyToUL(QKeyEvent *event, ultralight::KeyEvent::Type type);

    // JS bridge setup (called from OnDOMReady)
    void injectBridge();

    ultralight::RefPtr<ultralight::View> m_view;
    QImage m_bitmap;
    QTimer *m_tickTimer = nullptr;
    bool m_domReady = false;
    bool m_needsPaint = true;

    QMap<QString, std::function<void(const QJsonValue &)>> m_jsCallbacks;

    // Queue of JS to evaluate once DOM is ready
    QStringList m_pendingScripts;
};

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
