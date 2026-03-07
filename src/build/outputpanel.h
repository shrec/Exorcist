#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>

class OutputPanel : public QWidget
{
    Q_OBJECT
public:
    explicit OutputPanel(QWidget *parent = nullptr);

    void setWorkingDirectory(const QString &dir);
    void runCommand(const QString &cmd, const QStringList &args = {});
    void appendText(const QString &text, const QColor &color = {});
    void clear();

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

private:
    void parseLine(const QString &line);
    void setupMatchers();

    QPlainTextEdit *m_output;
    QComboBox      *m_profileCombo;
    QPushButton    *m_runBtn;
    QPushButton    *m_stopBtn;
    QPushButton    *m_clearBtn;
    QProcess       *m_process = nullptr;
    QString         m_workDir;

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
