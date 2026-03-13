#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class ILspService : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    virtual ~ILspService() = default;

    virtual void startServer(const QString &workspaceRoot,
                             const QStringList &args = {}) = 0;
    virtual void stopServer() = 0;

signals:
    void navigateToLocation(const QString &path, int line, int character);
    void workspaceEditRequested(const QJsonObject &workspaceEdit);
    void referencesReady(const QJsonArray &locations);
    void statusMessage(const QString &msg, int timeout);
    void serverReady();
};
