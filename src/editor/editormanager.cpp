#include "editormanager.h"

#include "editorview.h"

#include <QTabWidget>

EditorManager::EditorManager(QObject *parent)
    : QObject(parent)
{
}

EditorView *EditorManager::currentEditor() const
{
    return m_tabs ? qobject_cast<EditorView *>(m_tabs->currentWidget()) : nullptr;
}

EditorView *EditorManager::editorAt(int index) const
{
    return m_tabs ? qobject_cast<EditorView *>(m_tabs->widget(index)) : nullptr;
}
