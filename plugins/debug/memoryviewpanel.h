#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QLabel;
class IDebugAdapter;

/// Memory inspector dock for the Debug plugin.
///
/// Allows the user to enter an address (e.g. `0x7fff12340000`), pick a row
/// width and row count, and read raw bytes from the running debuggee.
///
/// Wiring strategy (no SDK changes required):
///   - The panel calls `IDebugAdapter::evaluate("(char(*)[N])addr")` and
///     listens to the `evaluateResult(expression, result)` signal.
///   - The result text is parsed for hex byte values and rendered as a
///     classic hex-dump (offset | hex bytes | ASCII).
///   - When no adapter is set or the debugger is not running, the panel
///     falls back to a synthetic dump so the UI is verifiable in isolation.
class MemoryViewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryViewPanel(QWidget *parent = nullptr);

    /// Connect the panel to a debug adapter. Pass `nullptr` to disconnect.
    void setAdapter(IDebugAdapter *adapter);

public slots:
    /// Read using the current address / width / row-count fields.
    void readMemory();

    /// Re-issue the last read (preserves address + dimensions).
    void refresh();

private slots:
    void onEvaluateResult(const QString &expression, const QString &result);

private:
    void setupUi();
    void renderDump(quint64 baseAddress, const QByteArray &bytes);
    void renderFakeDump(quint64 baseAddress, int totalBytes);
    void setStatus(const QString &text, bool isError = false);

    /// Build the GDB expression cast that returns `count` bytes at `address`.
    static QString buildEvaluateExpression(quint64 address, int count);

    /// Parse a GDB `-data-evaluate-expression` result of an array-of-chars
    /// cast into a raw byte buffer. Recognises forms like:
    ///   `{0x48, 0x65, 0x6c, ...}`   (hex array)
    ///   `"Hello\\000World"`         (escaped string literal)
    static QByteArray parseEvaluateBytes(const QString &result, int expectedCount);

    /// Parse a hex address (with or without `0x` prefix). Returns 0 on failure.
    static quint64 parseAddress(const QString &text, bool *ok = nullptr);

    IDebugAdapter *m_adapter = nullptr;

    // Top toolbar
    QLineEdit   *m_addrEdit       = nullptr;
    QPushButton *m_readButton     = nullptr;
    QPushButton *m_refreshButton  = nullptr;
    QSpinBox    *m_bytesPerRow    = nullptr;
    QSpinBox    *m_rowCount       = nullptr;
    QLabel      *m_statusLabel    = nullptr;

    // Hex dump view
    QTextEdit   *m_dumpView       = nullptr;

    // Last-issued request — used by Refresh and to filter evaluate replies.
    quint64 m_pendingAddress     = 0;
    int     m_pendingTotalBytes  = 0;
    QString m_pendingExpression;
    bool    m_awaitingResult     = false;
};
