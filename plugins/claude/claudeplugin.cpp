#include "claudeplugin.h"
#include "claudeprovider.h"

#include <QMetaObject>

PluginInfo ClaudePlugin::info() const
{
    return {QStringLiteral("claude"),
            QStringLiteral("Anthropic Claude"),
            QStringLiteral("1.0.0"),
            QStringLiteral("Anthropic Claude AI assistant"),
            QStringLiteral("Exorcist"),
            QStringLiteral("1.0"),
            {PluginPermission::NetworkAccess}};
}

bool ClaudePlugin::initialize(IHostServices *host)
{
    m_host = host;
    return true;
}

void ClaudePlugin::initialize(QObject *services)
{
    m_services = services;
}

void ClaudePlugin::shutdown() {}

QList<IAgentProvider *> ClaudePlugin::createProviders(QObject *parent)
{
    auto *provider = new ClaudeProvider;
    if (parent)
        provider->setParent(parent);

    // Wire SecureKeyStorage via QMetaObject::invokeMethod (no link-time
    // dependency on core).  Same pattern as CopilotPlugin.
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
