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
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QShowEvent>
#include <QMenu>
#include <QClipboard>
#include <QGuiApplication>
#include <QTimer>
#include <QUrl>

#include <JavaScriptCore/JavaScript.h>

namespace exorcist {

// ── JSClass for bridge function objects ──────────────────────────────────────
// Each UltralightWidget creates a callable JSObject with itself as private data.
// No static map needed — the widget pointer lives inside the function object.
static JSClassRef s_bridgeClass = nullptr;

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
    return QString::fromUtf8(
        ulStringGetData(s),
        static_cast<int>(ulStringGetLength(s)));
}

static QString fromJSString(JSContextRef ctx, JSStringRef jsStr)
{
    const size_t maxLen = JSStringGetMaximumUTF8CStringSize(jsStr);
    QByteArray buf(static_cast<int>(maxLen), Qt::Uninitialized);
    JSStringGetUTF8CString(jsStr, buf.data(), maxLen);
    return QString::fromUtf8(buf.constData());
}

// ── Static Callbacks ─────────────────────────────────────────────────────────

static void ulDOMReadyCallback(void *userData, ULView /*caller*/,
                               unsigned long long frameId,
                               bool isMainFrame, ULString /*url*/)
{
    auto *widget = static_cast<UltralightWidget *>(userData);
    widget->onDOMReady(frameId, isMainFrame);
}

static void ulBeginLoadingCallback(void *userData, ULView /*caller*/,
                                   unsigned long long /*frameId*/,
                                   bool isMainFrame, ULString url)
{
    if (!isMainFrame)
        return;
    const QString qUrl = fromULString(url);
    qWarning().noquote() << "UltralightWidget: begin loading" << qUrl;
    Q_UNUSED(userData)
}

static void ulFinishLoadingCallback(void *userData, ULView /*caller*/,
                                    unsigned long long /*frameId*/,
                                    bool isMainFrame, ULString url)
{
    if (!isMainFrame)
        return;
    const QString qUrl = fromULString(url);
    qWarning().noquote() << "UltralightWidget: finish loading" << qUrl;
    Q_UNUSED(userData)
}

static void ulFailLoadingCallback(void *userData, ULView /*caller*/,
                                  unsigned long long /*frameId*/,
                                  bool isMainFrame, ULString url, ULString description,
                                  ULString errorDomain, int errorCode)
{
    if (!isMainFrame)
        return;
    const QString qUrl = fromULString(url);
    const QString desc = fromULString(description);
    const QString domain = fromULString(errorDomain);
    qWarning().noquote() << "UltralightWidget: load failed url=" << qUrl
                         << "error=" << desc << "domain=" << domain
                         << "code=" << errorCode;
    Q_UNUSED(userData)
}

static void ulConsoleCallback(void *userData, ULView /*caller*/, ULMessageSource /*source*/,
                              ULMessageLevel /*level*/, ULString message,
                              unsigned int lineNumber, unsigned int columnNumber,
                              ULString sourceId)
{
    const QString msg = fromULString(message);
    const QString src = fromULString(sourceId);
    qWarning().noquote() << "UltralightWidget console:" << msg
                         << "(" << src << ":" << lineNumber << ":" << columnNumber << ")";
    Q_UNUSED(userData)
}

// ── Constructor / Destructor ─────────────────────────────────────────────────

UltralightWidget::UltralightWidget(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    initView();

    // Tick timer: drives Ultralight's update + render at ~60fps.
    // Starts paused — activated by loadHTML/loadURL.
    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(16);
    connect(m_tickTimer, &QTimer::timeout, this, &UltralightWidget::tick);
}

UltralightWidget::~UltralightWidget()
{
    if (m_tickTimer)
        m_tickTimer->stop();
    if (m_view) {
        ulDestroyView(m_view);
    }
    m_view = nullptr;
}

void UltralightWidget::initView()
{
    auto &engine = UltralightEngine::instance();
    if (!engine.isInitialized()) {
        const QString resPath = QCoreApplication::applicationDirPath()
                                + QStringLiteral("/ultralight-resources/");
        engine.initialize(resPath);
    }

    ULRenderer renderer = engine.renderer();
    if (!renderer)
        return;

    ULViewConfig config = ulCreateViewConfig();
    ulViewConfigSetIsAccelerated(config, false); // CPU rendering
    ulViewConfigSetIsTransparent(config, false);

    const unsigned int w = static_cast<unsigned int>(qMax(width(), 100));
    const unsigned int h = static_cast<unsigned int>(qMax(height(), 100));

    m_view = ulCreateView(renderer, w, h, config, nullptr);
    ulDestroyViewConfig(config);

    // Register DOMReady callback
    ulViewSetDOMReadyCallback(m_view, ulDOMReadyCallback, this);
    ulViewSetBeginLoadingCallback(m_view, ulBeginLoadingCallback, this);
    ulViewSetFinishLoadingCallback(m_view, ulFinishLoadingCallback, this);
    ulViewSetFailLoadingCallback(m_view, ulFailLoadingCallback, this);
    ulViewSetAddConsoleMessageCallback(m_view, ulConsoleCallback, this);

    m_bitmap = QImage(static_cast<int>(w), static_cast<int>(h),
                      QImage::Format_ARGB32_Premultiplied);
    m_bitmap.fill(Qt::black);
}

// ── HTML Loading ─────────────────────────────────────────────────────────────

void UltralightWidget::loadHTML(const QString &html, const QString & /*baseUrl*/)
{
    if (!m_view)
        return;
    m_domReady = false;

    // Both ulViewLoadHTML and file:// URLs crash in Ultralight 1.4 beta.
    // Strategy: load about:blank (safe), then inject the full HTML via JS
    // once DOM is ready.  Scripts injected via innerHTML don't execute,
    // so we parse <style>/<script> tags and create real DOM elements.
    m_pendingHtml = html;
    loadURL(QStringLiteral("about:blank"));
}

void UltralightWidget::loadURL(const QString &url)
{
    if (!m_view)
        return;
    m_domReady = false;
    ULString ulUrl = toULString(url);
    ulViewLoadURL(m_view, ulUrl);
    ulDestroyString(ulUrl);
    if (m_tickTimer && !m_tickTimer->isActive())
        m_tickTimer->start();
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
    ULString ulJs = toULString(js);
    ULString exception = nullptr;
    ulViewEvaluateScript(m_view, ulJs, &exception);
    if (exception && ulStringGetLength(exception) > 0) {
        const QString errMsg = fromULString(exception);
        const QString snippet = js.left(120);
        qWarning("UltralightWidget JS error: %s\n  script: %s",
                 qPrintable(errMsg), qPrintable(snippet));
    }
    ulDestroyString(ulJs);
}

void UltralightWidget::insertTextAtCursor(const QString &text)
{
    // Escape for safe JS string insertion (JSON-encode then strip quotes).
    const QByteArray json = QJsonDocument(
        QJsonArray{text}).toJson(QJsonDocument::Compact);
    // json looks like '["the text"]', extract inner string including quotes.
    const QString escaped = QString::fromUtf8(json.mid(1, json.size() - 2));

    evaluateScript(QStringLiteral(
        "(function() {"
        "  var el = document.activeElement;"
        "  if (!el || (el.tagName !== 'TEXTAREA' && el.tagName !== 'INPUT')) {"
        "    el = document.getElementById('chatInput');"
        "  }"
        "  if (el && (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT')) {"
        "    el.focus();"
        "    var start = el.selectionStart || 0, end = el.selectionEnd || 0;"
        "    var before = el.value.substring(0, start);"
        "    var after  = el.value.substring(end);"
        "    el.value = before + %1 + after;"
        "    el.selectionStart = el.selectionEnd = start + %1.length;"
        "    el.dispatchEvent(new Event('input', { bubbles: true }));"
        "  }"
        "})();").arg(escaped));
}

void UltralightWidget::registerJSCallback(const QString &type,
                                          std::function<void(const QJsonValue &)> callback)
{
    m_jsCallbacks[type] = std::move(callback);
}

// ── Rendering ────────────────────────────────────────────────────────────────

void UltralightWidget::tick()
{
    ULRenderer renderer = UltralightEngine::instance().renderer();
    if (!renderer || !m_view)
        return;

    if (!m_loggedTick) {
        m_loggedTick = true;
        qWarning("UltralightWidget: tick running (firstPaint=%d)", m_loggedFirstPaint ? 1 : 0);
    }

    if (m_view && width() > 0 && height() > 0) {
        const unsigned int vw = ulViewGetWidth(m_view);
        const unsigned int vh = ulViewGetHeight(m_view);
        if (vw != static_cast<unsigned int>(width()) ||
            vh != static_cast<unsigned int>(height())) {
            if (!m_loggedResizeMismatch) {
                m_loggedResizeMismatch = true;
                qWarning("UltralightWidget: view resize %ux%u -> %dx%d",
                         vw, vh, width(), height());
            }
            ulViewResize(m_view,
                         static_cast<unsigned int>(width()),
                         static_cast<unsigned int>(height()));
            ulViewSetNeedsPaint(m_view, true);
        }
    }

    if (!m_loggedFirstPaint)
        ulViewSetNeedsPaint(m_view, true);

    ulUpdate(renderer);
    ulRender(renderer);

    if (!m_loggedFirstPaint) {
        qWarning("UltralightWidget: tick pre-updateBitmap");
        updateBitmap();
        update();
        return;
    }

    // Check if the view surface has changed
    ULSurface surface = ulViewGetSurface(m_view);
    if (!surface && !m_loggedNoSurface) {
        m_loggedNoSurface = true;
        qWarning("UltralightWidget: no surface available");
    }
    if (surface) {
        ULIntRect dirty = ulSurfaceGetDirtyBounds(surface);
        if (dirty.right > dirty.left && dirty.bottom > dirty.top) {
            updateBitmap();
            ulSurfaceClearDirtyBounds(surface);
            update(); // schedule paintEvent
        }
    }
}

void UltralightWidget::updateBitmap()
{
    if (!m_loggedUpdateBitmap) {
        m_loggedUpdateBitmap = true;
        qWarning("UltralightWidget: updateBitmap called");
    }
    ULSurface surface = ulViewGetSurface(m_view);
    if (!surface) {
        if (!m_loggedNoSurface) {
            m_loggedNoSurface = true;
            qWarning("UltralightWidget: updateBitmap no surface");
        }
        return;
    }

    ULBitmap bitmap = ulBitmapSurfaceGetBitmap(surface);
    if (!bitmap) {
        if (!m_loggedNoBitmap) {
            m_loggedNoBitmap = true;
            qWarning("UltralightWidget: updateBitmap no bitmap");
        }
        return;
    }

    void *pixels = ulBitmapLockPixels(bitmap);
    if (!pixels) {
        if (!m_loggedNoPixels) {
            m_loggedNoPixels = true;
            qWarning("UltralightWidget: updateBitmap no pixels");
        }
        ulBitmapUnlockPixels(bitmap);
        return;
    }

    const unsigned int w = ulBitmapGetWidth(bitmap);
    const unsigned int h = ulBitmapGetHeight(bitmap);
    const unsigned int srcStride = ulBitmapGetRowBytes(bitmap);

    if (w == 0 || h == 0) {
        if (!m_loggedZeroSize) {
            m_loggedZeroSize = true;
            qWarning("UltralightWidget: updateBitmap zero size w=%u h=%u", w, h);
        }
        ulBitmapUnlockPixels(bitmap);
        return;
    }

    if (m_bitmap.width() != static_cast<int>(w) ||
        m_bitmap.height() != static_cast<int>(h))
        m_bitmap = QImage(static_cast<int>(w), static_cast<int>(h),
                          QImage::Format_ARGB32_Premultiplied);

    const unsigned int dstStride = static_cast<unsigned int>(m_bitmap.bytesPerLine());
    const unsigned int copyBytes = qMin(srcStride, dstStride);

    // Copy BGRA pixels row by row (Ultralight uses BGRA premultiplied alpha,
    // which matches QImage::Format_ARGB32_Premultiplied on little-endian)
    for (unsigned int y = 0; y < h; ++y) {
        const auto *src = static_cast<const uint8_t *>(pixels) + y * srcStride;
        auto *dst = m_bitmap.scanLine(static_cast<int>(y));
        memcpy(dst, src, copyBytes);
    }

    ulBitmapUnlockPixels(bitmap);

    if (!m_loggedFirstPaint) {
        m_loggedFirstPaint = true;
        qWarning("UltralightWidget: first paint %ux%u", w, h);
    }
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
    qWarning("UltralightWidget: resizeEvent %dx%d", width(), height());
    if (m_view && width() > 0 && height() > 0) {
        ulViewResize(m_view,
                     static_cast<unsigned int>(width()),
                     static_cast<unsigned int>(height()));
        ulViewSetNeedsPaint(m_view, true);
    }
}

void UltralightWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_view && width() > 0 && height() > 0) {
        qWarning("UltralightWidget: showEvent resize %dx%d", width(), height());
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

void UltralightWidget::mousePressEvent(QMouseEvent *event)
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

void UltralightWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_view) return;
    ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseUp,
                                          event->pos().x(), event->pos().y(),
                                          qtButtonToUL(event->button()));
    ulViewFireMouseEvent(m_view, evt);
    ulDestroyMouseEvent(evt);
    event->accept();
}

void UltralightWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_view) return;
    // Pass the held button so Ultralight can do text selection during drag
    ULMouseButton btn = kMouseButton_None;
    if (event->buttons() & Qt::LeftButton)   btn = kMouseButton_Left;
    else if (event->buttons() & Qt::RightButton)  btn = kMouseButton_Right;
    else if (event->buttons() & Qt::MiddleButton) btn = kMouseButton_Middle;
    ULMouseEvent evt = ulCreateMouseEvent(kMouseEventType_MouseMoved,
                                          event->pos().x(), event->pos().y(),
                                          btn);
    ulViewFireMouseEvent(m_view, evt);
    ulDestroyMouseEvent(evt);
    event->accept();
}

void UltralightWidget::wheelEvent(QWheelEvent *event)
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
    if (mods & Qt::AltModifier)     m |= (1 << 0); // kMod_AltKey
    if (mods & Qt::ControlModifier) m |= (1 << 1); // kMod_CtrlKey
    if (mods & Qt::MetaModifier)    m |= (1 << 2); // kMod_MetaKey
    if (mods & Qt::ShiftModifier)   m |= (1 << 3); // kMod_ShiftKey
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
        text,
        unmod,
        false,  // is_keypad
        event->isAutoRepeat(),
        false   // is_system_key
    );

    ulViewFireKeyEvent(view, evt);
    ulDestroyKeyEvent(evt);
    ulDestroyString(text);
    ulDestroyString(unmod);
}

bool UltralightWidget::event(QEvent *event)
{
    // Accept ShortcutOverride for clipboard/select-all key sequences so that
    // global QAction shortcuts (Edit menu) do not steal them when we have focus.
    if (event->type() == QEvent::ShortcutOverride) {
        auto *ke = static_cast<QKeyEvent *>(event);
        if (ke->matches(QKeySequence::Paste)
            || ke->matches(QKeySequence::Copy)
            || ke->matches(QKeySequence::Cut)
            || ke->matches(QKeySequence::SelectAll)
            || ke->matches(QKeySequence::Undo)
            || ke->matches(QKeySequence::Redo)) {
            event->accept();
            return true;
        }
    }
    return QWidget::event(event);
}

void UltralightWidget::keyPressEvent(QKeyEvent *event)
{
    if (!m_view) return;

    // Intercept Ctrl+C — Ultralight has no native clipboard access,
    // so we grab the selection via JS and copy it to the system clipboard.
    if (event->matches(QKeySequence::Copy)) {
        evaluateScript(QStringLiteral(
            "(function() {"
            "  var sel = window.getSelection();"
            "  var text = sel ? sel.toString() : '';"
            "  if (text && window.exorcist) {"
            "    window.exorcist.sendToHost('copyText', { text: text });"
            "  }"
            "})();"
        ));
        event->accept();
        return;
    }

    // Intercept Ctrl+V — read system clipboard and insert into focused element.
    if (event->matches(QKeySequence::Paste)) {
        const QString clipText = QGuiApplication::clipboard()->text();
        if (!clipText.isEmpty())
            insertTextAtCursor(clipText);
        event->accept();
        return;
    }

    // Intercept Ctrl+X — cut selection to system clipboard.
    if (event->matches(QKeySequence::Cut)) {
        evaluateScript(QStringLiteral(
            "(function() {"
            "  var el = document.activeElement;"
            "  if (!el || (el.tagName !== 'TEXTAREA' && el.tagName !== 'INPUT')) {"
            "    el = document.getElementById('chatInput');"
            "  }"
            "  if (el && (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT')) {"
            "    var start = el.selectionStart || 0, end = el.selectionEnd || 0;"
            "    if (start !== end) {"
            "      var selected = el.value.substring(start, end);"
            "      if (window.exorcist) window.exorcist.sendToHost('copyText', { text: selected });"
            "      el.value = el.value.substring(0, start) + el.value.substring(end);"
            "      el.selectionStart = el.selectionEnd = start;"
            "      el.dispatchEvent(new Event('input', { bubbles: true }));"
            "    }"
            "  } else {"
            "    var sel = window.getSelection();"
            "    var text = sel ? sel.toString() : '';"
            "    if (text && window.exorcist) window.exorcist.sendToHost('copyText', { text: text });"
            "    if (sel) sel.deleteFromDocument();"
            "  }"
            "})();"
        ));
        event->accept();
        return;
    }

    // Fire RawKeyDown first
    fireKeyEvent(m_view, event, kKeyEventType_RawKeyDown);

    // Then fire Char event if it produces text
    if (!event->text().isEmpty())
        fireKeyEvent(m_view, event, kKeyEventType_Char);

    event->accept();
}

void UltralightWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (!m_view) return;
    fireKeyEvent(m_view, event, kKeyEventType_KeyUp);
    event->accept();
}

void UltralightWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    if (m_view) ulViewFocus(m_view);
}

void UltralightWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    if (m_view) ulViewUnfocus(m_view);
}

void UltralightWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    auto *copyAction = menu.addAction(tr("Copy"), QKeySequence::Copy);
    auto *pasteAction = menu.addAction(tr("Paste"), QKeySequence::Paste);
    auto *cutAction = menu.addAction(tr("Cut"), QKeySequence::Cut);
    menu.addSeparator();
    auto *selectAllAction = menu.addAction(tr("Select All"), QKeySequence::SelectAll);

    auto *chosen = menu.exec(event->globalPos());
    if (chosen == pasteAction) {
        const QString clipText = QGuiApplication::clipboard()->text();
        if (!clipText.isEmpty())
            insertTextAtCursor(clipText);
    } else if (chosen == cutAction) {
        evaluateScript(QStringLiteral(
            "(function() {"
            "  var el = document.activeElement;"
            "  if (el && (el.tagName === 'TEXTAREA' || el.tagName === 'INPUT')) {"
            "    var start = el.selectionStart, end = el.selectionEnd;"
            "    if (start !== end) {"
            "      var selected = el.value.substring(start, end);"
            "      if (window.exorcist) window.exorcist.sendToHost('copyText', { text: selected });"
            "      el.value = el.value.substring(0, start) + el.value.substring(end);"
            "      el.selectionStart = el.selectionEnd = start;"
            "      el.dispatchEvent(new Event('input', { bubbles: true }));"
            "    }"
            "  }"
            "})();"
        ));
    } else if (chosen == copyAction) {
        evaluateScript(QStringLiteral(
            "(function() {"
            "  var sel = window.getSelection();"
            "  var text = sel ? sel.toString() : '';"
            "  if (text && window.exorcist) {"
            "    window.exorcist.sendToHost('copyText', { text: text });"
            "  }"
            "})();"
        ));
    } else if (chosen == selectAllAction) {
        evaluateScript(QStringLiteral(
            "(function() {"
            "  var range = document.createRange();"
            "  var transcript = document.getElementById('transcript');"
            "  if (transcript) {"
            "    range.selectNodeContents(transcript);"
            "    var sel = window.getSelection();"
            "    sel.removeAllRanges();"
            "    sel.addRange(range);"
            "  }"
            "})();"
        ));
    }
}

// ── Ultralight Callbacks ─────────────────────────────────────────────────────

void UltralightWidget::onDOMReady(unsigned long long /*frameId*/,
                                  bool isMainFrame)
{
    if (!isMainFrame)
        return;

    // If we have pending HTML (from loadHTML workaround), inject it now
    // via document.write().  This triggers a second DOM load cycle —
    // onDOMReady will fire again with m_pendingHtml empty.
    if (!m_pendingHtml.isEmpty()) {
        const QString html = m_pendingHtml;
        m_pendingHtml.clear();

        // Use base64 + a JS bootstrap that:
        //  1. Decodes the full HTML
        //  2. Extracts <style> blocks and creates real <style> elements
        //  3. Extracts <script> blocks and creates real <script> elements
        //  4. Sets body innerHTML for everything else
        // This is necessary because innerHTML does not execute <script> tags.
        const QByteArray b64 = html.toUtf8().toBase64();
        const QString inject = QStringLiteral(
            "(function(){"
            "  var html = atob('%1');"
            // Extract and inject all <style> blocks
            "  var styles = html.match(/<style[^>]*>[\\s\\S]*?<\\/style>/gi) || [];"
            "  for(var i=0;i<styles.length;i++){"
            "    var c = styles[i].replace(/<\\/?style[^>]*>/gi,'');"
            "    var el = document.createElement('style');"
            "    el.textContent = c;"
            "    document.head.appendChild(el);"
            "  }"
            // Extract body content (between <body> and </body>)
            "  var bodyMatch = html.match(/<body[^>]*>([\\s\\S]*)<\\/body>/i);"
            "  var bodyHtml = bodyMatch ? bodyMatch[1] : html;"
            // Separate scripts from body HTML
            "  var scripts = [];"
            "  bodyHtml = bodyHtml.replace(/<script[^>]*>([\\s\\S]*?)<\\/script>/gi,"
            "    function(m, code){ scripts.push(code); return ''; });"
            "  document.body.innerHTML = bodyHtml;"
            // Create and execute script elements
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
            qWarning("UltralightWidget: HTML inject error: %s",
                     qPrintable(fromULString(exception)));
        }
        ulDestroyString(ulJs);
        // Fall through — DOM is already ready, proceed with bridge + scripts
    }

    qWarning("UltralightWidget: DOM ready");
    injectBridge();
    if (!m_loggedFirstPaint) {
        updateBitmap();
        update();
    }

    // Ultralight fires DOMReady before inline <script> tags finish
    // executing. We poll for ChatApp availability before flushing
    // queued scripts and emitting domReady.
    m_domReadyRetries = 0;
    flushPendingOrRetry();
}

void UltralightWidget::setReadyProbe(const QString &globalName)
{
    m_readyProbe = globalName;
}

void UltralightWidget::flushPendingOrRetry()
{
    bool ready = true;
    if (!m_readyProbe.isEmpty()) {
        // Probe whether inline <script> tags have finished executing
        // by checking if the configured JS global exists.
        ULString probe = toULString(m_readyProbe);
        ULString exception = nullptr;
        ulViewEvaluateScript(m_view, probe, &exception);
        ready = (exception == nullptr) || (ulStringGetLength(exception) == 0);
        ulDestroyString(probe);
    }

    if (ready) {
        m_domReady = true;
        for (int i = 0; i < m_pendingScripts.size(); ++i) {
            const auto &js = m_pendingScripts[i];
            ULString ulJs = toULString(js);
            ULString jsException = nullptr;
            ulViewEvaluateScript(m_view, ulJs, &jsException);
            if (jsException && ulStringGetLength(jsException) > 0) {
                const QString errMsg = fromULString(jsException);
                const QString snippet = js.left(120);
                qWarning("UltralightWidget queued JS error [%d]: %s\n  script: %s",
                         i, qPrintable(errMsg), qPrintable(snippet));
            }
            ulDestroyString(ulJs);
        }
        m_pendingScripts.clear();
        emit domReady();
    } else if (m_domReadyRetries < 50) {
        // Retry after a tick (16ms) — inline scripts may still be loading
        ++m_domReadyRetries;
        QTimer::singleShot(16, this, &UltralightWidget::flushPendingOrRetry);
    } else {
        qWarning("UltralightWidget: %s not available after %d retries, flushing anyway",
                 qPrintable(m_readyProbe), m_domReadyRetries);
        m_domReady = true;
        m_pendingScripts.clear();
        emit domReady();
    }
}

// ── JS Bridge Injection ──────────────────────────────────────────────────────

// callAsFunction callback for the bridge class.
// The widget pointer is stored as private data on the JSObject itself,
// so we never need a static context→widget map.
static JSValueRef bridgeCallAsFunction(JSContextRef ctx,
                                        JSObjectRef function,
                                        JSObjectRef /*thisObj*/,
                                        size_t argumentCount,
                                        const JSValueRef arguments[],
                                        JSValueRef * /*exception*/)
{
    auto *widget = static_cast<UltralightWidget *>(JSObjectGetPrivate(function));
    if (!widget || argumentCount < 2) {
        qWarning("UltralightWidget bridge: widget=%p argCount=%zu", static_cast<void*>(widget), argumentCount);
        return JSValueMakeUndefined(ctx);
    }

    // Extract type string
    JSStringRef jsType = JSValueToStringCopy(ctx, arguments[0], nullptr);
    const QString type = fromJSString(ctx, jsType);
    JSStringRelease(jsType);

    // Extract payload JSON string
    JSStringRef jsPayload = JSValueToStringCopy(ctx, arguments[1], nullptr);
    const QString payload = fromJSString(ctx, jsPayload);
    JSStringRelease(jsPayload);

    qWarning("UltralightWidget bridge: type=%s payload=%s", qPrintable(type), qPrintable(payload.left(200)));
    widget->dispatchBridgeMessage(type, payload);

    return JSValueMakeUndefined(ctx);
}

static void ensureBridgeClass()
{
    if (s_bridgeClass)
        return;
    JSClassDefinition def = kJSClassDefinitionEmpty;
    def.callAsFunction = bridgeCallAsFunction;
    s_bridgeClass = JSClassCreate(&def);
}

void UltralightWidget::dispatchBridgeMessage(const QString &type,
                                              const QString &payload)
{
    const auto doc = QJsonDocument::fromJson(payload.toUtf8());
    const auto obj = doc.isObject() ? doc.object() : QJsonObject();

    auto it = m_jsCallbacks.constFind(type);
    if (it != m_jsCallbacks.constEnd()) {
        qWarning("UltralightWidget dispatch: found callback for '%s'", qPrintable(type));
        (*it)(QJsonValue(obj));
    } else {
        qWarning("UltralightWidget dispatch: NO callback for '%s' (registered: %lld)", qPrintable(type), static_cast<long long>(m_jsCallbacks.size()));
    }

    emit jsMessage(type, obj);
}

void UltralightWidget::injectBridge()
{
    if (!m_view)
        return;

    JSContextRef ctx = ulViewLockJSContext(m_view);
    ensureBridgeClass();

    // Create a callable object with this widget as private data.
    // When JS calls window.__exorcist_bridge(type, payload),
    // bridgeCallAsFunction retrieves 'this' via JSObjectGetPrivate.
    JSObjectRef fn = JSObjectMake(ctx, s_bridgeClass, this);
    JSStringRef fnName = JSStringCreateWithUTF8CString("__exorcist_bridge");
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

    ulViewUnlockJSContext(m_view);
}

} // namespace exorcist

#endif // EXORCIST_HAS_ULTRALIGHT
