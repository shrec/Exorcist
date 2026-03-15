#pragma once

#include "core/icomponentfactory.h"

#include <QObject>

/// TerminalComponentFactory — Produces TerminalPanel instances.
///
/// This is a component DLL that provides terminal panels as
/// independently-loadable dock widgets. Supports multiple instances
/// (e.g., separate terminals for build, debug, and serial output).
class TerminalComponentFactory : public QObject, public IComponentFactory
{
    Q_OBJECT
    Q_INTERFACES(IComponentFactory)
    Q_PLUGIN_METADATA(IID EXORCIST_COMPONENT_IID)

public:
    explicit TerminalComponentFactory(QObject *parent = nullptr);

    // ── IComponentFactory ───────────────────────────────────────────

    QString componentId() const override;
    QString displayName() const override;
    QString description() const override;
    QString iconPath() const override;
    DockArea preferredArea() const override;
    bool supportsMultipleInstances() const override;
    QString category() const override;

    QWidget *createInstance(const QString &instanceId,
                            IHostServices *hostServices,
                            QWidget *parent) override;
};
