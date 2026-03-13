#pragma once

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QLabel;
class IDiagnosticsService;
class OutputPanel;

/// Unified problems panel aggregating LSP diagnostics and build errors.
class ProblemsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ProblemsPanel(QWidget *parent = nullptr);

    /// Set the LSP diagnostics service (not owned).
    void setDiagnosticsService(IDiagnosticsService *svc);

    /// Set the build output panel for build errors (not owned).
    void setOutputPanel(OutputPanel *panel);

    /// Refresh the problems list from all sources.
    void refresh();

    /// Current count of displayed problems.
    int problemCount() const;

signals:
    void navigateToFile(const QString &file, int line, int column);

private slots:
    void onFilterChanged(int index);
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);

private:
    void rebuildTree();

    // Problem entry for internal aggregation
    struct ProblemEntry {
        enum Severity { Error, Warning, Info, Hint };
        Severity severity = Error;
        QString  file;
        int      line   = 0;
        int      column = 0;
        QString  message;
        QString  source;   // "clangd", "build", etc.
    };

    QList<ProblemEntry> gatherProblems() const;
    QIcon severityIcon(ProblemEntry::Severity sev) const;
    QString severityText(ProblemEntry::Severity sev) const;
    bool matchesFilter(const ProblemEntry &entry) const;

    IDiagnosticsService *m_diagSvc    = nullptr;
    OutputPanel         *m_outputPanel = nullptr;
    QTreeWidget         *m_tree       = nullptr;
    QComboBox           *m_filterCombo = nullptr;
    QLabel              *m_countLabel  = nullptr;
};
