#pragma once

#include <QHash>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class SearchService;
struct SearchQuery;

class SearchPanel : public QWidget
{
    Q_OBJECT

public:
    explicit SearchPanel(SearchService *service, QWidget *parent = nullptr);
    ~SearchPanel() override;

    void setRootPath(const QString &path);

    // Focus the search input (called when Ctrl+Shift+F opens the panel)
    void activateSearch();

signals:
    // Emitted when the user clicks a search result
    void resultActivated(const QString &filePath, int line, int column);

private:
    // ── UI helpers ───────────────────────────────────────────────────────
    QToolButton *makeToggleButton(const QString &label, const QString &tooltip);
    void buildUi();
    void applyTheme();

    // ── Search lifecycle ─────────────────────────────────────────────────
    void startSearch();
    void clearResults();
    void addMatch(const QString &filePath, int line, int column,
                  const QString &preview);
    void onItemActivated(QTreeWidgetItem *item, int column);
    void onItemClicked(QTreeWidgetItem *item, int column);
    void toggleReplaceMode(bool on);
    void runReplaceAll();
    SearchQuery buildQuery() const;

    // ── Persistence ──────────────────────────────────────────────────────
    void loadSettings();
    void saveSettings();

    // ── Match-preview rendering ──────────────────────────────────────────
    QString renderPreview(const QString &preview, int matchColumn) const;

    QString m_rootPath;
    SearchService *m_service;

    // Inputs
    QLineEdit *m_input;
    QLineEdit *m_replaceInput;
    QLineEdit *m_includeFilter;
    QLineEdit *m_excludeFilter;

    // Toggle buttons (replace previous QCheckBoxes)
    QToolButton *m_caseBtn;
    QToolButton *m_wordBtn;
    QToolButton *m_regexBtn;
    QToolButton *m_replaceModeBtn;

    // Action buttons
    QPushButton *m_searchButton;
    QPushButton *m_replaceAllButton;

    // Output
    QTreeWidget *m_results;
    QLabel *m_statusLabel;

    // Quick lookup: absolute file path → top-level item
    QHash<QString, QTreeWidgetItem *> m_fileItems;

    int m_matchCount = 0;
    int m_fileCount  = 0;
};
