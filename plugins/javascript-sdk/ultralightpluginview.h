#pragma once
// ── UltralightPluginView ─────────────────────────────────────────────────────
//
// QWidget hosting an Ultralight WebView for HTML-based JS plugins.
//
// Each plugin view loads an HTML file from the plugin directory, injects
// the `ex.*` SDK (via JsHostAPI), and renders the result as a bitmap
// using CPU rendering (same approach as UltralightWidget).
//
// The JS developer writes HTML/CSS/JS.  The `ex.*` SDK is available in
// the DOM context for IDE interactions (commands, editor, notifications,
// workspace, git, diagnostics, logging, events).
//
// External <script src="..."> and <link href="..."> references are read
// from disk and inlined into the HTML before loading, due to the
// Ultralight 1.4-beta crash on file:// URLs and ulViewLoadHTML().

#include <QWidget>
#include <QImage>
#include <QTimer>

#include <cstdint>
#include <Ultralight/CAPI.h>
#include <JavaScriptCore/JavaScript.h>

#include "jspluginruntime.h"   // PluginContext

namespace jssdk {

class UltralightPluginView : public QWidget
{
    Q_OBJECT

public:
    /// Create a view that loads the given HTML file and injects `ex.*` SDK.
    /// @param htmlPath  Absolute path to the plugin's index.html.
    /// @param pctx      Plugin context (permissions, host services, pluginId).
    /// @param parent    Qt parent widget.
    explicit UltralightPluginView(const QString &htmlPath,
                                  PluginContext *pctx,
                                  QWidget *parent = nullptr);
    ~UltralightPluginView() override;

    /// Evaluate arbitrary JavaScript in the view context.
    void evaluateScript(const QString &js);

    /// True after DOM is ready and SDK is injected.
    bool isReady() const { return m_domReady; }

signals:
    /// Emitted once DOM is ready and the `ex.*` SDK has been injected.
    void ready();

protected:
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
    void showEvent(QShowEvent *event) override;

public:
    /// Called by the static DOMReady callback (needs public access).
    void onDOMReady(bool isMainFrame);

private:
    void initRenderer();
    void initView();
    void loadHtml();
    void tick();
    void updateBitmap();
    void injectSdkAndInitialize();

    static QString inlineResources(const QString &htmlPath);

    ULRenderer m_renderer = nullptr;
    ULView     m_view     = nullptr;
    QImage     m_bitmap;
    QTimer    *m_tickTimer = nullptr;
    bool       m_domReady  = false;

    QString        m_htmlPath;
    PluginContext *m_pctx;
    QString        m_pendingHtml;
};

} // namespace jssdk
