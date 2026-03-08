#pragma once

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QString>

class QAction;
class QSettings;

// ── KeymapManager ─────────────────────────────────────────────────────────────
// Stores named keybindings with defaults, loads/saves overrides via QSettings,
// and applies bindings to QActions.

class KeymapManager : public QObject
{
    Q_OBJECT

public:
    explicit KeymapManager(QObject *parent = nullptr);

    // Register an action with a logical name and its default shortcut.
    void registerAction(const QString &id, QAction *action,
                        const QKeySequence &defaultKey);

    // Override a shortcut.  Empty sequence resets to default.
    void setShortcut(const QString &id, const QKeySequence &key);

    // Get current shortcut for an action.
    QKeySequence shortcut(const QString &id) const;
    QKeySequence defaultShortcut(const QString &id) const;

    // Get all registered action IDs.
    QStringList actionIds() const;

    // Get the display name (text) for an action.
    QString actionText(const QString &id) const;

    // Persist all overrides to QSettings.
    void save();

    // Reload overrides from QSettings.
    void load();

    // Reset a single binding to default.
    void resetToDefault(const QString &id);

    // Reset all bindings to defaults.
    void resetAllToDefaults();

signals:
    void shortcutChanged(const QString &id, const QKeySequence &newKey);

private:
    struct Binding {
        QAction      *action = nullptr;
        QKeySequence  defaultKey;
        QKeySequence  currentKey;
    };
    QHash<QString, Binding> m_bindings;
};
