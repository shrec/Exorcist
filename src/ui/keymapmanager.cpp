#include "keymapmanager.h"

#include <QAction>
#include <QSettings>

KeymapManager::KeymapManager(QObject *parent)
    : QObject(parent)
{
}

void KeymapManager::registerAction(const QString &id, QAction *action,
                                   const QKeySequence &defaultKey)
{
    Binding b;
    b.action     = action;
    b.defaultKey = defaultKey;
    b.currentKey = defaultKey;
    m_bindings.insert(id, b);
}

void KeymapManager::setShortcut(const QString &id, const QKeySequence &key)
{
    auto it = m_bindings.find(id);
    if (it == m_bindings.end()) return;

    it->currentKey = key.isEmpty() ? it->defaultKey : key;
    if (it->action)
        it->action->setShortcut(it->currentKey);
    emit shortcutChanged(id, it->currentKey);
}

QKeySequence KeymapManager::shortcut(const QString &id) const
{
    auto it = m_bindings.constFind(id);
    return it != m_bindings.constEnd() ? it->currentKey : QKeySequence();
}

QKeySequence KeymapManager::defaultShortcut(const QString &id) const
{
    auto it = m_bindings.constFind(id);
    return it != m_bindings.constEnd() ? it->defaultKey : QKeySequence();
}

QStringList KeymapManager::actionIds() const
{
    QStringList ids = m_bindings.keys();
    ids.sort();
    return ids;
}

QString KeymapManager::actionText(const QString &id) const
{
    auto it = m_bindings.constFind(id);
    if (it != m_bindings.constEnd() && it->action)
        return it->action->text().remove(QLatin1Char('&'));
    return id;
}

void KeymapManager::save()
{
    QSettings settings(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    settings.beginGroup(QStringLiteral("keymap"));
    settings.remove(QString());  // clear group

    for (auto it = m_bindings.constBegin(); it != m_bindings.constEnd(); ++it) {
        if (it->currentKey != it->defaultKey)
            settings.setValue(it.key(), it->currentKey.toString());
    }
    settings.endGroup();
}

void KeymapManager::load()
{
    QSettings settings(QStringLiteral("Exorcist"), QStringLiteral("Exorcist"));
    settings.beginGroup(QStringLiteral("keymap"));

    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        const QString stored = settings.value(it.key()).toString();
        if (!stored.isEmpty()) {
            it->currentKey = QKeySequence(stored);
            if (it->action)
                it->action->setShortcut(it->currentKey);
        }
    }
    settings.endGroup();
}

void KeymapManager::resetToDefault(const QString &id)
{
    auto it = m_bindings.find(id);
    if (it == m_bindings.end()) return;

    it->currentKey = it->defaultKey;
    if (it->action)
        it->action->setShortcut(it->currentKey);
    emit shortcutChanged(id, it->currentKey);
}

void KeymapManager::resetAllToDefaults()
{
    for (auto it = m_bindings.begin(); it != m_bindings.end(); ++it) {
        it->currentKey = it->defaultKey;
        if (it->action)
            it->action->setShortcut(it->currentKey);
    }
}
