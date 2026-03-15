#pragma once

#include <QObject>
#include <QHash>
#include <QString>

#include "plugininterface.h"

class QWidget;
class IHostServices;
namespace jssdk { class JsPluginRuntime; }

class JsPluginSdkPlugin : public QObject, public IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin)

public:
    PluginInfo info() const override;
    bool initialize(IHostServices *host) override;
    void shutdown() override;

private:
    void registerHtmlPluginViews();

    IHostServices *m_host = nullptr;
    std::unique_ptr<jssdk::JsPluginRuntime> m_runtime;
    QHash<QString, QWidget *> m_htmlViews; // viewId → widget (IDE owns)
};
