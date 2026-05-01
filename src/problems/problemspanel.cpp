#include "problemspanel.h"
#include "../sdk/idiagnosticsservice.h"
#include "../build/outputpanel.h"

#include <QTreeWidget>
#include <QHeaderView>
#include <QToolButton>
#include <QLineEdit>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QFileInfo>
#include <QFont>
#include <QHash>
#include <QMap>

namespace {

QString fileGroupKey(const QString &absPath)
{
    // Use absolute path as the unique grouping key, but show only filename in UI.
    return absPath;
}

} // namespace

ProblemsPanel::ProblemsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Toolbar (severity toggles + search + count) ──────────────────────────
    auto *toolbarWidget = new QWidget;
    toolbarWidget->setFixedHeight(30);
    toolbarWidget->setStyleSheet(QStringLiteral(
        "QWidget { background: #2d2d30; }"
        "QToolButton {"
        "  background: transparent; color: #d4d4d4; border: 1px solid transparent;"
        "  padding: 2px 6px; margin: 0; font-size: 12px; min-width: 30px;"
        "}"
        "QToolButton:hover { background: #3e3e42; border: 1px solid #555558; }"
        "QToolButton:checked { background: #094771; color: #ffffff; border: 1px solid #1177bb; }"
        "QToolButton:!checked { color: #6a6a6a; }"
        "QLineEdit {"
        "  background: #3c3c3c; color: #d4d4d4; border: 1px solid #555558;"
        "  padding: 2px 6px; font-size: 12px; selection-background-color: #094771;"
        "}"
        "QLineEdit:focus { border: 1px solid #1177bb; }"
        "QLabel { color: #9d9d9d; font-size: 12px; background: transparent; }"
    ));
    auto *toolbarInner = new QHBoxLayout(toolbarWidget);
    toolbarInner->setContentsMargins(6, 2, 6, 2);
    toolbarInner->setSpacing(4);

    // Severity toggle buttons (checkable, default all on).
    m_btnErrors = new QToolButton;
    m_btnErrors->setCheckable(true);
    m_btnErrors->setChecked(true);
    m_btnErrors->setText(QStringLiteral("⊘ Errors"));   // ⊘
    m_btnErrors->setToolTip(tr("Show errors"));

    m_btnWarnings = new QToolButton;
    m_btnWarnings->setCheckable(true);
    m_btnWarnings->setChecked(true);
    m_btnWarnings->setText(QStringLiteral("⚠ Warnings")); // ⚠
    m_btnWarnings->setToolTip(tr("Show warnings"));

    m_btnInfo = new QToolButton;
    m_btnInfo->setCheckable(true);
    m_btnInfo->setChecked(true);
    m_btnInfo->setText(QStringLiteral("ℹ Info"));        // ℹ
    m_btnInfo->setToolTip(tr("Show info / hints"));

    toolbarInner->addWidget(m_btnErrors);
    toolbarInner->addWidget(m_btnWarnings);
    toolbarInner->addWidget(m_btnInfo);

    // Search field.
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(tr("Filter problems..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(160);
    toolbarInner->addSpacing(6);
    toolbarInner->addWidget(m_searchEdit, 1);

    m_countLabel = new QLabel;
    toolbarInner->addWidget(m_countLabel);

    layout->addWidget(toolbarWidget);

    // ── Tree (grouped by file) ───────────────────────────────────────────────
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Severity"), tr("Message"), tr("Line:Col"), tr("Source")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(true);          // show expand/collapse arrows for file groups
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);    // we use double-click for navigation
    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget {"
        "  background: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: none;"
        "  font-size: 12px;"
        "}"
        "QTreeWidget::item { padding: 2px 0; border: none; }"
        "QTreeWidget::item:hover { background: #2a2d2e; }"
        "QTreeWidget::item:selected {"
        "  background: #094771; color: #ffffff;"
        "}"
        "QTreeWidget::branch:has-children:!has-siblings:closed,"
        "QTreeWidget::branch:closed:has-children:has-siblings {"
        "  border-image: none; image: none;"
        "}"
        "QHeaderView::section {"
        "  background: #252526;"
        "  color: #858585;"
        "  border: none;"
        "  border-right: 1px solid #3e3e42;"
        "  border-bottom: 1px solid #3e3e42;"
        "  padding: 3px 6px;"
        "  font-size: 11px;"
        "}"
        "QScrollBar:vertical { background: #1e1e1e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #424242; min-height: 20px; border-radius: 5px; }"
        "QScrollBar::handle:vertical:hover { background: #686868; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
    layout->addWidget(m_tree);

    // ── Wiring ───────────────────────────────────────────────────────────────
    connect(m_btnErrors,   &QToolButton::toggled, this, &ProblemsPanel::onFilterChanged);
    connect(m_btnWarnings, &QToolButton::toggled, this, &ProblemsPanel::onFilterChanged);
    connect(m_btnInfo,     &QToolButton::toggled, this, &ProblemsPanel::onFilterChanged);
    connect(m_searchEdit,  &QLineEdit::textChanged, this, &ProblemsPanel::onSearchTextChanged);
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
    int leafCount = 0;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        leafCount += m_tree->topLevelItem(i)->childCount();
    return leafCount;
}

void ProblemsPanel::onFilterChanged()
{
    rebuildTree();
}

void ProblemsPanel::onSearchTextChanged(const QString & /*text*/)
{
    rebuildTree();
}

void ProblemsPanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item)
        return;

    // File-group rows have no parent and only carry a path; toggle expansion.
    if (item->parent() == nullptr) {
        item->setExpanded(!item->isExpanded());
        return;
    }

    const QString file = item->data(0, Qt::UserRole).toString();
    const int     line = item->data(0, Qt::UserRole + 1).toInt();
    const int     col  = item->data(0, Qt::UserRole + 2).toInt();

    if (!file.isEmpty()) {
        emit navigateToFile(file, line, col);
        emit problemActivated(file, line, col);
    }
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
    switch (entry.severity) {
    case ProblemEntry::Error:   return m_btnErrors   && m_btnErrors->isChecked();
    case ProblemEntry::Warning: return m_btnWarnings && m_btnWarnings->isChecked();
    case ProblemEntry::Info:
    case ProblemEntry::Hint:    return m_btnInfo     && m_btnInfo->isChecked();
    }
    return true;
}

bool ProblemsPanel::matchesSearch(const ProblemEntry &entry) const
{
    if (!m_searchEdit)
        return true;
    const QString needle = m_searchEdit->text().trimmed();
    if (needle.isEmpty())
        return true;

    if (entry.message.contains(needle, Qt::CaseInsensitive)) return true;
    if (entry.file.contains(needle, Qt::CaseInsensitive))    return true;
    if (entry.source.contains(needle, Qt::CaseInsensitive))  return true;
    return false;
}

void ProblemsPanel::rebuildTree()
{
    m_tree->clear();

    const auto problems = gatherProblems();

    // Bucket visible problems by file (preserve insertion order).
    QMap<QString, QList<ProblemEntry>> byFile;
    QList<QString> fileOrder;

    int totalErrors = 0, totalWarnings = 0;

    for (const auto &p : problems) {
        if (p.severity == ProblemEntry::Error)   ++totalErrors;
        if (p.severity == ProblemEntry::Warning) ++totalWarnings;

        if (!matchesFilter(p) || !matchesSearch(p))
            continue;

        const QString key = fileGroupKey(p.file);
        if (!byFile.contains(key))
            fileOrder.append(key);
        byFile[key].append(p);
    }

    int displayed = 0;

    for (const QString &key : fileOrder) {
        const auto &entries = byFile.value(key);
        const QString shown  = key.isEmpty() ? tr("(no file)")
                                             : QFileInfo(key).fileName();

        // Per-file counts for header.
        int e = 0, w = 0, i = 0;
        for (const auto &p : entries) {
            switch (p.severity) {
            case ProblemEntry::Error:   ++e; break;
            case ProblemEntry::Warning: ++w; break;
            case ProblemEntry::Info:
            case ProblemEntry::Hint:    ++i; break;
            }
        }

        QStringList parts;
        if (e > 0) parts << tr("%1 error%2").arg(e).arg(e == 1 ? "" : "s");
        if (w > 0) parts << tr("%1 warning%2").arg(w).arg(w == 1 ? "" : "s");
        if (i > 0) parts << tr("%1 info").arg(i);
        const QString header = parts.isEmpty()
            ? shown
            : QStringLiteral("%1  (%2)").arg(shown, parts.join(QStringLiteral(", ")));

        auto *fileItem = new QTreeWidgetItem(m_tree);
        fileItem->setFirstColumnSpanned(true);
        fileItem->setText(0, header);
        fileItem->setToolTip(0, key);
        fileItem->setData(0, Qt::UserRole, key); // store path on header for tooltip / future use
        QFont hf = fileItem->font(0);
        hf.setBold(true);
        fileItem->setFont(0, hf);
        fileItem->setForeground(0, QColor("#cccccc"));

        for (const auto &p : entries) {
            auto *item = new QTreeWidgetItem(fileItem);
            item->setIcon(0, severityIcon(p.severity));
            item->setText(0, severityText(p.severity));
            item->setText(1, p.message);
            item->setText(2, QStringLiteral("%1:%2").arg(p.line).arg(p.column));
            item->setText(3, p.source);

            // Store navigation data on the item itself.
            item->setData(0, Qt::UserRole,     p.file);
            item->setData(0, Qt::UserRole + 1, p.line);
            item->setData(0, Qt::UserRole + 2, p.column);

            const QString fullTip = QStringLiteral("%1\n%2:%3:%4")
                                        .arg(p.message, p.file)
                                        .arg(p.line).arg(p.column);
            item->setToolTip(0, fullTip);
            item->setToolTip(1, fullTip);
            item->setToolTip(2, fullTip);
            item->setToolTip(3, p.source);

            ++displayed;
        }

        fileItem->setExpanded(true);
    }

    // Status / count label (shows totals across all sources, regardless of filter).
    m_countLabel->setText(tr("%1 errors, %2 warnings").arg(totalErrors).arg(totalWarnings));
    if (totalErrors > 0)
        m_countLabel->setStyleSheet(QStringLiteral("color: #f14c4c; font-weight: bold;"));
    else if (totalWarnings > 0)
        m_countLabel->setStyleSheet(QStringLiteral("color: #e2c08d; font-weight: bold;"));
    else
        m_countLabel->setStyleSheet(QStringLiteral("color: #73c991;"));
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
