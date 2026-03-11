#pragma once

#include <QObject>

class GhService;
class GhPanel;

namespace exdock { class DockManager; class ExDockWidget; }

/// Self-contained bootstrap for the GitHub CLI integration panel.
/// Owns GhService, GhPanel, and the dock widget — keeps MainWindow clean.
class GhBootstrap : public QObject
{
    Q_OBJECT

public:
    explicit GhBootstrap(QObject *parent = nullptr);

    /// Create service, panel, and dock widget. Call once after DockManager exists.
    void initialize(exdock::DockManager *dockManager, QWidget *dockParent);

    /// Update working directory (e.g. when the user opens a folder).
    void setWorkingDirectory(const QString &path);

    /// Refresh all GitHub data.
    void refresh();

    /// Whether gh CLI was found on PATH.
    bool isAvailable() const;

    GhService *service() const { return m_service; }
    GhPanel   *panel()   const { return m_panel; }
    exdock::ExDockWidget *dock() const { return m_dock; }

private:
    GhService            *m_service = nullptr;
    GhPanel              *m_panel   = nullptr;
    exdock::ExDockWidget *m_dock    = nullptr;
};
