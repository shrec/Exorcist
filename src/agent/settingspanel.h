#pragma once

#include <QMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QSettings;
class QTabWidget;

/// Settings panel for AI / chat configuration.
class SettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPanel(QWidget *parent = nullptr);

    void loadSettings();
    void saveSettings();

    int contextTokenLimit() const;
    int maxSteps() const;
    QString reasoningEffortString() const;
    QStringList disabledCompletionLanguages() const;
    QString customEndpoint() const;
    QString customApiKey() const;
    QString completionModel() const;

    /// Populate the Tools tab with toggle checkboxes for each tool.
    void setToolNames(const QStringList &names);
    /// Returns list of tool names that the user has disabled.
    QStringList disabledTools() const;

signals:
    void settingsChanged();

private:
    void buildGeneralTab(QWidget *tab);
    void buildModelTab(QWidget *tab);
    void buildToolsTab(QWidget *tab);
    void buildContextTab(QWidget *tab);

    QTabWidget *m_tabs;

    // General
    QCheckBox *m_enableInlineChat;
    QCheckBox *m_enableGhostText;
    QCheckBox *m_enableMemory;
    QCheckBox *m_enableReview;
    QLineEdit *m_disabledLangs;

    // Model
    QSpinBox  *m_maxSteps;
    QSpinBox  *m_maxTokens;
    QComboBox *m_reasoningEffort;
    QLineEdit *m_customEndpoint;
    QLineEdit *m_customApiKey;
    QLineEdit *m_completionModel;

    // Context
    QCheckBox *m_includeOpenFiles;
    QCheckBox *m_includeTerminal;
    QCheckBox *m_includeDiagnostics;
    QCheckBox *m_includeGitDiff;
    QSpinBox  *m_contextTokenLimit;

    // Tools
    QWidget               *m_toolsContainer = nullptr;
    QMap<QString, QCheckBox *> m_toolChecks;
};
