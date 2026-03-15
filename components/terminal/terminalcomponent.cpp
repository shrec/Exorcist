#include "terminalcomponent.h"
#include "terminal/terminalpanel.h"

#include <memory>

TerminalComponentFactory::TerminalComponentFactory(QObject *parent)
    : QObject(parent)
{
}

QString TerminalComponentFactory::componentId() const
{
    return QStringLiteral("exorcist.terminal");
}

QString TerminalComponentFactory::displayName() const
{
    return tr("Terminal");
}

QString TerminalComponentFactory::description() const
{
    return tr("Multi-tab terminal emulator with ConPTY/PTY support");
}

QString TerminalComponentFactory::iconPath() const
{
    return QStringLiteral(":/icons/terminal.svg");
}

IComponentFactory::DockArea TerminalComponentFactory::preferredArea() const
{
    return DockArea::Bottom;
}

bool TerminalComponentFactory::supportsMultipleInstances() const
{
    return true;
}

QString TerminalComponentFactory::category() const
{
    return QStringLiteral("Core");
}

QWidget *TerminalComponentFactory::createInstance(const QString &instanceId,
                                                   IHostServices *hostServices,
                                                   QWidget *parent)
{
    Q_UNUSED(instanceId);
    Q_UNUSED(hostServices);
    auto panel = std::make_unique<TerminalPanel>(parent);
    return panel.release(); // ownership transfers to caller (ComponentRegistry)
}
