#pragma once

#include <QMap>
#include <QWidget>

#include "../aiinterface.h"

class QComboBox;
class QLineEdit;

// ─────────────────────────────────────────────────────────────────────────────
// ModelConfigWidget — per-mode model selection UI.
//
// Allows users to pick different models for Ask, Edit, Agent, and ghost text.
// Shows model capabilities (vision, tool-calling, thinking) as icons.
// Supports custom endpoint/headers for BYOK.
// ─────────────────────────────────────────────────────────────────────────────

class ModelConfigWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ModelConfigWidget(QWidget *parent = nullptr);

    void setModels(const QList<ModelInfo> &models);
    QString selectedModel(const QString &mode) const;
    void setSelectedModel(const QString &mode, const QString &model);

    QString customEndpoint() const;
    QString customApiKey() const;
    int thinkingBudget() const;

signals:
    void modelChanged(const QString &mode, const QString &model);
    void endpointConfigured(const QString &url, const QString &apiKey,
                            const QString &extraHeaders);

private:
    QMap<QString, QComboBox *> m_modeComboBoxes;
    QLineEdit *m_endpointEdit;
    QLineEdit *m_apiKeyEdit;
    QLineEdit *m_headersEdit;
    QLineEdit *m_thinkBudgetEdit;
};
