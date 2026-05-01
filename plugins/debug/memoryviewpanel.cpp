#include "memoryviewpanel.h"

#include "sdk/idebugadapter.h"

#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTextEdit>
#include <QVBoxLayout>

// ── Construction / UI ────────────────────────────────────────────────────────

MemoryViewPanel::MemoryViewPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void MemoryViewPanel::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // VS2022 dark style for the whole panel. Kept consistent with DebugPanel.
    setStyleSheet(QStringLiteral(
        "MemoryViewPanel { background: #1e1e1e; }"
        "QLineEdit, QSpinBox {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3e3e42; border-radius: 2px;"
        "  padding: 2px 6px; selection-background-color: #094771; }"
        "QLineEdit:focus, QSpinBox:focus { border: 1px solid #007acc; }"
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

    // ── Top toolbar row: address input + buttons + dimensions ────────────
    auto *topBar = new QWidget(this);
    topBar->setStyleSheet(QStringLiteral(
        "QWidget { background: #2d2d30; border-bottom: 1px solid #3e3e42; }"
    ));
    auto *topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(6, 4, 6, 4);
    topLay->setSpacing(6);

    auto *addrLabel = new QLabel(tr("Address:"), topBar);
    topLay->addWidget(addrLabel);

    m_addrEdit = new QLineEdit(topBar);
    m_addrEdit->setPlaceholderText(tr("Address (e.g. 0x7fff...)"));
    m_addrEdit->setMinimumWidth(220);
    topLay->addWidget(m_addrEdit, 1);

    m_readButton = new QPushButton(tr("Read"), topBar);
    m_readButton->setDefault(true);
    topLay->addWidget(m_readButton);

    m_refreshButton = new QPushButton(tr("Refresh"), topBar);
    topLay->addWidget(m_refreshButton);

    topLay->addSpacing(8);

    auto *bprLabel = new QLabel(tr("Bytes/row:"), topBar);
    topLay->addWidget(bprLabel);
    m_bytesPerRow = new QSpinBox(topBar);
    m_bytesPerRow->setRange(4, 64);
    m_bytesPerRow->setSingleStep(4);
    m_bytesPerRow->setValue(16);
    m_bytesPerRow->setFixedWidth(64);
    topLay->addWidget(m_bytesPerRow);

    auto *rowsLabel = new QLabel(tr("Rows:"), topBar);
    topLay->addWidget(rowsLabel);
    m_rowCount = new QSpinBox(topBar);
    m_rowCount->setRange(1, 1024);
    m_rowCount->setValue(32);
    m_rowCount->setFixedWidth(72);
    topLay->addWidget(m_rowCount);

    root->addWidget(topBar);

    // ── Status row ───────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("Idle"), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #808080; font-size: 11px;"
        " background: #252526; border-bottom: 1px solid #3e3e42;"));
    root->addWidget(m_statusLabel);

    // ── Hex dump view ────────────────────────────────────────────────────
    m_dumpView = new QTextEdit(this);
    m_dumpView->setReadOnly(true);
    m_dumpView->setLineWrapMode(QTextEdit::NoWrap);
    QFont monoFont(QStringLiteral("Consolas"));
    if (!QFontDatabase::families().contains(QStringLiteral("Consolas")))
        monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_dumpView->setFont(monoFont);
    m_dumpView->setPlaceholderText(
        tr("Enter an address above and press Read to inspect memory."));
    root->addWidget(m_dumpView, 1);

    connect(m_readButton,    &QPushButton::clicked, this, &MemoryViewPanel::readMemory);
    connect(m_refreshButton, &QPushButton::clicked, this, &MemoryViewPanel::refresh);
    connect(m_addrEdit,      &QLineEdit::returnPressed, this, &MemoryViewPanel::readMemory);
}

// ── Adapter wiring ───────────────────────────────────────────────────────────

void MemoryViewPanel::setAdapter(IDebugAdapter *adapter)
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

void MemoryViewPanel::readMemory()
{
    bool ok = false;
    const QString addrText = m_addrEdit->text().trimmed();
    const quint64 addr = parseAddress(addrText, &ok);
    if (!ok || addrText.isEmpty()) {
        setStatus(tr("Invalid address: '%1'").arg(addrText), /*isError=*/true);
        return;
    }

    const int bpr = m_bytesPerRow->value();
    const int rows = m_rowCount->value();
    const int total = bpr * rows;

    m_pendingAddress    = addr;
    m_pendingTotalBytes = total;
    m_pendingExpression = buildEvaluateExpression(addr, total);

    if (m_adapter && m_adapter->isRunning()) {
        m_awaitingResult = true;
        setStatus(tr("Reading %1 bytes at 0x%2...").arg(total)
                      .arg(addr, 0, 16));
        // frameId 0 → use the currently selected frame.
        m_adapter->evaluate(m_pendingExpression, 0);
    } else {
        // No live debugger — render a synthetic block so the UI is testable.
        // TODO: wire to GDB `-data-read-memory-bytes` once a non-evaluate
        //       memory API is available on IDebugAdapter.
        setStatus(tr("Debugger not running — showing synthetic dump"));
        renderFakeDump(addr, total);
    }
}

void MemoryViewPanel::refresh()
{
    if (m_pendingTotalBytes <= 0) {
        readMemory();
        return;
    }

    if (m_adapter && m_adapter->isRunning()) {
        m_awaitingResult = true;
        setStatus(tr("Refreshing %1 bytes at 0x%2...")
                      .arg(m_pendingTotalBytes)
                      .arg(m_pendingAddress, 0, 16));
        m_adapter->evaluate(m_pendingExpression, 0);
    } else {
        setStatus(tr("Debugger not running — showing synthetic dump"));
        renderFakeDump(m_pendingAddress, m_pendingTotalBytes);
    }
}

// ── Adapter result handling ──────────────────────────────────────────────────

void MemoryViewPanel::onEvaluateResult(const QString &expression,
                                       const QString &result)
{
    if (!m_awaitingResult)
        return;
    if (expression != m_pendingExpression)
        return;  // belongs to another evaluator (locals tab, watch, etc.)

    m_awaitingResult = false;

    const QByteArray bytes = parseEvaluateBytes(result, m_pendingTotalBytes);
    if (bytes.isEmpty()) {
        setStatus(tr("Could not parse %1 bytes from result").arg(m_pendingTotalBytes),
                  /*isError=*/true);
        m_dumpView->setPlainText(result);
        return;
    }

    setStatus(tr("Read %1 bytes at 0x%2")
                  .arg(bytes.size())
                  .arg(m_pendingAddress, 0, 16));
    renderDump(m_pendingAddress, bytes);
}

// ── Rendering ────────────────────────────────────────────────────────────────

void MemoryViewPanel::renderDump(quint64 baseAddress, const QByteArray &bytes)
{
    const int bpr = m_bytesPerRow->value();
    const int total = bytes.size();

    QString out;
    out.reserve(total * 4 + 64);

    for (int rowStart = 0; rowStart < total; rowStart += bpr) {
        const int rowLen = qMin(bpr, total - rowStart);

        // Offset column: 0xADDR (16 hex digits, lowercase to match GDB)
        out += QStringLiteral("0x%1  ")
                   .arg(baseAddress + static_cast<quint64>(rowStart),
                        16, 16, QLatin1Char('0'));

        // Hex bytes column
        for (int i = 0; i < bpr; ++i) {
            if (i < rowLen) {
                const uchar b = static_cast<uchar>(bytes.at(rowStart + i));
                out += QStringLiteral("%1 ")
                           .arg(static_cast<uint>(b), 2, 16,
                                QLatin1Char('0'))
                           .toUpper();
            } else {
                out += QStringLiteral("   ");  // padding for short last row
            }
        }

        // ASCII column
        out += QLatin1Char(' ');
        for (int i = 0; i < rowLen; ++i) {
            const uchar b = static_cast<uchar>(bytes.at(rowStart + i));
            out += (b >= 0x20 && b < 0x7F) ? QChar::fromLatin1(static_cast<char>(b))
                                           : QChar::fromLatin1('.');
        }
        out += QLatin1Char('\n');
    }

    m_dumpView->setPlainText(out);
}

void MemoryViewPanel::renderFakeDump(quint64 baseAddress, int totalBytes)
{
    // Synthetic content — repeating "Hello World\0" pattern, so a developer
    // can sanity-check the layout without running a debug session.
    static const char kPattern[] = "Hello World\x00";
    constexpr int kLen = sizeof(kPattern) - 1;

    QByteArray bytes;
    bytes.resize(totalBytes);
    for (int i = 0; i < totalBytes; ++i)
        bytes[i] = kPattern[i % kLen];

    renderDump(baseAddress, bytes);
}

void MemoryViewPanel::setStatus(const QString &text, bool isError)
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

QString MemoryViewPanel::buildEvaluateExpression(quint64 address, int count)
{
    // GDB accepts `(unsigned char(*)[N])0xADDR` casts which yield an array
    // formatted as `{0xNN, 0xNN, ...}` when evaluated. This is portable across
    // most GDB versions even when -data-read-memory-bytes is unavailable.
    return QStringLiteral("*(unsigned char(*)[%1])0x%2")
        .arg(count)
        .arg(address, 0, 16);
}

QByteArray MemoryViewPanel::parseEvaluateBytes(const QString &result,
                                               int expectedCount)
{
    QByteArray out;
    out.reserve(expectedCount > 0 ? expectedCount : 256);

    // GDB array form: "{0x48, 0x65, 0x6c, ...}" — most common.
    static const QRegularExpression hexRe(
        QStringLiteral("0x([0-9a-fA-F]{1,2})"));

    auto it = hexRe.globalMatch(result);
    while (it.hasNext()) {
        const auto m = it.next();
        bool ok = false;
        const uint v = m.captured(1).toUInt(&ok, 16);
        if (ok)
            out.append(static_cast<char>(v & 0xFF));
        if (expectedCount > 0 && out.size() >= expectedCount)
            break;
    }

    return out;
}

quint64 MemoryViewPanel::parseAddress(const QString &text, bool *ok)
{
    QString s = text.trimmed();
    if (s.startsWith(QLatin1String("0x"), Qt::CaseInsensitive))
        s = s.mid(2);
    s.remove(QLatin1Char('_'));
    return s.toULongLong(ok, 16);
}
