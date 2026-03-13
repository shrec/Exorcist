#include "problemspanel.h"
#include "../sdk/idiagnosticsservice.h"
#include "../build/outputpanel.h"

#include <QTreeWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QFileInfo>

ProblemsPanel::ProblemsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(4, 2, 4, 2);

    m_filterCombo = new QComboBox;
    m_filterCombo->addItem(tr("All"),      -1);
    m_filterCombo->addItem(tr("Errors"),    static_cast<int>(ProblemEntry::Error));
    m_filterCombo->addItem(tr("Warnings"),  static_cast<int>(ProblemEntry::Warning));
    m_filterCombo->addItem(tr("Info"),      static_cast<int>(ProblemEntry::Info));
    toolbar->addWidget(m_filterCombo);

    toolbar->addStretch();

    m_countLabel = new QLabel;
    toolbar->addWidget(m_countLabel);

    layout->addLayout(toolbar);

    // Tree
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Severity"), tr("Message"), tr("File"), tr("Line"), tr("Source")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(false);
    layout->addWidget(m_tree);

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProblemsPanel::onFilterChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &ProblemsPanel::onItemDoubleClicked);
}

void ProblemsPanel::setDiagnosticsService(IDiagnosticsService *svc)
{
    m_diagSvc = svc;
}

void ProblemsPanel::setOutputPanel(OutputPanel *panel)
{
    if (m_outputPanel)
        disconnect(m_outputPanel, nullptr, this, nullptr);

    m_outputPanel = panel;
    if (m_outputPanel) {
        connect(m_outputPanel, &OutputPanel::problemsChanged,
                this, &ProblemsPanel::refresh);
    }
}

void ProblemsPanel::refresh()
{
    rebuildTree();
}

int ProblemsPanel::problemCount() const
{
    return m_tree->topLevelItemCount();
}

void ProblemsPanel::onFilterChanged(int /*index*/)
{
    rebuildTree();
}

void ProblemsPanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    const QString file = item->data(0, Qt::UserRole).toString();
    const int     line = item->data(0, Qt::UserRole + 1).toInt();
    const int     col  = item->data(0, Qt::UserRole + 2).toInt();

    if (!file.isEmpty())
        emit navigateToFile(file, line, col);
}

QList<ProblemsPanel::ProblemEntry> ProblemsPanel::gatherProblems() const
{
    QList<ProblemEntry> result;

    // Gather LSP diagnostics
    if (m_diagSvc) {
        for (const auto &d : m_diagSvc->diagnostics()) {
            ProblemEntry entry;
            entry.file    = d.filePath;
            entry.line    = d.line;
            entry.column  = d.column;
            entry.message = d.message;
            entry.source  = d.source;

            switch (d.severity) {
            case SDKDiagnostic::Error:   entry.severity = ProblemEntry::Error;   break;
            case SDKDiagnostic::Warning: entry.severity = ProblemEntry::Warning; break;
            case SDKDiagnostic::Info:    entry.severity = ProblemEntry::Info;    break;
            case SDKDiagnostic::Hint:    entry.severity = ProblemEntry::Hint;    break;
            }
            result.append(entry);
        }
    }

    // Gather build problems
    if (m_outputPanel) {
        for (const auto &p : m_outputPanel->problems()) {
            ProblemEntry entry;
            entry.file    = p.file;
            entry.line    = p.line;
            entry.column  = p.column;
            entry.message = p.message;
            entry.source  = QStringLiteral("build");

            switch (p.severity) {
            case OutputPanel::ProblemMatch::Error:   entry.severity = ProblemEntry::Error;   break;
            case OutputPanel::ProblemMatch::Warning: entry.severity = ProblemEntry::Warning; break;
            case OutputPanel::ProblemMatch::Info:    entry.severity = ProblemEntry::Info;    break;
            }
            result.append(entry);
        }
    }

    return result;
}

bool ProblemsPanel::matchesFilter(const ProblemEntry &entry) const
{
    const int filterVal = m_filterCombo->currentData().toInt();
    if (filterVal < 0) // "All"
        return true;
    return static_cast<int>(entry.severity) == filterVal;
}

void ProblemsPanel::rebuildTree()
{
    m_tree->clear();

    const auto problems = gatherProblems();
    int displayed = 0;

    for (const auto &p : problems) {
        if (!matchesFilter(p))
            continue;

        auto *item = new QTreeWidgetItem(m_tree);
        item->setIcon(0, severityIcon(p.severity));
        item->setText(0, severityText(p.severity));
        item->setText(1, p.message);
        item->setText(2, QFileInfo(p.file).fileName());
        item->setText(3, QString::number(p.line));
        item->setText(4, p.source);

        // Store navigation data
        item->setData(0, Qt::UserRole,     p.file);
        item->setData(0, Qt::UserRole + 1, p.line);
        item->setData(0, Qt::UserRole + 2, p.column);

        item->setToolTip(1, p.message);
        item->setToolTip(2, p.file);

        ++displayed;
    }

    int errors = 0, warnings = 0;
    for (const auto &p : problems) {
        if (p.severity == ProblemEntry::Error)   ++errors;
        if (p.severity == ProblemEntry::Warning) ++warnings;
    }

    m_countLabel->setText(tr("%1 errors, %2 warnings").arg(errors).arg(warnings));

    if (errors > 0)
        m_countLabel->setStyleSheet(QStringLiteral("color: #e74c3c; font-weight: bold;"));
    else if (warnings > 0)
        m_countLabel->setStyleSheet(QStringLiteral("color: #f39c12; font-weight: bold;"));
    else
        m_countLabel->setStyleSheet(QStringLiteral("color: #2ecc71;"));
}

QIcon ProblemsPanel::severityIcon(ProblemEntry::Severity sev) const
{
    switch (sev) {
    case ProblemEntry::Error:   return style()->standardIcon(QStyle::SP_MessageBoxCritical);
    case ProblemEntry::Warning: return style()->standardIcon(QStyle::SP_MessageBoxWarning);
    case ProblemEntry::Info:    return style()->standardIcon(QStyle::SP_MessageBoxInformation);
    case ProblemEntry::Hint:    return style()->standardIcon(QStyle::SP_MessageBoxInformation);
    }
    return {};
}

QString ProblemsPanel::severityText(ProblemEntry::Severity sev) const
{
    switch (sev) {
    case ProblemEntry::Error:   return tr("Error");
    case ProblemEntry::Warning: return tr("Warning");
    case ProblemEntry::Info:    return tr("Info");
    case ProblemEntry::Hint:    return tr("Hint");
    }
    return {};
}
