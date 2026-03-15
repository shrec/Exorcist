#include "componentregistry.h"

#include "../core/icomponentfactory.h"
#include "../sdk/ihostservices.h"
#include "../ui/dock/ExDockWidget.h"
#include "../ui/dock/DockManager.h"

#include <QDir>
#include <QMainWindow>
#include <QPluginLoader>
#include <QCoreApplication>

struct ComponentRegistry::LoadedComponent {
    std::unique_ptr<QPluginLoader> loader;
    IComponentFactory *factory = nullptr; // owned by the loader
};

ComponentRegistry::ComponentRegistry(IHostServices *hostServices,
                                      exdock::DockManager *dockManager,
                                      QObject *parent)
    : QObject(parent)
    , m_hostServices(hostServices)
    , m_dockManager(dockManager)
{
}

ComponentRegistry::~ComponentRegistry()
{
    // Destroy all instances first
    const auto ids = m_instances.keys();
    for (const auto &id : ids) {
        destroyInstance(id);
    }

    // Unload DLLs
    m_loadedDlls.clear();
}

int ComponentRegistry::loadComponentsFromDirectory(const QString &dirPath)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return 0;

    int loaded = 0;

#if defined(Q_OS_WIN)
    const QStringList filters = {QStringLiteral("*.dll")};
#elif defined(Q_OS_MAC)
    const QStringList filters = {QStringLiteral("*.dylib")};
#else
    const QStringList filters = {QStringLiteral("*.so")};
#endif

    const auto entries = dir.entryInfoList(filters, QDir::Files);
    for (const auto &entry : entries) {
        auto lc = std::make_unique<LoadedComponent>();
        lc->loader = std::make_unique<QPluginLoader>(entry.absoluteFilePath());

        QObject *instance = lc->loader->instance();
        if (!instance) {
            qWarning("ComponentRegistry: failed to load %s: %s",
                     qPrintable(entry.fileName()),
                     qPrintable(lc->loader->errorString()));
            continue;
        }

        auto *factory = qobject_cast<IComponentFactory *>(instance);
        if (!factory) {
            qWarning("ComponentRegistry: %s does not implement IComponentFactory",
                     qPrintable(entry.fileName()));
            lc->loader->unload();
            continue;
        }

        const QString compId = factory->componentId();
        if (m_factories.contains(compId)) {
            qWarning("ComponentRegistry: duplicate component ID '%s' from %s",
                     qPrintable(compId), qPrintable(entry.fileName()));
            lc->loader->unload();
            continue;
        }

        lc->factory = factory;
        m_factories.insert(compId, factory);
        m_loadedDlls.push_back(std::move(lc));
        ++loaded;

        emit componentRegistered(compId);
    }

    return loaded;
}

bool ComponentRegistry::registerFactory(IComponentFactory *factory)
{
    if (!factory)
        return false;

    const QString id = factory->componentId();
    if (m_factories.contains(id))
        return false;

    m_factories.insert(id, factory);
    emit componentRegistered(id);
    return true;
}

// ── Instance management ─────────────────────────────────────────────────────

QString ComponentRegistry::createInstance(const QString &componentId)
{
    auto *fac = factory(componentId);
    if (!fac)
        return {};

    // Check multi-instance support
    if (!fac->supportsMultipleInstances() && instanceCount(componentId) > 0)
        return {};

    const QString instanceId = generateInstanceId(componentId);

    QWidget *widget = fac->createInstance(instanceId, m_hostServices, nullptr);
    if (!widget)
        return {};

    // Determine dock title
    QString title = fac->displayName();
    if (fac->supportsMultipleInstances()) {
        int num = m_nextInstanceNum.value(componentId, 1);
        title = QStringLiteral("%1 %2").arg(title).arg(num);
    }

    // Wrap in ExDockWidget
    exdock::ExDockWidget *dock = nullptr;
    if (m_dockManager) {
        dock = new exdock::ExDockWidget(title, m_dockManager->mainWindow());
        dock->setDockId(instanceId);
        dock->setContentWidget(widget);

        exdock::SideBarArea area;
        switch (fac->preferredArea()) {
        case IComponentFactory::Left:   area = exdock::SideBarArea::Left; break;
        case IComponentFactory::Right:  area = exdock::SideBarArea::Right; break;
        case IComponentFactory::Bottom: area = exdock::SideBarArea::Bottom; break;
        default:                        area = exdock::SideBarArea::Left; break;
        }

        m_dockManager->addDockWidget(dock, area, true);
    }

    InstanceInfo info;
    info.componentId = componentId;
    info.widget = widget;
    info.dock = dock;
    m_instances.insert(instanceId, info);

    emit instanceCreated(instanceId, componentId);
    return instanceId;
}

bool ComponentRegistry::destroyInstance(const QString &instanceId)
{
    auto it = m_instances.find(instanceId);
    if (it == m_instances.end())
        return false;

    const InstanceInfo info = it.value();

    // Notify factory
    auto *fac = factory(info.componentId);
    if (fac)
        fac->destroyInstance(instanceId);

    // Remove from dock manager
    if (info.dock && m_dockManager)
        m_dockManager->removeDockWidget(info.dock);

    m_instances.erase(it);
    emit instanceDestroyed(instanceId, info.componentId);
    return true;
}

void ComponentRegistry::destroyAllInstances(const QString &componentId)
{
    const auto ids = instancesOf(componentId);
    for (const auto &id : ids) {
        destroyInstance(id);
    }
}

// ── Query ───────────────────────────────────────────────────────────────────

QStringList ComponentRegistry::availableComponents() const
{
    return m_factories.keys();
}

IComponentFactory *ComponentRegistry::factory(const QString &componentId) const
{
    return m_factories.value(componentId, nullptr);
}

QStringList ComponentRegistry::activeInstances() const
{
    return m_instances.keys();
}

QStringList ComponentRegistry::instancesOf(const QString &componentId) const
{
    QStringList result;
    for (auto it = m_instances.constBegin(); it != m_instances.constEnd(); ++it) {
        if (it.value().componentId == componentId)
            result.append(it.key());
    }
    return result;
}

QWidget *ComponentRegistry::instanceWidget(const QString &instanceId) const
{
    auto it = m_instances.find(instanceId);
    if (it != m_instances.end())
        return it.value().widget;
    return nullptr;
}

int ComponentRegistry::instanceCount(const QString &componentId) const
{
    int count = 0;
    for (auto it = m_instances.constBegin(); it != m_instances.constEnd(); ++it) {
        if (it.value().componentId == componentId)
            ++count;
    }
    return count;
}

bool ComponentRegistry::hasComponent(const QString &componentId) const
{
    return m_factories.contains(componentId);
}

// ── Private ─────────────────────────────────────────────────────────────────

QString ComponentRegistry::generateInstanceId(const QString &componentId)
{
    int &num = m_nextInstanceNum[componentId];
    ++num;
    return QStringLiteral("%1-%2").arg(componentId).arg(num);
}
