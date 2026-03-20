#pragma once

/// IMenuManager — Abstract interface for menu bar management.
///
/// Plugins and subsystems use this interface to contribute menus and
/// menu items to the IDE menu bar at runtime. The core shell provides
/// the concrete implementation. Plugins obtain this via ServiceRegistry
/// under key "menuManager".
///
/// The core IDE creates minimal default menus (File, Edit, View, Help).
/// Everything else — Run, Debug, Tools, language-specific menus — comes
/// from plugins through this interface.

#include <QString>
#include <QStringList>

class QAction;
class QMenu;

class IMenuManager
{
public:
    virtual ~IMenuManager() = default;

    // ── Well-known menu locations ───────────────────────────────────

    /// Standard menu bar locations that plugins can contribute to.
    enum MenuLocation {
        File,
        Edit,
        View,
        Git,
        Project,
        Selection,
        Build,
        Debug,
        Test,
        Analyze,
        Run,
        Terminal,
        Tools,
        Extensions,
        Window,
        Help,
        Custom     ///< Plugin-defined top-level menu
    };

    // ── Menu creation ───────────────────────────────────────────────

    /// Get or create a top-level menu. If the menu doesn't exist, it is
    /// created at the specified position. Returns nullptr only on error.
    virtual QMenu *menu(MenuLocation location) = 0;

    /// Create a custom top-level menu with the given title.
    /// Returns the menu, or an existing one if the ID is already used.
    virtual QMenu *createMenu(const QString &id,
                              const QString &title) = 0;

    /// Remove a custom top-level menu by ID. Standard menus cannot be
    /// removed. Returns false if not found or not removable.
    virtual bool removeMenu(const QString &id) = 0;

    // ── Action contribution ─────────────────────────────────────────

    /// Add an action to a standard menu location.
    /// The group parameter controls ordering (e.g., "navigation",
    /// "1_modification", "z_commands"). Actions in the same group
    /// are separated from other groups by separators.
    virtual void addAction(MenuLocation location,
                           QAction *action,
                           const QString &group = {}) = 0;

    /// Add an action to a custom menu by ID.
    virtual void addAction(const QString &menuId,
                           QAction *action,
                           const QString &group = {}) = 0;

    /// Add a submenu to a standard menu location.
    virtual QMenu *addSubmenu(MenuLocation location,
                              const QString &title,
                              const QString &group = {}) = 0;

    /// Add a separator to a standard menu location.
    virtual void addSeparator(MenuLocation location) = 0;

    /// Remove an action from any menu. Returns false if not found.
    virtual bool removeAction(QAction *action) = 0;

    // ── Context menus ───────────────────────────────────────────────

    /// Register an action for the editor context menu.
    /// The when condition controls visibility (e.g., "editorLangId == cpp").
    virtual void addEditorContextAction(QAction *action,
                                         const QString &group = {},
                                         const QString &when = {}) = 0;

    /// Register an action for the file explorer context menu.
    virtual void addExplorerContextAction(QAction *action,
                                           const QString &group = {},
                                           const QString &when = {}) = 0;

    // ── Query ───────────────────────────────────────────────────────

    /// All custom menu IDs.
    virtual QStringList customMenuIds() const = 0;

    /// Get a custom menu by ID. Returns nullptr if not found.
    virtual QMenu *customMenu(const QString &id) const = 0;
};
