#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>
#include <QElapsedTimer>

class QLabel;
class QTimer;

// ── TaskProfile ───────────────────────────────────────────────────────────────
// A single build/run task definition, serialisable to JSON.
struct TaskProfile
{
    QString label;                          // display name
    QString command;                        // program or shell command
    QStringList args;                       // arguments (may contain ${vars})
    QString workDir;                        // override working directory
    QMap<QString, QString> env;             // extra env vars
    bool isDefault = false;                 // auto-selected for Ctrl+Shift+B
};

class OutputPanel : public QWidget
{
    Q_OBJECT
public:
    explicit OutputPanel(QWidget *parent = nullptr);

    void setWorkingDirectory(const QString &dir);
    void runCommand(const QString &cmd, const QStringList &args = {});
    void runRemoteCommand(const QString &host, int port,
                          const QString &user, const QString &privateKeyPath,
                          const QString &remoteCmd,
                          const QString &remoteWorkDir = {});
    void appendText(const QString &text, const QColor &color = {});
    void clear();

    // Task profile management
    void setTaskProfiles(const QList<TaskProfile> &profiles);
    QList<TaskProfile> taskProfiles() const { return m_tasks; }
    void loadTasksFromJson(const QString &jsonPath);
    void saveTasksToJson(const QString &jsonPath) const;
    void autoDetectTasks();

    // Problem matcher types
    struct ProblemMatch {
        QString file;
        int     line   = 0;
        int     column = 0;
        enum Severity { Error, Warning, Info } severity = Error;
        QString message;
    };

    QList<ProblemMatch> problems() const { return m_problems; }

signals:
    void problemClicked(const QString &file, int line, int column);
    void buildFinished(int exitCode);
    void problemsChanged();

private slots:
    void onReadyReadStdout();
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onTextClicked();
    void onElapsedTick();

private:
    void parseLine(const QString &line);
    void setupMatchers();
    void populateCombo();
    QString substituteVars(const QString &input) const;
    void runTask(const TaskProfile &task);

    QPlainTextEdit *m_output;
    QComboBox      *m_profileCombo;
    QPushButton    *m_runBtn;
    QPushButton    *m_stopBtn;
    QPushButton    *m_clearBtn;
    QLabel         *m_elapsedLabel = nullptr;
    QProcess       *m_process = nullptr;
    QString         m_workDir;

    // Task profiles
    QList<TaskProfile> m_tasks;

    // Elapsed time tracking
    QElapsedTimer  m_elapsed;
    QTimer        *m_elapsedTimer = nullptr;

    struct Matcher {
        QRegularExpression regex;
        int fileGroup   = 1;
        int lineGroup   = 2;
        int columnGroup = 0;   // 0 = not captured
        int msgGroup    = 3;
        ProblemMatch::Severity severity = ProblemMatch::Error;
    };
    QList<Matcher> m_matchers;
    QList<ProblemMatch> m_problems;

    // Map output line number -> ProblemMatch index
    QHash<int, int> m_lineToProb;
};
