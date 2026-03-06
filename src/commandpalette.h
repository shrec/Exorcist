#pragma once

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListWidget;

// A lightweight command/file picker that appears centered on the parent window.
//
// FileMode  (Ctrl+P):          fuzzy search over files in the project root.
// CommandMode (Ctrl+Shift+P):  fixed list of registered commands.
//
// After the dialog closes, check selectedFile() or selectedCommand().

class CommandPalette : public QDialog
{
    Q_OBJECT

public:
    enum Mode { FileMode, CommandMode };

    explicit CommandPalette(Mode mode, QWidget *parent = nullptr);

    // FileMode: call before exec() to populate file list.
    void setFiles(const QStringList &filePaths);

    // CommandMode: call before exec() to populate command list.
    // Each entry is "Command Name\tShortcut" (shortcut is optional).
    void setCommands(const QStringList &commands);

    // Valid after accept().
    QString selectedFile() const;
    int     selectedCommandIndex() const;  // index into setCommands() list

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void filterItems(const QString &text);
    void acceptCurrent();

private:
    Mode        m_mode;
    QLineEdit  *m_input;
    QListWidget *m_list;
    QStringList  m_allItems;  // full list (files or commands)
    int          m_selectedIndex = -1;
};
