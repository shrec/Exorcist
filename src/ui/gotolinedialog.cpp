#include "gotolinedialog.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

GoToLineDialog::GoToLineDialog(int currentLine, int totalLines, QWidget *parent)
    : QDialog(parent), m_totalLines(totalLines)
{
    setWindowTitle(tr("Go to Line"));
    setModal(true);
    setMinimumWidth(360);

    // VS dark theme styling — consistent with AboutDialog / KeymapDialog.
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #252526; }"
        "QLabel { color: #d4d4d4; }"
        "QLabel#hintLabel { color: #9cdcfe; font-size: 11px; }"
        "QLabel#errorLabel { color: #f48771; font-size: 11px; }"
        "QLineEdit {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 4px 6px; border-radius: 2px;"
        "  selection-background-color: #264f78;"
        "}"
        "QLineEdit:focus { border: 1px solid #007acc; }"
        "QPushButton {"
        "  background-color: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 5px 16px; border-radius: 2px;"
        "}"
        "QPushButton:hover  { background-color: #3e3e42; }"
        "QPushButton:default { border: 1px solid #007acc; }"
        "QPushButton:disabled { color: #6d6d6d; border-color: #2d2d30; }"
    ));

    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(16, 14, 16, 14);

    auto *prompt = new QLabel(tr("Line number (or line:column):"), this);
    root->addWidget(prompt);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Line number (e.g. 42 or 42:10)"));
    QRegularExpression rx(QStringLiteral("\\d+(:\\d+)?"));
    m_input->setValidator(new QRegularExpressionValidator(rx, this));
    if (currentLine > 0)
        m_input->setText(QString::number(currentLine));
    m_input->selectAll();
    root->addWidget(m_input);

    m_hint = new QLabel(this);
    m_hint->setObjectName(QStringLiteral("hintLabel"));
    m_hint->setText(tr("Current: line %1 of %2").arg(currentLine).arg(totalLines));
    root->addWidget(m_hint);

    m_error = new QLabel(this);
    m_error->setObjectName(QStringLiteral("errorLabel"));
    m_error->setVisible(false);
    root->addWidget(m_error);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto *cancelBtn = new QPushButton(tr("Cancel"), this);
    m_goButton = new QPushButton(tr("Go"), this);
    m_goButton->setDefault(true);
    m_goButton->setAutoDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(m_goButton);
    root->addLayout(btnRow);

    connect(cancelBtn,  &QPushButton::clicked,  this, &QDialog::reject);
    connect(m_goButton, &QPushButton::clicked,  this, &GoToLineDialog::onAccept);
    connect(m_input,    &QLineEdit::returnPressed, this, &GoToLineDialog::onAccept);

    m_input->setFocus();
}

void GoToLineDialog::onAccept()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) {
        m_error->setText(tr("Please enter a line number."));
        m_error->setVisible(true);
        return;
    }

    int line = 0;
    int col  = 0;
    const int colonIdx = text.indexOf(QLatin1Char(':'));
    if (colonIdx < 0) {
        line = text.toInt();
    } else {
        line = text.left(colonIdx).toInt();
        col  = text.mid(colonIdx + 1).toInt();
    }

    if (line <= 0) {
        m_error->setText(tr("Line number must be greater than zero."));
        m_error->setVisible(true);
        return;
    }
    if (m_totalLines > 0 && line > m_totalLines) {
        m_error->setText(tr("Line %1 exceeds total lines (%2).").arg(line).arg(m_totalLines));
        m_error->setVisible(true);
        return;
    }

    emit lineSelected(line, col);
    accept();
}
