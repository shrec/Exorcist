#pragma once

#include <QString>
#include <QWidget>

class QAction;
class QLabel;
class QSplitter;
class QToolBar;
class QTreeWidget;
class QTreeWidgetItem;

/// Visual editor for Qt Resource Collection (.qrc) files.
///
/// Instead of presenting the raw XML, this widget shows a two-pane view:
///   • left  — tree of <qresource prefix="..."> entries with their child <file> entries;
///   • right — preview pane (label) for the selected file (image preview for
///     png/jpg/jpeg/bmp/gif/svg, otherwise file metadata).
///
/// A toolbar provides Add Prefix / Add File / Remove / Save actions.
/// The widget tracks a "modified" flag and emits modificationChanged(bool)
/// so the surrounding tab strip can render an asterisk in the title.
///
/// The widget is deliberately a plain QWidget (not an EditorView) so that
/// EditorManager::currentEditor() / editorAt() (which qobject_cast to
/// EditorView*) safely return nullptr for QRC tabs — matching the existing
/// pattern used by ImagePreviewWidget and HexEditorWidget.
class QrcEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QrcEditorWidget(QWidget *parent = nullptr);
    ~QrcEditorWidget() override;

    /// Parse a .qrc XML file from disk and populate the tree view.
    /// Returns true on success.  On failure the tree is cleared and the
    /// status bar shows an error message.
    bool loadFromFile(const QString &path);

    /// Serialize the current tree back to a .qrc XML file at @p path.
    /// Returns true on success.  On success the modified flag is cleared.
    bool saveToFile(const QString &path);

    /// Path of the currently loaded .qrc file (empty if none).
    QString filePath() const { return m_filePath; }

    /// Returns true if the tree has unsaved changes.
    bool isModified() const { return m_modified; }

signals:
    /// Emitted whenever the modified flag flips.  MainWindow / tab strip can
    /// listen for this to render an asterisk on the tab title.
    void modificationChanged(bool modified);

public slots:
    void addPrefix();
    void addFile();
    void removeSelected();
    void save();

private slots:
    void onSelectionChanged();
    void onItemEdited(QTreeWidgetItem *item, int column);

private:
    void buildUi();
    void applyTheme();
    void setModified(bool modified);
    void updateStatus();
    void updatePreview();

    QTreeWidgetItem *createPrefixItem(const QString &prefix);
    QTreeWidgetItem *createFileItem(QTreeWidgetItem *prefixItem,
                                    const QString &filePath,
                                    const QString &alias);

    /// Returns the prefix item for @p item — either @p item itself if it is a
    /// prefix node, or its parent if @p item is a file node.  May return nullptr.
    QTreeWidgetItem *prefixItemFor(QTreeWidgetItem *item) const;

    static QString formatFileSize(qint64 bytes);

    // ── State ──────────────────────────────────────────────────────────────
    QString m_filePath;
    bool    m_modified = false;
    bool    m_loading  = false;     // suppress modified-flag flips during load

    // ── Widgets ────────────────────────────────────────────────────────────
    QToolBar   *m_toolbar    = nullptr;
    QSplitter  *m_splitter   = nullptr;
    QTreeWidget*m_tree       = nullptr;
    QLabel     *m_preview    = nullptr;
    QLabel     *m_status     = nullptr;

    QAction    *m_addPrefixAction = nullptr;
    QAction    *m_addFileAction   = nullptr;
    QAction    *m_removeAction    = nullptr;
    QAction    *m_saveAction      = nullptr;
};
