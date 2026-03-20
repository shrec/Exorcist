#pragma once

#include <QObject>

#include "plugin/iviewcontributor.h"
#include "plugin/workbenchpluginbase.h"

#include <memory>

class SerialMonitorController;
class SerialMonitorPanel;

class SerialMonitorPlugin : public QObject, public WorkbenchPluginBase, public IViewContributor
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID EXORCIST_PLUGIN_IID FILE "plugin.json")
    Q_INTERFACES(IPlugin IViewContributor)

public:
    explicit SerialMonitorPlugin(QObject *parent = nullptr);

    PluginInfo info() const override;
    QWidget *createView(const QString &viewId, QWidget *parent) override;

private:
    bool initializePlugin() override;
    void registerCommands();
    void installMenusAndToolBar();
    void restorePanelSettings() const;
    void savePanelSettings() const;
    QString formatLogLine(const QString &text) const;
    QString externalMonitorCommand() const;

    std::unique_ptr<SerialMonitorController> m_controller;
    SerialMonitorPanel *m_panel = nullptr;
};