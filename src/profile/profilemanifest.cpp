#include "profilemanifest.h"

#include <QJsonArray>

ProfileManifest ProfileManifest::fromJson(const QJsonObject &obj)
{
    ProfileManifest m;

    m.id          = obj.value(QStringLiteral("id")).toString();
    m.name        = obj.value(QStringLiteral("name")).toString();
    m.description = obj.value(QStringLiteral("description")).toString();
    m.icon        = obj.value(QStringLiteral("icon")).toString();
    m.category    = obj.value(QStringLiteral("category")).toString();

    // Plugin lists
    const auto req = obj.value(QStringLiteral("requiredPlugins")).toArray();
    for (const auto &v : req)
        m.requiredPlugins.append(v.toString());

    const auto opt = obj.value(QStringLiteral("optionalPlugins")).toArray();
    for (const auto &v : opt)
        m.optionalPlugins.append(v.toString());

    const auto dis = obj.value(QStringLiteral("disabledPlugins")).toArray();
    for (const auto &v : dis)
        m.disabledPlugins.append(v.toString());

    // Detection rules
    const auto rules = obj.value(QStringLiteral("detection")).toArray();
    for (const auto &rv : rules) {
        const auto robj = rv.toObject();
        ProfileDetectionRule rule;

        const QString typeStr = robj.value(QStringLiteral("type")).toString();
        if (typeStr == QStringLiteral("fileContent"))
            rule.type = ProfileDetectionRule::FileContent;
        else if (typeStr == QStringLiteral("directoryExists"))
            rule.type = ProfileDetectionRule::DirectoryExists;
        else
            rule.type = ProfileDetectionRule::FileExists;

        rule.pattern = robj.value(QStringLiteral("pattern")).toString();
        rule.weight  = robj.value(QStringLiteral("weight")).toInt(1);
        m.detectionRules.append(rule);
    }
    m.detectionThreshold = obj.value(QStringLiteral("detectionThreshold")).toInt(1);

    // Dock defaults
    const auto docks = obj.value(QStringLiteral("dockDefaults")).toArray();
    for (const auto &dv : docks) {
        const auto dobj = dv.toObject();
        ProfileDockDefault dd;
        dd.dockId  = dobj.value(QStringLiteral("id")).toString();
        dd.visible = dobj.value(QStringLiteral("visible")).toBool(false);
        m.dockDefaults.append(dd);
    }

    // Settings overrides
    const auto sets = obj.value(QStringLiteral("settings")).toObject();
    for (auto it = sets.begin(); it != sets.end(); ++it) {
        ProfileSetting s;
        s.key   = it.key();
        s.value = it.value().toVariant();
        m.settings.append(s);
    }

    return m;
}
