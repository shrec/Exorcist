#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QList>
#include <QString>

class QLabel;
class QTimer;
class QLineEdit;
class QToolButton;

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
    void appendBuildLine(const QString &line, bool isError = false);
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

    // ── Line categorization & filtering ────────────────────────────────────
    enum LineCategory {
        CategoryBuild = 0,  // no bracketed prefix (build output, generic)
        CategoryDebug = 1,  // [DEBUG]
        CategoryGdb   = 2,  // [GDB] or [CMD]
    };

    // Toggle visibility of an individual category (re-renders panel).
    void setCategoryFilter(LineCategory cat, bool visible);
    bool categoryFilter(LineCategory cat) const;

    // Set substring filter (case-insensitive). Empty = no substring filter.
    void setSearchFilter(const QString &needle);

    // ── Line buffer record (public so internal helpers can reference it) ───
    struct Line {
        QString      text;
        QColor       color;        // invalid -> default fg
        LineCategory category = CategoryBuild;
        int          problemIdx = -1; // index into m_problems if any
    };

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

    // Filter helpers
    static LineCategory categorize(const QString &line);
    bool isLineVisible(const Line &ln) const;
    void rebuildVisibleText();
    void appendVisibleLine(const Line &ln);

    QPlainTextEdit *m_output;
    QComboBox      *m_profileCombo;
    QPushButton    *m_runBtn;
    QPushButton    *m_stopBtn;
    QPushButton    *m_clearBtn;
    QLabel         *m_elapsedLabel = nullptr;
    QProcess       *m_process = nullptr;
    QString         m_workDir;

    // Filter UI (in toolbar row)
    QToolButton    *m_filterBuildBtn = nullptr;
    QToolButton    *m_filterDebugBtn = nullptr;
    QToolButton    *m_filterGdbBtn   = nullptr;
    QToolButton    *m_filterAllBtn   = nullptr;
    QLineEdit      *m_filterEdit     = nullptr;

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

    // ── Line buffer for filtering ──────────────────────────────────────────
    // Every line appended to the panel is recorded here. The QPlainTextEdit
    // text is treated as a derived view that we rebuild from this buffer
    // whenever filters change.
    QList<Line> m_lines;

    // Active filter state (true = category visible)
    bool m_showBuild = true;
    bool m_showDebug = true;
    bool m_showGdb   = true;
    QString m_searchFilter; // case-insensitive substring; empty = no filter

    // Suppress rebuilds during bulk operations (e.g. setSearchFilter typing).
    bool m_suspendRender = false;
};
