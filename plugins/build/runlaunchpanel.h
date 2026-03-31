#pragma once

#include <QWidget>
#include <QProcess>

class QComboBox;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QTimer;
class QElapsedTimer;

// ── LaunchProfile ─────────────────────────────────────────────────────────────
// A single run/debug target, serialisable to .exorcist/launch.json.

struct LaunchProfile
{
    QString name;                           // display name (e.g. "Run exorcist")
    QString program;                        // executable path (supports ${vars})
    QStringList args;                       // arguments
    QString workDir;                        // working directory
    QMap<QString, QString> env;             // extra env vars
    bool isDefault = false;
};

// ── RunLaunchPanel ────────────────────────────────────────────────────────────
// Simple run launcher panel — F5 to run, Shift+F5 to stop.
// Displays stdout/stderr output, supports launch.json profiles.

class RunLaunchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RunLaunchPanel(QWidget *parent = nullptr);

    void setWorkingDirectory(const QString &dir);
    void loadLaunchJson(const QString &jsonPath);
    void saveLaunchJson(const QString &jsonPath) const;
    void autoDetectTargets();

    void launch();                          // run selected profile
    void stopProcess();

    /// Append text to the output area (for external callers like IRunOutputService).
    void appendOutput(const QString &text, bool isError = false);

    QList<LaunchProfile> profiles() const { return m_profiles; }

signals:
    void processStarted(const QString &name);
    void processFinished(const QString &name, int exitCode);

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onElapsedTick();

private:
    void populateCombo();
    QString substituteVars(const QString &input) const;
    void runProfile(const LaunchProfile &profile);

    QComboBox      *m_profileCombo;
    QPushButton    *m_runBtn;
    QPushButton    *m_stopBtn;
    QPushButton    *m_clearBtn;
    QLabel         *m_statusLabel;
    QPlainTextEdit *m_output;
    QProcess       *m_process = nullptr;
    QString         m_workDir;

    QList<LaunchProfile> m_profiles;

    QElapsedTimer  *m_elapsed;
    QTimer         *m_elapsedTimer;
};
