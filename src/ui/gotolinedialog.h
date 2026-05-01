#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

/// "Go to Line" dialog — jump to a specific line (or line:column) in the active editor.
///
/// Accepts input in the form `42` or `42:10`. Emits `lineSelected(line, column)` on accept;
/// `column` is 0 if no column was supplied. The dialog only validates input — it does not
/// move the cursor itself.
class GoToLineDialog : public QDialog
{
    Q_OBJECT
public:
    GoToLineDialog(int currentLine, int totalLines, QWidget *parent = nullptr);

signals:
    void lineSelected(int line, int column);

private:
    void onAccept();

    int          m_totalLines = 0;
    QLineEdit   *m_input      = nullptr;
    QLabel      *m_hint       = nullptr;
    QLabel      *m_error      = nullptr;
    QPushButton *m_goButton   = nullptr;
};
