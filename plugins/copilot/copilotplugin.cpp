#include "copilotplugin.h"
#include "copilotprovider.h"

#include <QMetaObject>

PluginInfo CopilotPlugin::info() const
{
    return {QStringLiteral("copilot"),
            QStringLiteral("GitHub Copilot"),
            QStringLiteral("1.0.0"),
            QStringLiteral("GitHub Copilot AI assistant"),
            QStringLiteral("Exorcist")};
}

void CopilotPlugin::initialize(QObject *services)
{
    m_services = services;
}

void CopilotPlugin::shutdown() {}

QList<IAgentProvider *> CopilotPlugin::createProviders(QObject *parent)
{
    auto *provider = new CopilotProvider;
    if (parent)
        provider->setParent(parent);

    // Wire secure key storage via QMetaObject::invokeMethod (no link-time dependency)
    if (m_services) {
        QObject *ks = nullptr;
        QMetaObject::invokeMethod(m_services, "service",
                                  Q_RETURN_ARG(QObject *, ks),
                                  Q_ARG(QString, QStringLiteral("secureKeyStorage")));
        if (ks) {
            provider->setKeyStorageCallbacks(
                [ks](const QString &svc, const QString &key) -> bool {
                    bool ok = false;
                    QMetaObject::invokeMethod(ks, "storeKey",
                                              Q_RETURN_ARG(bool, ok),
                                              Q_ARG(QString, svc),
                                              Q_ARG(QString, key));
                    return ok;
                },
                [ks](const QString &svc) -> QString {
                    QString val;
                    QMetaObject::invokeMethod(ks, "retrieveKey",
                                              Q_RETURN_ARG(QString, val),
                                              Q_ARG(QString, svc));
                    return val;
                },
                [ks](const QString &svc) -> bool {
                    bool ok = false;
                    QMetaObject::invokeMethod(ks, "deleteKey",
                                              Q_RETURN_ARG(bool, ok),
                                              Q_ARG(QString, svc));
                    return ok;
                });
        }
    }

    return {provider};
}
