#include "templateservicesbootstrap.h"

#include <memory>

#include "project/builtintemplateprovider.h"
#include "project/projecttemplateregistry.h"
#include "serviceregistry.h"

TemplateServicesBootstrap::TemplateServicesBootstrap(QObject *parent)
    : QObject(parent)
{
}

void TemplateServicesBootstrap::initialize(ServiceRegistry *services)
{
    if (!services)
        return;

    if (!m_projectTemplates) {
        auto projectTemplates = std::make_unique<ProjectTemplateRegistry>(this);
        auto builtinProvider = std::make_unique<BuiltinTemplateProvider>(this);
        projectTemplates->addProvider(builtinProvider.release());
        m_projectTemplates = projectTemplates.release();
        services->registerService(QStringLiteral("projectTemplateRegistry"), m_projectTemplates);
    }
}