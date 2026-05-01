#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QVector>

class QTabWidget;
class QToolButton;
class QComboBox;
class TerminalWidget;

// ── TerminalPanel ─────────────────────────────────────────────────────────────
// Multi-tab terminal container.  Each tab holds an independent TerminalWidget.
//
// Tab bar layout:
//   [ Term1 ][ Term2 ][ ssh:host ][ + ]                       [profile ▼]
//   ^ regular tabs, closable      ^ corner: new-terminal button
//                                                              ^ corner: shell profile selector
//
// Features:
//   • "+" spawns a new terminal using the currently selected profile.
//   • Profile dropdown lets the user pick cmd / powershell / pwsh / bash / wsl
//     (Windows) or bash / zsh / fish / sh (POSIX) before clicking +.
//   • Per-terminal QShortcuts for Ctrl+Shift+= (zoom in) and Ctrl+Shift+-
//     (zoom out).  Global Ctrl+Shift+T to add a new terminal.
//   • Right-click context menu (in TerminalView) provides copy/paste/clear/
//     find/zoom.
//   • Double-click a tab title to rename it.
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
    QString selectedText() const;

    // Open an SSH terminal tab for the given remote host.
    TerminalWidget *addSshTerminal(const QString &label,
                                   const QString &host, int port,
                                   const QString &user,
                                   const QString &privateKeyPath = {});

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // ── Profile ───────────────────────────────────────────────────────────────
    struct Profile {
        QString id;            // stable id, e.g. "cmd", "powershell"
        QString label;         // display name shown in dropdown
        QString program;       // executable, e.g. "cmd.exe"
        QStringList args;      // arguments passed to the program
    };

    void  buildProfiles();
    const Profile *findProfile(const QString &id) const;

    TerminalWidget *addTerminal();                      // uses current profile
    TerminalWidget *addTerminal(const Profile &profile);
    TerminalWidget *currentTerminal() const;
    void renameTab(int index);
    void installShortcuts(TerminalWidget *term);
    QString tabLabelFor(const Profile &profile) const;

    QTabWidget       *m_tabs;
    QComboBox        *m_profileCombo = nullptr;
    QVector<Profile>  m_profiles;
    QString           m_workDir;
    int               m_counter = 0;
};
