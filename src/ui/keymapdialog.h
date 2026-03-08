#pragma once

#include <QDialog>

class QKeySequenceEdit;
class QLineEdit;
class QPushButton;
class QTableWidget;
class KeymapManager;

// ── KeymapDialog ──────────────────────────────────────────────────────────────
// Lets the user view and rebind keyboard shortcuts.
// Shows a searchable table of all registered actions with their current keys.

class KeymapDialog : public QDialog
{
    Q_OBJECT

public:
    explicit KeymapDialog(KeymapManager *manager, QWidget *parent = nullptr);

private slots:
    void onFilter(const QString &text);
    void onCellDoubleClicked(int row, int col);
    void onApply();
    void onResetAll();

private:
    void populateTable();
    void stopEditing();

    KeymapManager       *m_manager;
    QLineEdit           *m_filterEdit;
    QTableWidget        *m_table;
    QPushButton         *m_applyBtn;
    QPushButton         *m_resetBtn;
    QKeySequenceEdit    *m_keyEditor = nullptr;
    int                  m_editingRow = -1;
};
