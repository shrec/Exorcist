#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>

// ── Debug data types ──────────────────────────────────────────────────────────

struct DebugBreakpoint
{
    int     id = -1;            // adapter-assigned ID (-1 = unset)
    QString filePath;
    int     line = 0;           // 1-based
    QString condition;          // optional conditional expression
    bool    enabled = true;
    bool    verified = false;   // confirmed by debugger backend
};

struct DebugFrame
{
    int     id = 0;             // frame index (0 = top)
    QString name;               // function name
    QString filePath;
    int     line = 0;
    int     column = 0;
};

struct DebugVariable
{
    QString name;
    QString value;
    QString type;
    int     variablesReference = 0; // >0 means expandable
    QList<DebugVariable> children;
};

/// Represents a GDB/MI Variable Object for lazy tree expansion.
struct DebugVarObj
{
    QString varName;        // GDB/MI var-obj name (e.g. "var1", "var1.member")
    QString expression;     // original expression (e.g. "myStruct", "myStruct.x")
    QString value;
    QString type;
    int     numChildren = 0;
    bool    hasMore = false;
    bool    changed = false;
};

/// Change entry from -var-update.
struct DebugVarChange
{
    QString varName;
    QString newValue;
    bool    inScope = true;
    bool    typeChanged = false;
    int     newNumChildren = -1;
};

struct DebugThread
{
    int     id = 0;
    QString name;
    bool    stopped = false;
};

// ── Stop reason ───────────────────────────────────────────────────────────────

enum class DebugStopReason
{
    Breakpoint,
    Step,
    Exception,
    Pause,
    Entry,
    Unknown
};

// ── IDebugAdapter ─────────────────────────────────────────────────────────────

/// Abstract interface for debug adapters (GDB/MI, LLDB, DAP, etc.).
///
/// All methods that communicate with the debugger are asynchronous.
/// Results are delivered via signals.
class IDebugAdapter : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    virtual ~IDebugAdapter() = default;

    // ── Lifecycle ─────────────────────────────────────────────────────────

    /// Launch the target program under the debugger.
    virtual void launch(const QString &executable,
                        const QStringList &args = {},
                        const QString &workingDir = {},
                        const QJsonObject &env = {}) = 0;

    /// Attach to a running process.
    virtual void attach(int pid) = 0;

    /// Disconnect / terminate the debug session.
    virtual void terminate() = 0;

    virtual bool isRunning() const = 0;

    // ── Execution control ─────────────────────────────────────────────────

    virtual void continueExecution(int threadId = 0) = 0;
    virtual void stepOver(int threadId = 0) = 0;
    virtual void stepInto(int threadId = 0) = 0;
    virtual void stepOut(int threadId = 0) = 0;
    virtual void pause(int threadId = 0) = 0;

    // ── Breakpoints ───────────────────────────────────────────────────────

    virtual void addBreakpoint(const DebugBreakpoint &bp) = 0;
    virtual void removeBreakpoint(int breakpointId) = 0;
    virtual void removeAllBreakpoints() = 0;

    // ── Inspection ────────────────────────────────────────────────────────

    virtual void requestThreads() = 0;
    virtual void requestStackTrace(int threadId) = 0;
    virtual void requestScopes(int frameId) = 0;
    virtual void requestVariables(int variablesReference) = 0;
    virtual void evaluate(const QString &expression, int frameId = 0) = 0;

    // ── Variable Objects (lazy tree expansion) ────────────────────────────

    /// Create a variable object for an expression in the given frame.
    virtual void createVarObject(const QString &expression, int frameId = 0) = 0;

    /// List children of a variable object (for tree expansion).
    virtual void listVarChildren(const QString &varName, int from = 0, int count = -1) = 0;

    /// Update all variable objects (call after step/continue to detect changes).
    virtual void updateVarObjects() = 0;

    /// Delete a variable object (call when collapsing tree or removing watch).
    virtual void deleteVarObject(const QString &varName) = 0;

    /// Assign a new value to a variable object.
    virtual void assignVarValue(const QString &varName, const QString &newValue) = 0;

signals:
    // ── Lifecycle signals ─────────────────────────────────────────────────

    void started();
    void terminated();
    void error(const QString &message);
    void outputProduced(const QString &text, const QString &category);

    // ── Stop / continue ───────────────────────────────────────────────────

    void stopped(int threadId, DebugStopReason reason,
                 const QString &description);
    void continued(int threadId);

    // ── Breakpoint signals ────────────────────────────────────────────────

    void breakpointSet(const DebugBreakpoint &bp);
    void breakpointRemoved(int breakpointId);

    // ── Data signals ──────────────────────────────────────────────────────

    void threadsReceived(const QList<DebugThread> &threads);
    void stackTraceReceived(int threadId, const QList<DebugFrame> &frames);
    void variablesReceived(int reference, const QList<DebugVariable> &vars);
    void evaluateResult(const QString &expression, const QString &result);

    // ── Variable Object signals ───────────────────────────────────────────

    /// A variable object was created (response to createVarObject).
    void varObjectCreated(const DebugVarObj &varObj);

    /// Children of a variable object were listed (response to listVarChildren).
    void varChildrenListed(const QString &parentVarName,
                           const QList<DebugVarObj> &children);

    /// Variable objects were updated after step/continue.
    void varObjectsUpdated(const QList<DebugVarChange> &changes);

    /// A variable object was deleted.
    void varObjectDeleted(const QString &varName);

    /// Assign succeeded, new value available.
    void varValueAssigned(const QString &varName, const QString &newValue);

    /// An error occurred in a variable object operation.
    void varObjectError(const QString &expression, const QString &message);
};
