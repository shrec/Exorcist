#pragma once

#include <QWidget>
#include <QString>

class QTabWidget;
class QToolButton;
class TerminalWidget;

// ── TerminalPanel ─────────────────────────────────────────────────────────────
// Multi-tab terminal container.  Each tab holds an independent TerminalWidget.
// A "+" button in the tab bar corner lets the user spawn new terminals.
class TerminalPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalPanel(QWidget *parent = nullptr);

    // Convenience: forward to the *current* terminal tab.
    void setWorkingDirectory(const QString &dir);
    void sendCommand(const QString &cmd);
    void sendInput(const QString &text);
    QString recentOutput(int maxLines = 80) const;

private:
    TerminalWidget *addTerminal();
    TerminalWidget *currentTerminal() const;

    QTabWidget  *m_tabs;
    QString      m_workDir;
    int          m_counter = 0;
};
