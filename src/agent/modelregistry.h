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
    explicit ModelRegistry(QObject *parent = nullptr)
        : QObject(parent)
    {
        registerBuiltinModels();
    }

    /// Register a model
    void registerModel(const ModelInfo &model)
    {
        m_models[model.id] = model;
        emit modelsChanged();
    }

    /// Remove a model
    void removeModel(const QString &id)
    {
        m_models.remove(id);
        emit modelsChanged();
    }

    /// Get model info by ID
    ModelInfo model(const QString &id) const { return m_models.value(id); }

    /// Get all model IDs
    QStringList modelIds() const { return m_models.keys(); }

    /// Get all models for a vendor
    QList<ModelInfo> modelsForVendor(const QString &vendor) const
    {
        QList<ModelInfo> result;
        for (const auto &m : m_models) {
            if (m.vendor == vendor)
                result.append(m);
        }
        return result;
    }

    /// Get models with tool call support
    QList<ModelInfo> modelsWithToolCalls() const
    {
        QList<ModelInfo> result;
        for (const auto &m : m_models) {
            if (m.capabilities.toolCalls)
                result.append(m);
        }
        return result;
    }

    /// Get models with vision support
    QList<ModelInfo> modelsWithVision() const
    {
        QList<ModelInfo> result;
        for (const auto &m : m_models) {
            if (m.capabilities.vision)
                result.append(m);
        }
        return result;
    }

    /// Per-mode model selection
    void setModelForMode(const QString &mode, const QString &modelId)
    {
        m_modeModels[mode] = modelId;
    }

    QString modelForMode(const QString &mode) const
    {
        return m_modeModels.value(mode);
    }

    /// Get display name list (for combo boxes)
    QStringList displayNames() const
    {
        QStringList names;
        for (const auto &m : m_models)
            names << m.name;
        return names;
    }

    /// Find model by display name
    ModelInfo modelByName(const QString &name) const
    {
        for (const auto &m : m_models) {
            if (m.name == name)
                return m;
        }
        return {};
    }

    /// Export all models as JSON
    QJsonArray toJson() const
    {
        QJsonArray arr;
        for (const auto &m : m_models) {
            QJsonObject obj;
            obj[QStringLiteral("id")] = m.id;
            obj[QStringLiteral("name")] = m.name;
            obj[QStringLiteral("vendor")] = m.vendor;
            arr.append(obj);
        }
        return arr;
    }

signals:
    void modelsChanged();

private:
    void registerBuiltinModels()
    {
        auto make = [](const QString &id, const QString &name, const QString &vendor,
                       bool vision, bool tools, bool thinking,
                       int maxCtx, int maxOut,
                       bool premium = false, double multiplier = 1.0) -> ModelInfo {
            ModelInfo m;
            m.id = id;
            m.name = name;
            m.vendor = vendor;
            m.capabilities.vision = vision;
            m.capabilities.toolCalls = tools;
            m.capabilities.thinking = thinking;
            m.capabilities.streaming = true;
            m.capabilities.maxContextWindowTokens = maxCtx;
            m.capabilities.maxOutputTokens = maxOut;
            m.capabilities.type = QStringLiteral("chat");
            m.billing.isPremium = premium;
            m.billing.multiplier = multiplier;
            return m;
        };

        // OpenAI — rates from GitHub Copilot billing tiers
        registerModel(make(QStringLiteral("gpt-4o"), QStringLiteral("GPT-4o"),
                           QStringLiteral("OpenAI"), true, true, false, 128000, 16384,
                           false, 1.0));
        registerModel(make(QStringLiteral("gpt-4o-mini"), QStringLiteral("GPT-4o mini"),
                           QStringLiteral("OpenAI"), true, true, false, 128000, 16384,
                           false, 0.25));
        registerModel(make(QStringLiteral("o3"), QStringLiteral("o3"),
                           QStringLiteral("OpenAI"), true, true, true, 200000, 100000,
                           true, 1.0));
        registerModel(make(QStringLiteral("o4-mini"), QStringLiteral("o4-mini"),
                           QStringLiteral("OpenAI"), true, true, true, 200000, 100000,
                           false, 0.33));

        // Anthropic — rates from API pricing tiers
        registerModel(make(QStringLiteral("claude-sonnet-4-20250514"), QStringLiteral("Claude Sonnet 4"),
                           QStringLiteral("Anthropic"), true, true, true, 200000, 64000,
                           false, 1.0));
        registerModel(make(QStringLiteral("claude-opus-4-20250514"), QStringLiteral("Claude Opus 4"),
                           QStringLiteral("Anthropic"), true, true, true, 200000, 64000,
                           true, 5.0));
        registerModel(make(QStringLiteral("claude-3.5-haiku-20241022"), QStringLiteral("Claude 3.5 Haiku"),
                           QStringLiteral("Anthropic"), false, true, false, 200000, 8192,
                           false, 0.2));

        // Google
        registerModel(make(QStringLiteral("gemini-2.5-pro"), QStringLiteral("Gemini 2.5 Pro"),
                           QStringLiteral("Google"), true, true, true, 1000000, 65536,
                           true, 1.25));

        // xAI
        registerModel(make(QStringLiteral("grok-3"), QStringLiteral("Grok 3"),
                           QStringLiteral("xAI"), true, true, true, 131072, 16384,
                           true, 1.0));
    }

    QMap<QString, ModelInfo> m_models;
    QMap<QString, QString> m_modeModels;  // mode -> modelId
};
