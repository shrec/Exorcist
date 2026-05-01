#pragma once

#include <QByteArray>
#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

/// Read-only hex dump viewer used in place of the text editor for binary files.
///
/// Renders a classic offset / hex / ASCII layout (16 bytes per row), styled to
/// match the VS2022 dark theme used elsewhere in the IDE.  The widget is a
/// plain QWidget (not an EditorView), so EditorManager::currentEditor() and
/// editorAt() correctly return nullptr for hex tabs (they qobject_cast to
/// EditorView).  This keeps existing flows that assume "active widget = code
/// editor" safe.
///
/// Capacity is intentionally capped (kMaxBytes) so opening a 500MB binary does
/// not allocate hundreds of megabytes of QPlainTextEdit document — the user is
/// shown a single-shot snapshot of the start of the file.  An on-disk re-read
/// can be triggered with the Reload button.
class HexEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HexEditorWidget(QWidget *parent = nullptr);
    ~HexEditorWidget() override;

    /// Read up to kMaxBytes from disk and render the hex dump.  Returns false
    /// on I/O failure or empty file.
    bool loadFile(const QString &path);

    /// Path of the currently loaded file, empty if none.
    QString filePath() const { return m_filePath; }

    /// Maximum number of bytes loaded from disk into the dump (1 MB).
    static constexpr qint64 kMaxBytes = 1024 * 1024;

    /// Bytes rendered per row in the dump.
    static constexpr int kBytesPerRow = 16;

private slots:
    void onGotoActivated();
    void onReloadClicked();
    void onCursorMoved();

private:
    void setupUi();
    void renderDump();
    void updateStatus();
    static QString formatFileSize(qint64 bytes);

    /// Convert a logical byte offset into the QPlainTextEdit cursor position
    /// of the corresponding hex pair.  Returns -1 if offset is out of range.
    int textPositionForOffset(qint64 offset) const;

    /// Inverse of textPositionForOffset — translate a cursor position in the
    /// dump view back to a logical byte offset, or -1 if outside data.
    qint64 offsetForTextPosition(int pos) const;

    QLineEdit       *m_gotoEdit   = nullptr;
    QPushButton     *m_reloadBtn  = nullptr;
    QPlainTextEdit  *m_dumpView   = nullptr;
    QLabel          *m_statusLabel = nullptr;

    QString    m_filePath;
    QByteArray m_bytes;
    qint64     m_fileSize = 0;
    int        m_offsetColWidth = 10;  // "0x00000000" — 10 chars
};
