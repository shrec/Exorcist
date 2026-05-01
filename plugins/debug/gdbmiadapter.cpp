#include "gdbmiadapter.h"

#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>

GdbMiAdapter::GdbMiAdapter(QObject *parent)
    : IDebugAdapter(parent)
{
    connect(&m_process, &QProcess::readyReadStandardOutput,
            this, &GdbMiAdapter::onReadyRead);
    connect(&m_process, &QProcess::readyReadStandardError,
            this, [this]() {
        const QString text = QString::fromUtf8(m_process.readAllStandardError());
        emit outputProduced(text, QStringLiteral("stderr"));
    });
    connect(&m_process, &QProcess::started,
            this, &GdbMiAdapter::onProcessStarted);
    connect(&m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GdbMiAdapter::onProcessFinished);
    connect(&m_process, &QProcess::errorOccurred,
            this, &GdbMiAdapter::onProcessError);
}

GdbMiAdapter::~GdbMiAdapter()
{
    if (m_process.state() != QProcess::NotRunning) {
        m_process.kill();
        m_process.waitForFinished(500);
    }
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void GdbMiAdapter::launch(const QString &executable,
                           const QStringList &args,
                           const QString &workingDir,
                           const QJsonObject &env)
{
    emit outputProduced(QStringLiteral("[GDB] launch() called, debugger=%1").arg(m_debuggerPath),
                        QStringLiteral("debug"));

    if (m_process.state() != QProcess::NotRunning) {
        emit outputProduced(QStringLiteral("[GDB] ERROR: already running, state=%1")
                                .arg(int(m_process.state())),
                            QStringLiteral("debug"));
        emit error(tr("Debugger already running"));
        return;
    }

    QStringList gdbArgs;
    gdbArgs << QStringLiteral("--interpreter=mi2")
            << QStringLiteral("--quiet");

    m_process.setProgram(m_debuggerPath);
    m_process.setArguments(gdbArgs);

    if (!workingDir.isEmpty())
        m_process.setWorkingDirectory(workingDir);

    if (!env.isEmpty()) {
        auto sysEnv = QProcessEnvironment::systemEnvironment();
        for (auto it = env.begin(); it != env.end(); ++it)
            sysEnv.insert(it.key(), it.value().toString());
        m_process.setProcessEnvironment(sysEnv);
    }

    // Store launch parameters — consumed by onProcessStarted() when GDB is ready.
    m_pendingExe  = executable;
    m_pendingArgs = args;
    m_launched    = false;

    emit outputProduced(QStringLiteral("[GDB] starting process (async)..."), QStringLiteral("debug"));
    m_process.start();
    // Returns immediately — onProcessStarted() fires when GDB is ready.
    // onProcessError() fires if GDB fails to start (FailedToStart).
}

// ── Called by QProcess::started signal (non-blocking path) ──────────────────

void GdbMiAdapter::onProcessStarted()
{
    emit outputProduced(QStringLiteral("[GDB] started OK, pid=%1").arg(m_process.processId()),
                        QStringLiteral("debug"));

    // ── Pretty-printer / value-formatting setup ──────────────────────────
    // Configure GDB so STL types (std::string, std::vector, std::map, ...) and
    // user types render as human-readable values rather than raw struct dumps.
    //
    //   -gdb-set print pretty on        Multi-line, indented struct/array output
    //   -gdb-set print object on        Show actual derived type for polymorphic ptrs/refs
    //   -gdb-set print static-members off  Hide static members in struct dumps (cleaner)
    //   -gdb-set print elements 200     Cap printed array/string length at 200 elements
    //   -gdb-set auto-load safe-path /  Permit loading libstdc++ Python printer scripts
    //                                   that ship next to the runtime libraries
    //   -enable-pretty-printing         Tell the GDB/MI variable-object subsystem to
    //                                   actually invoke registered pretty-printers when
    //                                   formatting values for -var-create, -var-update,
    //                                   and -data-evaluate-expression results
    //
    // These must be sent before breakpoints / -exec-run so that subsequent value
    // queries (locals, watches, evaluate) come back already pretty-formatted.
    sendCommand(QStringLiteral("-gdb-set print pretty on"));
    sendCommand(QStringLiteral("-gdb-set print object on"));
    sendCommand(QStringLiteral("-gdb-set print static-members off"));
    sendCommand(QStringLiteral("-gdb-set print elements 200"));
    sendCommand(QStringLiteral("-gdb-set auto-load safe-path /"));
    sendCommand(QStringLiteral("-enable-pretty-printing"));

    // ── Attach flow ───────────────────────────────────────────────────────
    if (m_pendingExe == QLatin1String("__attach__")) {
        const int pid = m_pendingArgs.value(0).toInt();
        sendCommand(QStringLiteral("-target-attach %1").arg(pid));
        m_launched = true;
        emit started();
        return;
    }

    // ── Launch flow ───────────────────────────────────────────────────────

    // GDB requires forward slashes (backslashes cause parse errors on Windows)
    const QString exePath = QString(m_pendingExe).replace(QLatin1Char('\\'), QLatin1Char('/'));

    // Set the inferior (program to debug)
    sendCommand(QStringLiteral("-file-exec-and-symbols \"%1\"").arg(exePath));

    // Set inferior arguments
    if (!m_pendingArgs.isEmpty()) {
        QString argStr;
        for (const auto &a : m_pendingArgs) {
            if (!argStr.isEmpty()) argStr += QLatin1Char(' ');
            argStr += QLatin1Char('"') + a + QLatin1Char('"');
        }
        sendCommand(QStringLiteral("-exec-arguments %1").arg(argStr));
    }

    // Flush any breakpoints set before launch
    emit outputProduced(QStringLiteral("[DEBUG] prelaunch BPs: %1").arg(m_prelaunchBreakpoints.size()),
                        QStringLiteral("debug"));
    for (const DebugBreakpoint &bp : m_prelaunchBreakpoints) {
        QString bpPath = QString(bp.filePath).replace(QLatin1Char('\\'), QLatin1Char('/'));
        QString cmd = QStringLiteral("-break-insert ");
        if (!bp.condition.isEmpty())
            cmd += QStringLiteral("-c \"%1\" ").arg(bp.condition);
        if (!bp.enabled)
            cmd += QStringLiteral("-d ");
        cmd += QStringLiteral("\"%1:%2\"").arg(bpPath).arg(bp.line);
        const int token = sendCommand(cmd);
        m_pendingBps.insert(token, bp);
    }
    m_prelaunchBreakpoints.clear();

    // Run
    sendCommand(QStringLiteral("-exec-run"));
    m_launched = true;
    emit started();
    emit outputProduced(QStringLiteral("[DEBUG] -exec-run sent, session active"),
                        QStringLiteral("debug"));
}

void GdbMiAdapter::attach(int pid)
{
    if (m_process.state() != QProcess::NotRunning) {
        emit error(tr("Debugger already running"));
        return;
    }

    QStringList gdbArgs;
    gdbArgs << QStringLiteral("--interpreter=mi2")
            << QStringLiteral("--quiet");

    m_process.setProgram(m_debuggerPath);
    m_process.setArguments(gdbArgs);

    // Store attach PID for use in onProcessStarted (re-uses the started signal).
    // Encode as negative to distinguish from launch flow.
    m_pendingExe  = QStringLiteral("__attach__");
    m_pendingArgs = { QString::number(pid) };
    m_process.start();
    // onProcessStarted() fires when GDB is ready, sends -target-attach.
}

void GdbMiAdapter::terminate()
{
    // Re-queue confirmed breakpoints so the next debug session starts with them.
    m_prelaunchBreakpoints.clear();
    for (auto it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it) {
        DebugBreakpoint bp = it.value();
        bp.id       = -1;
        bp.verified = false;
        m_prelaunchBreakpoints.append(bp);
    }
    m_breakpoints.clear();

    m_launched = false;

    if (m_process.state() == QProcess::NotRunning) return;

    // Try graceful exit first (300ms), then force kill.
    // Never block longer than 300ms — this can be called from closeEvent.
    sendCommand(QStringLiteral("-gdb-exit"));
    if (!m_process.waitForFinished(300)) {
        m_process.kill();
        m_process.waitForFinished(200);
    }
}

bool GdbMiAdapter::isRunning() const
{
    return m_process.state() != QProcess::NotRunning && m_launched;
}

// ── Execution control ─────────────────────────────────────────────────────────

void GdbMiAdapter::continueExecution(int threadId)
{
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-continue --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-continue"));
}

void GdbMiAdapter::stepOver(int threadId)
{
    emit outputProduced(QStringLiteral("[GDB] stepOver thread=%1 launched=%2 procState=%3")
                            .arg(threadId).arg(m_launched).arg(int(m_process.state())),
                        QStringLiteral("debug"));
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-next --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-next"));
}

void GdbMiAdapter::stepInto(int threadId)
{
    emit outputProduced(QStringLiteral("[GDB] stepInto thread=%1 launched=%2")
                            .arg(threadId).arg(m_launched),
                        QStringLiteral("debug"));
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-step --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-step"));
}

void GdbMiAdapter::stepOut(int threadId)
{
    emit outputProduced(QStringLiteral("[GDB] stepOut thread=%1 launched=%2")
                            .arg(threadId).arg(m_launched),
                        QStringLiteral("debug"));
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-finish --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-finish"));
}

void GdbMiAdapter::pause(int threadId)
{
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-interrupt --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-interrupt"));
}

// ── Reverse execution (GDB record/replay) ─────────────────────────────────────
//
// GDB's `record full` builds an in-memory log of the target's execution, after
// which `--reverse` variants of -exec-next/-exec-step/-exec-continue can step
// backwards through that log. Async records (*running / *stopped) flow through
// parseMiLine() exactly like forward stepping, so no special routing needed.

void GdbMiAdapter::startRecording()
{
    sendCommand(QStringLiteral("-interpreter-exec console \"record full\""));
    emit outputProduced(QStringLiteral("[GDB] Recording started"),
                        QStringLiteral("debug"));
}

void GdbMiAdapter::stopRecording()
{
    sendCommand(QStringLiteral("-interpreter-exec console \"record stop\""));
    emit outputProduced(QStringLiteral("[GDB] Recording stopped"),
                        QStringLiteral("debug"));
}

void GdbMiAdapter::reverseStepOver(int threadId)
{
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-next --reverse --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-next --reverse"));
}

void GdbMiAdapter::reverseStepInto(int threadId)
{
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-step --reverse --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-step --reverse"));
}

void GdbMiAdapter::reverseContinue(int threadId)
{
    if (threadId > 0)
        sendCommand(QStringLiteral("-exec-continue --reverse --thread %1").arg(threadId));
    else
        sendCommand(QStringLiteral("-exec-continue --reverse"));
}

// ── Breakpoints ───────────────────────────────────────────────────────────────

void GdbMiAdapter::addBreakpoint(const DebugBreakpoint &bp)
{
    if (!m_launched) {
        // GDB isn't running yet — queue for flush in launch()
        m_prelaunchBreakpoints.append(bp);
        return;
    }

    const QString bpPath = QString(bp.filePath).replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString cmd = QStringLiteral("-break-insert ");
    if (!bp.condition.isEmpty())
        cmd += QStringLiteral("-c \"%1\" ").arg(bp.condition);
    if (!bp.enabled)
        cmd += QStringLiteral("-d ");
    cmd += QStringLiteral("\"%1:%2\"").arg(bpPath).arg(bp.line);

    const int token = sendCommand(cmd);
    m_pendingBps.insert(token, bp);
}

void GdbMiAdapter::removeBreakpoint(int breakpointId)
{
    sendCommand(QStringLiteral("-break-delete %1").arg(breakpointId));
    m_breakpoints.remove(breakpointId);
    emit breakpointRemoved(breakpointId);
}

void GdbMiAdapter::removeAllBreakpoints()
{
    const auto ids = m_breakpoints.keys();
    for (int id : ids)
        removeBreakpoint(id);
}

// ── Watchpoints (data breakpoints) ────────────────────────────────────────────

void GdbMiAdapter::addWatchpoint(const DebugWatchpoint &wp)
{
    // -break-watch [-r|-a] EXPR
    //   (no flag) write watchpoint  — break on value change
    //   -r       read watchpoint   — break on read
    //   -a       access watchpoint — break on read or write
    QString cmd = QStringLiteral("-break-watch ");
    if (wp.type == DebugWatchpoint::Read)
        cmd += QStringLiteral("-r ");
    else if (wp.type == DebugWatchpoint::Access)
        cmd += QStringLiteral("-a ");
    cmd += wp.expression;

    const int token = sendCommand(cmd);
    m_pendingWatchpoints.insert(token, wp);
}

void GdbMiAdapter::removeWatchpoint(int watchpointId)
{
    // GDB uses the same -break-delete for both breakpoints and watchpoints.
    sendCommand(QStringLiteral("-break-delete %1").arg(watchpointId));
    m_watchpoints.remove(watchpointId);
    emit watchpointRemoved(watchpointId);
}

// ── Inspection ────────────────────────────────────────────────────────────────

void GdbMiAdapter::requestThreads()
{
    sendCommand(QStringLiteral("-thread-info"));
}

void GdbMiAdapter::requestStackTrace(int threadId)
{
    emit outputProduced(QStringLiteral("[GDB] requestStackTrace thread=%1").arg(threadId),
                        QStringLiteral("debug"));
    const int token = (threadId > 0)
        ? sendCommand(QStringLiteral("-stack-list-frames --thread %1").arg(threadId))
        : sendCommand(QStringLiteral("-stack-list-frames"));
    m_stackTraceRequests.insert(token, threadId);
}

void GdbMiAdapter::requestScopes(int /*frameId*/)
{
    // GDB/MI doesn't have scopes — we list locals with full values + types.
    sendCommand(QStringLiteral("-stack-list-locals --all-values"));
}

void GdbMiAdapter::requestVariables(int /*variablesReference*/)
{
    sendCommand(QStringLiteral("-stack-list-locals --all-values"));
}

void GdbMiAdapter::evaluate(const QString &expression, int frameId)
{
    Q_UNUSED(frameId)
    sendCommand(QStringLiteral("-data-evaluate-expression \"%1\"")
                    .arg(expression));
}

void GdbMiAdapter::stackSelectFrame(int frameId)
{
    emit outputProduced(QStringLiteral("[GDB] stackSelectFrame frame=%1")
                            .arg(frameId),
                        QStringLiteral("debug"));
    sendCommand(QStringLiteral("-stack-select-frame %1").arg(frameId));

    // Refresh stack + locals for the newly selected frame so UI updates.
    requestStackTrace(m_currentThread);
    sendCommand(QStringLiteral("-stack-list-locals --all-values"));
}

// ── Variable Objects ──────────────────────────────────────────────────────────

void GdbMiAdapter::createVarObject(const QString &expression, int frameId)
{
    const QString varName = QStringLiteral("var%1").arg(m_nextVarId++);
    const QString cmd = (frameId > 0)
        ? QStringLiteral("-var-create %1 %2 \"%3\"").arg(varName).arg(frameId).arg(expression)
        : QStringLiteral("-var-create %1 * \"%2\"").arg(varName, expression);
    const int token = sendCommand(cmd);
    m_varObjRequests.insert(token, VarObjCmd::Create);
    m_varCreateExprs.insert(token, expression);
}

void GdbMiAdapter::listVarChildren(const QString &varName, int from, int count)
{
    QString cmd = QStringLiteral("-var-list-children --all-values %1").arg(varName);
    if (from >= 0 && count > 0)
        cmd += QStringLiteral(" %1 %2").arg(from).arg(from + count);
    const int token = sendCommand(cmd);
    m_varObjRequests.insert(token, VarObjCmd::ListChildren);
    m_varListParents.insert(token, varName);
}

void GdbMiAdapter::updateVarObjects()
{
    const int token = sendCommand(QStringLiteral("-var-update --all-values *"));
    m_varObjRequests.insert(token, VarObjCmd::Update);
}

void GdbMiAdapter::deleteVarObject(const QString &varName)
{
    const int token = sendCommand(QStringLiteral("-var-delete %1").arg(varName));
    m_varObjRequests.insert(token, VarObjCmd::Delete);
    m_varDeleteNames.insert(token, varName);
}

void GdbMiAdapter::assignVarValue(const QString &varName, const QString &newValue)
{
    const int token = sendCommand(
        QStringLiteral("-var-assign %1 \"%2\"").arg(varName, newValue));
    m_varObjRequests.insert(token, VarObjCmd::Assign);
    m_varAssignNames.insert(token, varName);
}

// ── Memory inspection ─────────────────────────────────────────────────────────

void GdbMiAdapter::readMemory(quint64 addr, int count)
{
    const QString cmd = QStringLiteral("-data-read-memory-bytes 0x%1 %2")
                            .arg(addr, 0, 16).arg(count);
    int token = sendCommand(cmd);
    m_pendingMemReads.insert(token, {addr, count});
}

void GdbMiAdapter::handleMemoryReadResult(int token, const QHash<QString, QString> &attrs)
{
    const auto req = m_pendingMemReads.take(token);

    // The MI response shape is:
    //   memory=[{begin="0x...",offset="0x0",end="0x...",contents="48656c6c6f..."}]
    // The body is captured as a single string blob by parseMiBody — pull
    // contents="..." out with a regex.
    const QString memory = attrs.value(QStringLiteral("memory"));
    QByteArray bytes;
    static const QRegularExpression contentsRx(
        QStringLiteral("contents=\"([0-9A-Fa-f]+)\""));
    auto m = contentsRx.match(memory);
    if (m.hasMatch()) {
        const QString hex = m.captured(1);
        bytes = QByteArray::fromHex(hex.toLatin1());
    }
    emit memoryReceived(req.addr, bytes);
}

// ── Disassembly ───────────────────────────────────────────────────────────────

void GdbMiAdapter::disassemble(quint64 startAddr, int instructionCount, int mode)
{
    // -data-disassemble -s <start> -e <end> -- <mode>
    //   mode 0 = plain assembly only
    //   mode 1 = mixed source + assembly (deprecated forms 2/3 use the same shape)
    //
    // GDB requires an end-address; use a generous upper bound based on
    // instructionCount so we always cover the requested window. Real x86-64
    // instructions average ~4 bytes (max ~15) — 16 bytes/instr is a safe
    // overshoot. GDB will simply stop emitting once the range ends.
    if (instructionCount <= 0) instructionCount = 1;
    const quint64 endAddr = startAddr + static_cast<quint64>(instructionCount) * 16ULL;

    const QString cmd = QStringLiteral("-data-disassemble -s 0x%1 -e 0x%2 -- %3")
                            .arg(startAddr, 0, 16)
                            .arg(endAddr,   0, 16)
                            .arg(mode);
    const int token = sendCommand(cmd);
    m_pendingDisasm.insert(token, true);
}

void GdbMiAdapter::handleDisassemblyResult(int token, const QHash<QString, QString> &attrs)
{
    m_pendingDisasm.remove(token);

    QList<DisasmLine> result;

    // The MI response shape (mode 0) is:
    //   asm_insns=[{address="0x...",func-name="main",offset="12",inst="mov ..."},...]
    // For mode 1 (source + asm) the shape is grouped per source line:
    //   asm_insns=[src_and_asm_line={line="...",file="...",
    //              line_asm_insn=[{address="0x...",inst="..."},...]},...]
    // Either way, the inner instruction objects are flat {...} groups with
    // address= / inst= fields, so a brace-aware scan extracts both.
    const QString asmInsns = attrs.value(QStringLiteral("asm_insns"));
    if (asmInsns.isEmpty()) {
        emit disassemblyReceived(result);
        return;
    }

    // Walk every {...} group and only keep ones that look like an instruction
    // record (have an `address=` field). This skips the outer src_and_asm_line
    // wrapper for mode 1 while still picking up its nested instruction list.
    int i = 0;
    while (i < asmInsns.length()) {
        const int open = asmInsns.indexOf(QLatin1Char('{'), i);
        if (open < 0) break;
        int depth = 1;
        int j = open + 1;
        while (j < asmInsns.length() && depth > 0) {
            const QChar ch = asmInsns.at(j);
            if (ch == QLatin1Char('"')) {
                ++j;
                while (j < asmInsns.length() && asmInsns.at(j) != QLatin1Char('"')) {
                    if (asmInsns.at(j) == QLatin1Char('\\')) ++j;
                    ++j;
                }
            } else if (ch == QLatin1Char('{')) {
                ++depth;
            } else if (ch == QLatin1Char('}')) {
                --depth;
            }
            ++j;
        }

        const QString body = asmInsns.mid(open + 1, j - open - 2);
        const auto fields  = parseMiBody(body);

        // Skip groups that don't carry an instruction (e.g. mode-1 wrappers).
        const QString addrStr = fields.value(QStringLiteral("address"));
        const QString inst    = fields.value(QStringLiteral("inst"));
        if (!addrStr.isEmpty() && !inst.isEmpty()) {
            DisasmLine d;
            // address comes as "0x..." — strip prefix before base-16 parse.
            QString hex = addrStr;
            if (hex.startsWith(QStringLiteral("0x")) || hex.startsWith(QStringLiteral("0X")))
                hex = hex.mid(2);
            bool okHex = false;
            d.address     = hex.toULongLong(&okHex, 16);
            d.function    = fields.value(QStringLiteral("func-name"));
            d.offset      = fields.value(QStringLiteral("offset"),
                                         QStringLiteral("0")).toInt();
            d.instruction = inst;
            if (okHex)
                result.append(d);
        }

        // Advance past this group, but step *into* nested groups so that
        // mode-1's `line_asm_insn=[{...},{...}]` instruction children are
        // also picked up.
        i = open + 1;
    }

    emit disassemblyReceived(result);
}

// ── MI command sending ────────────────────────────────────────────────────────

int GdbMiAdapter::sendCommand(const QString &miCommand)
{
    const int token = m_nextToken++;
    const QString line = QStringLiteral("%1%2\n").arg(token).arg(miCommand);

    m_pending.insert(token, {miCommand, token});
    m_process.write(line.toUtf8());

    emit outputProduced(QStringLiteral("> %1").arg(line.trimmed()),
                        QStringLiteral("command"));
    return token;
}

// ── MI output parsing ─────────────────────────────────────────────────────────

void GdbMiAdapter::onReadyRead()
{
    m_lineBuffer += QString::fromUtf8(m_process.readAllStandardOutput());

    int consumed = 0;
    while (true) {
        const int nl = m_lineBuffer.indexOf(QLatin1Char('\n'), consumed);
        if (nl < 0) break;
        const QString line = m_lineBuffer.mid(consumed, nl - consumed).trimmed();
        consumed = nl + 1;
        if (!line.isEmpty())
            parseMiLine(line);
    }
    if (consumed > 0)
        m_lineBuffer.remove(0, consumed);
}

void GdbMiAdapter::parseMiLine(const QString &line)
{
    // (gdb) prompt — ignore
    if (line == QLatin1String("(gdb)")) return;

    // Console output: ~"text"
    if (line.startsWith(QLatin1Char('~'))) {
        QString text = line.mid(2, line.length() - 3); // strip ~" and "
        text.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        text.replace(QStringLiteral("\\t"), QStringLiteral("\t"));
        emit outputProduced(text, QStringLiteral("console"));
        return;
    }

    // Target output: @"text"
    if (line.startsWith(QLatin1Char('@'))) {
        QString text = line.mid(2, line.length() - 3);
        emit outputProduced(text, QStringLiteral("target"));
        return;
    }

    // Log output: &"text"
    if (line.startsWith(QLatin1Char('&'))) return;

    // Async exec record: *stopped, *running
    if (line.startsWith(QLatin1Char('*'))) {
        if (line.startsWith(QStringLiteral("*stopped"))) {
            handleStopped(parseMiBody(line.mid(9)));
        } else if (line.startsWith(QStringLiteral("*running"))) {
            // Extract thread-id if present
            const auto attrs = parseMiBody(line.mid(9));
            int tid = attrs.value(QStringLiteral("thread-id"), QStringLiteral("0")).toInt();
            emit continued(tid);
        }
        return;
    }

    // Async notify: =thread-created, =breakpoint-modified, etc.
    if (line.startsWith(QLatin1Char('='))) return;

    // Result record: token^done,... or token^error,...
    static const QRegularExpression resultRx(
        QStringLiteral("^(\\d+)\\^(done|running|error|connected|exit)(.*)$"));
    const auto match = resultRx.match(line);
    if (match.hasMatch()) {
        const int token = match.captured(1).toInt();
        const QString resultClass = match.captured(2);
        const QString body = match.captured(3);

        if (resultClass == QStringLiteral("error")) {
            auto attrs = parseMiBody(body);
            const QString msg = attrs.value(QStringLiteral("msg"), QStringLiteral("Unknown error"));
            // Drop any pending watchpoint tied to this token so it isn't
            // mistakenly applied to a later result.
            if (m_pendingWatchpoints.contains(token))
                m_pendingWatchpoints.remove(token);
            // If a var-obj command failed, route the error to the var-obj signal
            if (m_varObjRequests.contains(token)) {
                const auto cmd = m_varObjRequests.take(token);
                QString expr;
                if (cmd == VarObjCmd::Create)
                    expr = m_varCreateExprs.take(token);
                else if (cmd == VarObjCmd::ListChildren)
                    expr = m_varListParents.take(token);
                else if (cmd == VarObjCmd::Delete)
                    m_varDeleteNames.remove(token);
                else if (cmd == VarObjCmd::Assign)
                    expr = m_varAssignNames.take(token);
                emit varObjectError(expr, msg);
            } else {
                emit error(msg);
            }
        } else if (resultClass == QStringLiteral("exit")) {
            // handled by processFinished
        } else {
            auto attrs = parseMiBody(body);

            // Route result to correct handler based on pending command
            if (m_varObjRequests.contains(token)) {
                const auto cmd = m_varObjRequests.take(token);
                switch (cmd) {
                case VarObjCmd::Create:       handleVarCreateResult(token, attrs);       break;
                case VarObjCmd::ListChildren: handleVarListChildrenResult(token, attrs); break;
                case VarObjCmd::Update:       handleVarUpdateResult(token, attrs);       break;
                case VarObjCmd::Delete:       handleVarDeleteResult(token, attrs);       break;
                case VarObjCmd::Assign:       handleVarAssignResult(token, attrs);       break;
                }
            } else if (m_pendingBps.contains(token)) {
                handleBreakInsertResult(token, attrs);
            } else if (m_pendingWatchpoints.contains(token)) {
                handleBreakWatchResult(token, attrs);
            } else if (m_pendingMemReads.contains(token)) {
                handleMemoryReadResult(token, attrs);
            } else if (m_pendingDisasm.contains(token)) {
                handleDisassemblyResult(token, attrs);
            } else if (m_stackTraceRequests.contains(token)) {
                handleStackListResult(token, attrs);
            } else if (body.contains(QStringLiteral("threads="))) {
                handleThreadInfoResult(token, attrs);
            } else if (body.contains(QStringLiteral("locals="))) {
                handleLocalsResult(token, attrs);
            } else if (body.contains(QStringLiteral("value="))) {
                handleEvaluateResult(token, attrs);
            }
        }

        m_pending.remove(token);
        return;
    }
}

QHash<QString, QString> GdbMiAdapter::parseMiBody(const QString &body)
{
    QHash<QString, QString> result;
    if (body.isEmpty()) return result;

    // Simple key=value parser (handles quoted values)
    const QString s = body.startsWith(QLatin1Char(',')) ? body.mid(1) : body;

    int i = 0;
    while (i < s.length()) {
        // Find key
        int eq = s.indexOf(QLatin1Char('='), i);
        if (eq < 0) break;
        const QString key = s.mid(i, eq - i).trimmed();

        i = eq + 1;
        if (i >= s.length()) break;

        QString value;
        if (s.at(i) == QLatin1Char('"')) {
            // Quoted value
            ++i;
            int end = i;
            while (end < s.length()) {
                if (s.at(end) == QLatin1Char('\\') && end + 1 < s.length()) {
                    end += 2;
                    continue;
                }
                if (s.at(end) == QLatin1Char('"')) break;
                ++end;
            }
            value = s.mid(i, end - i);
            value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
            value.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
            i = end + 1;
        } else if (s.at(i) == QLatin1Char('{') || s.at(i) == QLatin1Char('[')) {
            // Nested structure — grab until matching close
            const QChar open = s.at(i);
            const QChar close = (open == QLatin1Char('{')) ? QLatin1Char('}') : QLatin1Char(']');
            int depth = 1;
            int start = i;
            ++i;
            while (i < s.length() && depth > 0) {
                if (s.at(i) == open) ++depth;
                else if (s.at(i) == close) --depth;
                else if (s.at(i) == QLatin1Char('"')) {
                    ++i;
                    while (i < s.length() && s.at(i) != QLatin1Char('"')) {
                        if (s.at(i) == QLatin1Char('\\')) ++i;
                        ++i;
                    }
                }
                ++i;
            }
            value = s.mid(start, i - start);
        } else {
            // Unquoted value (rare)
            int end = s.indexOf(QLatin1Char(','), i);
            if (end < 0) end = s.length();
            value = s.mid(i, end - i).trimmed();
            i = end;
        }

        result.insert(key, value);

        // Skip comma
        if (i < s.length() && s.at(i) == QLatin1Char(','))
            ++i;
    }

    return result;
}

// ── Result handlers ───────────────────────────────────────────────────────────

void GdbMiAdapter::handleStopped(const QHash<QString, QString> &attrs)
{
    const int threadId = attrs.value(QStringLiteral("thread-id"), QStringLiteral("0")).toInt();
    const QString reason = attrs.value(QStringLiteral("reason"));
    m_currentThread = threadId;

    emit outputProduced(QStringLiteral("[GDB] *stopped reason=%1 thread=%2")
                            .arg(reason, QString::number(threadId)),
                        QStringLiteral("debug"));

    DebugStopReason stopReason = DebugStopReason::Unknown;
    if (reason.contains(QStringLiteral("breakpoint")))
        stopReason = DebugStopReason::Breakpoint;
    else if (reason.contains(QStringLiteral("step")) || reason.contains(QStringLiteral("end-stepping")))
        stopReason = DebugStopReason::Step;
    else if (reason.contains(QStringLiteral("signal")) || reason.contains(QStringLiteral("exception")))
        stopReason = DebugStopReason::Exception;
    else if (reason.contains(QStringLiteral("exited"))) {
        emit terminated();
        return;
    }

    emit stopped(threadId, stopReason, reason);
}

void GdbMiAdapter::handleBreakInsertResult(int token, const QHash<QString, QString> &attrs)
{
    auto bp = m_pendingBps.take(token);

    // Parse bkpt={number="1",type="breakpoint",...}
    const QString bkptStr = attrs.value(QStringLiteral("bkpt"));
    if (!bkptStr.isEmpty()) {
        auto bkptAttrs = parseMiBody(bkptStr.mid(1, bkptStr.length() - 2));
        bp.id = bkptAttrs.value(QStringLiteral("number"), QStringLiteral("-1")).toInt();
        bp.verified = true;
        const QString file = bkptAttrs.value(QStringLiteral("fullname"));
        if (!file.isEmpty()) bp.filePath = file;
        const int line = bkptAttrs.value(QStringLiteral("line"), QStringLiteral("0")).toInt();
        if (line > 0) bp.line = line;
    }

    // Apply hit-count gate, if any. -break-after <bp> <count> tells GDB to
    // ignore the next <count> hits before actually stopping. We only send it
    // once GDB has confirmed the breakpoint and assigned a real id.
    if (bp.id > 0 && bp.hitCount > 0) {
        sendCommand(QStringLiteral("-break-after %1 %2")
                        .arg(bp.id)
                        .arg(bp.hitCount));
    }

    m_breakpoints.insert(bp.id, bp);
    emit breakpointSet(bp);
}

void GdbMiAdapter::handleBreakWatchResult(int token, const QHash<QString, QString> &attrs)
{
    auto wp = m_pendingWatchpoints.take(token);

    // GDB returns one of: wpt, hw-wpt, hw-rwpt, hw-awpt depending on the type
    // of watchpoint that was created. Try each in order.
    QString wptStr = attrs.value(QStringLiteral("wpt"));
    if (wptStr.isEmpty()) wptStr = attrs.value(QStringLiteral("hw-wpt"));
    if (wptStr.isEmpty()) wptStr = attrs.value(QStringLiteral("hw-rwpt"));
    if (wptStr.isEmpty()) wptStr = attrs.value(QStringLiteral("hw-awpt"));

    if (!wptStr.isEmpty()) {
        // Strip surrounding {} and parse number=...,exp=...
        auto wptAttrs = parseMiBody(wptStr.mid(1, wptStr.length() - 2));
        wp.id = wptAttrs.value(QStringLiteral("number"), QStringLiteral("-1")).toInt();
        const QString exp = wptAttrs.value(QStringLiteral("exp"));
        if (!exp.isEmpty()) wp.expression = exp;
        wp.verified = true;
    }

    if (wp.id > 0)
        m_watchpoints.insert(wp.id, wp);

    emit watchpointSet(wp);
}

void GdbMiAdapter::handleStackListResult(int token, const QHash<QString, QString> &attrs)
{
    const int threadId = m_stackTraceRequests.take(token);

    // Parse stack=[frame={...},frame={...},...]
    const QString stack = attrs.value(QStringLiteral("stack"));
    QList<DebugFrame> frames;

    if (!stack.isEmpty()) {
        // Nested-brace-aware extraction of each frame={...} block.
        // The old regex [^}]+ broke when args contained {}, losing file/line.
        int pos = 0;
        while (pos < stack.length()) {
            const int tag = stack.indexOf(QStringLiteral("frame={"), pos);
            if (tag < 0) break;
            int start = tag + 7; // after "frame={"
            int depth = 1;
            int j = start;
            while (j < stack.length() && depth > 0) {
                const QChar ch = stack.at(j);
                if (ch == QLatin1Char('{')) ++depth;
                else if (ch == QLatin1Char('}')) --depth;
                else if (ch == QLatin1Char('"')) {
                    ++j;
                    while (j < stack.length() && stack.at(j) != QLatin1Char('"')) {
                        if (stack.at(j) == QLatin1Char('\\')) ++j;
                        ++j;
                    }
                }
                if (depth > 0) ++j;
            }
            frames.append(parseFrame(stack.mid(start, j - start)));
            pos = j + 1;
        }
    }

    emit outputProduced(QStringLiteral("[GDB] stackTrace: %1 frames, top=%2:%3")
                            .arg(frames.size())
                            .arg(frames.isEmpty() ? QString() : frames.first().filePath)
                            .arg(frames.isEmpty() ? 0 : frames.first().line),
                        QStringLiteral("debug"));
    emit stackTraceReceived(threadId, frames);
}

void GdbMiAdapter::handleThreadInfoResult(int /*token*/, const QHash<QString, QString> &attrs)
{
    const QString threads = attrs.value(QStringLiteral("threads"));
    QList<DebugThread> result;

    if (!threads.isEmpty()) {
        static const QRegularExpression threadRx(
            QStringLiteral("\\{id=\"(\\d+)\",.*?name=\"([^\"]*)\""
                           ".*?state=\"([^\"]*)\""));
        auto it = threadRx.globalMatch(threads);
        while (it.hasNext()) {
            auto m = it.next();
            DebugThread t;
            t.id = m.captured(1).toInt();
            t.name = m.captured(2);
            t.stopped = (m.captured(3) == QStringLiteral("stopped"));
            result.append(t);
        }
    }

    emit threadsReceived(result);
}

void GdbMiAdapter::handleVarResult(int /*token*/, const QHash<QString, QString> &attrs)
{
    Q_UNUSED(attrs)
    // Variable listing handled via requestVariables output
}

void GdbMiAdapter::handleEvaluateResult(int /*token*/, const QHash<QString, QString> &attrs)
{
    const QString value = attrs.value(QStringLiteral("value"));
    emit evaluateResult(QString(), value);
}

void GdbMiAdapter::handleLocalsResult(int /*token*/, const QHash<QString, QString> &attrs)
{
    // Parse locals=[{name="argc",value="1",type="int"},{name="argv",value="0x..."},...]
    const QString locals = attrs.value(QStringLiteral("locals"));
    QList<DebugVariable> result;

    if (!locals.isEmpty()) {
        // Walk through {...} groups inside the brackets
        int i = 0;
        while (i < locals.length()) {
            const int open = locals.indexOf(QLatin1Char('{'), i);
            if (open < 0) break;
            int depth = 1;
            int j = open + 1;
            while (j < locals.length() && depth > 0) {
                if (locals.at(j) == QLatin1Char('"')) {
                    ++j;
                    while (j < locals.length() && locals.at(j) != QLatin1Char('"')) {
                        if (locals.at(j) == QLatin1Char('\\')) ++j;
                        ++j;
                    }
                } else if (locals.at(j) == QLatin1Char('{')) ++depth;
                else if (locals.at(j) == QLatin1Char('}')) --depth;
                ++j;
            }
            const QString body = locals.mid(open + 1, j - open - 2);
            const auto fields = parseMiBody(body);
            DebugVariable v;
            v.name  = fields.value(QStringLiteral("name"));
            v.value = fields.value(QStringLiteral("value"));
            v.type  = fields.value(QStringLiteral("type"));
            if (!v.name.isEmpty())
                result.append(v);
            i = j;
        }
    }

    emit variablesReceived(0, result);
}

// ── Variable Object result handlers ───────────────────────────────────────────

void GdbMiAdapter::handleVarCreateResult(int token, const QHash<QString, QString> &attrs)
{
    const QString expr = m_varCreateExprs.take(token);

    DebugVarObj obj;
    obj.varName     = attrs.value(QStringLiteral("name"));
    obj.expression  = expr;
    obj.value       = attrs.value(QStringLiteral("value"));
    obj.type        = attrs.value(QStringLiteral("type"));
    obj.numChildren = attrs.value(QStringLiteral("numchild"), QStringLiteral("0")).toInt();
    obj.hasMore     = attrs.value(QStringLiteral("has_more"), QStringLiteral("0")) != QStringLiteral("0");

    emit varObjectCreated(obj);
}

void GdbMiAdapter::handleVarListChildrenResult(int token, const QHash<QString, QString> &attrs)
{
    const QString parent = m_varListParents.take(token);
    const QString childrenStr = attrs.value(QStringLiteral("children"));
    QList<DebugVarObj> children = parseChildList(childrenStr);
    emit varChildrenListed(parent, children);
}

void GdbMiAdapter::handleVarUpdateResult(int /*token*/, const QHash<QString, QString> &attrs)
{
    const QString changeStr = attrs.value(QStringLiteral("changelist"));
    QList<DebugVarChange> changes = parseChangeList(changeStr);
    emit varObjectsUpdated(changes);
}

void GdbMiAdapter::handleVarDeleteResult(int token, const QHash<QString, QString> &/*attrs*/)
{
    const QString varName = m_varDeleteNames.take(token);
    emit varObjectDeleted(varName);
}

void GdbMiAdapter::handleVarAssignResult(int token, const QHash<QString, QString> &attrs)
{
    const QString varName = m_varAssignNames.take(token);
    const QString newVal  = attrs.value(QStringLiteral("value"));
    emit varValueAssigned(varName, newVal);
}

// ── Var-obj output parsers ────────────────────────────────────────────────────

QList<DebugVarObj> GdbMiAdapter::parseChildList(const QString &raw)
{
    QList<DebugVarObj> result;
    if (raw.isEmpty()) return result;

    // Find each child={...} entry
    static const QRegularExpression childRx(QStringLiteral("child=\\{([^}]+(?:\\{[^}]*\\}[^}]*)*)\\}"));
    auto it = childRx.globalMatch(raw);
    while (it.hasNext()) {
        auto m = it.next();
        auto attrs = parseMiBody(m.captured(1));
        DebugVarObj obj;
        obj.varName     = attrs.value(QStringLiteral("name"));
        obj.expression  = attrs.value(QStringLiteral("exp"));
        obj.value       = attrs.value(QStringLiteral("value"));
        obj.type        = attrs.value(QStringLiteral("type"));
        obj.numChildren = attrs.value(QStringLiteral("numchild"), QStringLiteral("0")).toInt();
        obj.hasMore     = attrs.value(QStringLiteral("has_more"), QStringLiteral("0")) != QStringLiteral("0");
        result.append(obj);
    }
    return result;
}

QList<DebugVarChange> GdbMiAdapter::parseChangeList(const QString &raw)
{
    QList<DebugVarChange> result;
    if (raw.isEmpty()) return result;

    // Parse changelist=[{name="var1",value="42",in_scope="true",...},...]
    static const QRegularExpression entryRx(QStringLiteral("\\{([^}]+)\\}"));
    auto it = entryRx.globalMatch(raw);
    while (it.hasNext()) {
        auto m = it.next();
        auto attrs = parseMiBody(m.captured(1));
        DebugVarChange chg;
        chg.varName        = attrs.value(QStringLiteral("name"));
        chg.newValue       = attrs.value(QStringLiteral("value"));
        chg.inScope        = attrs.value(QStringLiteral("in_scope"), QStringLiteral("true")) == QStringLiteral("true");
        chg.typeChanged    = attrs.value(QStringLiteral("type_changed"), QStringLiteral("false")) == QStringLiteral("true");
        const QString nc   = attrs.value(QStringLiteral("new_num_children"), QStringLiteral("-1"));
        chg.newNumChildren = nc.toInt();
        result.append(chg);
    }
    return result;
}

DebugFrame GdbMiAdapter::parseFrame(const QString &frameStr) const
{
    auto attrs = parseMiBody(frameStr);
    DebugFrame f;
    f.id = attrs.value(QStringLiteral("level"), QStringLiteral("0")).toInt();
    f.name = attrs.value(QStringLiteral("func"));
    f.filePath = attrs.value(QStringLiteral("fullname"),
                             attrs.value(QStringLiteral("file")));
    f.line = attrs.value(QStringLiteral("line"), QStringLiteral("0")).toInt();
    return f;
}

// ── Process events ────────────────────────────────────────────────────────────

void GdbMiAdapter::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode)
    Q_UNUSED(status)
    m_launched = false;

    // Re-queue confirmed breakpoints so the next debug session starts with them.
    if (!m_breakpoints.isEmpty()) {
        for (auto it = m_breakpoints.begin(); it != m_breakpoints.end(); ++it) {
            DebugBreakpoint bp = it.value();
            bp.id       = -1;
            bp.verified = false;
            m_prelaunchBreakpoints.append(bp);
        }
        m_breakpoints.clear();
    }

    m_pending.clear();
    m_pendingBps.clear();
    m_pendingWatchpoints.clear();
    m_watchpoints.clear();
    m_stackTraceRequests.clear();
    m_varObjRequests.clear();
    m_varCreateExprs.clear();
    m_varListParents.clear();
    m_varDeleteNames.clear();
    m_varAssignNames.clear();
    m_pendingMemReads.clear();
    m_pendingDisasm.clear();
    emit terminated();
}

void GdbMiAdapter::onProcessError(QProcess::ProcessError err)
{
    if (err == QProcess::FailedToStart) {
        emit outputProduced(
            QStringLiteral("[GDB] FAILED to start: %1 | path=%2")
                .arg(m_process.errorString(), m_debuggerPath),
            QStringLiteral("debug"));
        emit error(QStringLiteral("GDB failed to start: %1 | path=%2")
                       .arg(m_process.errorString(), m_debuggerPath));
    } else {
        emit error(tr("Debugger process error: %1").arg(m_process.errorString()));
    }
}
