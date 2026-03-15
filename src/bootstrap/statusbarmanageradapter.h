#pragma once

#include "core/istatusbarmanager.h"

#include <QHash>
#include <QObject>

class QStatusBar;
class QLabel;

/// Concrete implementation of IStatusBarManager.
/// Registered in ServiceRegistry under key "statusBarManager".
class StatusBarManagerAdapter : public QObject, public IStatusBarManager
{
    Q_OBJECT

public:
    explicit StatusBarManagerAdapter(QStatusBar *statusBar,
                                     QObject *parent = nullptr);

    // ── IStatusBarManager ───────────────────────────────────────────

    void addWidget(const QString &id, QWidget *widget,
                   Alignment alignment = Left,
                   int priority = 0) override;
    bool removeWidget(const QString &id) override;
    void setText(const QString &id, const QString &text) override;
    void setTooltip(const QString &id, const QString &tooltip) override;
    void setVisible(const QString &id, bool visible) override;
    void showMessage(const QString &text, int timeoutMs = 3000) override;
    QStringList itemIds() const override;
    QWidget *widget(const QString &id) const override;

private:
    QStatusBar *m_statusBar;

    struct ItemEntry {
        QWidget   *widget    = nullptr;
        Alignment  alignment = Left;
        int        priority  = 0;
    };
    QHash<QString, ItemEntry> m_items;
};
