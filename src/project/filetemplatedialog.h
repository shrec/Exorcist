#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPlainTextEdit;
class QPushButton;
class QLabel;

/// New File from Template dialog.
///
/// Lets the user pick a hardcoded template (C++ source/header, Python script,
/// Markdown, JSON, CMakeLists, .gitignore), preview its content, choose a
/// filename + folder, then create the file on disk. Emits templateSelected()
/// after a successful create so the caller can open the file.
///
/// Templates are static (defined in the .cpp). Each template has:
///   - id, name, description
///   - default file extension
///   - content (with `{BASENAME}` substitution applied at create time)
class FileTemplateDialog : public QDialog
{
    Q_OBJECT

public:
    struct Template {
        QString id;
        QString name;
        QString description;
        QString extension;       // e.g. ".cpp", ".py", ".md"
        QString defaultName;     // suggested filename (without extension)
        QString content;         // raw template body, may contain {BASENAME}
    };

    explicit FileTemplateDialog(const QString &defaultFolder, QWidget *parent = nullptr);

    /// Path of the file created after accept() (empty if dialog rejected).
    QString createdFilePath() const { return m_createdFilePath; }

signals:
    /// Emitted after the file has been written to disk.
    void templateSelected(const QString &filePath, const QString &content);

private slots:
    void onSearchChanged(const QString &text);
    void onTemplateSelected();
    void onBrowseFolder();
    void onCreateClicked();

private:
    void buildUi();
    void populateTemplates();
    void filterList(const QString &text);
    void updatePreview();
    void updateFilenameSuggestion();
    void updateCreateEnabled();

    static QVector<Template> builtinTemplates();
    static QString applyPlaceholders(const QString &content, const QString &basename);

    QLineEdit       *m_searchEdit   = nullptr;
    QListWidget     *m_listWidget   = nullptr;
    QLabel          *m_descLabel    = nullptr;
    QPlainTextEdit  *m_previewEdit  = nullptr;
    QLineEdit       *m_filenameEdit = nullptr;
    QLineEdit       *m_folderEdit   = nullptr;
    QPushButton     *m_browseBtn    = nullptr;
    QPushButton     *m_createBtn    = nullptr;
    QPushButton     *m_cancelBtn    = nullptr;

    QVector<Template> m_templates;
    QString m_createdFilePath;
};
