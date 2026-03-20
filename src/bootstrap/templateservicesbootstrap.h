#ifndef C3653B71_E8B8_4A3C_A406_CB0CD14D5AC8
#define C3653B71_E8B8_4A3C_A406_CB0CD14D5AC8

#include <QObject>

class ServiceRegistry;
class ProjectTemplateRegistry;

/// Registers template-catalog services used by project creation workflows.
class TemplateServicesBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit TemplateServicesBootstrap(QObject *parent = nullptr);

    void initialize(ServiceRegistry *services);

    ProjectTemplateRegistry *projectTemplateRegistry() const { return m_projectTemplates; }

private:
    ProjectTemplateRegistry *m_projectTemplates = nullptr;
};


#endif /* C3653B71_E8B8_4A3C_A406_CB0CD14D5AC8 */
