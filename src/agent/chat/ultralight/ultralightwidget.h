#pragma once
// ── UltralightWidget ─────────────────────────────────────────────────────────
//
// QWidget hosting an Ultralight HTML view with CPU rendering (C API).
// Surface pixels are copied to a QImage and drawn via QPainter.
// Mouse/keyboard events are forwarded from Qt to Ultralight.
// JS↔C++ bridge via evaluateScript() and registerJSCallback().
//
// Uses the Ultralight C API to avoid ABI/mangling issues with llvm-mingw.

#ifdef EXORCIST_HAS_ULTRALIGHT

#include <QWidget>
#include <QImage>
#include <QTimer>
#include <QJsonObject>
#include <QJsonValue>
#include <functional>
#include <QMap>

#include <cstdint>
#include <Ultralight/CAPI.h>
#include <JavaScriptCore/JavaScript.h>

namespace exorcist {

class UltralightWidget : public QWidget
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

    /// Insert text at the cursor position in the focused textarea/input.
    void insertTextAtCursor(const QString &text);

    /// Register a C++ callback callable from JS via window.exorcist.sendToHost(type, payload).
    void registerJSCallback(const QString &type,
                            std::function<void(const QJsonValue &)> callback);

    /// True after the initial HTML has loaded and DOMContentLoaded fired.
    bool isDomReady() const { return m_domReady; }

    /// Dispatch a JS→C++ bridge message (called by internal bridge callback).
    void dispatchBridgeMessage(const QString &type, const QString &payload);

    /// Called by the static DOMReady callback.
    void onDOMReady(unsigned long long frameId, bool isMainFrame);

    /// Set the JS global name to probe for readiness (default: "ChatApp").
    /// Set to empty string to skip the probe and mark ready immediately.
    void setReadyProbe(const QString &globalName);

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
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    void initView();
    void tick();
    void updateBitmap();

    // JS bridge setup (called from OnDOMReady callback)
    void injectBridge();
    void flushPendingOrRetry();

    ULView m_view = nullptr;
    QImage m_bitmap;
    QTimer *m_tickTimer = nullptr;
    bool m_domReady = false;
    int m_domReadyRetries = 0;
    bool m_loggedFirstPaint = false;
    bool m_loggedNoSurface = false;
    bool m_loggedNoBitmap = false;
    bool m_loggedNoPixels = false;
    bool m_loggedTick = false;
    bool m_loggedZeroSize = false;
    bool m_loggedUpdateBitmap = false;
    bool m_loggedResizeMismatch = false;

    QMap<QString, std::function<void(const QJsonValue &)>> m_jsCallbacks;

    // Queue of JS to evaluate once DOM is ready
    QStringList m_pendingScripts;

    // HTML to inject after about:blank DOM is ready (workaround for
    // ulViewLoadHTML / file:// crashes in Ultralight 1.4 beta)
    QString m_pendingHtml;

    // JS global name to probe for readiness (empty = skip probe)
    QString m_readyProbe = QStringLiteral("ChatApp");
};

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
