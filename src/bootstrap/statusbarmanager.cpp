#include "statusbarmanager.h"

#include <QEvent>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

#include "serviceregistry.h"
#include "sdk/idebugadapter.h"  // DebugFrame, DebugStopReason

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

#ifdef Q_OS_UNIX
#include <QFile>
#include <unistd.h>
#endif

namespace {
double currentMemoryMB()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
#elif defined(Q_OS_UNIX)
    QFile statm(QStringLiteral("/proc/self/statm"));
    if (statm.open(QIODevice::ReadOnly)) {
        const QByteArray data = statm.readAll();
        const QList<QByteArray> parts = data.split(' ');
        if (parts.size() >= 2) {
            long pages = parts[1].toLong();
            return static_cast<double>(pages) * sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0);
        }
    }
#endif
    return -1.0;
}

// VS-style status colors for the build/debug indicator.
constexpr const char *kColorReady    = "#9e9e9e";  // gray
constexpr const char *kColorBuilding = "#75bfff";  // blue
constexpr const char *kColorSuccess  = "#89d185";  // green
constexpr const char *kColorFailed   = "#f44747";  // red
constexpr const char *kColorPaused   = "#dcdcaa";  // yellow
constexpr const char *kColorDebug    = "#89d185";  // green

} // namespace

StatusBarManager::StatusBarManager(QStatusBar *statusBar, QObject *parent)
    : QObject(parent), m_statusBar(statusBar)
{
}

void StatusBarManager::initialize()
{
    m_posLabel           = new QLabel(tr("Ln 1, Col 1"), m_statusBar);
    m_encodingLabel      = new QLabel(tr("UTF-8"), m_statusBar);
    m_backgroundLabel    = new QLabel(m_statusBar);
    m_branchLabel        = new QLabel(m_statusBar);
    m_copilotStatusLabel = new QLabel(tr("— No AI"), m_statusBar);
    m_indexLabel         = new QLabel(m_statusBar);
    m_memoryLabel        = new QLabel(m_statusBar);
    m_buildDebugLabel    = new QLabel(tr("○ Ready"), m_statusBar);

    m_posLabel->setMinimumWidth(110);
    m_encodingLabel->setMinimumWidth(55);
    m_branchLabel->setMinimumWidth(80);
    m_copilotStatusLabel->setMinimumWidth(80);
    m_buildDebugLabel->setMinimumWidth(90);

    const QString labelStyle = QStringLiteral("padding: 0 8px;");
    m_posLabel->setStyleSheet(labelStyle);
    m_encodingLabel->setStyleSheet(labelStyle);
    m_backgroundLabel->setStyleSheet(labelStyle);
    m_branchLabel->setStyleSheet(labelStyle);
    m_indexLabel->setStyleSheet(labelStyle + QStringLiteral("color:#75bfff;"));
    m_indexLabel->setToolTip(tr("Workspace index status"));
    m_memoryLabel->setStyleSheet(labelStyle + QStringLiteral("color:#888;"));
    m_memoryLabel->setToolTip(tr("Process memory (working set)"));
    m_copilotStatusLabel->setStyleSheet(labelStyle + QStringLiteral("color:#888;"));
    m_copilotStatusLabel->setCursor(Qt::PointingHandCursor);
    m_copilotStatusLabel->setToolTip(tr("AI Assistant status — click to open AI panel"));
    m_copilotStatusLabel->installEventFilter(this);

    // Compact build/debug indicator -- VS-style, sits on the LEFT side.
    m_buildDebugLabel->setToolTip(tr("Build / debug state"));
    setBuildDebugStatus(QStringLiteral("○ ") + tr("Ready"),
                        QString::fromUtf8(kColorReady));
    m_statusBar->addWidget(m_buildDebugLabel);  // left-aligned

    // ── VS-style right-aligned indicator buttons ─────────────────────
    // Three clickable QToolButtons on the right edge: line/col, encoding,
    // indent. addPermanentWidget() right-aligns them. Each emits a *Requested
    // signal on click so external code (MainWindow / plugins) can open menus.
    const QString buttonStyle = QStringLiteral(
        "QToolButton {"
        "  background: transparent;"
        "  color: #d4d4d4;"
        "  padding: 0 8px;"
        "  border: none;"
        "  font-size: 11px;"
        "}"
        "QToolButton:hover {"
        "  background: #094771;"
        "  color: white;"
        "}");

    m_lineColButton = new QToolButton(m_statusBar);
    m_lineColButton->setText(tr("Ln 1, Col 1"));
    m_lineColButton->setToolTip(tr("Go to Line/Column"));
    m_lineColButton->setCursor(Qt::PointingHandCursor);
    m_lineColButton->setAutoRaise(true);
    m_lineColButton->setFocusPolicy(Qt::NoFocus);
    m_lineColButton->setStyleSheet(buttonStyle);
    connect(m_lineColButton, &QToolButton::clicked,
            this, &StatusBarManager::gotoLineRequested);

    m_encodingButton = new QToolButton(m_statusBar);
    m_encodingButton->setText(tr("UTF-8"));
    m_encodingButton->setToolTip(tr("Select Encoding"));
    m_encodingButton->setCursor(Qt::PointingHandCursor);
    m_encodingButton->setAutoRaise(true);
    m_encodingButton->setFocusPolicy(Qt::NoFocus);
    m_encodingButton->setStyleSheet(buttonStyle);
    connect(m_encodingButton, &QToolButton::clicked,
            this, &StatusBarManager::encodingMenuRequested);

    m_indentButton = new QToolButton(m_statusBar);
    m_indentButton->setText(tr("Spaces: 4"));
    m_indentButton->setToolTip(tr("Select Indentation"));
    m_indentButton->setCursor(Qt::PointingHandCursor);
    m_indentButton->setAutoRaise(true);
    m_indentButton->setFocusPolicy(Qt::NoFocus);
    m_indentButton->setStyleSheet(buttonStyle);
    connect(m_indentButton, &QToolButton::clicked,
            this, &StatusBarManager::indentMenuRequested);

    m_statusBar->addPermanentWidget(m_copilotStatusLabel);
    m_statusBar->addPermanentWidget(m_indexLabel);
    m_statusBar->addPermanentWidget(m_memoryLabel);
    m_statusBar->addPermanentWidget(m_backgroundLabel);
    m_statusBar->addPermanentWidget(m_branchLabel);
    m_statusBar->addPermanentWidget(m_indentButton);
    m_statusBar->addPermanentWidget(m_encodingButton);
    m_statusBar->addPermanentWidget(m_lineColButton);

    // Legacy QLabels (m_posLabel, m_encodingLabel) are kept as hidden carriers
    // for backward compatibility with existing MainWindow setText() callers,
    // but the new QToolButtons are the visible indicators on the right side.
    m_posLabel->setVisible(false);
    m_encodingLabel->setVisible(false);
    m_posLabel->setParent(m_statusBar);  // ownership only; not shown
    m_encodingLabel->setParent(m_statusBar);

    // Periodic memory usage update (every 5 s)
    auto *memTimer = new QTimer(this);
    connect(memTimer, &QTimer::timeout, this, [this]() {
        const double mb = currentMemoryMB();
        if (mb >= 0.0)
            m_memoryLabel->setText(QStringLiteral("%1 MB").arg(mb, 0, 'f', 1));
    });
    memTimer->start(5000);

    // Show initial value
    const double mb = currentMemoryMB();
    if (mb >= 0.0)
        m_memoryLabel->setText(QStringLiteral("%1 MB").arg(mb, 0, 'f', 1));

    // Single-shot auto-clear timer for transient build states
    // (e.g. revert "Build succeeded" -> "Ready" after 5 s).
    m_autoClearTimer = new QTimer(this);
    m_autoClearTimer->setSingleShot(true);
    connect(m_autoClearTimer, &QTimer::timeout, this, [this]() {
        if (!m_debugActive)
            setBuildDebugStatus(QStringLiteral("○ ") + tr("Ready"),
                                QString::fromUtf8(kColorReady));
    });
}

bool StatusBarManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_copilotStatusLabel && event->type() == QEvent::MouseButtonPress) {
        emit copilotStatusClicked();
        return true;
    }
    return QObject::eventFilter(watched, event);
}

// ---- Build / debug status wiring ------------------------------------------

void StatusBarManager::setBuildDebugStatus(const QString &text,
                                           const QString &color,
                                           int autoClearMs)
{
    if (!m_buildDebugLabel)
        return;
    m_buildDebugLabel->setText(text);
    m_buildDebugLabel->setStyleSheet(
        QStringLiteral("padding: 2px 8px; color: %1; font-size: 11px;").arg(color));
    if (m_autoClearTimer) {
        if (autoClearMs > 0)
            m_autoClearTimer->start(autoClearMs);
        else
            m_autoClearTimer->stop();
    }
}

void StatusBarManager::wireBuildDebugStatus(ServiceRegistry *services)
{
    if (!services || !m_buildDebugLabel)
        return;

    // SIGNAL/SLOT string-based connect -- required because IBuildSystem,
    // IDebugService and IDebugAdapter live in plugin DLLs (libbuild.dll,
    // libdebug.dll). PMF-based connect silently fails across DLL boundaries
    // (MOC duplication).

    if (QObject *buildSys = services->service(QStringLiteral("buildSystem"))) {
        connect(buildSys, SIGNAL(buildOutput(QString,bool)),
                this, SLOT(onBuildOutput(QString,bool)));
        connect(buildSys, SIGNAL(configureFinished(bool,QString)),
                this, SLOT(onConfigureFinished(bool,QString)));
        connect(buildSys, SIGNAL(buildFinished(bool,int)),
                this, SLOT(onBuildFinished(bool,int)));
        connect(buildSys, SIGNAL(cleanFinished(bool)),
                this, SLOT(onCleanFinished(bool)));
    }

    if (QObject *debugSvc = services->service(QStringLiteral("debugService"))) {
        connect(debugSvc, SIGNAL(debugStopped(QList<DebugFrame>)),
                this, SLOT(onDebugStopped(QList<DebugFrame>)));
        connect(debugSvc, SIGNAL(debugTerminated()),
                this, SLOT(onDebugTerminated()));
    }

    if (QObject *adapter = services->service(QStringLiteral("debugAdapter"))) {
        connect(adapter, SIGNAL(started()),
                this, SLOT(onDebugStarted()));
        // The adapter also signals terminated() -- mirror to onDebugTerminated
        // in case the IDebugService bridge isn't connected yet.
        connect(adapter, SIGNAL(terminated()),
                this, SLOT(onDebugTerminated()));
    }
}

// ---- Build slots ----------------------------------------------------------

void StatusBarManager::onBuildOutput(const QString &line, bool isError)
{
    Q_UNUSED(line);
    Q_UNUSED(isError);
    if (m_debugActive)
        return;
    if (!m_buildDebugLabel)
        return;
    // Switch into "Building..." on first output line; avoid restyling on every
    // single line for performance.
    const QString buildingText = QStringLiteral("▶ ") + tr("Building...");
    if (m_buildDebugLabel->text() != buildingText)
        setBuildDebugStatus(buildingText, QString::fromUtf8(kColorBuilding));
}

void StatusBarManager::onConfigureFinished(bool success, const QString &error)
{
    Q_UNUSED(error);
    if (m_debugActive)
        return;
    if (success)
        setBuildDebugStatus(QStringLiteral("○ ") + tr("Configured"),
                            QString::fromUtf8(kColorReady), 5000);
    else
        setBuildDebugStatus(QStringLiteral("⚠ ") + tr("Configure failed"),
                            QString::fromUtf8(kColorFailed), 5000);
}

void StatusBarManager::onBuildFinished(bool success, int exitCode)
{
    if (m_debugActive)
        return;
    if (success) {
        setBuildDebugStatus(QStringLiteral("✓ ") + tr("Build succeeded"),
                            QString::fromUtf8(kColorSuccess), 5000);
    } else {
        setBuildDebugStatus(
            QStringLiteral("✗ ") + tr("Build failed (%1)").arg(exitCode),
            QString::fromUtf8(kColorFailed), 5000);
    }
}

void StatusBarManager::onCleanFinished(bool success)
{
    if (m_debugActive)
        return;
    if (success)
        setBuildDebugStatus(QStringLiteral("○ ") + tr("Cleaned"),
                            QString::fromUtf8(kColorReady), 5000);
    else
        setBuildDebugStatus(QStringLiteral("⚠ ") + tr("Clean failed"),
                            QString::fromUtf8(kColorFailed), 5000);
}

// ---- Debug slots ----------------------------------------------------------

void StatusBarManager::onDebugStarted()
{
    m_debugActive = true;
    setBuildDebugStatus(QStringLiteral("▶ ") + tr("Debugging"),
                        QString::fromUtf8(kColorDebug));
}

void StatusBarManager::onDebugStopped(const QList<DebugFrame> &frames)
{
    // The DebugStopReason is not delivered through IDebugService::debugStopped
    // (only frames). Show a generic "Paused" with the function name of the
    // top frame when available.
    m_debugActive = true;
    QString detail;
    if (!frames.isEmpty() && !frames.first().name.isEmpty())
        detail = QStringLiteral(" at %1").arg(frames.first().name);
    setBuildDebugStatus(
        QStringLiteral("⏸ ") + tr("Paused%1").arg(detail),
        QString::fromUtf8(kColorPaused));
}

void StatusBarManager::onDebugTerminated()
{
    m_debugActive = false;
    setBuildDebugStatus(QStringLiteral("○ ") + tr("Ready"),
                        QString::fromUtf8(kColorReady));
}

// ---- Right-side indicator setters -----------------------------------------

void StatusBarManager::setLineColumn(int line, int col)
{
    const QString text = tr("Ln %1, Col %2").arg(line).arg(col);
    if (m_lineColButton)
        m_lineColButton->setText(text);
    if (m_posLabel)  // mirror to legacy label for backward compat
        m_posLabel->setText(text);
}

void StatusBarManager::setEncoding(const QString &encoding)
{
    const QString text = encoding.isEmpty() ? QStringLiteral("UTF-8") : encoding;
    if (m_encodingButton)
        m_encodingButton->setText(text);
    if (m_encodingLabel)  // mirror to legacy label for backward compat
        m_encodingLabel->setText(text);
}

void StatusBarManager::setIndentInfo(bool spaces, int width)
{
    if (!m_indentButton)
        return;
    const QString text = spaces
        ? tr("Spaces: %1").arg(width)
        : tr("Tabs: %1").arg(width);
    m_indentButton->setText(text);
}
