#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class IDebugAdapter;

/// Disassembly inspector dock for the Debug plugin.
///
/// Shows GDB disassembly for the current frame's PC (default) or for a
/// user-supplied function name. Mirrors `MemoryViewPanel`'s wiring strategy:
///
///   - The panel calls `IDebugAdapter::evaluate(<expr>)` and listens to the
///     `evaluateResult(expression, result)` signal (cross-DLL safe).
///   - For the "current frame" mode the panel queries `$pc` to learn the
///     program counter, then emits an evaluate that approximates a
///     disassembly request. Real `-data-disassemble` requires a direct
///     command channel that the SDK does not currently expose, so the
///     first pass renders a labelled placeholder when no live debugger is
///     attached.
///   - When no adapter is set or the debugger is not running, the panel
///     falls back to a synthetic listing so the UI is verifiable in
///     isolation.
///
/// TODO: once `IDebugAdapter` exposes a raw `-data-disassemble` channel
///       (or a typed disassembly API), replace the placeholder rendering
///       with parsed real instructions.
class DisassemblyPanel : public QWidget
{
    Q_OBJECT

public:
    /// Whether the listing should mix source lines with assembly or show
    /// raw instructions only. Mirrors GDB's `-data-disassemble` mode flags.
    enum class Mode {
        SourceAndAsm = 0,
        AsmOnly      = 1,
    };

    explicit DisassemblyPanel(QWidget *parent = nullptr);

    /// Connect the panel to a debug adapter. Pass `nullptr` to disconnect.
    void setAdapter(IDebugAdapter *adapter);

public slots:
    /// Re-issue the disassembly request using the current Function / Mode.
    void refresh();

private slots:
    void onEvaluateResult(const QString &expression, const QString &result);

private:
    void setupUi();
    void renderListing(const QString &functionLabel,
                       quint64 baseAddress,
                       const QStringList &lines);
    void renderFakeListing(const QString &functionLabel,
                           quint64 baseAddress);
    void setStatus(const QString &text, bool isError = false);

    /// Build the GDB evaluate expression used to learn the current PC for
    /// the selected frame. Falls back to `$pc` when no override is given.
    static QString buildPcExpression();

    /// Build the evaluate expression that returns the function name + PC
    /// for an explicit function name (e.g. `&main`).
    static QString buildFunctionAddrExpression(const QString &fn);

    /// Parse a hex address out of a GDB evaluate result. Returns 0 on failure.
    static quint64 parsePcFromResult(const QString &result, bool *ok = nullptr);

    IDebugAdapter *m_adapter = nullptr;

    // Top toolbar
    QLineEdit   *m_functionEdit   = nullptr;
    QPushButton *m_refreshButton  = nullptr;
    QComboBox   *m_modeCombo      = nullptr;
    QLabel      *m_statusLabel    = nullptr;

    // Listing view
    QTextEdit   *m_listingView    = nullptr;

    // Last-issued request — used to filter evaluate replies.
    QString m_pendingFunction;       // empty → "$pc" (current frame)
    QString m_pendingPcExpression;   // expression we emitted for PC lookup
    bool    m_awaitingResult = false;
};
