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

    // Keyboard → routed through handleInput for shell-exit filtering
    connect(m_view, &TerminalView::inputData,
            this,   &TerminalWidget::handleInput);

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

#if defined(Q_OS_WIN)
    const QString     shell = QStringLiteral("cmd.exe");
    const QStringList args  = {QStringLiteral("/K"), QStringLiteral("chcp 65001>nul")};
#else
    const QString     shell = qEnvironmentVariable("SHELL", "/bin/bash");
    const QStringList args  = {QStringLiteral("-l")};
#endif

    if (!m_backend->start(shell, args, workDir, m_screen->cols(), m_screen->rows())) {
        m_screen->feed(QByteArrayLiteral(
            "\r\n\x1B[31m[Failed to start shell]\x1B[0m\r\n"));
    }

    // The Qt layout may not have fully settled when start() runs, leaving
    // ConPTY at 80×24 while the view is a different size.  After a short
    // delay the layout is guaranteed to be final, so push the real
    // dimensions to ConPTY.  This makes cmd.exe re-query the window size
    // and re-draw the prompt at correct coordinates.
    QTimer::singleShot(150, this, [this]() {
        if (m_backend->isRunning())
            m_backend->resize(m_screen->cols(), m_screen->rows());
    });
}

void TerminalWidget::startProgram(const QString &program,
                                  const QStringList &args,
                                  const QString &workDir)
{
    if (m_started) return;
    m_started = true;
    m_workDir = workDir;

    if (!m_backend->start(program, args, workDir,
                          m_screen->cols(), m_screen->rows())) {
        m_screen->feed(QByteArrayLiteral(
            "\r\n\x1B[31m[Failed to start program]\x1B[0m\r\n"));
    }

    QTimer::singleShot(150, this, [this]() {
        if (m_backend->isRunning())
            m_backend->resize(m_screen->cols(), m_screen->rows());
    });
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
#if defined(Q_OS_WIN)
    sendCommand(QStringLiteral("cd /d \"") + dir + '"');
#else
    sendCommand(QStringLiteral("cd ") + '\'' + QString(dir).replace('\'', "'\\''") + '\'');
#endif
}

void TerminalWidget::sendCommand(const QString &cmd)
{
    if (!m_backend->isRunning()) {
        m_started = false;
        m_shellExited = false;
        startShell(m_workDir);
        // Queue the command after the shell initialises (best-effort: small delay)
        QTimer::singleShot(300, this, [this, cmd]() {
            m_backend->write((cmd + QStringLiteral("\r\n")).toUtf8());
        });
        return;
    }
    m_backend->write((cmd + QStringLiteral("\r\n")).toUtf8());
    m_view->scrollToBottom();
}

void TerminalWidget::sendInput(const QString &text)
{
    if (!m_backend->isRunning()) {
        m_started = false;
        m_shellExited = false;
        startShell(m_workDir);
        QTimer::singleShot(300, this, [this, text]() {
            m_backend->write(text.toUtf8());
            m_view->scrollToBottom();
        });
        return;
    }
    m_backend->write(text.toUtf8());
    m_view->scrollToBottom();
}

QString TerminalWidget::recentOutput(int maxLines) const
{
    return m_screen ? m_screen->recentText(maxLines) : QString();
}

QString TerminalWidget::selectedText() const
{
    return m_view ? m_view->selectedText() : QString();
}

// ── Show event ────────────────────────────────────────────────────────────────

void TerminalWidget::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    // Defer start so that resizeEvent fires first and establishes the
    // correct terminal dimensions.  Without this the ConPTY is created
    // at 80x24 regardless of the actual view size.
    if (!m_started)
        QTimer::singleShot(0, this, [this]() { if (!m_started) startShell(m_workDir); });
}

// ── Shell exit ────────────────────────────────────────────────────────────────

void TerminalWidget::onShellFinished(int exitCode)
{
    const QString msg = QStringLiteral(
        "\r\n\x1B[33m[Process exited with code %1 — press Enter to restart]\x1B[0m\r\n")
        .arg(exitCode);
    m_screen->feed(msg.toUtf8());
    m_started    = false;
    m_shellExited = true;
}

void TerminalWidget::handleInput(const QByteArray &data)
{
    if (m_shellExited) {
        // After shell exits, only Enter restarts — ignore everything else
        if (data.contains('\r') || data.contains('\n')) {
            m_shellExited = false;
            startShell(m_workDir);
        }
        return;
    }
    m_backend->write(data);
}
