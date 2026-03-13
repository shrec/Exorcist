#pragma once

#include "sdk/idebugadapter.h"

#include <QProcess>
#include <QQueue>
#include <QHash>

/// Debug adapter that speaks the GDB/MI protocol via QProcess.
///
/// Works with GDB (and compatible: lldb-mi). Sends MI commands,
/// parses async/result records, and emits the IDebugAdapter signals.
class GdbMiAdapter : public IDebugAdapter
{
    Q_OBJECT

public:
    explicit GdbMiAdapter(QObject *parent = nullptr);
    ~GdbMiAdapter() override;

    /// Path to the gdb/lldb-mi binary (defaults to "gdb").
    void setDebuggerPath(const QString &path) override { m_debuggerPath = path; }
    QString debuggerPath() const { return m_debuggerPath; }

    // ── IDebugAdapter interface ──────────────────────────────────────────

    void launch(const QString &executable,
                const QStringList &args = {},
                const QString &workingDir = {},
                const QJsonObject &env = {}) override;
    void attach(int pid) override;
    void terminate() override;
    bool isRunning() const override;

    void continueExecution(int threadId = 0) override;
    void stepOver(int threadId = 0) override;
    void stepInto(int threadId = 0) override;
    void stepOut(int threadId = 0) override;
    void pause(int threadId = 0) override;

    void addBreakpoint(const DebugBreakpoint &bp) override;
    void removeBreakpoint(int breakpointId) override;
    void removeAllBreakpoints() override;

    void requestThreads() override;
    void requestStackTrace(int threadId) override;
    void requestScopes(int frameId) override;
    void requestVariables(int variablesReference) override;
    void evaluate(const QString &expression, int frameId = 0) override;

    // ── Variable Objects ─────────────────────────────────────────────────
    void createVarObject(const QString &expression, int frameId = 0) override;
    void listVarChildren(const QString &varName, int from = 0, int count = -1) override;
    void updateVarObjects() override;
    void deleteVarObject(const QString &varName) override;
    void assignVarValue(const QString &varName, const QString &newValue) override;

private slots:
    void onReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError err);

private:
    /// Send an MI command and return its token number.
    int sendCommand(const QString &miCommand);

    /// Parse one GDB/MI output line.
    void parseMiLine(const QString &line);

    /// Parse an MI result/async record body into key-value pairs.
    static QHash<QString, QString> parseMiBody(const QString &body);

    /// Parse a stack frame from MI output (e.g. from *stopped).
    DebugFrame parseFrame(const QString &frameStr) const;

    /// Handle a *stopped async record.
    void handleStopped(const QHash<QString, QString> &attrs);

    /// Handle result records from specific commands.
    void handleBreakInsertResult(int token, const QHash<QString, QString> &attrs);
    void handleStackListResult(int token, const QHash<QString, QString> &attrs);
    void handleThreadInfoResult(int token, const QHash<QString, QString> &attrs);
    void handleVarResult(int token, const QHash<QString, QString> &attrs);
    void handleEvaluateResult(int token, const QHash<QString, QString> &attrs);

    // Variable Object result handlers
    void handleVarCreateResult(int token, const QHash<QString, QString> &attrs);
    void handleVarListChildrenResult(int token, const QHash<QString, QString> &attrs);
    void handleVarUpdateResult(int token, const QHash<QString, QString> &attrs);
    void handleVarDeleteResult(int token, const QHash<QString, QString> &attrs);
    void handleVarAssignResult(int token, const QHash<QString, QString> &attrs);

    /// Parse a list of changelist entries from -var-update output.
    QList<DebugVarChange> parseChangeList(const QString &raw);

    /// Parse child entries from -var-list-children output.
    QList<DebugVarObj> parseChildList(const QString &raw);

    QProcess  m_process;
    QString   m_debuggerPath = QStringLiteral("gdb");
    int       m_nextToken = 1;
    QString   m_lineBuffer;
    bool      m_launched = false;

    // Track pending commands by token
    struct PendingCmd {
        QString command;
        int     token;
    };
    QHash<int, PendingCmd> m_pending;

    // Pending breakpoints: adapter-ID is unknown until GDB confirms.
    // Map token → original breakpoint request.
    QHash<int, DebugBreakpoint> m_pendingBps;

    // Active breakpoints: GDB id → DebugBreakpoint
    QHash<int, DebugBreakpoint> m_breakpoints;

    // For requestStackTrace: token → threadId
    QHash<int, int> m_stackTraceRequests;

    // Variable Object tracking: token → command type
    enum class VarObjCmd { Create, ListChildren, Update, Delete, Assign };
    QHash<int, VarObjCmd>  m_varObjRequests;

    // For create: token → original expression
    QHash<int, QString>    m_varCreateExprs;

    // For list-children: token → parent var name
    QHash<int, QString>    m_varListParents;

    // For delete: token → var name
    QHash<int, QString>    m_varDeleteNames;

    // For assign: token → var name
    QHash<int, QString>    m_varAssignNames;

    // Next unique var-object name counter
    int m_nextVarId = 1;
};
