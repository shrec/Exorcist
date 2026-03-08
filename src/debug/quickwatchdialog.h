#pragma once

#include <QDialog>

class QLineEdit;
class QTreeView;
class QPushButton;
class WatchTreeModel;
class IDebugAdapter;

/// QuickWatch dialog — evaluate an expression and browse its members
/// in an expandable tree, similar to Visual Studio's QuickWatch.
class QuickWatchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit QuickWatchDialog(IDebugAdapter *adapter, QWidget *parent = nullptr);
    ~QuickWatchDialog() override;

    /// Set the initial expression (e.g. from hovering over a variable).
    void setExpression(const QString &expression);

signals:
    /// User clicked "Add Watch" — add expression to the Watch panel.
    void addToWatch(const QString &expression);

private slots:
    void onEvaluate();
    void onAddWatch();

private:
    IDebugAdapter   *m_adapter;
    WatchTreeModel  *m_model;
    QLineEdit       *m_exprInput;
    QTreeView       *m_tree;
    QPushButton     *m_evalBtn;
    QPushButton     *m_addWatchBtn;
    QPushButton     *m_closeBtn;
};
