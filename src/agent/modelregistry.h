#pragma once

#include "../aiinterface.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

// ─────────────────────────────────────────────────────────────────────────────
// ModelRegistry — maintains a registry of available AI models.
//
// Each provider (OpenAI, Anthropic, Ollama, custom) reports its models.
// The registry aggregates them and provides model selection per mode.
// Uses ModelInfo/ModelCapabilities from aiinterface.h
// ─────────────────────────────────────────────────────────────────────────────

class ModelRegistry : public QObject
{
    Q_OBJECT

public:
    explicit ModelRegistry(QObject *parent = nullptr);

    void registerModel(const ModelInfo &model);
    void removeModel(const QString &id);

    ModelInfo model(const QString &id) const { return m_models.value(id); }
    QStringList modelIds() const { return m_models.keys(); }

    QList<ModelInfo> modelsForVendor(const QString &vendor) const;
    QList<ModelInfo> modelsWithToolCalls() const;
    QList<ModelInfo> modelsWithVision() const;

    void setModelForMode(const QString &mode, const QString &modelId);
    QString modelForMode(const QString &mode) const;

    QStringList displayNames() const;
    ModelInfo modelByName(const QString &name) const;

    QJsonArray toJson() const;

signals:
    void modelsChanged();

private:
    void registerBuiltinModels();

    QMap<QString, ModelInfo> m_models;
    QMap<QString, QString> m_modeModels;  // mode -> modelId
};
