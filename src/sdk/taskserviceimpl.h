#ifndef A0E60A2B_330B_4D33_875A_FCF2937B663F
#define A0E60A2B_330B_4D33_875A_FCF2937B663F

#include "sdk/itaskservice.h"

#include <QObject>

class ServiceRegistry;

class TaskServiceImpl : public QObject, public ITaskService
{
    Q_OBJECT

public:
    explicit TaskServiceImpl(QObject *parent = nullptr);

    void setServiceRegistry(ServiceRegistry *registry);

    void runTask(const QString &taskId) override;
    void cancelTask(const QString &taskId) override;
    bool isTaskRunning(const QString &taskId) const override;

private:
    ServiceRegistry *m_registry = nullptr;
};


#endif /* A0E60A2B_330B_4D33_875A_FCF2937B663F */
