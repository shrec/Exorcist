#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include "plugininterface.h"

class QPluginLoader;

class PluginManager : public QObject
{
public:
    explicit PluginManager(QObject *parent = nullptr);

    int loadPluginsFrom(const QString &path);
    void initializeAll(QObject *services);
    void shutdownAll();

    const QStringList &errors() const;
    QVector<IPlugin *> plugins() const;
    QVector<QObject *> pluginObjects() const;

private:
    struct LoadedPlugin {
        IPlugin *instance;
        QPluginLoader *loader;
    };

    QVector<LoadedPlugin> m_loaded;
    QStringList m_errors;
};
