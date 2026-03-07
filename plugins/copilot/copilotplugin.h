#pragma once

#include <QObject>

#include "agent/iagentplugin.h"
#include "plugininterface.h"

class CopilotPlugin : public QObject, public IPlugin, public IAgentPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID)
    Q_INTERFACES(IPlugin IAgentPlugin)

public:
    PluginInfo info() const override;
    void initialize(QObject *services) override;
    void shutdown() override;

    QList<IAgentProvider *> createProviders(QObject *parent) override;
};
