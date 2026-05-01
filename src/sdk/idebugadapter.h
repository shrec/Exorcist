#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonObject>
#include <QByteArray>

// ── Debug data types ──────────────────────────────────────────────────────────

struct DebugBreakpoint
{
    int     id = -1;            // adapter-assigned ID (-1 = unset)
    QString filePath;
    int     line = 0;           // 1-based
    QString condition;          // optional conditional expression
    int     hitCount = 0;       // 0 = always break; N = break after N hits
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

/// Data breakpoint — fires when a memory location's value changes
/// (write), is read, or is accessed (read or write).
struct DebugWatchpoint
{
    int     id = -1;            // adapter-assigned ID (-1 = unset)
    QString expression;         // expression to watch (e.g. "myVar", "*p", "arr[3]")
    enum Type { Write, Read, Access } type = Write;
    bool    enabled = true;
    bool    verified = false;   // confirmed by debugger backend
};

/// One disassembled instruction. Returned in lists by
/// `IDebugAdapter::disassemblyReceived`.
struct DisasmLine
{
    quint64 address = 0;        // absolute instruction address
    QString function;           // containing function name (optional)
    int     offset  = 0;        // offset within `function`
    QString instruction;        // raw assembly text (mnemonic + operands)
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

    /// Set the path to the debugger executable (e.g. gdb, lldb).
    virtual void setDebuggerPath(const QString &path) { Q_UNUSED(path); }

    // ── Execution control ─────────────────────────────────────────────────

    virtual void continueExecution(int threadId = 0) = 0;
    virtual void stepOver(int threadId = 0) = 0;
    virtual void stepInto(int threadId = 0) = 0;
    virtual void stepOut(int threadId = 0) = 0;
    virtual void pause(int threadId = 0) = 0;

    // ── Reverse execution (record/replay) ─────────────────────────────────
    //
    // Default no-op implementations — not all adapters support reverse
    // debugging. Adapters that do (e.g. GdbMiAdapter via GDB's `record full`
    // and --reverse flag) override these. Once recording is started, the
    // reverse* methods step backwards through the recorded execution.

    /// Start recording execution for reverse debugging.
    virtual void startRecording() {}

    /// Stop recording.
    virtual void stopRecording() {}

    /// Step backwards over the previous source line.
    virtual void reverseStepOver(int threadId = 0) { Q_UNUSED(threadId) }

    /// Step backwards into the previous call.
    virtual void reverseStepInto(int threadId = 0) { Q_UNUSED(threadId) }

    /// Continue execution backwards until previous breakpoint or start.
    virtual void reverseContinue(int threadId = 0) { Q_UNUSED(threadId) }

    // ── Breakpoints ───────────────────────────────────────────────────────

    virtual void addBreakpoint(const DebugBreakpoint &bp) = 0;
    virtual void removeBreakpoint(int breakpointId) = 0;
    virtual void removeAllBreakpoints() = 0;

    // ── Watchpoints (data breakpoints) ────────────────────────────────────
    //
    // Default no-op implementations — not all adapters support data
    // breakpoints. Adapters that do (e.g. GdbMiAdapter via -break-watch)
    // override these and emit watchpointSet / watchpointRemoved.

    virtual void addWatchpoint(const DebugWatchpoint &wp) { Q_UNUSED(wp); }
    virtual void removeWatchpoint(int watchpointId) { Q_UNUSED(watchpointId); }

    // ── Inspection ────────────────────────────────────────────────────────

    virtual void requestThreads() = 0;
    virtual void requestStackTrace(int threadId) = 0;
    virtual void requestScopes(int frameId) = 0;
    virtual void requestVariables(int variablesReference) = 0;
    virtual void evaluate(const QString &expression, int frameId = 0) = 0;

    /// Switch the debugger's current stack frame. Subsequent inspection
    /// commands (-stack-list-locals, -data-evaluate-expression, etc.) will
    /// operate on this frame until another frame is selected.
    virtual void stackSelectFrame(int frameId) = 0;

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

    // ── Memory inspection ────────────────────────────────────────────────
    //
    // Default no-op implementation — adapters that support raw memory reads
    // (e.g. GdbMiAdapter via -data-read-memory-bytes) override and emit
    // memoryReceived when the read completes.

    /// Read raw memory bytes at the given address.
    virtual void readMemory(quint64 addr, int count) { Q_UNUSED(addr) Q_UNUSED(count) }

    // ── Disassembly ──────────────────────────────────────────────────────
    //
    // Default no-op implementation — adapters that support raw disassembly
    // (e.g. GdbMiAdapter via -data-disassemble) override and emit
    // `disassemblyReceived` when the request completes.
    //
    // `mode` matches GDB/MI's -data-disassemble mode flag:
    //   0 = plain assembly only
    //   1 = mixed source + assembly (deprecated 5; mode 1 is the typical UI choice)

    /// Disassemble a memory range. Adapters compute an upper bound from
    /// `instructionCount` (heuristic: ~16 bytes/instr for x86-64).
    virtual void disassemble(quint64 startAddr, int instructionCount, int mode)
    {
        Q_UNUSED(startAddr) Q_UNUSED(instructionCount) Q_UNUSED(mode)
    }

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

    // ── Watchpoint signals ────────────────────────────────────────────────

    void watchpointSet(const DebugWatchpoint &wp);
    void watchpointRemoved(int watchpointId);

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

    // ── Memory inspection signals ─────────────────────────────────────────

    /// Emitted when readMemory completes.
    void memoryReceived(quint64 addr, const QByteArray &bytes);

    // ── Disassembly signals ──────────────────────────────────────────────

    /// Emitted when disassemble() completes.
    void disassemblyReceived(const QList<DisasmLine> &lines);
};

Q_DECLARE_METATYPE(DisasmLine)
Q_DECLARE_METATYPE(QList<DisasmLine>)
