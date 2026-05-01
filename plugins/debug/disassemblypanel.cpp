#include "disassemblypanel.h"

#include "sdk/idebugadapter.h"

#include <QComboBox>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextEdit>
#include <QVBoxLayout>

// ── Construction / UI ────────────────────────────────────────────────────────

DisassemblyPanel::DisassemblyPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void DisassemblyPanel::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // VS2022 dark style — kept consistent with MemoryViewPanel / DebugPanel.
    setStyleSheet(QStringLiteral(
        "DisassemblyPanel { background: #1e1e1e; }"
        "QLineEdit, QComboBox {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3e3e42; border-radius: 2px;"
        "  padding: 2px 6px; selection-background-color: #094771; }"
        "QLineEdit:focus, QComboBox:focus { border: 1px solid #007acc; }"
        "QComboBox::drop-down { border: none; width: 16px; }"
        "QComboBox QAbstractItemView {"
        "  background: #252526; color: #d4d4d4;"
        "  selection-background-color: #094771;"
        "  border: 1px solid #3e3e42; }"
        "QPushButton {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3e3e42; border-radius: 2px;"
        "  padding: 3px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #3e3e42; }"
        "QPushButton:pressed { background: #094771; }"
        "QPushButton:disabled { color: #555558; }"
        "QLabel { color: #9d9d9d; font-size: 11px; }"
        "QTextEdit {"
        "  background: #1e1e1e; color: #d4d4d4;"
        "  border: none; selection-background-color: #264f78; }"
    ));

    // ── Top toolbar row: function + refresh + mode ───────────────────────
    auto *topBar = new QWidget(this);
    topBar->setStyleSheet(QStringLiteral(
        "QWidget { background: #2d2d30; border-bottom: 1px solid #3e3e42; }"
    ));
    auto *topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(6, 4, 6, 4);
    topLay->setSpacing(6);

    auto *fnLabel = new QLabel(tr("Function:"), topBar);
    topLay->addWidget(fnLabel);

    m_functionEdit = new QLineEdit(topBar);
    m_functionEdit->setPlaceholderText(tr("Empty = current frame (e.g. main)"));
    m_functionEdit->setMinimumWidth(220);
    topLay->addWidget(m_functionEdit, 1);

    m_refreshButton = new QPushButton(tr("Refresh"), topBar);
    m_refreshButton->setDefault(true);
    topLay->addWidget(m_refreshButton);

    topLay->addSpacing(8);

    auto *modeLabel = new QLabel(tr("Mode:"), topBar);
    topLay->addWidget(modeLabel);
    m_modeCombo = new QComboBox(topBar);
    m_modeCombo->addItem(tr("Source + Asm"), static_cast<int>(Mode::SourceAndAsm));
    m_modeCombo->addItem(tr("Asm only"),     static_cast<int>(Mode::AsmOnly));
    m_modeCombo->setFixedWidth(120);
    topLay->addWidget(m_modeCombo);

    root->addWidget(topBar);

    // ── Status row ───────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #808080; font-size: 11px;"
        " background: #252526; border-bottom: 1px solid #3e3e42;"));
    root->addWidget(m_statusLabel);

    // ── Listing view ─────────────────────────────────────────────────────
    m_listingView = new QTextEdit(this);
    m_listingView->setReadOnly(true);
    m_listingView->setLineWrapMode(QTextEdit::NoWrap);
    QFont monoFont(QStringLiteral("Consolas"));
    if (!QFontDatabase::families().contains(QStringLiteral("Consolas")))
        monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_listingView->setFont(monoFont);
    m_listingView->setPlaceholderText(
        tr("Press Refresh to disassemble the current frame."));
    root->addWidget(m_listingView, 1);

    connect(m_refreshButton, &QPushButton::clicked,     this, &DisassemblyPanel::refresh);
    connect(m_functionEdit,  &QLineEdit::returnPressed, this, &DisassemblyPanel::refresh);
    connect(m_modeCombo,     QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { refresh(); });
}

// ── Adapter wiring ───────────────────────────────────────────────────────────

void DisassemblyPanel::setAdapter(IDebugAdapter *adapter)
{
    if (m_adapter == adapter)
        return;

    if (m_adapter) {
        // SIGNAL/SLOT keeps cross-DLL signal dispatch safe (IDebugAdapter MOC
        // is in the host exe).
        disconnect(m_adapter, SIGNAL(evaluateResult(QString,QString)),
                   this, SLOT(onEvaluateResult(QString,QString)));
    }

    m_adapter = adapter;

    if (m_adapter) {
        connect(m_adapter, SIGNAL(evaluateResult(QString,QString)),
                this, SLOT(onEvaluateResult(QString,QString)));
    }
}

// ── Public actions ───────────────────────────────────────────────────────────

void DisassemblyPanel::refresh()
{
    const QString fn = m_functionEdit->text().trimmed();

    m_pendingFunction = fn;
    m_pendingPcExpression = fn.isEmpty() ? buildPcExpression()
                                         : buildFunctionAddrExpression(fn);

    if (m_adapter && m_adapter->isRunning()) {
        m_awaitingResult = true;
        if (fn.isEmpty())
            setStatus(tr("Disassembling current frame ($pc)..."));
        else
            setStatus(tr("Disassembling %1...").arg(fn));
        // frameId 0 → use the currently selected frame.
        m_adapter->evaluate(m_pendingPcExpression, 0);
    } else {
        // No live debugger — render a synthetic listing so the UI is testable.
        // TODO: once IDebugAdapter exposes a direct -data-disassemble channel,
        //       replace the placeholder with real parsed instructions.
        setStatus(tr("Debugger not running — showing synthetic listing"));
        const QString label = fn.isEmpty() ? QStringLiteral("<current frame>") : fn;
        renderFakeListing(label, /*baseAddress=*/0x0000000000401000ULL);
    }
}

// ── Adapter result handling ──────────────────────────────────────────────────

void DisassemblyPanel::onEvaluateResult(const QString &expression,
                                        const QString &result)
{
    if (!m_awaitingResult)
        return;
    if (expression != m_pendingPcExpression)
        return;  // belongs to another evaluator (locals, watch, memory view, ...)

    m_awaitingResult = false;

    bool ok = false;
    const quint64 pc = parsePcFromResult(result, &ok);
    const QString label = m_pendingFunction.isEmpty()
        ? QStringLiteral("<current frame>")
        : m_pendingFunction;

    if (!ok || pc == 0) {
        setStatus(tr("Could not parse address from result"), /*isError=*/true);
        // Show the raw result so the user can diagnose what GDB returned.
        m_listingView->setPlainText(result);
        return;
    }

    setStatus(tr("PC for %1: 0x%2").arg(label).arg(pc, 0, 16));

    // TODO: real -data-disassemble wiring needs a direct command channel
    //       (the SDK only exposes evaluate()). Until that exists, render a
    //       placeholder listing anchored at the resolved PC so the UI is
    //       still useful as a sanity check.
    renderFakeListing(label, pc);
}

// ── Rendering ────────────────────────────────────────────────────────────────

void DisassemblyPanel::renderListing(const QString &functionLabel,
                                     quint64 baseAddress,
                                     const QStringList &lines)
{
    QString out;
    out.reserve(lines.size() * 64 + 128);

    out += QStringLiteral("# %1 @ 0x%2\n")
               .arg(functionLabel)
               .arg(baseAddress, 0, 16);
    out += QLatin1Char('\n');

    for (const QString &line : lines) {
        out += line;
        out += QLatin1Char('\n');
    }

    m_listingView->setPlainText(out);
}

void DisassemblyPanel::renderFakeListing(const QString &functionLabel,
                                         quint64 baseAddress)
{
    // Synthetic content — a short, plausible x86-64 prologue + a few ops, so
    // the UI is verifiable without a live debug session.
    //
    // Format mirrors the layout described in the spec:
    //   `0xADDR  <fn+off>:  mnemonic operands`
    static const struct {
        int  offset;
        const char *mnemonic;
        const char *operands;
        const char *sourceHint;   // shown in Source+Asm mode only; nullptr to skip
    } kOps[] = {
        {  0, "push", "rbp",                          "int main() {"     },
        {  1, "mov",  "rbp, rsp",                     nullptr            },
        {  4, "sub",  "rsp, 0x20",                    nullptr            },
        {  8, "mov",  "qword ptr [rbp-8], rdi",       "    arg0 = ..."   },
        { 12, "mov",  "rax, qword ptr [rbp-8]",       nullptr            },
        { 16, "call",  "0x401320 <printf>",           "    printf(...);" },
        { 21, "xor",  "eax, eax",                     "    return 0;"    },
        { 23, "leave", "",                            nullptr            },
        { 24, "ret",  "",                             "}"                },
    };

    const auto mode = static_cast<Mode>(
        m_modeCombo ? m_modeCombo->currentData().toInt()
                    : static_cast<int>(Mode::SourceAndAsm));

    QStringList lines;
    lines.reserve(static_cast<int>(sizeof(kOps) / sizeof(kOps[0])) * 2);

    for (const auto &op : kOps) {
        if (mode == Mode::SourceAndAsm && op.sourceHint) {
            lines << QStringLiteral("                          ; %1")
                         .arg(QString::fromLatin1(op.sourceHint));
        }

        const quint64 addr = baseAddress + static_cast<quint64>(op.offset);
        const QString fnTag = QStringLiteral("<%1+%2>")
                                  .arg(functionLabel)
                                  .arg(op.offset);

        QString line = QStringLiteral("0x%1  %2:  %3")
                           .arg(addr, 16, 16, QLatin1Char('0'))
                           .arg(fnTag, -16)
                           .arg(QString::fromLatin1(op.mnemonic));
        if (op.operands && *op.operands) {
            line += QLatin1Char(' ');
            line += QString::fromLatin1(op.operands);
        }
        lines << line;
    }

    renderListing(functionLabel, baseAddress, lines);
}

void DisassemblyPanel::setStatus(const QString &text, bool isError)
{
    if (!m_statusLabel)
        return;
    m_statusLabel->setText(text);
    m_statusLabel->setStyleSheet(
        isError
            ? QStringLiteral("padding: 2px 8px; color: #f48771; font-size: 11px;"
                             " background: #252526; border-bottom: 1px solid #3e3e42;")
            : QStringLiteral("padding: 2px 8px; color: #808080; font-size: 11px;"
                             " background: #252526; border-bottom: 1px solid #3e3e42;"));
}

// ── Helpers ──────────────────────────────────────────────────────────────────

QString DisassemblyPanel::buildPcExpression()
{
    // GDB exposes the program counter as the convenience variable `$pc`.
    // Casting to (void*) coerces the formatted result into a hex pointer
    // ("(void *) 0x401234 <main+12>") which is easy to parse.
    return QStringLiteral("(void*)$pc");
}

QString DisassemblyPanel::buildFunctionAddrExpression(const QString &fn)
{
    // `&fn` resolves to the entry point of `fn` for ordinary symbols. Cast
    // to (void*) for a stable hex pointer formatting.
    return QStringLiteral("(void*)&%1").arg(fn);
}

quint64 DisassemblyPanel::parsePcFromResult(const QString &result, bool *ok)
{
    // Typical formats produced by GDB:
    //   "(void *) 0x401234 <main+12>"
    //   "0x401234"
    //   "$1 = (void *) 0x401234 <main+12>"
    static const QRegularExpression hexRe(
        QStringLiteral("0x([0-9a-fA-F]+)"));

    const auto m = hexRe.match(result);
    if (!m.hasMatch()) {
        if (ok) *ok = false;
        return 0;
    }
    bool localOk = false;
    const quint64 pc = m.captured(1).toULongLong(&localOk, 16);
    if (ok) *ok = localOk;
    return localOk ? pc : 0ULL;
}
