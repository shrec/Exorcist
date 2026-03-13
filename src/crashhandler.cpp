#include "crashhandler.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QSysInfo>

#include <array>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <mutex>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <DbgHelp.h>
#include <Psapi.h>
#include <TlHelp32.h>
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
#include <execinfo.h>
#include <unistd.h>
#include <cxxabi.h>
#include <dlfcn.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// ── Static state
// ═══════════════════════════════════════════════════════════════════════════════

static QString g_crashDir;

// Ring buffer for recent log lines (lock-free write index)
static std::array<QString, CrashHandler::kRingBufferSize> g_ringBuffer;
static std::atomic<int> g_ringIndex{0};
static std::mutex g_ringMutex; // only for reading the full buffer

// ═══════════════════════════════════════════════════════════════════════════════
// ── Ring buffer
// ═══════════════════════════════════════════════════════════════════════════════

void CrashHandler::recordLogLine(const QString &line)
{
    const int idx = g_ringIndex.fetch_add(1, std::memory_order_relaxed)
                    % kRingBufferSize;
    g_ringBuffer[static_cast<size_t>(idx)] = line;
}

QStringList CrashHandler::recentLogLines()
{
    std::lock_guard<std::mutex> lock(g_ringMutex);
    QStringList lines;
    const int total = g_ringIndex.load(std::memory_order_relaxed);
    const int count = qMin(total, kRingBufferSize);
    const int start = total >= kRingBufferSize ? total % kRingBufferSize : 0;
    for (int i = 0; i < count; ++i) {
        const int idx = (start + i) % kRingBufferSize;
        const QString &entry = g_ringBuffer[static_cast<size_t>(idx)];
        if (!entry.isEmpty())
            lines.append(entry);
    }
    return lines;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ── Helpers
// ═══════════════════════════════════════════════════════════════════════════════

static QString crashTimestamp()
{
    return QDateTime::currentDateTime().toString(
        QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
}

static void writeCrashFile(const QString &report)
{
    const QString stamp = crashTimestamp();
    const QString path = g_crashDir + QStringLiteral("/crash_")
                         + stamp + QStringLiteral(".txt");

    // Use raw C I/O — safer in signal/exception context
    FILE *f = fopen(path.toUtf8().constData(), "w");
    if (f) {
        fputs(report.toUtf8().constData(), f);
        fclose(f);
        fprintf(stderr, "\n[CrashHandler] Crash report written to %s\n",
                path.toUtf8().constData());
    } else {
        fprintf(stderr, "\n[CrashHandler] FAILED to write crash report to %s\n",
                path.toUtf8().constData());
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ── Windows implementation
// ═══════════════════════════════════════════════════════════════════════════════

#ifdef Q_OS_WIN

// ── Exception code → human string ─────────────────────────────────────────────

static QString exceptionCodeToString(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:      return QStringLiteral("EXCEPTION_ACCESS_VIOLATION");
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return QStringLiteral("EXCEPTION_ARRAY_BOUNDS_EXCEEDED");
    case EXCEPTION_BREAKPOINT:            return QStringLiteral("EXCEPTION_BREAKPOINT");
    case EXCEPTION_DATATYPE_MISALIGNMENT: return QStringLiteral("EXCEPTION_DATATYPE_MISALIGNMENT");
    case EXCEPTION_FLT_DENORMAL_OPERAND:  return QStringLiteral("EXCEPTION_FLT_DENORMAL_OPERAND");
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return QStringLiteral("EXCEPTION_FLT_DIVIDE_BY_ZERO");
    case EXCEPTION_FLT_INEXACT_RESULT:    return QStringLiteral("EXCEPTION_FLT_INEXACT_RESULT");
    case EXCEPTION_FLT_INVALID_OPERATION: return QStringLiteral("EXCEPTION_FLT_INVALID_OPERATION");
    case EXCEPTION_FLT_OVERFLOW:          return QStringLiteral("EXCEPTION_FLT_OVERFLOW");
    case EXCEPTION_FLT_STACK_CHECK:       return QStringLiteral("EXCEPTION_FLT_STACK_CHECK");
    case EXCEPTION_FLT_UNDERFLOW:         return QStringLiteral("EXCEPTION_FLT_UNDERFLOW");
    case EXCEPTION_ILLEGAL_INSTRUCTION:   return QStringLiteral("EXCEPTION_ILLEGAL_INSTRUCTION");
    case EXCEPTION_IN_PAGE_ERROR:         return QStringLiteral("EXCEPTION_IN_PAGE_ERROR");
    case EXCEPTION_INT_DIVIDE_BY_ZERO:    return QStringLiteral("EXCEPTION_INT_DIVIDE_BY_ZERO");
    case EXCEPTION_INT_OVERFLOW:          return QStringLiteral("EXCEPTION_INT_OVERFLOW");
    case EXCEPTION_INVALID_DISPOSITION:   return QStringLiteral("EXCEPTION_INVALID_DISPOSITION");
    case EXCEPTION_NONCONTINUABLE_EXCEPTION: return QStringLiteral("EXCEPTION_NONCONTINUABLE_EXCEPTION");
    case EXCEPTION_PRIV_INSTRUCTION:      return QStringLiteral("EXCEPTION_PRIV_INSTRUCTION");
    case EXCEPTION_SINGLE_STEP:           return QStringLiteral("EXCEPTION_SINGLE_STEP");
    case EXCEPTION_STACK_OVERFLOW:        return QStringLiteral("EXCEPTION_STACK_OVERFLOW");
    default:
        return QStringLiteral("UNKNOWN_EXCEPTION (0x%1)")
                   .arg(code, 8, 16, QLatin1Char('0'));
    }
}

// ── Find module for an address ────────────────────────────────────────────────

static QString moduleForAddress(DWORD64 addr)
{
    HMODULE hMod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                           | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(addr), &hMod) && hMod) {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(hMod, path, MAX_PATH))
            return QString::fromWCharArray(path);
    }
    return QStringLiteral("<unknown module>");
}

// ── Register dump ─────────────────────────────────────────────────────────────

static void appendRegisterDump(QString &report, const CONTEXT *ctx)
{
    report += QStringLiteral("\n--- CPU Registers ---\n");
#ifdef _M_X64
    report += QStringLiteral("  RAX = 0x%1    RBX = 0x%2\n")
                  .arg(ctx->Rax, 16, 16, QLatin1Char('0'))
                  .arg(ctx->Rbx, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  RCX = 0x%1    RDX = 0x%2\n")
                  .arg(ctx->Rcx, 16, 16, QLatin1Char('0'))
                  .arg(ctx->Rdx, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  RSI = 0x%1    RDI = 0x%2\n")
                  .arg(ctx->Rsi, 16, 16, QLatin1Char('0'))
                  .arg(ctx->Rdi, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  RSP = 0x%1    RBP = 0x%2\n")
                  .arg(ctx->Rsp, 16, 16, QLatin1Char('0'))
                  .arg(ctx->Rbp, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  R8  = 0x%1    R9  = 0x%2\n")
                  .arg(ctx->R8,  16, 16, QLatin1Char('0'))
                  .arg(ctx->R9,  16, 16, QLatin1Char('0'));
    report += QStringLiteral("  R10 = 0x%1    R11 = 0x%2\n")
                  .arg(ctx->R10, 16, 16, QLatin1Char('0'))
                  .arg(ctx->R11, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  R12 = 0x%1    R13 = 0x%2\n")
                  .arg(ctx->R12, 16, 16, QLatin1Char('0'))
                  .arg(ctx->R13, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  R14 = 0x%1    R15 = 0x%2\n")
                  .arg(ctx->R14, 16, 16, QLatin1Char('0'))
                  .arg(ctx->R15, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  RIP = 0x%1    EFLAGS = 0x%2\n")
                  .arg(ctx->Rip, 16, 16, QLatin1Char('0'))
                  .arg(static_cast<quint64>(ctx->EFlags), 8, 16, QLatin1Char('0'));
#elif defined(_M_IX86)
    report += QStringLiteral("  EAX = 0x%1    EBX = 0x%2\n")
                  .arg(ctx->Eax, 8, 16, QLatin1Char('0'))
                  .arg(ctx->Ebx, 8, 16, QLatin1Char('0'));
    report += QStringLiteral("  ECX = 0x%1    EDX = 0x%2\n")
                  .arg(ctx->Ecx, 8, 16, QLatin1Char('0'))
                  .arg(ctx->Edx, 8, 16, QLatin1Char('0'));
    report += QStringLiteral("  ESI = 0x%1    EDI = 0x%2\n")
                  .arg(ctx->Esi, 8, 16, QLatin1Char('0'))
                  .arg(ctx->Edi, 8, 16, QLatin1Char('0'));
    report += QStringLiteral("  ESP = 0x%1    EBP = 0x%2\n")
                  .arg(ctx->Esp, 8, 16, QLatin1Char('0'))
                  .arg(ctx->Ebp, 8, 16, QLatin1Char('0'));
    report += QStringLiteral("  EIP = 0x%1    EFLAGS = 0x%2\n")
                  .arg(ctx->Eip, 8, 16, QLatin1Char('0'))
                  .arg(ctx->EFlags, 8, 16, QLatin1Char('0'));
#elif defined(_M_ARM64)
    for (int i = 0; i < 29; i += 2) {
        report += QStringLiteral("  X%1 = 0x%2    X%3 = 0x%4\n")
                      .arg(i, 2, 10, QLatin1Char('0'))
                      .arg(ctx->X[i], 16, 16, QLatin1Char('0'))
                      .arg(i + 1, 2, 10, QLatin1Char('0'))
                      .arg(ctx->X[i + 1], 16, 16, QLatin1Char('0'));
    }
    report += QStringLiteral("  FP  = 0x%1    LR  = 0x%2\n")
                  .arg(ctx->Fp, 16, 16, QLatin1Char('0'))
                  .arg(ctx->Lr, 16, 16, QLatin1Char('0'));
    report += QStringLiteral("  SP  = 0x%1    PC  = 0x%2\n")
                  .arg(ctx->Sp, 16, 16, QLatin1Char('0'))
                  .arg(ctx->Pc, 16, 16, QLatin1Char('0'));
#endif
}

// ── Loaded modules list ───────────────────────────────────────────────────────

static void appendLoadedModules(QString &report)
{
    report += QStringLiteral("\n--- Loaded Modules ---\n");

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE
                                          | TH32CS_SNAPMODULE32,
                                          GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) {
        report += QStringLiteral("  (unable to enumerate modules)\n");
        return;
    }

    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    if (Module32FirstW(snap, &me)) {
        do {
            report += QStringLiteral("  0x%1  %2 KB  %3\n")
                          .arg(reinterpret_cast<quint64>(me.modBaseAddr),
                               16, 16, QLatin1Char('0'))
                          .arg(me.modBaseSize / 1024)
                          .arg(QString::fromWCharArray(me.szExePath));
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
}

// ── Stack walk from exception context ─────────────────────────────────────────

static void walkStackFromContext(QString &report, CONTEXT *ctx)
{
    report += QStringLiteral("\n--- Stack Trace ---\n");

    HANDLE process = GetCurrentProcess();
    HANDLE thread  = GetCurrentThread();

    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
    SymInitialize(process, nullptr, TRUE);

    STACKFRAME64 frame = {};
    DWORD machineType = 0;

#ifdef _M_X64
    machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset    = ctx->Rip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = ctx->Rbp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = ctx->Rsp;
    frame.AddrStack.Mode   = AddrModeFlat;
#elif defined(_M_IX86)
    machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset    = ctx->Eip;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = ctx->Ebp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = ctx->Esp;
    frame.AddrStack.Mode   = AddrModeFlat;
#elif defined(_M_ARM64)
    machineType = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset    = ctx->Pc;
    frame.AddrPC.Mode      = AddrModeFlat;
    frame.AddrFrame.Offset = ctx->Fp;
    frame.AddrFrame.Mode   = AddrModeFlat;
    frame.AddrStack.Offset = ctx->Sp;
    frame.AddrStack.Mode   = AddrModeFlat;
#endif

    // Symbol buffer
    char symBuf[sizeof(SYMBOL_INFO) + 512 * sizeof(TCHAR)];
    auto *symbol = reinterpret_cast<SYMBOL_INFO *>(symBuf);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 511;

    IMAGEHLP_LINE64 lineInfo;
    lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

    for (int depth = 0; depth < 128; ++depth) {
        if (!StackWalk64(machineType, process, thread, &frame, ctx,
                         nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr))
            break;

        if (frame.AddrPC.Offset == 0) break;

        const DWORD64 addr = frame.AddrPC.Offset;
        const QString mod = moduleForAddress(addr);

        // Strip path to module filename
        const int lastSlash = qMax(mod.lastIndexOf(QLatin1Char('/')),
                                   mod.lastIndexOf(QLatin1Char('\\')));
        const QString modName = (lastSlash >= 0) ? mod.mid(lastSlash + 1) : mod;

        QString entry = QStringLiteral("  #%1  0x%2  ")
                            .arg(depth, -3)
                            .arg(addr, 16, 16, QLatin1Char('0'));

        DWORD64 symDisplacement = 0;
        if (SymFromAddr(process, addr, &symDisplacement, symbol)) {
            entry += QString::fromLatin1(symbol->Name);
            if (symDisplacement)
                entry += QStringLiteral("+0x%1").arg(symDisplacement, 0, 16);
        } else {
            entry += QStringLiteral("<unknown>");
        }

        DWORD lineDisplacement = 0;
        if (SymGetLineFromAddr64(process, addr, &lineDisplacement, &lineInfo)) {
            entry += QStringLiteral("  at %1:%2")
                         .arg(QString::fromLocal8Bit(lineInfo.FileName))
                         .arg(lineInfo.LineNumber);
        }

        entry += QStringLiteral("  [%1]").arg(modName);
        report += entry + QLatin1Char('\n');
    }

    SymCleanup(process);
}

// ── Write minidump ────────────────────────────────────────────────────────────

static void writeMinidump(EXCEPTION_POINTERS *exInfo, const QString &stamp)
{
    const QString dmpPath = g_crashDir + QStringLiteral("/crash_")
                            + stamp + QStringLiteral(".dmp");

    HANDLE dmpFile = CreateFileW(
        reinterpret_cast<LPCWSTR>(dmpPath.utf16()),
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);

    if (dmpFile == INVALID_HANDLE_VALUE) return;

    MINIDUMP_EXCEPTION_INFORMATION mei;
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = exInfo;
    mei.ClientPointers = FALSE;

    MiniDumpWriteDump(
        GetCurrentProcess(), GetCurrentProcessId(), dmpFile,
        static_cast<MINIDUMP_TYPE>(
            MiniDumpWithDataSegs
            | MiniDumpWithHandleData
            | MiniDumpWithThreadInfo
            | MiniDumpWithUnloadedModules
            | MiniDumpWithIndirectlyReferencedMemory),
        &mei, nullptr, nullptr);

    CloseHandle(dmpFile);
    fprintf(stderr, "[CrashHandler] Minidump: %s\n",
            dmpPath.toUtf8().constData());
}

// ── Top-level SEH filter ──────────────────────────────────────────────────────

static LONG WINAPI exceptionFilter(EXCEPTION_POINTERS *exInfo)
{
    const QString stamp = crashTimestamp();

    // ── Minidump ──────────────────────────────────────────────────────────
    writeMinidump(exInfo, stamp);

    // ── Build report ──────────────────────────────────────────────────────
    const auto *rec = exInfo->ExceptionRecord;
    const DWORD code = rec->ExceptionCode;

    QString report;
    report.reserve(8192);

    report += QStringLiteral("╔══════════════════════════════════════════════════════════════╗\n");
    report += QStringLiteral("║              EXORCIST IDE — CRASH REPORT                    ║\n");
    report += QStringLiteral("╚══════════════════════════════════════════════════════════════╝\n\n");

    // System info
    report += QStringLiteral("--- System Information ---\n");
    report += QStringLiteral("  Time:      %1\n")
                  .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    report += QStringLiteral("  OS:        %1\n").arg(QSysInfo::prettyProductName());
    report += QStringLiteral("  Arch:      %1\n").arg(QSysInfo::currentCpuArchitecture());
    report += QStringLiteral("  Kernel:    %1 %2\n")
                  .arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
    report += QStringLiteral("  App:       %1 %2\n")
                  .arg(QCoreApplication::applicationName(),
                       QCoreApplication::applicationVersion());
    report += QStringLiteral("  PID:       %1\n").arg(GetCurrentProcessId());
    report += QStringLiteral("  Thread:    %1\n").arg(GetCurrentThreadId());

    // Exception details
    report += QStringLiteral("\n--- Exception ---\n");
    report += QStringLiteral("  Code:      %1 (0x%2)\n")
                  .arg(exceptionCodeToString(code))
                  .arg(code, 8, 16, QLatin1Char('0'));
    report += QStringLiteral("  Address:   0x%1\n")
                  .arg(reinterpret_cast<quint64>(rec->ExceptionAddress),
                       16, 16, QLatin1Char('0'));
    report += QStringLiteral("  Module:    %1\n")
                  .arg(moduleForAddress(
                       reinterpret_cast<DWORD64>(rec->ExceptionAddress)));

    // Access violation specifics
    if (code == EXCEPTION_ACCESS_VIOLATION && rec->NumberParameters >= 2) {
        const char *op = (rec->ExceptionInformation[0] == 0) ? "READ"
                       : (rec->ExceptionInformation[0] == 1) ? "WRITE"
                       : "EXECUTE";
        report += QStringLiteral("  Violation: %1 at address 0x%2\n")
                      .arg(QLatin1String(op))
                      .arg(rec->ExceptionInformation[1], 16, 16,
                           QLatin1Char('0'));
    }

    // Registers
    appendRegisterDump(report, exInfo->ContextRecord);

    // Stack trace from exception context
    // Make a copy so StackWalk64 can modify it
    CONTEXT ctxCopy = *exInfo->ContextRecord;
    walkStackFromContext(report, &ctxCopy);

    // Loaded modules
    appendLoadedModules(report);

    // Recent log lines
    const QStringList logs = CrashHandler::recentLogLines();
    if (!logs.isEmpty()) {
        report += QStringLiteral("\n--- Recent Log Lines (last %1) ---\n")
                      .arg(logs.size());
        for (const auto &logLine : logs)
            report += QStringLiteral("  ") + logLine;
    }

    report += QStringLiteral("\n═══════════════════════════════════════════"
                             "═══════════════════\n");

    writeCrashFile(report);

    return EXCEPTION_EXECUTE_HANDLER;
}

#endif // Q_OS_WIN

// ═══════════════════════════════════════════════════════════════════════════════
// ── POSIX (Linux / macOS)
// ═══════════════════════════════════════════════════════════════════════════════

#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)

static const char *signalName(int sig)
{
    switch (sig) {
    case SIGSEGV: return "SIGSEGV (Segmentation fault)";
    case SIGABRT: return "SIGABRT (Abort)";
    case SIGFPE:  return "SIGFPE (Floating-point exception)";
    case SIGBUS:  return "SIGBUS (Bus error)";
    case SIGILL:  return "SIGILL (Illegal instruction)";
    default:      return "Unknown signal";
    }
}

static QString demangleSymbol(const char *mangled)
{
    // Format: "module(symbol+offset) [addr]"
    // Try to demangle the C++ symbol inside parentheses
    QString raw = QString::fromLocal8Bit(mangled);
    const int lp = raw.indexOf(QLatin1Char('('));
    const int plus = raw.indexOf(QLatin1Char('+'), lp);
    if (lp < 0 || plus < 0) return raw;

    const QByteArray sym = raw.mid(lp + 1, plus - lp - 1).toUtf8();
    int status = -1;
    char *demangled = abi::__cxa_demangle(sym.constData(), nullptr, nullptr,
                                          &status);
    if (status == 0 && demangled) {
        const QString result = raw.left(lp + 1)
                               + QString::fromUtf8(demangled)
                               + raw.mid(plus);
        free(demangled);
        return result;
    }
    return raw;
}

static void posixSignalHandler(int sig)
{
    QString report;
    report.reserve(8192);

    report += QStringLiteral("╔══════════════════════════════════════════════════════════════╗\n");
    report += QStringLiteral("║              EXORCIST IDE — CRASH REPORT                    ║\n");
    report += QStringLiteral("╚══════════════════════════════════════════════════════════════╝\n\n");

    report += QStringLiteral("--- System Information ---\n");
    report += QStringLiteral("  Time:      %1\n")
                  .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    report += QStringLiteral("  OS:        %1\n").arg(QSysInfo::prettyProductName());
    report += QStringLiteral("  Arch:      %1\n").arg(QSysInfo::currentCpuArchitecture());
    report += QStringLiteral("  Kernel:    %1 %2\n")
                  .arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
    report += QStringLiteral("  App:       %1 %2\n")
                  .arg(QCoreApplication::applicationName(),
                       QCoreApplication::applicationVersion());
    report += QStringLiteral("  PID:       %1\n").arg(getpid());

    report += QStringLiteral("\n--- Signal ---\n");
    report += QStringLiteral("  Signal:    %1 (%2)\n")
                  .arg(QLatin1String(signalName(sig))).arg(sig);

    // Stack trace with demangled symbols
    report += QStringLiteral("\n--- Stack Trace ---\n");
    void *buffer[128];
    const int count = backtrace(buffer, 128);
    char **symbols = backtrace_symbols(buffer, count);
    if (symbols) {
        for (int i = 0; i < count; ++i) {
            Dl_info info;
            if (dladdr(buffer[i], &info) && info.dli_sname) {
                int status = -1;
                char *demangled = abi::__cxa_demangle(info.dli_sname, nullptr,
                                                      nullptr, &status);
                const char *name = (status == 0 && demangled)
                                       ? demangled : info.dli_sname;
                const ptrdiff_t offset =
                    static_cast<const char *>(buffer[i])
                    - static_cast<const char *>(info.dli_saddr);
                report += QStringLiteral("  #%1  0x%2  %3+0x%4  [%5]\n")
                              .arg(i, -3)
                              .arg(reinterpret_cast<quintptr>(buffer[i]),
                                   16, 16, QLatin1Char('0'))
                              .arg(QLatin1String(name))
                              .arg(offset, 0, 16)
                              .arg(QLatin1String(info.dli_fname));
                free(demangled);
            } else {
                report += QStringLiteral("  #%1  %2\n")
                              .arg(i, -3)
                              .arg(demangleSymbol(symbols[i]));
            }
        }
        free(symbols);
    }

    // Recent log lines
    const QStringList logs = CrashHandler::recentLogLines();
    if (!logs.isEmpty()) {
        report += QStringLiteral("\n--- Recent Log Lines (last %1) ---\n")
                      .arg(logs.size());
        for (const auto &logLine : logs)
            report += QStringLiteral("  ") + logLine;
    }

    report += QStringLiteral("\n═══════════════════════════════════════════"
                             "═══════════════════\n");

    writeCrashFile(report);

    // Restore default handler and re-raise for core dump
    signal(sig, SIG_DFL);
    raise(sig);
}

#endif // Q_OS_LINUX || Q_OS_MAC

// ═══════════════════════════════════════════════════════════════════════════════
// ── Public API
// ═══════════════════════════════════════════════════════════════════════════════

void CrashHandler::install(const QString &crashDir)
{
    if (crashDir.isEmpty()) {
        g_crashDir = QCoreApplication::applicationDirPath()
                     + QStringLiteral("/crashes");
    } else {
        g_crashDir = crashDir;
    }

    QDir().mkpath(g_crashDir);

#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(exceptionFilter);
#endif

#if defined(Q_OS_LINUX) || defined(Q_OS_MAC)
    struct sigaction sa;
    sa.sa_handler = posixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
#endif

    fprintf(stderr, "[CrashHandler] Installed — crash dir: %s\n",
            g_crashDir.toUtf8().constData());
}

QString CrashHandler::crashDirectory()
{
    return g_crashDir;
}
