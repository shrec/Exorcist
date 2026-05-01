#include "breakpointconditiondialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

BreakpointConditionDialog::BreakpointConditionDialog(QWidget *parent,
                                                     const QString &existingCondition,
                                                     int line)
    : QDialog(parent)
{
    setWindowTitle(tr("Breakpoint Condition"));
    setModal(true);
    setMinimumWidth(420);

    // VS dark theme palette
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #252526; color: #d4d4d4; }"
        "QLabel { color: #d4d4d4; }"
        "QLineEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  padding: 4px 6px;"
        "  selection-background-color: #094771;"
        "}"
        "QLineEdit:focus { border: 1px solid #007acc; }"
        "QPushButton {"
        "  background-color: #2d2d30;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  padding: 4px 14px;"
        "  min-width: 70px;"
        "}"
        "QPushButton:hover { background-color: #3e3e42; }"
        "QPushButton:default { background-color: #0e639c; border: 1px solid #1177bb; }"
        "QPushButton:default:hover { background-color: #1177bb; }"
    ));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    QString headerText;
    if (line > 0)
        headerText = tr("Stop at line %1 only when this expression is true:").arg(line);
    else
        headerText = tr("Stop only when this expression is true:");
    auto *header = new QLabel(headerText, this);
    layout->addWidget(header);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("e.g. i == 42 && ptr != nullptr"));
    m_input->setText(existingCondition);
    m_input->selectAll();
    layout->addWidget(m_input);

    auto *hint = new QLabel(
        tr("Leave empty to remove the condition (unconditional breakpoint)."),
        this);
    QFont hintFont = hint->font();
    hintFont.setPointSizeF(qMax(7.0, hintFont.pointSizeF() - 1.0));
    hint->setFont(hintFont);
    hint->setStyleSheet(QStringLiteral("color: #808080;"));
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *okBtn = buttons->button(QDialogButtonBox::Ok)) {
        okBtn->setText(tr("OK"));
        okBtn->setDefault(true);
    }
    if (auto *cancelBtn = buttons->button(QDialogButtonBox::Cancel))
        cancelBtn->setText(tr("Cancel"));
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    m_input->setFocus();
}

QString BreakpointConditionDialog::condition() const
{
    return m_input ? m_input->text().trimmed() : QString();
}
