#pragma once

#include <QWidget>

class QComboBox;
class QToolButton;
class QLabel;
class CMakeIntegration;
class ToolchainManager;
class IDebugAdapter;

/// Unified toolbar for Build / Run / Debug workflows.
/// Provides configuration dropdowns, build/run/debug buttons,
/// and status display — similar to Visual Studio's main toolbar.
class BuildToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit BuildToolbar(QWidget *parent = nullptr);

    void setCMakeIntegration(CMakeIntegration *cmake);
    void setToolchainManager(ToolchainManager *mgr);
    void setDebugAdapter(IDebugAdapter *adapter);

    /// Refresh configuration and target combos from current project state.
    void refresh();

    /// Get the currently selected build config index.
    int selectedConfigIndex() const;

    /// Get the currently selected launch target path.
    QString selectedTarget() const;

signals:
    /// User clicked Configure (CMake configure).
    void configureRequested();

    /// User clicked Build (Ctrl+Shift+B).
    void buildRequested();

    /// User clicked Run (Ctrl+F5 — run without debugger).
    void runRequested(const QString &executable);

    /// User clicked Debug (F5 — run with debugger).
    void debugRequested(const QString &executable);

    /// User clicked Stop.
    void stopRequested();

    /// User clicked Clean.
    void cleanRequested();

    /// Build config selection changed.
    void configChanged(int index);

private slots:
    void onDebugStarted();
    void onDebugTerminated();

private:
    void setupUi();
    void updateButtonStates(bool building, bool debugging);

    CMakeIntegration *m_cmake = nullptr;
    ToolchainManager *m_toolchainMgr = nullptr;
    IDebugAdapter    *m_debugAdapter = nullptr;

    QComboBox   *m_configCombo;     // Debug / Release / preset
    QComboBox   *m_targetCombo;     // executable targets
    QToolButton *m_configureBtn;    // CMake configure
    QToolButton *m_buildBtn;        // Build
    QToolButton *m_runBtn;          // Run without debugging
    QToolButton *m_debugBtn;        // Debug (F5)
    QToolButton *m_stopBtn;         // Stop
    QToolButton *m_cleanBtn;        // Clean
    QLabel      *m_statusLabel;     // "Building..." / "Ready"
};
