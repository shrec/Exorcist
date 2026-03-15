#pragma once

#include <QMap>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;

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
    explicit APIKeyManagerWidget(QWidget *parent = nullptr);

    void setProviders(const QStringList &providers, const QMap<QString, bool> &hasKey);

signals:
    void keySaved(const QString &provider, const QString &key);
    void keyRemoved(const QString &provider);
    void testRequested(const QString &provider);

private slots:
    void saveKey();
    void removeKey();
    void testConnection();

private:
    QListWidget *m_providerList;
    QLineEdit *m_providerEdit;
    QLineEdit *m_keyEdit;
    QLabel *m_statusLabel;
};
