#include "statusbarmanager.h"

#include <QEvent>
#include <QLabel>
#include <QStatusBar>
#include <QTimer>

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
    m_copilotStatusLabel = new QLabel(tr("\u2014 No AI"), m_statusBar);
    m_indexLabel         = new QLabel(m_statusBar);
    m_memoryLabel        = new QLabel(m_statusBar);

    m_posLabel->setMinimumWidth(110);
    m_encodingLabel->setMinimumWidth(55);
    m_branchLabel->setMinimumWidth(80);
    m_copilotStatusLabel->setMinimumWidth(80);

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
    m_copilotStatusLabel->setToolTip(tr("AI Assistant status \u2014 click to open AI panel"));
    m_copilotStatusLabel->installEventFilter(this);

    m_statusBar->addPermanentWidget(m_copilotStatusLabel);
    m_statusBar->addPermanentWidget(m_indexLabel);
    m_statusBar->addPermanentWidget(m_memoryLabel);
    m_statusBar->addPermanentWidget(m_backgroundLabel);
    m_statusBar->addPermanentWidget(m_branchLabel);
    m_statusBar->addPermanentWidget(m_encodingLabel);
    m_statusBar->addPermanentWidget(m_posLabel);

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
}

bool StatusBarManager::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_copilotStatusLabel && event->type() == QEvent::MouseButtonPress) {
        emit copilotStatusClicked();
        return true;
    }
    return QObject::eventFilter(watched, event);
}
