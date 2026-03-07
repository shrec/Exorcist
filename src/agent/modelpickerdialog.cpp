#include "modelpickerdialog.h"
#include "agentorchestrator.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

ModelPickerDialog::ModelPickerDialog(AgentOrchestrator *orchestrator,
                                     QWidget *parent)
    : QDialog(parent),
      m_orchestrator(orchestrator),
      m_providerCombo(new QComboBox(this)),
      m_modelList(new QListWidget(this)),
      m_infoLabel(new QLabel(this))
{
    setWindowTitle(tr("Select Model"));
    setMinimumSize(400, 350);

    auto *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(tr("Provider:"), this));
    layout->addWidget(m_providerCombo);

    layout->addWidget(new QLabel(tr("Model:"), this));
    layout->addWidget(m_modelList);

    m_infoLabel->setWordWrap(true);
    m_infoLabel->setStyleSheet(QStringLiteral(
        "color: #888; font-size: 11px; padding: 4px;"));
    layout->addWidget(m_infoLabel);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Populate providers
    const auto providers = orchestrator->providers();
    for (auto *prov : providers) {
        m_providerCombo->addItem(prov->displayName(), prov->id());
    }

    // Pre-select active provider
    if (auto *active = orchestrator->activeProvider()) {
        const int idx = m_providerCombo->findData(active->id());
        if (idx >= 0)
            m_providerCombo->setCurrentIndex(idx);
    }

    connect(m_providerCombo, &QComboBox::currentIndexChanged,
            this, [this]() { populateModels(); });

    connect(m_modelList, &QListWidget::currentItemChanged,
            this, [this](QListWidgetItem *current) {
        if (!current) return;
        m_selectedModel = current->data(Qt::UserRole).toString();

        // Show model info if available
        const QString provId = m_providerCombo->currentData().toString();
        auto *prov = m_orchestrator->provider(provId);
        if (!prov) return;

        const auto infos = prov->modelInfoList();
        for (const auto &info : infos) {
            if (info.id == m_selectedModel) {
                QStringList caps;
                if (info.capabilities.toolCalls) caps << tr("Tool Calls");
                if (info.capabilities.vision)    caps << tr("Vision");
                if (info.capabilities.thinking)  caps << tr("Thinking");
                if (info.capabilities.streaming) caps << tr("Streaming");

                QString text = info.name;
                if (!info.vendor.isEmpty())
                    text += QStringLiteral(" — ") + info.vendor;
                if (!caps.isEmpty())
                    text += QStringLiteral("\n") + tr("Capabilities: %1").arg(caps.join(QStringLiteral(", ")));
                if (info.capabilities.maxContextWindowTokens > 0)
                    text += QStringLiteral("\n") + tr("Context: %1 tokens").arg(info.capabilities.maxContextWindowTokens);
                if (info.billing.isPremium)
                    text += QStringLiteral("\n") + tr("Premium model (%1x)").arg(info.billing.multiplier);

                m_infoLabel->setText(text);
                return;
            }
        }
        m_infoLabel->setText(m_selectedModel);
    });

    populateModels();
}

void ModelPickerDialog::populateModels()
{
    m_modelList->clear();
    m_infoLabel->clear();

    const QString provId = m_providerCombo->currentData().toString();
    m_selectedProviderId = provId;

    auto *prov = m_orchestrator->provider(provId);
    if (!prov) return;

    const QStringList models = prov->availableModels();
    const QString current = prov->currentModel();

    for (const QString &model : models) {
        auto *item = new QListWidgetItem(model, m_modelList);
        item->setData(Qt::UserRole, model);
        if (model == current) {
            item->setText(model + tr(" (current)"));
            QFont f = item->font();
            f.setBold(true);
            item->setFont(f);
        }
    }

    // Pre-select current model
    for (int i = 0; i < m_modelList->count(); ++i) {
        if (m_modelList->item(i)->data(Qt::UserRole).toString() == current) {
            m_modelList->setCurrentRow(i);
            break;
        }
    }

    m_selectedModel = current;
}
