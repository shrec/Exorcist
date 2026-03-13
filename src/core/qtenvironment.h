#pragma once

#include <QObject>

#include "ienvironment.h"

class QtEnvironment : public QObject, public IEnvironment
{
    Q_OBJECT

public:
    explicit QtEnvironment(QObject *parent = nullptr);

    QString variable(const QString &name) const override;
    bool hasVariable(const QString &name) const override;
    void setVariable(const QString &name, const QString &value) override;
    QStringList variableNames() const override;

    QString platform() const override;
    QString homeDirectory() const override;
    QString tempDirectory() const override;
};
