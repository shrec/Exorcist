#ifdef EXORCIST_HAS_ULTRALIGHT

#include "ultralightwidget.h"
#include "ultralightengine.h"

#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFocusEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <Ultralight/platform/Platform.h>

#include <JavaScriptCore/JavaScript.h>

namespace exorcist {

// ── Helpers ──────────────────────────────────────────────────────────────────

static ultralight::String toULString(const QString &s)
{
    return ultralight::String16(
        reinterpret_cast<const ultralight::Char16 *>(s.utf16()), s.size());
}

static QString fromJSString(JSContextRef ctx, JSStringRef jsStr)
{
    const size_t maxLen = JSStringGetMaximumUTF8CStringSize(jsStr);
    QByteArray buf(static_cast<int>(maxLen), Qt::Uninitialized);
    JSStringGetUTF8CString(jsStr, buf.data(), maxLen);
    return QString::fromUtf8(buf.constData());
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

UltralightWidget::UltralightWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_InputMethodEnabled);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    initView();

    // Tick timer: drives Ultralight's update + render at 60fps
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(16); // ~60fps
    connect(m_tickTimer, &QTimer::timeout, this, &UltralightWidget::tick);
    m_tickTimer->start();
}

UltralightWidget::~UltralightWidget()
{
    if (m_tickTimer)
        m_tickTimer->stop();
    m_view = nullptr;
}

void UltralightWidget::initView()
{
    auto &engine = UltralightEngine::instance();
    if (!engine.isInitialized()) {
        // Auto-initialize with default resource path next to executable
        const QString resPath = QCoreApplication::applicationDirPath()
                                + QStringLiteral("/ultralight-resources/");
        engine.initialize(resPath);
    }

    auto renderer = engine.renderer();
    if (!renderer)
        return;

    ultralight::ViewConfig config;
    config.is_accelerated = false; // CPU rendering
    config.is_transparent = false;

    const int w = qMax(width(), 100);
    const int h = qMax(height(), 100);

    m_view = renderer->CreateView(w, h, config, nullptr);
    m_view->set_load_listener(this);
    m_view->set_view_listener(this);

    m_bitmap = QImage(w, h, QImage::Format_ARGB32_Premultiplied);
    m_bitmap.fill(Qt::black);
}

// ── HTML Loading ─────────────────────────────────────────────────────────────

void UltralightWidget::loadHTML(const QString &html, const QString &baseUrl)
{
    if (!m_view)
        return;
    m_domReady = false;
    const auto ulBase = baseUrl.isEmpty()
                            ? ultralight::String("file:///")
                            : toULString(baseUrl);
    m_view->LoadHTML(toULString(html), ulBase);
}

void UltralightWidget::loadURL(const QString &url)
{
    if (!m_view)
        return;
    m_domReady = false;
    m_view->LoadURL(toULString(url));
}

// ── JS Evaluation ────────────────────────────────────────────────────────────

void UltralightWidget::evaluateScript(const QString &js)
{
    if (!m_domReady) {
        m_pendingScripts.append(js);
        return;
    }
    if (!m_view)
        return;
    m_view->EvaluateScript(toULString(js));
}

void UltralightWidget::registerJSCallback(const QString &type,
                                          std::function<void(const QJsonValue &)> callback)
{
    m_jsCallbacks[type] = std::move(callback);
}

// ── Rendering ────────────────────────────────────────────────────────────────

void UltralightWidget::tick()
{
    auto renderer = UltralightEngine::instance().renderer();
    if (!renderer || !m_view)
        return;

    renderer->Update();
    renderer->Render();

    // Check if the view surface has changed
    auto *surface = static_cast<ultralight::BitmapSurface *>(m_view->surface());
    if (surface && !surface->dirty_bounds().IsEmpty()) {
        updateBitmap();
        surface->ClearDirtyBounds();
        update(); // schedule paintEvent
    }
}

void UltralightWidget::updateBitmap()
{
    auto *surface = static_cast<ultralight::BitmapSurface *>(m_view->surface());
    if (!surface)
        return;

    auto bitmap = surface->bitmap();
    void *pixels = bitmap->LockPixels();
    const uint32_t w = bitmap->width();
    const uint32_t h = bitmap->height();
    const uint32_t stride = bitmap->row_bytes();

    if (m_bitmap.width() != static_cast<int>(w) ||
        m_bitmap.height() != static_cast<int>(h))
        m_bitmap = QImage(static_cast<int>(w), static_cast<int>(h),
                          QImage::Format_ARGB32_Premultiplied);

    // Copy BGRA pixels row by row (Ultralight uses BGRA premultiplied alpha)
    for (uint32_t y = 0; y < h; ++y) {
        const auto *src = static_cast<const uint8_t *>(pixels) + y * stride;
        auto *dst = m_bitmap.scanLine(static_cast<int>(y));
        memcpy(dst, src, w * 4);
    }

    bitmap->UnlockPixels();
}

void UltralightWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    if (!m_bitmap.isNull())
        p.drawImage(0, 0, m_bitmap);
}

void UltralightWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_view && width() > 0 && height() > 0) {
        m_view->Resize(static_cast<uint32_t>(width()),
                       static_cast<uint32_t>(height()));
        m_bitmap = QImage(width(), height(), QImage::Format_ARGB32_Premultiplied);
        m_bitmap.fill(Qt::black);
    }
}

// ── Mouse Events ─────────────────────────────────────────────────────────────

ultralight::MouseEvent::Button UltralightWidget::qtButtonToUL(Qt::MouseButton btn)
{
    switch (btn) {
    case Qt::LeftButton:   return ultralight::MouseEvent::kButton_Left;
    case Qt::MiddleButton: return ultralight::MouseEvent::kButton_Middle;
    case Qt::RightButton:  return ultralight::MouseEvent::kButton_Right;
    default:               return ultralight::MouseEvent::kButton_None;
    }
}

void UltralightWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_view) return;
    ultralight::MouseEvent evt;
    evt.type = ultralight::MouseEvent::kType_MouseDown;
    evt.x = event->pos().x();
    evt.y = event->pos().y();
    evt.button = qtButtonToUL(event->button());
    m_view->FireMouseEvent(evt);
    event->accept();
}

void UltralightWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_view) return;
    ultralight::MouseEvent evt;
    evt.type = ultralight::MouseEvent::kType_MouseUp;
    evt.x = event->pos().x();
    evt.y = event->pos().y();
    evt.button = qtButtonToUL(event->button());
    m_view->FireMouseEvent(evt);
    event->accept();
}

void UltralightWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_view) return;
    ultralight::MouseEvent evt;
    evt.type = ultralight::MouseEvent::kType_MouseMoved;
    evt.x = event->pos().x();
    evt.y = event->pos().y();
    evt.button = ultralight::MouseEvent::kButton_None;
    m_view->FireMouseEvent(evt);
    event->accept();
}

void UltralightWidget::wheelEvent(QWheelEvent *event)
{
    if (!m_view) return;
    ultralight::ScrollEvent evt;
    evt.type = ultralight::ScrollEvent::kType_ScrollByPixel;
    evt.delta_x = event->angleDelta().x();
    evt.delta_y = event->angleDelta().y();
    m_view->FireScrollEvent(evt);
    event->accept();
}

// ── Keyboard Events ──────────────────────────────────────────────────────────

ultralight::KeyEvent UltralightWidget::qtKeyToUL(QKeyEvent *event,
                                                  ultralight::KeyEvent::Type type)
{
    ultralight::KeyEvent evt;
    evt.type = type;
    evt.virtual_key_code = event->nativeVirtualKey();
    evt.native_key_code = event->nativeScanCode();
    evt.modifiers = 0;

    if (event->modifiers() & Qt::AltModifier)
        evt.modifiers |= ultralight::KeyEvent::kMod_AltKey;
    if (event->modifiers() & Qt::ControlModifier)
        evt.modifiers |= ultralight::KeyEvent::kMod_CtrlKey;
    if (event->modifiers() & Qt::ShiftModifier)
        evt.modifiers |= ultralight::KeyEvent::kMod_ShiftKey;
    if (event->modifiers() & Qt::MetaModifier)
        evt.modifiers |= ultralight::KeyEvent::kMod_MetaKey;

    // For char events, set the text
    if (type == ultralight::KeyEvent::kType_Char && !event->text().isEmpty()) {
        evt.text = toULString(event->text());
    }

    ultralight::GetKeyIdentifierFromVirtualKeyCode(
        evt.virtual_key_code, evt.key_identifier);

    return evt;
}

void UltralightWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_view) return;

    // Fire RawKeyDown first
    m_view->FireKeyEvent(qtKeyToUL(event, ultralight::KeyEvent::kType_RawKeyDown));

    // Then fire Char event if it produces text
    if (!event->text().isEmpty())
        m_view->FireKeyEvent(qtKeyToUL(event, ultralight::KeyEvent::kType_Char));

    event->accept();
}

void UltralightWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_view) return;
    m_view->FireKeyEvent(qtKeyToUL(event, ultralight::KeyEvent::kType_KeyUp));
    event->accept();
}

void UltralightWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    if (m_view) m_view->Focus();
}

void UltralightWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    if (m_view) m_view->Unfocus();
}

// ── Ultralight Callbacks ─────────────────────────────────────────────────────

void UltralightWidget::OnDOMReady(ultralight::View * /*caller*/,
                                  uint64_t /*frame_id*/,
                                  bool is_main_frame,
                                  const ultralight::String & /*url*/)
{
    if (!is_main_frame)
        return;

    injectBridge();
    m_domReady = true;

    // Execute queued scripts
    for (const auto &js : m_pendingScripts)
        m_view->EvaluateScript(toULString(js));
    m_pendingScripts.clear();

    emit domReady();
}

// ── JS Bridge Injection ──────────────────────────────────────────────────────

// C function registered as window.__exorcist_bridge(type, payloadJson)
static JSValueRef bridgeCallback(JSContextRef ctx,
                                 JSObjectRef /*function*/,
                                 JSObjectRef /*thisObj*/,
                                 size_t argumentCount,
                                 const JSValueRef arguments[],
                                 JSValueRef * /*exception*/)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(ctx);

    // Extract type string
    JSStringRef jsType = JSValueToStringCopy(ctx, arguments[0], nullptr);
    const QString type = fromJSString(ctx, jsType);
    JSStringRelease(jsType);

    // Extract payload JSON string
    JSStringRef jsPayload = JSValueToStringCopy(ctx, arguments[1], nullptr);
    const QString payload = fromJSString(ctx, jsPayload);
    JSStringRelease(jsPayload);

    // Find the widget from the private data
    auto *widget = static_cast<UltralightWidget *>(
        JSObjectGetPrivate(JSContextGetGlobalObject(ctx)));

    if (widget) {
        const auto doc = QJsonDocument::fromJson(payload.toUtf8());
        const auto obj = doc.isObject() ? doc.object() : QJsonObject();

        // Invoke registered callback
        auto it = widget->m_jsCallbacks.constFind(type);
        if (it != widget->m_jsCallbacks.constEnd())
            (*it)(QJsonValue(obj));

        emit widget->jsMessage(type, obj);
    }

    return JSValueMakeUndefined(ctx);
}

void UltralightWidget::injectBridge()
{
    if (!m_view)
        return;

    auto jsCtx = m_view->LockJSContext();
    JSContextRef ctx = jsCtx->ctx();

    // Store widget pointer in global object's private data
    JSObjectSetPrivate(JSContextGetGlobalObject(ctx), this);

    // Create window.__exorcist_bridge function
    JSStringRef fnName = JSStringCreateWithUTF8CString("__exorcist_bridge");
    JSObjectRef fn = JSObjectMakeFunctionWithCallback(ctx, fnName, bridgeCallback);
    JSObjectSetProperty(ctx, JSContextGetGlobalObject(ctx), fnName, fn,
                        kJSPropertyAttributeReadOnly, nullptr);
    JSStringRelease(fnName);

    // Inject the friendly JS bridge object
    const char *bridgeJS = R"(
        window.exorcist = {
            sendToHost: function(type, payload) {
                window.__exorcist_bridge(type, JSON.stringify(payload || {}));
            }
        };
    )";
    JSStringRef script = JSStringCreateWithUTF8CString(bridgeJS);
    JSEvaluateScript(ctx, script, nullptr, nullptr, 0, nullptr);
    JSStringRelease(script);
}

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
