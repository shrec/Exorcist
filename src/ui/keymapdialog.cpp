#include "keymapdialog.h"
#include "keymapmanager.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

KeymapDialog::KeymapDialog(KeymapManager *manager, QWidget *parent)
    : QDialog(parent), m_manager(manager)
{
    setWindowTitle(tr("Keyboard Shortcuts"));
    resize(600, 500);

    auto *vbox = new QVBoxLayout(this);

    // Filter / search bar
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText(tr("Search shortcuts…"));
    m_filterEdit->setClearButtonEnabled(true);
    vbox->addWidget(m_filterEdit);

    // Table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({tr("Action"), tr("Shortcut"), tr("Default")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->setColumnWidth(1, 160);
    m_table->setColumnWidth(2, 160);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    vbox->addWidget(m_table);

    // Buttons
    auto *btnRow = new QDialogButtonBox(this);
    m_resetBtn = btnRow->addButton(tr("Reset All"), QDialogButtonBox::ResetRole);
    m_applyBtn = btnRow->addButton(tr("Apply"), QDialogButtonBox::ApplyRole);
    btnRow->addButton(QDialogButtonBox::Close);
    vbox->addWidget(btnRow);

    populateTable();

    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &KeymapDialog::onFilter);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &KeymapDialog::onCellDoubleClicked);
    connect(m_applyBtn, &QPushButton::clicked,
            this, &KeymapDialog::onApply);
    connect(m_resetBtn, &QPushButton::clicked,
            this, &KeymapDialog::onResetAll);
    connect(btnRow, &QDialogButtonBox::rejected,
            this, &QDialog::accept);
}

void KeymapDialog::populateTable()
{
    const QStringList ids = m_manager->actionIds();
    m_table->setRowCount(ids.size());

    for (int i = 0; i < ids.size(); ++i) {
        const QString &id = ids[i];

        auto *nameItem = new QTableWidgetItem(m_manager->actionText(id));
        nameItem->setData(Qt::UserRole, id);
        m_table->setItem(i, 0, nameItem);

        auto *keyItem = new QTableWidgetItem(
            m_manager->shortcut(id).toString(QKeySequence::NativeText));
        m_table->setItem(i, 1, keyItem);

        auto *defaultItem = new QTableWidgetItem(
            m_manager->defaultShortcut(id).toString(QKeySequence::NativeText));
        defaultItem->setFlags(defaultItem->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(i, 2, defaultItem);
    }
}

void KeymapDialog::onFilter(const QString &text)
{
    for (int i = 0; i < m_table->rowCount(); ++i) {
        const QString name = m_table->item(i, 0)->text();
        const QString key  = m_table->item(i, 1)->text();
        const bool match = text.isEmpty()
                           || name.contains(text, Qt::CaseInsensitive)
                           || key.contains(text, Qt::CaseInsensitive);
        m_table->setRowHidden(i, !match);
    }
}

void KeymapDialog::onCellDoubleClicked(int row, int col)
{
    Q_UNUSED(col)
    stopEditing();

    m_editingRow = row;
    m_keyEditor = new QKeySequenceEdit(this);
    m_keyEditor->setKeySequence(
        QKeySequence(m_table->item(row, 1)->text()));
    m_table->setCellWidget(row, 1, m_keyEditor);
    m_keyEditor->setFocus();

    connect(m_keyEditor, &QKeySequenceEdit::editingFinished,
            this, &KeymapDialog::stopEditing);
}

void KeymapDialog::stopEditing()
{
    if (m_editingRow < 0 || !m_keyEditor) return;

    const QKeySequence seq = m_keyEditor->keySequence();
    m_table->item(m_editingRow, 1)->setText(
        seq.toString(QKeySequence::NativeText));
    m_table->removeCellWidget(m_editingRow, 1);

    m_keyEditor = nullptr;
    m_editingRow = -1;
}

void KeymapDialog::onApply()
{
    stopEditing();

    for (int i = 0; i < m_table->rowCount(); ++i) {
        const QString id = m_table->item(i, 0)->data(Qt::UserRole).toString();
        const QKeySequence newKey(m_table->item(i, 1)->text());
        m_manager->setShortcut(id, newKey);
    }
    m_manager->save();
}

void KeymapDialog::onResetAll()
{
    m_manager->resetAllToDefaults();
    m_manager->save();

    // Refresh table
    const QStringList ids = m_manager->actionIds();
    for (int i = 0; i < ids.size() && i < m_table->rowCount(); ++i) {
        m_table->item(i, 1)->setText(
            m_manager->shortcut(ids[i]).toString(QKeySequence::NativeText));
    }
}
