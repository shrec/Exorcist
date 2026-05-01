#include "watchpointdialog.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

WatchpointDialog::WatchpointDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Watchpoint"));
    setMinimumWidth(420);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    // Help text
    auto *help = new QLabel(
        tr("A watchpoint pauses execution when a memory location changes,\n"
           "is read, or is accessed. Enter a variable name or expression\n"
           "(e.g. \"myVar\", \"*p\", \"arr[3]\")."),
        this);
    help->setStyleSheet(QStringLiteral("color: #9d9d9d; font-size: 11px;"));
    help->setWordWrap(true);
    root->addWidget(help);

    // Expression row
    auto *exprRow = new QHBoxLayout;
    auto *exprLbl = new QLabel(tr("Expression:"), this);
    exprLbl->setMinimumWidth(80);
    m_exprInput = new QLineEdit(this);
    m_exprInput->setPlaceholderText(tr("Variable name or memory expression"));
    m_exprInput->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #2b2b2b; color: #d4d4d4; "
        "border: 1px solid #3c3c3c; padding: 6px; "
        "font-family: 'Consolas','Courier New',monospace; font-size: 13px; }"
        "QLineEdit:focus { border: 1px solid #007acc; }"));
    exprRow->addWidget(exprLbl);
    exprRow->addWidget(m_exprInput, 1);
    root->addLayout(exprRow);

    // Type row
    auto *typeRow = new QHBoxLayout;
    auto *typeLbl = new QLabel(tr("Trigger on:"), this);
    typeLbl->setMinimumWidth(80);
    m_typeCombo = new QComboBox(this);
    // Order matches DebugWatchpoint::Type enum: Write=0, Read=1, Access=2.
    m_typeCombo->addItem(tr("Write (value changes)"));
    m_typeCombo->addItem(tr("Read"));
    m_typeCombo->addItem(tr("Access (read or write)"));
    m_typeCombo->setCurrentIndex(0);
    m_typeCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background: #2b2b2b; color: #d4d4d4; "
        "border: 1px solid #3c3c3c; padding: 4px 6px; font-size: 12px; }"
        "QComboBox:focus { border: 1px solid #007acc; }"
        "QComboBox QAbstractItemView { background: #2b2b2b; color: #d4d4d4; "
        "selection-background-color: #094771; border: 1px solid #3c3c3c; }"));
    typeRow->addWidget(typeLbl);
    typeRow->addWidget(m_typeCombo, 1);
    root->addLayout(typeRow);

    root->addStretch();

    // Buttons
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_okBtn = new QPushButton(tr("OK"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_okBtn->setDefault(true);
    btnRow->addWidget(m_okBtn);
    btnRow->addWidget(m_cancelBtn);
    root->addLayout(btnRow);

    // Dialog-level dark theme
    setStyleSheet(QStringLiteral(
        "QDialog { background: #252526; }"
        "QLabel { color: #d4d4d4; font-size: 12px; }"
        "QPushButton { background: #2d2d30; color: #d4d4d4; "
        "border: 1px solid #3c3c3c; padding: 5px 14px; min-width: 70px; }"
        "QPushButton:hover { background: #3e3e42; }"
        "QPushButton:pressed { background: #094771; }"
        "QPushButton:default { background: #0e639c; border-color: #1177bb; }"
        "QPushButton:default:hover { background: #1177bb; }"));

    connect(m_okBtn,     &QPushButton::clicked, this, &WatchpointDialog::onAccept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_exprInput, &QLineEdit::returnPressed,
            this, &WatchpointDialog::onAccept);

    m_exprInput->setFocus();
}

WatchpointDialog::~WatchpointDialog() = default;

void WatchpointDialog::setExpression(const QString &expression)
{
    if (m_exprInput)
        m_exprInput->setText(expression);
}

QString WatchpointDialog::expression() const
{
    return m_exprInput ? m_exprInput->text().trimmed() : QString();
}

int WatchpointDialog::watchType() const
{
    return m_typeCombo ? m_typeCombo->currentIndex() : 0;
}

void WatchpointDialog::onAccept()
{
    const QString expr = expression();
    if (expr.isEmpty()) {
        // Refuse empty expression — keep dialog open and refocus input.
        m_exprInput->setFocus();
        return;
    }
    emit watchpointConfigured(expr, watchType());
    accept();
}
