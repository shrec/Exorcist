#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;

/// "Quick Open" dialog (Ctrl+P) — VS Code–style fuzzy file finder.
///
/// Scans the workspace root recursively (skipping common build/cache folders),
/// then fuzzy-matches the filename query against the collected paths. Each
/// character of the query must appear in order in the candidate path
/// (case-insensitive); ranking favors exact basename prefix matches and
/// consecutive-character matches.
///
/// Emits `fileSelected(QString)` when the user accepts a result via Open
/// button, double-click, or Enter. Escape closes the dialog.
class QuickOpenDialog : public QDialog
{
    Q_OBJECT
public:
    explicit QuickOpenDialog(const QString &workspaceRoot,
                             QWidget *parent = nullptr);
    ~QuickOpenDialog() override;

signals:
    void fileSelected(const QString &filePath);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onQueryChanged(const QString &text);
    void onItemActivated(QListWidgetItem *item);
    void onAcceptCurrent();
    void onScanFinished();

private:
    static QStringList scanWorkspace(const QString &root, int cap);
    void rebuildResults();
    int  fuzzyScore(const QString &query, const QString &path,
                    const QString &basename) const;

    QString m_root;
    QStringList m_allFiles;            // populated when scan completes
    bool m_scanning = true;

    QLineEdit  *m_queryEdit  = nullptr;
    QListWidget *m_results   = nullptr;
    QPushButton *m_openBtn   = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QLabel      *m_status    = nullptr;

    QFutureWatcher<QStringList> *m_watcher = nullptr;
};
