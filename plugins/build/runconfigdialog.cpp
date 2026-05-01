#include "runconfigdialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

// ── Construction ────────────────────────────────────────────────────────────

RunConfigDialog::RunConfigDialog(const QString &args,
                                 const QString &envBlock,
                                 const QString &cwd,
                                 QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Edit Run Configuration"));
    setModal(true);
    setMinimumWidth(520);
    setMinimumHeight(420);

    // VS dark theme styling — consistent with GoToLineDialog / KeymapDialog.
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #252526; }"
        "QLabel { color: #d4d4d4; }"
        "QLabel#sectionLabel { color: #9cdcfe; font-size: 11px; }"
        "QLineEdit, QPlainTextEdit {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 4px 6px; border-radius: 2px;"
        "  selection-background-color: #264f78;"
        "}"
        "QLineEdit:focus, QPlainTextEdit:focus { border: 1px solid #007acc; }"
        "QCheckBox { color: #d4d4d4; }"
        "QCheckBox::indicator {"
        "  width: 14px; height: 14px;"
        "  background-color: #1e1e1e; border: 1px solid #3f3f46;"
        "}"
        "QCheckBox::indicator:checked { background-color: #007acc; border-color: #007acc; }"
        "QPushButton {"
        "  background-color: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 5px 16px; border-radius: 2px;"
        "}"
        "QPushButton:hover    { background-color: #3e3e42; }"
        "QPushButton:default  { border: 1px solid #007acc; }"
        "QPushButton:disabled { color: #6d6d6d; border-color: #2d2d30; }"
    ));

    buildUi(args, envBlock, cwd);
}

// ── UI ──────────────────────────────────────────────────────────────────────

void RunConfigDialog::buildUi(const QString &args,
                              const QString &envBlock,
                              const QString &cwd)
{
    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(16, 14, 16, 14);

    // Program arguments
    auto *argsLabel = new QLabel(tr("Program arguments"), this);
    argsLabel->setObjectName(QStringLiteral("sectionLabel"));
    root->addWidget(argsLabel);

    m_argsEdit = new QLineEdit(this);
    m_argsEdit->setPlaceholderText(QStringLiteral("--verbose --port=8080"));
    m_argsEdit->setText(args);
    root->addWidget(m_argsEdit);

    // Environment variables
    auto *envLabel = new QLabel(tr("Environment variables (one per line: KEY=value)"), this);
    envLabel->setObjectName(QStringLiteral("sectionLabel"));
    root->addWidget(envLabel);

    m_envEdit = new QPlainTextEdit(this);
    m_envEdit->setPlaceholderText(QStringLiteral("PATH=/usr/local/bin:$PATH\nLOG_LEVEL=debug"));
    m_envEdit->setPlainText(envBlock);
    m_envEdit->setTabChangesFocus(true);
    root->addWidget(m_envEdit, /*stretch=*/1);

    // Working directory + browse button
    auto *cwdLabel = new QLabel(tr("Working directory"), this);
    cwdLabel->setObjectName(QStringLiteral("sectionLabel"));
    root->addWidget(cwdLabel);

    auto *cwdRow = new QHBoxLayout();
    cwdRow->setSpacing(6);
    m_cwdEdit = new QLineEdit(this);
    m_cwdEdit->setPlaceholderText(tr("Leave empty to use workspace root"));
    m_cwdEdit->setText(cwd);
    m_browseBtn = new QPushButton(QStringLiteral("..."), this);
    m_browseBtn->setFixedWidth(36);
    cwdRow->addWidget(m_cwdEdit, /*stretch=*/1);
    cwdRow->addWidget(m_browseBtn);
    root->addLayout(cwdRow);

    // External terminal flag
    m_externalTerm = new QCheckBox(tr("Run in external terminal"), this);
    root->addWidget(m_externalTerm);

    // OK / Cancel
    auto *btnBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setDefault(true);
    root->addWidget(btnBox);

    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_browseBtn, &QPushButton::clicked,
            this, &RunConfigDialog::browseWorkingDir);

    m_argsEdit->setFocus();
}

// ── Slots ───────────────────────────────────────────────────────────────────

void RunConfigDialog::browseWorkingDir()
{
    const QString start = m_cwdEdit->text().trimmed();
    const QString chosen = QFileDialog::getExistingDirectory(
        this,
        tr("Select Working Directory"),
        start,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!chosen.isEmpty())
        m_cwdEdit->setText(chosen);
}

// ── Getters ─────────────────────────────────────────────────────────────────

QString RunConfigDialog::args() const
{
    return m_argsEdit ? m_argsEdit->text() : QString();
}

QString RunConfigDialog::envBlock() const
{
    return m_envEdit ? m_envEdit->toPlainText() : QString();
}

QString RunConfigDialog::workingDir() const
{
    return m_cwdEdit ? m_cwdEdit->text() : QString();
}

bool RunConfigDialog::useExternalTerminal() const
{
    return m_externalTerm && m_externalTerm->isChecked();
}
