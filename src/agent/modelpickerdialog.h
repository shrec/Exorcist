#pragma once

#include <QDialog>

class QComboBox;
class QLabel;
class QListWidget;
class AgentOrchestrator;

// ── ModelPickerDialog ─────────────────────────────────────────────────────────
// A dialog for selecting the active AI provider and model.
// Shows all registered providers and their available models.

class ModelPickerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ModelPickerDialog(AgentOrchestrator *orchestrator,
                               QWidget *parent = nullptr);

    QString selectedProviderId() const { return m_selectedProviderId; }
    QString selectedModel() const { return m_selectedModel; }

private:
    void populateModels();

    AgentOrchestrator *m_orchestrator;
    QComboBox         *m_providerCombo;
    QListWidget       *m_modelList;
    QLabel            *m_infoLabel;

    QString m_selectedProviderId;
    QString m_selectedModel;
};
