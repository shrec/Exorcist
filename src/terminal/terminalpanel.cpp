#include "terminalpanel.h"
#include "terminalwidget.h"

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

    connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *w = m_tabs->widget(index);
        m_tabs->removeTab(index);
        if (w) {
            // Delete synchronously so the PTY backend is fully cleaned up
            // before we potentially create a replacement tab.
            delete w;
        }
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
