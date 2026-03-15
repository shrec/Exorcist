#include "ultralightpluginview.h"
#include "jshostapi.h"

#include <AppCore/CAPI.h>

#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFocusEvent>
#include <QShowEvent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QClipboard>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>

namespace jssdk {

// ── Helpers ──────────────────────────────────────────────────────────────────

static ULString toULString(const QString &s)
{
    const QByteArray utf8 = s.toUtf8();
    return ulCreateStringUTF8(utf8.constData(), utf8.size());
}

static QString fromULString(ULString s)
{
    if (!s)
        return {};
    return QString::fromUtf8(ulStringGetData(s),
                             static_cast<int>(ulStringGetLength(s)));
}

// ── Static Ultralight callbacks ──────────────────────────────────────────────

static void domReadyCallback(void *userData, ULView /*caller*/,
                              unsigned long long /*frameId*/,
                              bool isMainFrame, ULString /*url*/)
{
    auto *view = static_cast<UltralightPluginView *>(userData);
    view->onDOMReady(isMainFrame);
}

static void consoleCallback(void * /*userData*/, ULView /*caller*/,
                             ULMessageSource /*source*/, ULMessageLevel /*level*/,
                             ULString message, unsigned int lineNumber,
                             unsigned int /*columnNumber*/, ULString sourceId)
{
    const QString msg = fromULString(message);
    const QString src = fromULString(sourceId);
    qWarning("[JsSDK/WebView] console: %s (%s:%u)",
             qPrintable(msg), qPrintable(src), lineNumber);
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

UltralightPluginView::UltralightPluginView(const QString &htmlPath,
                                            PluginContext *pctx,
                                            QWidget *parent)
    : QWidget(parent)
    , m_htmlPath(htmlPath)
    , m_pctx(pctx)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    initRenderer();
    initView();

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(16); // ~60fps
    connect(m_tickTimer, &QTimer::timeout, this, &UltralightPluginView::tick);

    loadHtml();
}

UltralightPluginView::~UltralightPluginView()
{
    if (m_tickTimer)
        m_tickTimer->stop();
    if (m_view)
        ulDestroyView(m_view);
    m_view = nullptr;
}

// ── Renderer management ─────────────────────────────────────────────────────
// Shares the process-wide Ultralight renderer via qApp property.
// If core (UltralightEngine) already created one, use it.
// Otherwise, initialize Ultralight ourselves.

void UltralightPluginView::initRenderer()
{
    // Check if core already created a renderer
    QVariant v = qApp->property("__exo_ul_renderer");
    if (v.isValid()) {
        m_renderer = reinterpret_cast<ULRenderer>(v.value<quintptr>());
        if (m_renderer)
            return;
    }

    // Initialize Ultralight ourselves
    const QString resPath =
        QCoreApplication::applicationDirPath() + QStringLiteral("/ultralight-resources/");

    ULConfig config = ulCreateConfig();

    const QByteArray resPathUtf8 = resPath.toUtf8();
    ULString ulResPath = ulCreateStringUTF8(resPathUtf8.constData(),
                                            resPathUtf8.size());
    ulConfigSetResourcePathPrefix(config, ulResPath);

    const QString cachePath = QDir::tempPath() + QStringLiteral("/exorcist-ul-cache");
    QDir().mkpath(cachePath);
    const QByteArray cachePathUtf8 = cachePath.toUtf8();
    ULString ulCachePath = ulCreateStringUTF8(cachePathUtf8.constData(),
                                              cachePathUtf8.size());
    ulConfigSetCachePath(config, ulCachePath);

    ulConfigSetForceRepaint(config, false);
    ulConfigSetAnimationTimerDelay(config, 1.0 / 60.0);

    ulEnablePlatformFontLoader();
    ulEnablePlatformFileSystem(ulResPath);

    m_renderer = ulCreateRenderer(config);

    // Share with other consumers
    qApp->setProperty("__exo_ul_renderer",
                       QVariant::fromValue(reinterpret_cast<quintptr>(m_renderer)));

    ulDestroyString(ulResPath);
    ulDestroyString(ulCachePath);
    ulDestroyConfig(config);
}

// ── View creation ────────────────────────────────────────────────────────────

void UltralightPluginView::initView()
{
    if (!m_renderer)
        return;

    ULViewConfig config = ulCreateViewConfig();
    ulViewConfigSetIsAccelerated(config, false);
    ulViewConfigSetIsTransparent(config, false);

    const unsigned int w = static_cast<unsigned int>(qMax(width(), 100));
    const unsigned int h = static_cast<unsigned int>(qMax(height(), 100));

    m_view = ulCreateView(m_renderer, w, h, config, nullptr);
    ulDestroyViewConfig(config);

    ulViewSetDOMReadyCallback(m_view, domReadyCallback, this);
    ulViewSetAddConsoleMessageCallback(m_view, consoleCallback, this);

    m_bitmap = QImage(static_cast<int>(w), static_cast<int>(h),
                      QImage::Format_ARGB32_Premultiplied);
    m_bitmap.fill(Qt::white);
}

// ── HTML loading ─────────────────────────────────────────────────────────────

QString UltralightPluginView::inlineResources(const QString &htmlPath)
{
    QFile f(htmlPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};

    QString html = QString::fromUtf8(f.readAll());
    const QDir baseDir = QFileInfo(htmlPath).dir();

    // Inline <script src="X"></script> → <script>contents</script>
    static const QRegularExpression scriptRe(
        QStringLiteral(R"(<script\s+[^>]*src\s*=\s*["']([^"']+)["'][^>]*>\s*</script>)"),
        QRegularExpression::CaseInsensitiveOption);

    auto sit = scriptRe.globalMatch(html);
    // Process matches in reverse order to preserve positions
    QList<QRegularExpressionMatch> scriptMatches;
    while (sit.hasNext())
        scriptMatches.append(sit.next());

    for (int i = scriptMatches.size() - 1; i >= 0; --i) {
        const auto &m = scriptMatches[i];
        const QString srcPath = baseDir.absoluteFilePath(m.captured(1));
        QFile sf(srcPath);
        if (sf.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(sf.readAll());
            const QString replacement =
                QStringLiteral("<script>") + content + QStringLiteral("</script>");
            html.replace(m.capturedStart(), m.capturedLength(), replacement);
        }
    }

    // Inline <link rel="stylesheet" href="X"> → <style>contents</style>
    static const QRegularExpression linkRe(
        QStringLiteral(R"(<link\s+[^>]*href\s*=\s*["']([^"']+)["'][^>]*/?>)"),
        QRegularExpression::CaseInsensitiveOption);

    auto lit = linkRe.globalMatch(html);
    QList<QRegularExpressionMatch> linkMatches;
    while (lit.hasNext()) {
        auto m = lit.next();
        // Only process stylesheet links
        if (m.captured(0).contains(QStringLiteral("stylesheet"),
                                    Qt::CaseInsensitive))
            linkMatches.append(m);
    }

    for (int i = linkMatches.size() - 1; i >= 0; --i) {
        const auto &m = linkMatches[i];
        const QString hrefPath = baseDir.absoluteFilePath(m.captured(1));
        QFile cf(hrefPath);
        if (cf.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(cf.readAll());
            const QString replacement =
                QStringLiteral("<style>") + content + QStringLiteral("</style>");
            html.replace(m.capturedStart(), m.capturedLength(), replacement);
        }
    }

    return html;
}

void UltralightPluginView::loadHtml()
{
    if (!m_view)
        return;

    m_domReady = false;
    m_pendingHtml = inlineResources(m_htmlPath);

    if (m_pendingHtml.isEmpty()) {
        qWarning("[JsSDK/WebView] Failed to read HTML from %s",
                 qPrintable(m_htmlPath));
        return;
    }

    // Use the about:blank + base64 injection workaround
    // (ulViewLoadHTML and file:// crash in Ultralight 1.4 beta)
    ULString ulUrl = toULString(QStringLiteral("about:blank"));
    ulViewLoadURL(m_view, ulUrl);
    ulDestroyString(ulUrl);

    if (m_tickTimer && !m_tickTimer->isActive())
        m_tickTimer->start();
}

// ── DOM Ready + SDK injection ────────────────────────────────────────────────

void UltralightPluginView::onDOMReady(bool isMainFrame)
{
    if (!isMainFrame)
        return;

    // First DOMReady: inject the pending HTML via base64
    if (!m_pendingHtml.isEmpty()) {
        const QString html = m_pendingHtml;
        m_pendingHtml.clear();

        const QByteArray b64 = html.toUtf8().toBase64();
        const QString inject = QStringLiteral(
            "(function(){"
            "  var html = atob('%1');"
            "  var styles = html.match(/<style[^>]*>[\\s\\S]*?<\\/style>/gi) || [];"
            "  for(var i=0;i<styles.length;i++){"
            "    var c = styles[i].replace(/<\\/?style[^>]*>/gi,'');"
            "    var el = document.createElement('style');"
            "    el.textContent = c;"
            "    document.head.appendChild(el);"
            "  }"
            "  var bodyMatch = html.match(/<body[^>]*>([\\s\\S]*)<\\/body>/i);"
            "  var bodyHtml = bodyMatch ? bodyMatch[1] : html;"
            "  var scripts = [];"
            "  bodyHtml = bodyHtml.replace(/<script[^>]*>([\\s\\S]*?)<\\/script>/gi,"
            "    function(m, code){ scripts.push(code); return ''; });"
            "  document.body.innerHTML = bodyHtml;"
            "  for(var j=0;j<scripts.length;j++){"
            "    var s = document.createElement('script');"
            "    s.textContent = scripts[j];"
            "    document.body.appendChild(s);"
            "  }"
            "})();").arg(QString::fromLatin1(b64));

        ULString ulJs = toULString(inject);
        ULString exception = nullptr;
        ulViewEvaluateScript(m_view, ulJs, &exception);
        if (exception && ulStringGetLength(exception) > 0) {
            qWarning("[JsSDK/WebView] HTML inject error: %s",
                     qPrintable(fromULString(exception)));
        }
        ulDestroyString(ulJs);
    }

    // Inject the ex.* SDK into the WebView's JS context
    injectSdkAndInitialize();
}

void UltralightPluginView::injectSdkAndInitialize()
{
    if (!m_view || !m_pctx)
        return;

    JSContextRef ctx = ulViewLockJSContext(m_view);

    // Register the ex.* SDK into this WebView context.
    // JsHostAPI::registerAll works with any JSContextRef — it creates
    // the `ex` global object and all permission-gated sub-namespaces.
    JsHostAPI::registerAll(ctx, m_pctx);

    ulViewUnlockJSContext(m_view);

    m_domReady = true;

    // Call initialize() if defined by the plugin's JS code
    const QString initScript = QStringLiteral(
        "if (typeof initialize === 'function') initialize();");
    ULString ulJs = toULString(initScript);
    ULString exception = nullptr;
    ulViewEvaluateScript(m_view, ulJs, &exception);
    if (exception && ulStringGetLength(exception) > 0) {
        qWarning("[JsSDK/WebView] initialize() error: %s",
                 qPrintable(fromULString(exception)));
    }
    ulDestroyString(ulJs);

    emit ready();
}

// ── JS evaluation ────────────────────────────────────────────────────────────

void UltralightPluginView::evaluateScript(const QString &js)
{
    if (!m_view)
        return;
    ULString ulJs = toULString(js);
    ULString exception = nullptr;
    ulViewEvaluateScript(m_view, ulJs, &exception);
    if (exception && ulStringGetLength(exception) > 0) {
        qWarning("[JsSDK/WebView] JS error: %s", qPrintable(fromULString(exception)));
    }
    ulDestroyString(ulJs);
}

// ── Rendering ────────────────────────────────────────────────────────────────

void UltralightPluginView::tick()
{
    if (!m_renderer || !m_view)
        return;

    // Sync view size
    if (width() > 0 && height() > 0) {
        const unsigned int vw = ulViewGetWidth(m_view);
        const unsigned int vh = ulViewGetHeight(m_view);
        if (vw != static_cast<unsigned int>(width()) ||
            vh != static_cast<unsigned int>(height())) {
            ulViewResize(m_view,
                         static_cast<unsigned int>(width()),
                         static_cast<unsigned int>(height()));
            ulViewSetNeedsPaint(m_view, true);
        }
    }

    ulUpdate(m_renderer);
    ulRender(m_renderer);

    ULSurface surface = ulViewGetSurface(m_view);
    if (surface) {
        ULIntRect dirty = ulSurfaceGetDirtyBounds(surface);
        if (dirty.right > dirty.left && dirty.bottom > dirty.top) {
            updateBitmap();
            ulSurfaceClearDirtyBounds(surface);
            update();
        }
    }
}

void UltralightPluginView::updateBitmap()
{
    ULSurface surface = ulViewGetSurface(m_view);
    if (!surface)
        return;

    ULBitmap bitmap = ulBitmapSurfaceGetBitmap(surface);
    if (!bitmap)
        return;

    void *pixels = ulBitmapLockPixels(bitmap);
    if (!pixels) {
        ulBitmapUnlockPixels(bitmap);
        return;
    }

    const unsigned int w = ulBitmapGetWidth(bitmap);
    const unsigned int h = ulBitmapGetHeight(bitmap);
    const unsigned int srcStride = ulBitmapGetRowBytes(bitmap);

    if (w == 0 || h == 0) {
        ulBitmapUnlockPixels(bitmap);
        return;
    }

    if (m_bitmap.width() != static_cast<int>(w) ||
        m_bitmap.height() != static_cast<int>(h))
        m_bitmap = QImage(static_cast<int>(w), static_cast<int>(h),
                          QImage::Format_ARGB32_Premultiplied);

    const unsigned int dstStride = static_cast<unsigned int>(m_bitmap.bytesPerLine());
    const unsigned int copyBytes = qMin(srcStride, dstStride);

    for (unsigned int y = 0; y < h; ++y) {
        const auto *src = static_cast<const uint8_t *>(pixels) + y * srcStride;
        auto *dst = m_bitmap.scanLine(static_cast<int>(y));
        memcpy(dst, src, copyBytes);
    }

    ulBitmapUnlockPixels(bitmap);
}

void UltralightPluginView::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    if (!m_bitmap.isNull())
        p.drawImage(0, 0, m_bitmap);
}

void UltralightPluginView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_view && width() > 0 && height() > 0) {
        ulViewResize(m_view,
                     static_cast<unsigned int>(width()),
                     static_cast<unsigned int>(height()));
        ulViewSetNeedsPaint(m_view, true);
    }
}

void UltralightPluginView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_view && width() > 0 && height() > 0) {
        ulViewResize(m_view,
                     static_cast<unsigned int>(width()),
                     static_cast<unsigned int>(height()));
        ulViewSetNeedsPaint(m_view, true);
    }
}

// ── Mouse Events ─────────────────────────────────────────────────────────────

static ULMouseButton qtButtonToUL(Qt::MouseButton btn)
{
    switch (btn) {
    case Qt::LeftButton:   return kMouseButton_Left;
    case Qt::MiddleButton: return kMouseButton_Middle;
    case Qt::RightButton:  return kMouseButton_Right;
    default:               return kMouseButton_None;
    }
}

void UltralightPluginView::mousePressEvent(QMouseEvent *event)
{
    if (!m_view) return;
    setFocus(Qt::MouseFocusReason);
    ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseDown,
                                          event->pos().x(), event->pos().y(),
                                          qtButtonToUL(event->button()));
    ulViewFireMouseEvent(m_view, evt);
    ulDestroyMouseEvent(evt);
    event->accept();
}

void UltralightPluginView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_view) return;
    ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseUp,
                                          event->pos().x(), event->pos().y(),
                                          qtButtonToUL(event->button()));
    ulViewFireMouseEvent(m_view, evt);
    ulDestroyMouseEvent(evt);
    event->accept();
}

void UltralightPluginView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_view) return;
    ULMouseButton btn = kMouseButton_None;
    if (event->buttons() & Qt::LeftButton)        btn = kMouseButton_Left;
    else if (event->buttons() & Qt::RightButton)  btn = kMouseButton_Right;
    else if (event->buttons() & Qt::MiddleButton) btn = kMouseButton_Middle;
    ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseMoved,
                                          event->pos().x(), event->pos().y(), btn);
    ulViewFireMouseEvent(m_view, evt);
    ulDestroyMouseEvent(evt);
    event->accept();
}

void UltralightPluginView::wheelEvent(QWheelEvent *event)
{
    if (!m_view) return;
    ULScrollEvent evt = ulCreateScrollEvent(kScrollEventType_ScrollByPixel,
                                            event->angleDelta().x(),
                                            event->angleDelta().y());
    ulViewFireScrollEvent(m_view, evt);
    ulDestroyScrollEvent(evt);
    event->accept();
}

// ── Keyboard Events ──────────────────────────────────────────────────────────

static unsigned int qtModsToUL(Qt::KeyboardModifiers mods)
{
    unsigned int m = 0;
    if (mods & Qt::AltModifier)     m |= (1 << 0);
    if (mods & Qt::ControlModifier) m |= (1 << 1);
    if (mods & Qt::MetaModifier)    m |= (1 << 2);
    if (mods & Qt::ShiftModifier)   m |= (1 << 3);
    return m;
}

static void fireKeyEvent(ULView view, QKeyEvent *event, ULKeyEventType type)
{
    ULString text = ulCreateStringUTF8("", 0);
    ULString unmod = ulCreateStringUTF8("", 0);
    if (type == kKeyEventType_Char && !event->text().isEmpty()) {
        ulDestroyString(text);
        ulDestroyString(unmod);
        const QByteArray utf8 = event->text().toUtf8();
        text = ulCreateStringUTF8(utf8.constData(), utf8.size());
        unmod = ulCreateStringUTF8(utf8.constData(), utf8.size());
    }

    ULKeyEvent evt = ulCreateKeyEvent(
        type,
        qtModsToUL(event->modifiers()),
        event->nativeVirtualKey(),
        event->nativeScanCode(),
        text, unmod,
        false,              // is_keypad
        event->isAutoRepeat(),
        false               // is_system_key
    );

    ulViewFireKeyEvent(view, evt);
    ulDestroyKeyEvent(evt);
    ulDestroyString(text);
    ulDestroyString(unmod);
}

void UltralightPluginView::keyPressEvent(QKeyEvent *event)
{
    if (!m_view) return;

    // Clipboard interception — Ultralight has no system clipboard access
    if (event->matches(QKeySequence::Paste)) {
        const QString clipText = QGuiApplication::clipboard()->text();
        if (!clipText.isEmpty()) {
            // Insert via JS
            const QByteArray json = QJsonDocument(
                QJsonArray{clipText}).toJson(QJsonDocument::Compact);
            const QString escaped = QString::fromUtf8(json.mid(1, json.size() - 2));
            evaluateScript(QStringLiteral(
                "(function(){"
                "  var el = document.activeElement;"
                "  if (el && (el.tagName==='TEXTAREA'||el.tagName==='INPUT')){"
                "    var s=el.selectionStart||0,e=el.selectionEnd||0;"
                "    el.value=el.value.substring(0,s)+%1+el.value.substring(e);"
                "    el.selectionStart=el.selectionEnd=s+%1.length;"
                "    el.dispatchEvent(new Event('input',{bubbles:true}));"
                "  }"
                "})();").arg(escaped));
        }
        event->accept();
        return;
    }

    fireKeyEvent(m_view, event, kKeyEventType_RawKeyDown);
    if (!event->text().isEmpty())
        fireKeyEvent(m_view, event, kKeyEventType_Char);
    event->accept();
}

void UltralightPluginView::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_view) return;
    fireKeyEvent(m_view, event, kKeyEventType_KeyUp);
    event->accept();
}

void UltralightPluginView::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    if (m_view) ulViewFocus(m_view);
}

void UltralightPluginView::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    if (m_view) ulViewUnfocus(m_view);
}

} // namespace jssdk
