#pragma once

#include <QObject>
#include <QTimer>

class EditorManager;
class EditorView;

/// Periodically saves all dirty editors that have an associated file path.
///
/// Settings (QSettings("Exorcist","Exorcist")):
///   - autosave/enabled       (bool, default true)
///   - autosave/interval      (int seconds, default 30)
///   - editor/formatOnSave    (bool, default false)
///
/// Untitled buffers (no filePath) are skipped. Saving is non-destructive: it
/// writes the editor's current text via QFile and clears the document's
/// modified flag on success.
///
/// When format-on-save is enabled, before writing each save the manager will
/// invoke the "cpp.formatDocument" command (resolved through the host's
/// ICommandService) for the active C/C++ editor, then save shortly after the
/// formatter has had a chance to apply its TextEdits. The format is async, so
/// a brief delay is used to let the edits land before the file is written.
class AutoSaveManager : public QObject
{
    Q_OBJECT

public:
    explicit AutoSaveManager(EditorManager *editorMgr, QObject *parent = nullptr);

    /// Set autosave interval in seconds. Persists to QSettings.
    void setInterval(int seconds);

    /// Enable or disable autosave. Persists to QSettings.
    void setEnabled(bool on);

    /// Enable or disable format-on-save. Persists to QSettings under
    /// "editor/formatOnSave". Off by default. Currently applies to C/C++
    /// editors only (.cpp/.cc/.cxx/.h/.hpp/.hxx).
    void setFormatOnSave(bool on);

    bool isEnabled() const { return m_enabled; }
    int  interval()  const { return m_intervalSeconds; }
    bool isFormatOnSaveEnabled() const { return m_formatOnSave; }

private slots:
    void onTimerTick();

private:
    void applyTimerState();

    /// Write the editor's contents to its associated filePath. No formatting.
    /// Returns true on success.
    bool saveEditorNow(EditorView *editor);

    /// True if @p path has a C/C++ source/header extension we ask clangd to format.
    static bool isCppLikePath(const QString &path);

    EditorManager *m_editorMgr      = nullptr;
    QTimer         m_timer;
    bool           m_enabled        = true;
    int            m_intervalSeconds = 30;
    bool           m_formatOnSave   = false;
};
