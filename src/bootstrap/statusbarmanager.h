#pragma once

#include <QObject>
#include <QList>
#include <QString>

class QLabel;
class QStatusBar;
class QTimer;
class QToolButton;
class ServiceRegistry;
struct DebugFrame;

class StatusBarManager : public QObject
{
    Q_OBJECT

public:
    explicit StatusBarManager(QStatusBar *statusBar, QObject *parent = nullptr);

    /// Create labels and add them to the status bar.
    void initialize();

    /// Subscribe to IBuildSystem and IDebugService/IDebugAdapter signals
    /// (registered in @p services) and update the build/debug status label.
    /// Safe to call after plugin DLLs have been loaded — no-op for any
    /// service that is not yet registered. Uses SIGNAL/SLOT string-based
    /// connect because services live in plugin DLLs (cross-DLL safe).
    void wireBuildDebugStatus(ServiceRegistry *services);

    QLabel *posLabel()             const { return m_posLabel; }
    QLabel *encodingLabel()        const { return m_encodingLabel; }
    QLabel *backgroundLabel()      const { return m_backgroundLabel; }
    QLabel *branchLabel()          const { return m_branchLabel; }
    QLabel *copilotStatusLabel()   const { return m_copilotStatusLabel; }
    QLabel *indexLabel()           const { return m_indexLabel; }
    QLabel *memoryLabel()          const { return m_memoryLabel; }
    QLabel *buildDebugStatusLabel() const { return m_buildDebugLabel; }

    /// VS-style right-aligned indicator buttons (clickable).
    QToolButton *lineColButton()   const { return m_lineColButton; }
    QToolButton *encodingButton()  const { return m_encodingButton; }
    QToolButton *indentButton()    const { return m_indentButton; }

    /// Update "Ln X, Col Y" indicator (right side).
    void setLineColumn(int line, int col);

    /// Update encoding indicator (right side). Default "UTF-8".
    void setEncoding(const QString &encoding);

    /// Update indent indicator. @p spaces=true → "Spaces: N", else "Tabs: N".
    void setIndentInfo(bool spaces, int width);

signals:
    void copilotStatusClicked();

    /// Emitted when the user clicks the line/column indicator
    /// (typically used to open a Go-To-Line dialog).
    void gotoLineRequested();

    /// Emitted when the user clicks the encoding indicator
    /// (typically used to open an encoding selection menu).
    void encodingMenuRequested();

    /// Emitted when the user clicks the indent indicator
    /// (typically used to open a Spaces/Tabs/width selection menu).
    void indentMenuRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    // ── Build system slots (string-based connect, cross-DLL safe) ──
    void onBuildOutput(const QString &line, bool isError);
    void onConfigureFinished(bool success, const QString &error);
    void onBuildFinished(bool success, int exitCode);
    void onCleanFinished(bool success);

    // ── Debug service slots (string-based connect, cross-DLL safe) ──
    void onDebugStopped(const QList<DebugFrame> &frames);
    void onDebugTerminated();

    // ── Debug adapter slots ──
    void onDebugStarted();

private:
    /// Apply text + color to the build/debug status label using VS-style colors.
    /// @p autoClearMs > 0 schedules an auto-revert to "Ready" after the delay.
    void setBuildDebugStatus(const QString &text, const QString &color,
                             int autoClearMs = 0);

    QStatusBar *m_statusBar = nullptr;

    QLabel *m_posLabel           = nullptr;
    QLabel *m_encodingLabel      = nullptr;
    QLabel *m_backgroundLabel    = nullptr;
    QLabel *m_branchLabel        = nullptr;
    QLabel *m_copilotStatusLabel = nullptr;
    QLabel *m_indexLabel         = nullptr;
    QLabel *m_memoryLabel        = nullptr;
    QLabel *m_buildDebugLabel    = nullptr;

    // VS-style right-aligned indicator buttons.
    QToolButton *m_lineColButton  = nullptr;
    QToolButton *m_encodingButton = nullptr;
    QToolButton *m_indentButton   = nullptr;

    QTimer *m_autoClearTimer = nullptr;

    // Track whether a debug session is active so build events don't
    // overwrite "Debugging" / "Paused" status while a session is live.
    bool m_debugActive = false;
};
