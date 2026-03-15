#pragma once

#include "core/idockmanager.h"

#include <QObject>

namespace exdock { class DockManager; class ExDockWidget; }

/// Adapts the concrete exdock::DockManager to the IDockManager interface.
/// Registered in ServiceRegistry under key "dockManager".
class DockManagerAdapter : public QObject, public IDockManager
{
    Q_OBJECT

public:
    explicit DockManagerAdapter(exdock::DockManager *impl, QObject *parent = nullptr);

    // ── IDockManager ────────────────────────────────────────────────

    bool addPanel(const QString &id, const QString &title,
                  QWidget *widget, Area area = Left,
                  bool startVisible = false) override;
    bool removePanel(const QString &id) override;

    void showPanel(const QString &id) override;
    void hidePanel(const QString &id) override;
    void togglePanel(const QString &id) override;
    bool isPanelVisible(const QString &id) const override;

    void pinPanel(const QString &id) override;
    void unpinPanel(const QString &id) override;
    bool isPanelPinned(const QString &id) const override;

    QStringList panelIds() const override;
    QAction *panelToggleAction(const QString &id) const override;
    QWidget *panelWidget(const QString &id) const override;

    QByteArray saveLayout() const override;
    bool restoreLayout(const QByteArray &data) override;

private:
    exdock::DockManager *m_impl;
};
