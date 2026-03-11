#include "terminalpanel.h"
#include "terminalwidget.h"

#include <QInputDialog>
#include <QMouseEvent>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

TerminalPanel::TerminalPanel(QWidget *parent)
    : QWidget(parent),
      m_tabs(new QTabWidget(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabs);

    m_tabs->setTabsClosable(true);
    m_tabs->setMovable(true);
    m_tabs->setDocumentMode(true);

    // "+" button to add new terminals
    auto *addBtn = new QToolButton(this);
    addBtn->setText(QStringLiteral("+"));
    addBtn->setAutoRaise(true);
    addBtn->setToolTip(tr("New Terminal"));
    m_tabs->setCornerWidget(addBtn, Qt::TopRightCorner);

    connect(addBtn, &QToolButton::clicked, this, [this]() { addTerminal(); });

    // Double-click tab bar to rename
    m_tabs->tabBar()->installEventFilter(this);

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *w = m_tabs->widget(index);
        m_tabs->removeTab(index);
        if (w)
            w->deleteLater();
        // If all tabs closed, create a fresh one
        if (m_tabs->count() == 0)
            addTerminal();
    });

    // Start with one terminal
    addTerminal();
}

TerminalWidget *TerminalPanel::addTerminal()
{
    ++m_counter;
    auto *term = new TerminalWidget(this);
    if (!m_workDir.isEmpty())
        term->setWorkingDirectory(m_workDir);

    const QString label = tr("Terminal %1").arg(m_counter);
    const int idx = m_tabs->addTab(term, label);
    m_tabs->setCurrentIndex(idx);
    term->setFocus();
    return term;
}

TerminalWidget *TerminalPanel::currentTerminal() const
{
    return qobject_cast<TerminalWidget *>(m_tabs->currentWidget());
}

void TerminalPanel::setWorkingDirectory(const QString &dir)
{
    m_workDir = dir;
    // Update all existing terminals
    for (int i = 0; i < m_tabs->count(); ++i) {
        auto *term = qobject_cast<TerminalWidget *>(m_tabs->widget(i));
        if (term)
            term->setWorkingDirectory(dir);
    }
}

void TerminalPanel::sendCommand(const QString &cmd)
{
    auto *term = currentTerminal();
    if (term)
        term->sendCommand(cmd);
}

void TerminalPanel::sendInput(const QString &text)
{
    auto *term = currentTerminal();
    if (term)
        term->sendInput(text);
}

QString TerminalPanel::recentOutput(int maxLines) const
{
    auto *term = currentTerminal();
    return term ? term->recentOutput(maxLines) : QString();
}

QString TerminalPanel::selectedText() const
{
    auto *term = currentTerminal();
    return term ? term->selectedText() : QString();
}

bool TerminalPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_tabs->tabBar() && event->type() == QEvent::MouseButtonDblClick) {
        auto *me = static_cast<QMouseEvent *>(event);
        const int idx = m_tabs->tabBar()->tabAt(me->pos());
        if (idx >= 0) {
            renameTab(idx);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TerminalPanel::renameTab(int index)
{
    bool ok = false;
    const QString current = m_tabs->tabText(index);
    const QString name = QInputDialog::getText(
        this, tr("Rename Terminal"), tr("Name:"),
        QLineEdit::Normal, current, &ok);
    if (ok && !name.isEmpty())
        m_tabs->setTabText(index, name);
}

TerminalWidget *TerminalPanel::addSshTerminal(const QString &label,
                                              const QString &host, int port,
                                              const QString &user,
                                              const QString &privateKeyPath)
{
    auto *term = new TerminalWidget(this);

    // Build the ssh command and args
    const QString program = QStringLiteral("ssh");
    QStringList args;
    args << QStringLiteral("-o") << QStringLiteral("StrictHostKeyChecking=accept-new")
         << QStringLiteral("-o") << QStringLiteral("ConnectTimeout=10");

    if (port != 22)
        args << QStringLiteral("-p") << QString::number(port);

    if (!privateKeyPath.isEmpty())
        args << QStringLiteral("-i") << privateKeyPath;

    args << (user.isEmpty() ? host : user + QStringLiteral("@") + host);

    term->startProgram(program, args);

    const QString tabLabel = label.isEmpty()
        ? tr("SSH: %1").arg(host)
        : label;
    const int idx = m_tabs->addTab(term, tabLabel);
    m_tabs->setCurrentIndex(idx);
    term->setFocus();
    return term;
}
