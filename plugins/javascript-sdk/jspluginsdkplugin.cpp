#include "jspluginsdkplugin.h"
#include "jspluginruntime.h"
#include "ultralightpluginview.h"

#include "sdk/ihostservices.h"
#include "sdk/iviewservice.h"

#include <QCoreApplication>
#include <QDir>

PluginInfo JsPluginSdkPlugin::info() const
{
    return {
        QStringLiteral("org.exorcist.javascript-sdk"),
        QStringLiteral("JavaScript Plugin SDK"),
        QStringLiteral("1.0.0"),
        QStringLiteral("Ultralight JSC-based runtime for lightweight JavaScript plugins"),
        QStringLiteral("Exorcist"),
        QStringLiteral("1.0"),
        {} // no special permissions — each JS plugin declares its own
    };
}

bool JsPluginSdkPlugin::initialize(IHostServices *host)
{
    m_host = host;
    m_runtime = std::make_unique<jssdk::JsPluginRuntime>(host, this);

    // Load JS plugins from <app>/plugins/javascript
    const QString jsPluginsDir =
        QCoreApplication::applicationDirPath() + QStringLiteral("/plugins/javascript");

    if (QDir(jsPluginsDir).exists()) {
        const int count = m_runtime->loadPluginsFrom(jsPluginsDir);
        if (count > 0) {
            m_runtime->initializeAll();
            m_runtime->enableHotReload(jsPluginsDir);
        }

        for (const QString &err : m_runtime->errors())
            qWarning("[JsSDK] %s", qUtf8Printable(err));
    }

    // Register WebView panels for HTML plugins
    registerHtmlPluginViews();

    return true;
}

void JsPluginSdkPlugin::shutdown()
{
    // Unregister HTML plugin views before shutting down the runtime
    if (m_host && m_host->views()) {
        for (auto it = m_htmlViews.constBegin(); it != m_htmlViews.constEnd(); ++it)
            m_host->views()->unregisterPanel(it.key());
    }
    m_htmlViews.clear();

    if (m_runtime) {
        m_runtime->shutdownAll();
        m_runtime.reset();
    }
}

void JsPluginSdkPlugin::registerHtmlPluginViews()
{
    if (!m_host || !m_host->views() || !m_runtime)
        return;

    for (auto &hp : m_runtime->htmlPlugins()) {
        for (const jssdk::JsViewContribution &vc : hp.info.views) {
            auto view = std::make_unique<jssdk::UltralightPluginView>(
                hp.info.htmlEntry, hp.pctx.get(), nullptr);

            QWidget *viewPtr = view.get();
            // registerPanel takes ownership — the IDE parents the widget
            m_host->views()->registerPanel(vc.id, vc.title, view.release());
            m_htmlViews[vc.id] = viewPtr;

            qInfo("[JsSDK] Registered HTML view: %s (%s)",
                  qUtf8Printable(vc.id), qUtf8Printable(vc.title));
        }
    }
}
