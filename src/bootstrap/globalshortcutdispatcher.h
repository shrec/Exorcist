#pragma once

#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QString>

class ICommandService;

/// Application-wide hot-key dispatcher.
///
/// Many Qt shortcuts attached to QActions inside docks, sidebars or the chat
/// panel never fire because the key event is consumed by a focused descendant
/// widget before Qt's shortcut machinery walks up to the QAction owner. This
/// dispatcher installs itself as a QApplication-level event filter so that
/// every QKeyEvent in the process is checked against a small registry of
/// (QKeySequence -> command id) pairs and routed through ICommandService,
/// regardless of which widget currently has focus.
///
/// The dispatcher is intentionally minimal:
///   * it does NOT replace the existing QAction-based shortcut wiring used by
///     plugins; both stay in place. When this filter consumes an event the
///     QAction shortcut simply won't fire (Qt sees the event handled), which
///     is what we want — single dispatch per keypress.
///   * it only knows about command ids registered explicitly via
///     registerShortcut(); unknown keys flow through Qt as normal.
///   * it is safe with no command service set (events pass through).
class GlobalShortcutDispatcher : public QObject
{
    Q_OBJECT

public:
    explicit GlobalShortcutDispatcher(QObject *parent = nullptr);

    /// Register a key sequence -> command id mapping. Replaces any previous
    /// mapping for the same sequence.
    void registerShortcut(const QKeySequence &seq, const QString &commandId);

    /// Drop a previously registered key sequence.
    void unregisterShortcut(const QKeySequence &seq);

    /// Set (or clear) the command service to dispatch through. The dispatcher
    /// holds a QPointer, so it tolerates the service being destroyed.
    void setCommandService(ICommandService *cmds);

    /// Currently bound command id for @p seq, or empty string if none.
    QString commandFor(const QKeySequence &seq) const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    /// Build a QKeySequence (modifiers + key) from a QKeyEvent. Returns an
    /// empty sequence for pure modifier presses (Shift / Ctrl / ... alone).
    static QKeySequence sequenceFromEvent(class QKeyEvent *ke);

    QHash<QKeySequence, QString> m_map;
    /// ICommandService is a non-QObject SDK interface; the host owns its
    /// lifetime. We hold a raw pointer and rely on setCommandService(nullptr)
    /// being called on shutdown if needed.
    ICommandService             *m_cmds = nullptr;
};
