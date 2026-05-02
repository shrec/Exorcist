#include "globalshortcutdispatcher.h"

#include "sdk/icommandservice.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QKeyEvent>

GlobalShortcutDispatcher::GlobalShortcutDispatcher(QObject *parent)
    : QObject(parent)
{
    if (auto *app = QCoreApplication::instance()) {
        app->installEventFilter(this);
    }
}

void GlobalShortcutDispatcher::registerShortcut(const QKeySequence &seq,
                                                const QString &commandId)
{
    if (seq.isEmpty() || commandId.isEmpty())
        return;
    m_map.insert(seq, commandId);
}

void GlobalShortcutDispatcher::unregisterShortcut(const QKeySequence &seq)
{
    m_map.remove(seq);
}

void GlobalShortcutDispatcher::setCommandService(ICommandService *cmds)
{
    m_cmds = cmds;
}

QString GlobalShortcutDispatcher::commandFor(const QKeySequence &seq) const
{
    return m_map.value(seq);
}

QKeySequence GlobalShortcutDispatcher::sequenceFromEvent(QKeyEvent *ke)
{
    if (!ke)
        return {};

    const int key = ke->key();
    if (key == 0)
        return {};

    // Pure modifier presses are not real shortcuts.
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_Meta:
    case Qt::Key_AltGr:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
        return {};
    default:
        break;
    }

    // Mask out keypad — modifier value should match what QKeySequence stores.
    int mods = static_cast<int>(ke->modifiers()) & ~Qt::KeypadModifier;
    return QKeySequence(key | mods);
}

bool GlobalShortcutDispatcher::eventFilter(QObject *watched, QEvent *event)
{
    if (!event)
        return QObject::eventFilter(watched, event);

    const QEvent::Type t = event->type();
    if (t != QEvent::ShortcutOverride && t != QEvent::KeyPress)
        return QObject::eventFilter(watched, event);

    if (m_map.isEmpty())
        return QObject::eventFilter(watched, event);

    auto *ke = static_cast<QKeyEvent *>(event);

    // Ignore auto-repeat for command dispatch — commands typically should
    // fire once per keystroke. (Without this, holding F10 would step a
    // dozen times before the user releases the key.)
    if (ke->isAutoRepeat())
        return QObject::eventFilter(watched, event);

    const QKeySequence seq = sequenceFromEvent(ke);
    if (seq.isEmpty())
        return QObject::eventFilter(watched, event);

    auto it = m_map.constFind(seq);
    if (it == m_map.constEnd())
        return QObject::eventFilter(watched, event);

    // We have a binding. For ShortcutOverride we accept the event so Qt
    // promotes it to a normal KeyPress that we handle below — this lets us
    // beat any focused widget that wanted to claim the key as plain input.
    if (t == QEvent::ShortcutOverride) {
        qDebug() << "[GSD] ShortcutOverride accepted for"
                 << seq.toString() << "→" << it.value()
                 << "(target:" << (watched ? watched->metaObject()->className() : "null") << ")";
        ke->accept();
        return true;
    }

    // KeyPress: dispatch through ICommandService and consume.
    qDebug() << "[GSD] KeyPress dispatching"
             << seq.toString() << "→" << it.value()
             << "cmds=" << (m_cmds ? "yes" : "NULL");
    if (m_cmds) {
        const bool ok = m_cmds->executeCommand(it.value());
        qDebug() << "[GSD]   executeCommand result=" << ok;
    }
    return true;
}
