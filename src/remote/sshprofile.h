#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>

/// SSH connection profile — persisted in .exorcist/ssh_profiles.json.
struct SshProfile
{
    QString id;                 // unique identifier (UUID)
    QString name;               // display name ("My Server")
    QString host;               // hostname or IP
    int     port = 22;
    QString user;               // remote user
    QString authMethod;         // "key" | "password" | "agent"
    QString privateKeyPath;     // path to private key (if authMethod == "key")
    QString remoteWorkDir;      // default remote working directory
    QString detectedArch;       // last detected architecture (e.g. "aarch64")
    QString detectedOs;         // last detected OS (e.g. "Linux")
    QString detectedDistro;     // last detected distro (e.g. "Ubuntu 24.04")
    QMap<QString, QString> env; // extra environment variables

    QJsonObject toJson() const
    {
        QJsonObject o;
        o[QLatin1String("id")]             = id;
        o[QLatin1String("name")]           = name;
        o[QLatin1String("host")]           = host;
        o[QLatin1String("port")]           = port;
        o[QLatin1String("user")]           = user;
        o[QLatin1String("authMethod")]     = authMethod;
        o[QLatin1String("privateKeyPath")] = privateKeyPath;
        o[QLatin1String("remoteWorkDir")]  = remoteWorkDir;        o[QLatin1String("detectedArch")]  = detectedArch;
        o[QLatin1String("detectedOs")]    = detectedOs;
        o[QLatin1String("detectedDistro")] = detectedDistro;        QJsonObject envObj;
        for (auto it = env.cbegin(); it != env.cend(); ++it)
            envObj[it.key()] = it.value();
        o[QLatin1String("env")] = envObj;
        return o;
    }

    static SshProfile fromJson(const QJsonObject &o)
    {
        SshProfile p;
        p.id             = o.value(QLatin1String("id")).toString();
        p.name           = o.value(QLatin1String("name")).toString();
        p.host           = o.value(QLatin1String("host")).toString();
        p.port           = o.value(QLatin1String("port")).toInt(22);
        p.user           = o.value(QLatin1String("user")).toString();
        p.authMethod     = o.value(QLatin1String("authMethod")).toString(QStringLiteral("key"));
        p.privateKeyPath = o.value(QLatin1String("privateKeyPath")).toString();
        p.remoteWorkDir  = o.value(QLatin1String("remoteWorkDir")).toString();
        p.detectedArch   = o.value(QLatin1String("detectedArch")).toString();
        p.detectedOs     = o.value(QLatin1String("detectedOs")).toString();
        p.detectedDistro = o.value(QLatin1String("detectedDistro")).toString();
        const auto envObj = o.value(QLatin1String("env")).toObject();
        for (auto it = envObj.begin(); it != envObj.end(); ++it)
            p.env.insert(it.key(), it.value().toString());
        return p;
    }
};
