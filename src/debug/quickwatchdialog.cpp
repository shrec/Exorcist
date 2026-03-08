#include "quickwatchdialog.h"
#include "watchtreemodel.h"
#include "idebugadapter.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeView>
#include <QVBoxLayout>

QuickWatchDialog::QuickWatchDialog(IDebugAdapter *adapter, QWidget *parent)
    : QDialog(parent)
    , m_adapter(adapter)
    , m_model(new WatchTreeModel(this))
{
    setWindowTitle(tr("QuickWatch"));
    setMinimumSize(500, 400);
    resize(600, 500);

    m_model->setAdapter(m_adapter);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(12, 12, 12, 12);

    // Expression input row
    auto *inputRow = new QHBoxLayout;
    auto *label = new QLabel(tr("Expression:"), this);
    m_exprInput = new QLineEdit(this);
    m_exprInput->setPlaceholderText(tr("Enter expression to evaluate..."));
    m_exprInput->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #2b2b2b; color: #d4d4d4; border: 1px solid #3c3c3c; "
        "padding: 6px; font-family: 'Consolas','Courier New',monospace; font-size: 13px; }"));

    m_evalBtn = new QPushButton(tr("Reevaluate"), this);

    inputRow->addWidget(label);
    inputRow->addWidget(m_exprInput, 1);
    inputRow->addWidget(m_evalBtn);
    root->addLayout(inputRow);

    // Tree view
    m_tree = new QTreeView(this);
    m_tree->setModel(m_model);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setAnimated(true);
    m_tree->header()->setStretchLastSection(true);
    m_tree->setColumnWidth(0, 180);
    m_tree->setColumnWidth(1, 250);
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeView { background: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; "
        "font-family: 'Consolas','Courier New',monospace; font-size: 12px; "
        "alternate-background-color: #252526; }"
        "QTreeView::item:selected { background: #264f78; }"
        "QHeaderView::section { background: #2d2d2d; color: #d4d4d4; "
        "border: 1px solid #3c3c3c; padding: 4px; }"));

    root->addWidget(m_tree, 1);

    // Button row
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_addWatchBtn = new QPushButton(tr("Add Watch"), this);
    m_closeBtn = new QPushButton(tr("Close"), this);
    btnRow->addWidget(m_addWatchBtn);
    btnRow->addWidget(m_closeBtn);
    root->addLayout(btnRow);

    // Connections
    connect(m_exprInput, &QLineEdit::returnPressed, this, &QuickWatchDialog::onEvaluate);
    connect(m_evalBtn, &QPushButton::clicked, this, &QuickWatchDialog::onEvaluate);
    connect(m_addWatchBtn, &QPushButton::clicked, this, &QuickWatchDialog::onAddWatch);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    setStyleSheet(QStringLiteral(
        "QDialog { background: #252526; }"
        "QLabel { color: #d4d4d4; }"
        "QPushButton { background: #0e639c; color: #ffffff; border: none; "
        "padding: 6px 16px; border-radius: 3px; font-size: 12px; }"
        "QPushButton:hover { background: #1177bb; }"
        "QPushButton:pressed { background: #0d5a8a; }"));
}

QuickWatchDialog::~QuickWatchDialog()
{
    // Clean up var-objects for this dialog's model
    m_model->clearAll();
}

void QuickWatchDialog::setExpression(const QString &expression)
{
    m_exprInput->setText(expression);
    if (!expression.isEmpty())
        onEvaluate();
}

void QuickWatchDialog::onEvaluate()
{
    const QString expr = m_exprInput->text().trimmed();
    if (expr.isEmpty()) return;

    // Clear previous result and add new watch
    m_model->clearAll();
    m_model->addWatch(expr);
}

void QuickWatchDialog::onAddWatch()
{
    const QString expr = m_exprInput->text().trimmed();
    if (!expr.isEmpty())
        emit addToWatch(expr);
}
