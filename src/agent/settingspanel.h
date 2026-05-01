#pragma once

#include <QHash>
#include <QList>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;
class QSettings;
class QTabWidget;
class QTimer;

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
    void buildAppearanceTab(QWidget *tab);

    void buildScopeBar();
    void onScopeChanged();
    void refreshWorkspaceState();      // enable/disable workspace radio
    void refreshAllBadges();           // sync badges + reset buttons
    void showStatus(const QString &msg);

    // Per-key registry: lets us refresh badges/reset buttons after edits.
    struct Tracked {
        QString    key;        // flat key, e.g. "AI/maxSteps"
        QPointer<QWidget> editor;
        QPointer<QLabel>  badge;
        QPointer<QPushButton> reset;
    };

    // Register a control as bound to a flat settings key.
    // Wires up signal handlers so edits write to the active scope, and
    // creates a [User]/[Workspace] badge + reset button next to the row label.
    enum EditorKind { CheckBoxK, SpinBoxK, ComboBoxK, LineEditK };
    void trackControl(const QString &key, QWidget *editor, EditorKind kind,
                      QLabel *labelToBadge);
    // Wraps a checkbox so a badge + reset button sit next to it. Returns
    // the wrapper widget the caller should add to its layout.
    QWidget *wrapCheckBoxWithBadge(const QString &key, QCheckBox *cb);

    void applyValueToEditor(QWidget *editor, EditorKind kind, const QVariant &v);
    QVariant editorValue(QWidget *editor, EditorKind kind) const;
    void writeFromEditor(const QString &key, QWidget *editor, EditorKind kind);
    void updateBadge(const Tracked &t);

    QTabWidget *m_tabs;

    // Scope bar
    QWidget       *m_scopeBar       = nullptr;
    QRadioButton  *m_scopeUser      = nullptr;
    QRadioButton  *m_scopeWorkspace = nullptr;
    QLabel        *m_scopeStatus    = nullptr;
    QTimer        *m_statusTimer    = nullptr;

    // Tracked editors: key -> info (for badge refresh after scope changes).
    QList<Tracked>          m_tracked;
    QHash<QWidget *, int>   m_editorIdx; // editor pointer -> m_tracked index
    QHash<QString, EditorKind> m_kindByKey;
    bool m_loading = false;            // suppress write-back during loadSettings()

    // General
    QCheckBox *m_enableInlineChat;
    QCheckBox *m_enableGhostText;
    QCheckBox *m_enableMemory;
    QCheckBox *m_enableReview;
    QLineEdit *m_disabledLangs;

    // Appearance & Editor
    QCheckBox *m_darkTheme       = nullptr;
    QCheckBox *m_autoSaveEnabled = nullptr;
    QSpinBox  *m_autoSaveInterval = nullptr;

    // Model
    QSpinBox  *m_maxSteps;
    QSpinBox  *m_maxTokens;
    QComboBox *m_reasoningEffort;
    QLineEdit *m_customEndpoint;
    QLineEdit *m_customApiKey;
    QLineEdit *m_completionModel;
    QLineEdit *m_searxngUrl;

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
