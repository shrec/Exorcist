#pragma once

#include <QString>
#include <QObject>

struct PluginInfo
{
    QString id;
    QString name;
    QString version;
    QString description;
    QString author;
};

class IPlugin
{
public:
    virtual ~IPlugin() = default;
    virtual PluginInfo info() const = 0;
    virtual void initialize(QObject *services) = 0;
    virtual void shutdown() = 0;
};

#define EXORCIST_PLUGIN_IID "org.exorcist.IPlugin/1.0"

Q_DECLARE_INTERFACE(IPlugin, EXORCIST_PLUGIN_IID)
