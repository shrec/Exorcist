#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QObject>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// APIKeyManagerWidget — UI for managing API keys per provider.
//
// Provides add/remove/test functionality for API keys stored via
// SecureKeyStorage. Shows a list of providers with key status.
// ─────────────────────────────────────────────────────────────────────────────

class APIKeyManagerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit APIKeyManagerWidget(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);

        auto *title = new QLabel(tr("API Key Management"), this);
        title->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 14px;"));
        layout->addWidget(title);

        m_providerList = new QListWidget(this);
        layout->addWidget(m_providerList);

        // Provider input row
        auto *inputRow = new QHBoxLayout;
        m_providerEdit = new QLineEdit(this);
        m_providerEdit->setPlaceholderText(tr("Provider name (e.g. openai, anthropic)"));
        inputRow->addWidget(m_providerEdit);

        m_keyEdit = new QLineEdit(this);
        m_keyEdit->setPlaceholderText(tr("API key"));
        m_keyEdit->setEchoMode(QLineEdit::Password);
        inputRow->addWidget(m_keyEdit);
        layout->addLayout(inputRow);

        // Buttons
        auto *btnRow = new QHBoxLayout;
        auto *addBtn = new QPushButton(tr("Save Key"), this);
        connect(addBtn, &QPushButton::clicked, this, &APIKeyManagerWidget::saveKey);
        btnRow->addWidget(addBtn);

        auto *removeBtn = new QPushButton(tr("Remove Key"), this);
        connect(removeBtn, &QPushButton::clicked, this, &APIKeyManagerWidget::removeKey);
        btnRow->addWidget(removeBtn);

        auto *testBtn = new QPushButton(tr("Test Connection"), this);
        connect(testBtn, &QPushButton::clicked, this, &APIKeyManagerWidget::testConnection);
        btnRow->addWidget(testBtn);

        btnRow->addStretch();
        layout->addLayout(btnRow);

        m_statusLabel = new QLabel(this);
        layout->addWidget(m_statusLabel);
        layout->addStretch();
    }

    // Set the list of known providers with their key status
    void setProviders(const QStringList &providers, const QMap<QString, bool> &hasKey)
    {
        m_providerList->clear();
        for (const auto &p : providers) {
            const QString icon = hasKey.value(p, false)
                ? QStringLiteral("\U0001F512 ") : QStringLiteral("\U0001F513 ");
            auto *item = new QListWidgetItem(icon + p);
            item->setData(Qt::UserRole, p);
            m_providerList->addItem(item);
        }
    }

signals:
    void keySaved(const QString &provider, const QString &key);
    void keyRemoved(const QString &provider);
    void testRequested(const QString &provider);

private slots:
    void saveKey()
    {
        const QString provider = m_providerEdit->text().trimmed();
        const QString key = m_keyEdit->text().trimmed();
        if (provider.isEmpty() || key.isEmpty()) {
            m_statusLabel->setText(tr("Provider name and key are required"));
            return;
        }
        emit keySaved(provider, key);
        m_keyEdit->clear();
        m_statusLabel->setText(tr("Key saved for %1").arg(provider));
    }

    void removeKey()
    {
        auto *item = m_providerList->currentItem();
        if (!item) return;
        const QString provider = item->data(Qt::UserRole).toString();
        emit keyRemoved(provider);
        m_statusLabel->setText(tr("Key removed for %1").arg(provider));
    }

    void testConnection()
    {
        auto *item = m_providerList->currentItem();
        if (!item) return;
        const QString provider = item->data(Qt::UserRole).toString();
        emit testRequested(provider);
        m_statusLabel->setText(tr("Testing connection to %1...").arg(provider));
    }

private:
    QListWidget *m_providerList;
    QLineEdit *m_providerEdit;
    QLineEdit *m_keyEdit;
    QLabel *m_statusLabel;
};
