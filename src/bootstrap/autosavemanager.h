#pragma once

#include <QObject>
#include <QTimer>

class EditorManager;

/// Periodically saves all dirty editors that have an associated file path.
///
/// Settings (QSettings("Exorcist","Exorcist")):
///   - autosave/enabled  (bool, default true)
///   - autosave/interval (int seconds, default 30)
///
/// Untitled buffers (no filePath) are skipped. Saving is non-destructive: it
/// writes the editor's current text via QFile and clears the document's
/// modified flag on success.
class AutoSaveManager : public QObject
{
    Q_OBJECT

public:
    explicit AutoSaveManager(EditorManager *editorMgr, QObject *parent = nullptr);

    /// Set autosave interval in seconds. Persists to QSettings.
    void setInterval(int seconds);

    /// Enable or disable autosave. Persists to QSettings.
    void setEnabled(bool on);

    bool isEnabled() const { return m_enabled; }
    int  interval()  const { return m_intervalSeconds; }

private slots:
    void onTimerTick();

private:
    void applyTimerState();

    EditorManager *m_editorMgr      = nullptr;
    QTimer         m_timer;
    bool           m_enabled        = true;
    int            m_intervalSeconds = 30;
};
