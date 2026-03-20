#pragma once

#include <QString>

struct EmbeddedCommandResolution
{
    QString workspaceSummary;
    QString flashCommand;
    QString monitorCommand;
    QString debugSummary;
    QString debugCommand;
};

class EmbeddedCommandResolver
{
public:
    EmbeddedCommandResolution resolve(const QString &workspaceRoot) const;
};
