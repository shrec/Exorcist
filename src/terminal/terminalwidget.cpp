#include "terminalwidget.h"
#include "terminalscreen.h"
#include "ptybackend.h"
#include "terminalview.h"

#include <QApplication>
#include <QScrollBar>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

TerminalWidget::TerminalWidget(QWidget *parent)
    : QWidget(parent),
      m_screen (new TerminalScreen(80, 24, this)),
      m_backend(new PtyBackend(this)),
      m_view   (new TerminalView(m_screen, this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, 1);

    // Keyboard → PTY
    connect(m_view,    &TerminalView::inputData,
            m_backend, &PtyBackend::write);

    // PTY output → screen parser
    connect(m_backend, &PtyBackend::dataReceived,
            m_screen,  &TerminalScreen::feed);

    // Pixel resize → update PTY window size
    connect(m_view, &TerminalView::sizeChanged,
            this,   [this](int cols, int rows) {
        m_backend->resize(cols, rows);
    });

    // Bell → system beep
    connect(m_screen, &TerminalScreen::bell,
            this, []() { QApplication::beep(); });

    // Shell exit → status message + option to restart
    connect(m_backend, &PtyBackend::finished,
            this, &TerminalWidget::onShellFinished);
}

TerminalWidget::~TerminalWidget()
{
    m_backend->terminate();
}

// ── Public API ────────────────────────────────────────────────────────────────

void TerminalWidget::startShell(const QString &workDir)
{
    if (m_started) return;
    m_started  = true;
    m_workDir  = workDir;

#ifdef _WIN32
    const QString     shell = QStringLiteral("cmd.exe");
    const QStringList args;
#else
    const QString     shell = qEnvironmentVariable("SHELL", "/bin/bash");
    const QStringList args  = {QStringLiteral("-l")};
#endif

    if (!m_backend->start(shell, args, workDir)) {
        m_screen->feed(QByteArrayLiteral(
            "\r\n\x1B[31m[Failed to start shell]\x1B[0m\r\n"));
    }
}

void TerminalWidget::setWorkingDirectory(const QString &dir)
{
    m_workDir = dir;

    if (!m_backend->isRunning()) {
        m_started = false;
        startShell(dir);
        return;
    }

    // Send a 'cd' command to the running shell
#ifdef _WIN32
    sendCommand(QStringLiteral("cd /d \"") + dir + '"');
#else
    sendCommand(QStringLiteral("cd ") + '\'' + QString(dir).replace('\'', "'\\''") + '\'');
#endif
}

void TerminalWidget::sendCommand(const QString &cmd)
{
    if (!m_backend->isRunning()) {
        m_started = false;
        startShell(m_workDir);
        // Queue the command after the shell initialises (best-effort: small delay)
        QTimer::singleShot(300, this, [this, cmd]() {
            m_backend->write((cmd + '\n').toLocal8Bit());
        });
        return;
    }
    m_backend->write((cmd + '\n').toLocal8Bit());
    m_view->scrollToBottom();
}

// ── Show event ────────────────────────────────────────────────────────────────

void TerminalWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Auto-start shell on first show
    if (!m_started)
        startShell(m_workDir);
}

// ── Shell exit ────────────────────────────────────────────────────────────────

void TerminalWidget::onShellFinished(int exitCode)
{
    const QString msg = QStringLiteral(
        "\r\n\x1B[33m[Process exited with code %1 — press Enter to restart]\x1B[0m\r\n")
        .arg(exitCode);
    m_screen->feed(msg.toLocal8Bit());
    m_started = false;

    // Restart on next keystroke (Enter) via a one-shot connection
    connect(m_view, &TerminalView::inputData, this,
        [this](const QByteArray &data) {
            if (data == "\r" || data == "\n") {
                startShell(m_workDir);
            }
        }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
}
