#pragma once

#include "core/itoolbarmanager.h"

#include <QObject>

namespace exdock { class DockToolBarManager; }

/// Adapts the concrete exdock::DockToolBarManager to the IToolBarManager
/// interface. Registered in ServiceRegistry under key "toolBarManager".
class ToolBarManagerAdapter : public QObject, public IToolBarManager
{
    Q_OBJECT

public:
    explicit ToolBarManagerAdapter(exdock::DockToolBarManager *impl,
                                   QObject *parent = nullptr);

    // ── IToolBarManager ─────────────────────────────────────────────

    bool createToolBar(const QString &id, const QString &title,
                       Edge edge = Top) override;
    bool removeToolBar(const QString &id) override;

    void addAction(const QString &toolBarId, QAction *action) override;
    void addWidget(const QString &toolBarId, QWidget *widget) override;
    void addSeparator(const QString &toolBarId) override;

    void setVisible(const QString &toolBarId, bool visible) override;
    bool isVisible(const QString &toolBarId) const override;

    void setEdge(const QString &toolBarId, Edge edge) override;

    void setAllLocked(bool locked) override;
    bool allLocked() const override;

    QStringList toolBarIds() const override;

    QByteArray saveLayout() const override;
    bool restoreLayout(const QByteArray &data) override;

private:
    exdock::DockToolBarManager *m_impl;
};
