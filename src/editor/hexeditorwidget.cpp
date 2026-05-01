#include "hexeditorwidget.h"

#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>
#include <QVBoxLayout>

// ── Construction / UI ────────────────────────────────────────────────────────

HexEditorWidget::HexEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    updateStatus();
}

HexEditorWidget::~HexEditorWidget() = default;

void HexEditorWidget::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // VS2022 dark theme — kept consistent with MemoryViewPanel.
    setStyleSheet(QStringLiteral(
        "HexEditorWidget { background: #1e1e1e; }"
        "QLineEdit {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3e3e42; border-radius: 2px;"
        "  padding: 2px 6px; selection-background-color: #094771; }"
        "QLineEdit:focus { border: 1px solid #007acc; }"
        "QPushButton {"
        "  background: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3e3e42; border-radius: 2px;"
        "  padding: 3px 12px; font-size: 12px; }"
        "QPushButton:hover  { background: #3e3e42; }"
        "QPushButton:pressed { background: #094771; }"
        "QPushButton:disabled { color: #555558; }"
        "QLabel { color: #9d9d9d; font-size: 11px; }"
        "QPlainTextEdit {"
        "  background: #1e1e1e; color: #d4d4d4;"
        "  border: none; selection-background-color: #264f78; }"
    ));

    // ── Top toolbar row: Offset label + goto input + Reload button ────────
    auto *topBar = new QWidget(this);
    topBar->setStyleSheet(QStringLiteral(
        "QWidget { background: #2d2d30; border-bottom: 1px solid #3e3e42; }"
    ));
    auto *topLay = new QHBoxLayout(topBar);
    topLay->setContentsMargins(6, 4, 6, 4);
    topLay->setSpacing(6);

    auto *offsetLabel = new QLabel(tr("Offset:"), topBar);
    topLay->addWidget(offsetLabel);

    m_gotoEdit = new QLineEdit(topBar);
    m_gotoEdit->setPlaceholderText(tr("Goto offset (e.g. 0x1A0 or 4096)"));
    m_gotoEdit->setMinimumWidth(220);
    topLay->addWidget(m_gotoEdit, 1);

    auto *gotoBtn = new QPushButton(tr("Goto"), topBar);
    topLay->addWidget(gotoBtn);

    m_reloadBtn = new QPushButton(tr("Reload"), topBar);
    topLay->addWidget(m_reloadBtn);

    root->addWidget(topBar);

    // ── Dump view ───────────────────────────────────────────────────────
    m_dumpView = new QPlainTextEdit(this);
    m_dumpView->setReadOnly(true);
    m_dumpView->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_dumpView->setUndoRedoEnabled(false);
    QFont monoFont(QStringLiteral("Consolas"));
    if (!QFontDatabase::families().contains(QStringLiteral("Consolas")))
        monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    monoFont.setPointSize(10);
    m_dumpView->setFont(monoFont);
    m_dumpView->setPlaceholderText(tr("No file loaded."));
    root->addWidget(m_dumpView, 1);

    // ── Bottom status bar ───────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No file loaded."), this);
    m_statusLabel->setStyleSheet(QStringLiteral(
        "padding: 2px 8px; color: #808080; font-size: 11px;"
        " background: #252526; border-top: 1px solid #3e3e42;"));
    root->addWidget(m_statusLabel);

    connect(gotoBtn,     &QPushButton::clicked,        this, &HexEditorWidget::onGotoActivated);
    connect(m_gotoEdit,  &QLineEdit::returnPressed,    this, &HexEditorWidget::onGotoActivated);
    connect(m_reloadBtn, &QPushButton::clicked,        this, &HexEditorWidget::onReloadClicked);
    connect(m_dumpView,  &QPlainTextEdit::cursorPositionChanged,
            this, &HexEditorWidget::onCursorMoved);
}

// ── Public API ───────────────────────────────────────────────────────────────

bool HexEditorWidget::loadFile(const QString &path)
{
    m_filePath = path;
    m_bytes.clear();
    m_fileSize = 0;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (m_dumpView)
            m_dumpView->setPlainText(tr("Failed to open: %1").arg(path));
        updateStatus();
        return false;
    }

    m_fileSize = f.size();
    m_bytes    = f.read(kMaxBytes);
    f.close();

    renderDump();
    updateStatus();
    return !m_bytes.isEmpty();
}

// ── Rendering ────────────────────────────────────────────────────────────────

void HexEditorWidget::renderDump()
{
    if (!m_dumpView)
        return;

    const int total = m_bytes.size();
    QString out;
    out.reserve(total * 4 + 64);

    for (int rowStart = 0; rowStart < total; rowStart += kBytesPerRow) {
        const int rowLen = qMin(kBytesPerRow, total - rowStart);

        // Offset column: "0x00000000  " (8 hex digits, lowercase, fixed-width).
        out += QStringLiteral("0x%1  ")
                   .arg(static_cast<quint32>(rowStart),
                        8, 16, QLatin1Char('0'));

        // Hex column: "48 65 6C ..." — pad short last row so ASCII column
        // stays aligned.
        for (int i = 0; i < kBytesPerRow; ++i) {
            if (i < rowLen) {
                const uchar b = static_cast<uchar>(m_bytes.at(rowStart + i));
                out += QStringLiteral("%1 ")
                           .arg(static_cast<uint>(b), 2, 16, QLatin1Char('0'))
                           .toUpper();
            } else {
                out += QStringLiteral("   ");
            }
        }

        // ASCII column.
        out += QLatin1Char(' ');
        for (int i = 0; i < rowLen; ++i) {
            const uchar b = static_cast<uchar>(m_bytes.at(rowStart + i));
            out += (b >= 0x20 && b < 0x7F) ? QChar::fromLatin1(static_cast<char>(b))
                                           : QChar::fromLatin1('.');
        }
        out += QLatin1Char('\n');
    }

    if (out.isEmpty())
        out = tr("(empty file)");

    m_dumpView->setPlainText(out);
    // Reset cursor to the top so updateStatus reports offset 0.
    QTextCursor c = m_dumpView->textCursor();
    c.movePosition(QTextCursor::Start);
    m_dumpView->setTextCursor(c);
}

void HexEditorWidget::updateStatus()
{
    if (!m_statusLabel)
        return;

    if (m_filePath.isEmpty()) {
        m_statusLabel->setText(tr("No file loaded."));
        return;
    }

    const qint64 loaded = static_cast<qint64>(m_bytes.size());
    const bool truncated = (m_fileSize > kMaxBytes);

    qint64 offset = -1;
    if (m_dumpView)
        offset = offsetForTextPosition(m_dumpView->textCursor().position());
    const QString offsetText = (offset < 0)
        ? tr("—")
        : QStringLiteral("0x%1 (%2)")
              .arg(static_cast<quint32>(offset), 0, 16)
              .arg(offset);

    QString text = tr("Size: %1   Loaded: %2 bytes%3   Cursor: %4")
                       .arg(formatFileSize(m_fileSize))
                       .arg(loaded)
                       .arg(truncated ? tr(" (truncated to %1)")
                                            .arg(formatFileSize(kMaxBytes))
                                       : QString())
                       .arg(offsetText);

    m_statusLabel->setText(text);
}

QString HexEditorWidget::formatFileSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    const double kb = bytes / 1024.0;
    if (kb < 1024.0)
        return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
    const double mb = kb / 1024.0;
    if (mb < 1024.0)
        return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
    const double gb = mb / 1024.0;
    return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
}

// ── Goto / cursor wiring ─────────────────────────────────────────────────────

int HexEditorWidget::textPositionForOffset(qint64 offset) const
{
    if (!m_dumpView || offset < 0 || offset >= m_bytes.size())
        return -1;

    const qint64 row    = offset / kBytesPerRow;
    const qint64 colByte = offset % kBytesPerRow;

    // Layout per row:
    //   offset (m_offsetColWidth) + "  " (2) + colByte * 3
    // where each hex byte takes 3 chars: "XX ".  The line itself is followed
    // by a '\n'.  Per-row total = m_offsetColWidth + 2 + 16*3 + 1 + 16 + 1
    // (offset + sep + hex + sep + ascii + '\n') — but we only need positions
    // up to the start of the hex pair, so we don't need the full row width
    // past the column.
    const int rowWidth = m_offsetColWidth + 2 + kBytesPerRow * 3
                       + 1 + kBytesPerRow + 1; // trailing '\n'
    const int rowStartPos = static_cast<int>(row * rowWidth);
    const int hexStart    = rowStartPos + m_offsetColWidth + 2
                          + static_cast<int>(colByte) * 3;
    return hexStart;
}

qint64 HexEditorWidget::offsetForTextPosition(int pos) const
{
    if (m_bytes.isEmpty())
        return -1;

    const int rowWidth = m_offsetColWidth + 2 + kBytesPerRow * 3
                       + 1 + kBytesPerRow + 1; // trailing '\n'
    if (rowWidth <= 0)
        return -1;

    const int row     = pos / rowWidth;
    const int rowOff  = pos % rowWidth;
    const int hexCol0 = m_offsetColWidth + 2;          // first hex char
    const int hexEnd  = hexCol0 + kBytesPerRow * 3;    // exclusive
    const int asciiCol0 = hexEnd + 1;                  // first ASCII char
    const int asciiEnd  = asciiCol0 + kBytesPerRow;    // exclusive

    int colByte = -1;
    if (rowOff >= hexCol0 && rowOff < hexEnd) {
        colByte = (rowOff - hexCol0) / 3;
    } else if (rowOff >= asciiCol0 && rowOff < asciiEnd) {
        colByte = rowOff - asciiCol0;
    } else {
        // Inside offset column or padding — clamp to start of row.
        colByte = 0;
    }

    const qint64 offset = static_cast<qint64>(row) * kBytesPerRow + colByte;
    if (offset < 0 || offset >= m_bytes.size())
        return -1;
    return offset;
}

void HexEditorWidget::onGotoActivated()
{
    if (!m_dumpView || !m_gotoEdit)
        return;

    QString text = m_gotoEdit->text().trimmed();
    if (text.isEmpty())
        return;

    bool ok = false;
    qint64 offset = -1;

    // Hex literal: "0x...", "0X...", or pure hex digits with at least one
    // a-f letter.  Decimal: anything else parseable as integer.
    if (text.startsWith(QLatin1String("0x"), Qt::CaseInsensitive)) {
        offset = text.mid(2).toLongLong(&ok, 16);
    } else {
        offset = text.toLongLong(&ok, 10);
        if (!ok) {
            // Fallback: try hex without 0x prefix.
            offset = text.toLongLong(&ok, 16);
        }
    }

    if (!ok || offset < 0)
        return;

    if (offset >= m_bytes.size()) {
        // Clamp to last valid byte rather than ignore — friendlier for users
        // who type the file size by accident.
        if (m_bytes.isEmpty())
            return;
        offset = m_bytes.size() - 1;
    }

    const int pos = textPositionForOffset(offset);
    if (pos < 0)
        return;

    QTextCursor c = m_dumpView->textCursor();
    c.setPosition(pos);
    m_dumpView->setTextCursor(c);
    m_dumpView->ensureCursorVisible();
    m_dumpView->setFocus();
    updateStatus();
}

void HexEditorWidget::onReloadClicked()
{
    if (m_filePath.isEmpty())
        return;
    loadFile(m_filePath);
}

void HexEditorWidget::onCursorMoved()
{
    updateStatus();
}
