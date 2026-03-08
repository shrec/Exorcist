# Debug Subsystem

Exorcist-ის debug subsystem მოდულარული debug adapter framework-ია, რომელიც
GDB/MI, LLDB და DAP backend-ებისთვისაა დაპროექტებული. ყველა კომუნიკაცია
ასინქრონულია — UI არასოდეს იბლოკება debugger-ის პასუხის მოლოდინში.

## Directory: `src/debug/`

| File | Purpose |
|------|---------|
| `idebugadapter.h` | Abstract adapter interface + shared data types |
| `gdbmiadapter.h/.cpp` | GDB/MI protocol implementation via QProcess |
| `debugpanel.h/.cpp` | Main debug UI panel (toolbar + tabs) |

## Data Types (`idebugadapter.h`)

| Type | Fields | Purpose |
|------|--------|---------|
| `DebugBreakpoint` | id, filePath, line, condition, enabled, verified | Breakpoint descriptor |
| `DebugFrame` | id, name, filePath, line, column | Stack frame |
| `DebugVariable` | name, value, type, variablesReference, children | Variable tree node |
| `DebugThread` | id, name, stopped | Thread descriptor |
| `DebugStopReason` | Breakpoint, Step, Exception, Pause, Entry, Unknown | Why the debugger stopped |

## IDebugAdapter Interface

Abstract QObject base class. All methods are asynchronous — results delivered via signals.

### Lifecycle
```cpp
virtual void launch(const QString &executable,
                    const QStringList &args = {},
                    const QString &workingDir = {},
                    const QJsonObject &env = {}) = 0;
virtual void attach(int pid) = 0;
virtual void terminate() = 0;
virtual bool isRunning() const = 0;
```

### Execution Control
```cpp
virtual void continueExecution(int threadId = 0) = 0;
virtual void stepOver(int threadId = 0) = 0;
virtual void stepInto(int threadId = 0) = 0;
virtual void stepOut(int threadId = 0) = 0;
virtual void pause(int threadId = 0) = 0;
```

### Breakpoints
```cpp
virtual void addBreakpoint(const DebugBreakpoint &bp) = 0;
virtual void removeBreakpoint(int breakpointId) = 0;
virtual void removeAllBreakpoints() = 0;
```

### Inspection
```cpp
virtual void requestThreads() = 0;
virtual void requestStackTrace(int threadId) = 0;
virtual void requestScopes(int frameId) = 0;
virtual void requestVariables(int variablesReference) = 0;
virtual void evaluate(const QString &expression, int frameId = 0) = 0;
```

### Signals
| Signal | Parameters | When |
|--------|-----------|------|
| `started()` | — | Debugger session begins |
| `terminated()` | — | Session ends |
| `error(message)` | `QString` | Fatal error |
| `stopped(threadId, reason, description)` | `int`, `DebugStopReason`, `QString` | Hit breakpoint / step / exception |
| `continued(threadId)` | `int` | Execution resumed |
| `breakpointSet(bp)` | `DebugBreakpoint` | Breakpoint confirmed by backend |
| `breakpointRemoved(id)` | `int` | Breakpoint cleared |
| `threadsReceived(threads)` | `QList<DebugThread>` | Thread list ready |
| `stackTraceReceived(threadId, frames)` | `int`, `QList<DebugFrame>` | Stack frames ready |
| `variablesReceived(ref, vars)` | `int`, `QList<DebugVariable>` | Variable values ready |
| `evaluateResult(expr, result)` | `QString`, `QString` | Watch expression result |
| `outputProduced(text, category)` | `QString`, `QString` | Debugger stdout/stderr |

## GdbMiAdapter

QProcess-based implementation of `IDebugAdapter` using the GDB/MI protocol.

### Protocol
- Commands sent as `<token>-<mi-command> <args>\n`
- Token is auto-incrementing integer for matching results
- Line-buffered output parsing via `readyReadStandardOutput`

### MI Output Categories
| Prefix | Meaning | Handler |
|--------|---------|---------|
| `~` | Console output | → `outputProduced` |
| `@` | Target output | → `outputProduced` |
| `&` | Log output | (ignored) |
| `*` | Async exec records (`*stopped`, `*running`) | → `handleStopped` / `continued` |
| `=` | Async notify records | (ignored) |
| `^` | Result records (`^done`, `^error`) | → dispatch by token |

### Key Implementation Details
- `parseMiBody()` — recursive parser for GDB/MI key=value pairs with nested `{}` and `[]`
- `m_pending` — `QHash<int, QString>` maps token → command name for result dispatch
- `m_pendingBps` — `QHash<int, DebugBreakpoint>` tracks breakpoints pending confirmation
- Configurable debugger path via `setDebuggerPath()` (default: `"gdb"`)

## DebugPanel UI

Main debug panel containing toolbar + tabbed sub-widgets.

### Toolbar Actions
| Action | Shortcut | Effect |
|--------|----------|--------|
| Launch | — | Start debug session (`launchRequested` signal) |
| Continue | — | `adapter->continueExecution()` |
| Step Over | — | `adapter->stepOver()` |
| Step Into | — | `adapter->stepInto()` |
| Step Out | — | `adapter->stepOut()` |
| Pause | — | `adapter->pause()` |
| Stop | — | `adapter->terminate()` |

### Tabs
| Tab | Widget | Content |
|-----|--------|---------|
| Call Stack | `QTableWidget` | Function, File, Line columns. Double-click → `navigateToSource` |
| Locals | `QTreeWidget` | Name, Value, Type columns. Expandable for nested structs |
| Breakpoints | `QTableWidget` | File, Line columns. External add/remove via slots |
| Watch | `QLineEdit` + `QPlainTextEdit` | Type expression → evaluate → display result |
| Output | `QPlainTextEdit` | Raw debugger output stream |

### Integration Points
- `navigateToSource(filePath, line)` — opens file in editor at debugged location
- `addBreakpointEntry(filePath, line)` / `removeBreakpointEntry(filePath, line)` — called from editor gutter
- `launchRequested()` — wired in MainWindow to configured launch profile

## Editor Integration

`EditorView` has built-in breakpoint gutter support:

- **Click to toggle**: Click in line number area → toggle breakpoint
- **Red circle**: Filled 10px red circle painted in gutter for active breakpoints
- **Debug line**: Yellow arrow indicator for current execution position
- **Signal**: `breakpointToggled(filePath, line, added)` → wired to DebugPanel + GdbMiAdapter

## Extension Points

New debug adapters (LLDB, DAP) should:
1. Create a new class inheriting `IDebugAdapter`
2. Implement all pure virtual methods
3. Wire it in MainWindow or make it a plugin-provided adapter

Future work:
- Debug Adapter Protocol (DAP) for language-agnostic debugging
- Conditional breakpoints UI
- Data breakpoints (watchpoints)
- Multi-session debugging
