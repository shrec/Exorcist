#include "filetemplatedialog.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

constexpr const char *kStyleSheet = R"(
FileTemplateDialog { background: #1e1e1e; }
QLabel { color: #cccccc; }
QLabel#sectionLabel {
    font-size: 12px; font-weight: 600; color: #888;
    margin-bottom: 2px;
}
QLabel#descLabel {
    font-size: 12px; color: #888; padding: 4px 0;
}
QLineEdit {
    background: #3c3c3c; border: 1px solid #3e3e42;
    color: #cccccc; padding: 6px; font-size: 13px;
}
QLineEdit:focus { border-color: #007acc; }
QListWidget {
    background: #252526; border: 1px solid #3e3e42;
    color: #cccccc; font-size: 13px;
    outline: 0;
}
QListWidget::item { padding: 6px 10px; }
QListWidget::item:selected { background: #094771; color: #ffffff; }
QListWidget::item:hover { background: #2a2d2e; }
QPlainTextEdit {
    background: #1e1e1e; border: 1px solid #3e3e42;
    color: #d4d4d4; font-family: "Consolas", "Courier New", monospace;
    font-size: 12px;
    selection-background-color: #264f78;
}
QPushButton {
    background: #0e639c; color: white; border: none;
    padding: 8px 20px; font-size: 13px; border-radius: 2px;
    min-width: 80px;
}
QPushButton:hover { background: #1177bb; }
QPushButton:disabled { background: #3e3e42; color: #888; }
QPushButton#browseBtn {
    background: #3c3c3c; color: #cccccc; padding: 6px 12px;
    min-width: 0;
}
QPushButton#browseBtn:hover { background: #505050; }
QPushButton#cancelBtn {
    background: transparent; color: #cccccc;
    border: 1px solid #3e3e42;
}
QPushButton#cancelBtn:hover { background: #2a2d2e; }
)";

} // namespace

// ── Builtin template definitions ────────────────────────────────────────────

QVector<FileTemplateDialog::Template> FileTemplateDialog::builtinTemplates()
{
    QVector<Template> t;

    t.append({
        QStringLiteral("cpp_source"),
        QStringLiteral("C++ Source"),
        QStringLiteral("Empty C++ source file that includes a matching header."),
        QStringLiteral(".cpp"),
        QStringLiteral("source"),
        QStringLiteral("#include \"{BASENAME}.h\"\n\n")
    });

    t.append({
        QStringLiteral("cpp_header"),
        QStringLiteral("C++ Header"),
        QStringLiteral("C++ header with #pragma once and a class skeleton."),
        QStringLiteral(".h"),
        QStringLiteral("header"),
        QStringLiteral(
            "#pragma once\n"
            "\n"
            "class MyClass\n"
            "{\n"
            "public:\n"
            "    MyClass();\n"
            "    ~MyClass();\n"
            "};\n")
    });

    t.append({
        QStringLiteral("python_script"),
        QStringLiteral("Python Script"),
        QStringLiteral("Python 3 script with shebang and main() entry point."),
        QStringLiteral(".py"),
        QStringLiteral("script"),
        QStringLiteral(
            "#!/usr/bin/env python3\n"
            "\n"
            "def main():\n"
            "    pass\n"
            "\n"
            "if __name__ == \"__main__\":\n"
            "    main()\n")
    });

    t.append({
        QStringLiteral("markdown_readme"),
        QStringLiteral("Markdown README"),
        QStringLiteral("README.md scaffold with project name, description, usage."),
        QStringLiteral(".md"),
        QStringLiteral("README"),
        QStringLiteral(
            "# Project Name\n"
            "\n"
            "## Description\n"
            "\n"
            "## Usage\n"
            "\n")
    });

    t.append({
        QStringLiteral("json_empty"),
        QStringLiteral("JSON"),
        QStringLiteral("Empty JSON object scaffold."),
        QStringLiteral(".json"),
        QStringLiteral("data"),
        QStringLiteral("{\n    \n}\n")
    });

    t.append({
        QStringLiteral("cmake_lists"),
        QStringLiteral("CMakeLists.txt"),
        QStringLiteral("Minimal CMake project with C++17 executable target."),
        QStringLiteral(".txt"),
        QStringLiteral("CMakeLists"),
        QStringLiteral(
            "cmake_minimum_required(VERSION 3.16)\n"
            "project(MyProject)\n"
            "\n"
            "set(CMAKE_CXX_STANDARD 17)\n"
            "\n"
            "add_executable(${PROJECT_NAME} main.cpp)\n")
    });

    t.append({
        QStringLiteral("gitignore"),
        QStringLiteral(".gitignore"),
        QStringLiteral("Common ignores for build dirs, IDE files, OS junk."),
        QStringLiteral(""),
        QStringLiteral(".gitignore"),
        QStringLiteral(
            "# Build directories\n"
            "build/\n"
            "build-*/\n"
            "out/\n"
            "cmake-build-*/\n"
            "\n"
            "# IDE / editor\n"
            ".vs/\n"
            ".vscode/\n"
            ".idea/\n"
            "*.user\n"
            "*.swp\n"
            "*~\n"
            "\n"
            "# OS\n"
            ".DS_Store\n"
            "Thumbs.db\n"
            "\n"
            "# Binaries\n"
            "*.o\n"
            "*.obj\n"
            "*.exe\n"
            "*.dll\n"
            "*.so\n"
            "*.dylib\n"
            "*.a\n"
            "*.lib\n"
            "*.pdb\n")
    });

    return t;
}

QString FileTemplateDialog::applyPlaceholders(const QString &content, const QString &basename)
{
    QString out = content;
    out.replace(QStringLiteral("{BASENAME}"), basename);
    return out;
}

// ── Construction & UI ───────────────────────────────────────────────────────

FileTemplateDialog::FileTemplateDialog(const QString &defaultFolder, QWidget *parent)
    : QDialog(parent)
    , m_templates(builtinTemplates())
{
    setWindowTitle(tr("New File from Template"));
    setMinimumSize(720, 520);
    resize(820, 580);

    setStyleSheet(QString::fromUtf8(kStyleSheet));

    buildUi();
    populateTemplates();

    if (!defaultFolder.isEmpty())
        m_folderEdit->setText(defaultFolder);
    else
        m_folderEdit->setText(QDir::homePath());

    if (m_listWidget->count() > 0) {
        m_listWidget->setCurrentRow(0);
        onTemplateSelected();
    }

    updateCreateEnabled();
}

void FileTemplateDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *titleLabel = new QLabel(tr("Create a new file from a template"), this);
    titleLabel->setStyleSheet(QStringLiteral(
        "font-size: 18px; font-weight: 300; color: #cccccc; margin-bottom: 4px;"));
    root->addWidget(titleLabel);

    // Search
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search templates..."));
    m_searchEdit->setClearButtonEnabled(true);
    root->addWidget(m_searchEdit);

    // Middle area: list | preview
    auto *middle = new QHBoxLayout;
    middle->setSpacing(10);

    auto *leftCol = new QVBoxLayout;
    auto *listLabel = new QLabel(tr("Template"), this);
    listLabel->setObjectName(QStringLiteral("sectionLabel"));
    leftCol->addWidget(listLabel);
    m_listWidget = new QListWidget(this);
    m_listWidget->setMinimumWidth(240);
    leftCol->addWidget(m_listWidget, 1);
    m_descLabel = new QLabel(this);
    m_descLabel->setObjectName(QStringLiteral("descLabel"));
    m_descLabel->setWordWrap(true);
    leftCol->addWidget(m_descLabel);
    middle->addLayout(leftCol, 0);

    auto *rightCol = new QVBoxLayout;
    auto *previewLabel = new QLabel(tr("Preview"), this);
    previewLabel->setObjectName(QStringLiteral("sectionLabel"));
    rightCol->addWidget(previewLabel);
    m_previewEdit = new QPlainTextEdit(this);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    rightCol->addWidget(m_previewEdit, 1);
    middle->addLayout(rightCol, 1);

    root->addLayout(middle, 1);

    // Filename row
    auto *nameRow = new QHBoxLayout;
    auto *nameLabel = new QLabel(tr("Filename:"), this);
    nameLabel->setMinimumWidth(70);
    nameRow->addWidget(nameLabel);
    m_filenameEdit = new QLineEdit(this);
    m_filenameEdit->setPlaceholderText(tr("e.g. mywidget.cpp"));
    nameRow->addWidget(m_filenameEdit, 1);
    root->addLayout(nameRow);

    // Folder row
    auto *folderRow = new QHBoxLayout;
    auto *folderLabel = new QLabel(tr("Folder:"), this);
    folderLabel->setMinimumWidth(70);
    folderRow->addWidget(folderLabel);
    m_folderEdit = new QLineEdit(this);
    folderRow->addWidget(m_folderEdit, 1);
    m_browseBtn = new QPushButton(QStringLiteral("..."), this);
    m_browseBtn->setObjectName(QStringLiteral("browseBtn"));
    m_browseBtn->setToolTip(tr("Browse for folder"));
    folderRow->addWidget(m_browseBtn);
    root->addLayout(folderRow);

    // Buttons
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    btnRow->addWidget(m_cancelBtn);
    m_createBtn = new QPushButton(tr("Create"), this);
    m_createBtn->setDefault(true);
    btnRow->addWidget(m_createBtn);
    root->addLayout(btnRow);

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &FileTemplateDialog::onSearchChanged);
    connect(m_listWidget, &QListWidget::itemSelectionChanged,
            this, &FileTemplateDialog::onTemplateSelected);
    connect(m_filenameEdit, &QLineEdit::textChanged,
            this, [this](const QString &) { updateCreateEnabled(); });
    connect(m_folderEdit, &QLineEdit::textChanged,
            this, [this](const QString &) { updateCreateEnabled(); });
    connect(m_browseBtn, &QPushButton::clicked,
            this, &FileTemplateDialog::onBrowseFolder);
    connect(m_createBtn, &QPushButton::clicked,
            this, &FileTemplateDialog::onCreateClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &QDialog::reject);
}

void FileTemplateDialog::populateTemplates()
{
    m_listWidget->clear();
    for (const Template &tpl : m_templates) {
        auto *item = new QListWidgetItem(tpl.name, m_listWidget);
        item->setData(Qt::UserRole, tpl.id);
        item->setToolTip(tpl.description);
    }
}

// ── Slots ───────────────────────────────────────────────────────────────────

void FileTemplateDialog::onSearchChanged(const QString &text)
{
    filterList(text);
}

void FileTemplateDialog::filterList(const QString &text)
{
    const QString needle = text.trimmed().toLower();
    int firstVisibleRow = -1;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        QListWidgetItem *item = m_listWidget->item(i);
        const QString id = item->data(Qt::UserRole).toString();
        const Template *tpl = nullptr;
        for (const Template &t : m_templates) {
            if (t.id == id) { tpl = &t; break; }
        }
        bool matches = needle.isEmpty();
        if (!matches && tpl) {
            matches = tpl->name.toLower().contains(needle)
                   || tpl->description.toLower().contains(needle)
                   || tpl->extension.toLower().contains(needle)
                   || tpl->id.toLower().contains(needle);
        }
        item->setHidden(!matches);
        if (matches && firstVisibleRow < 0)
            firstVisibleRow = i;
    }

    // If current selection is now hidden, jump to first visible.
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (!cur || cur->isHidden()) {
        if (firstVisibleRow >= 0) {
            m_listWidget->setCurrentRow(firstVisibleRow);
        } else {
            m_listWidget->clearSelection();
            m_previewEdit->clear();
            m_descLabel->clear();
            updateCreateEnabled();
        }
    }
}

void FileTemplateDialog::onTemplateSelected()
{
    updatePreview();
    updateFilenameSuggestion();
    updateCreateEnabled();
}

void FileTemplateDialog::updatePreview()
{
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (!cur) {
        m_previewEdit->clear();
        m_descLabel->clear();
        return;
    }
    const QString id = cur->data(Qt::UserRole).toString();
    for (const Template &tpl : m_templates) {
        if (tpl.id == id) {
            m_descLabel->setText(tpl.description);
            // Show preview with placeholder visible (use defaultName as basename
            // so the preview reflects what'll be written for typical use).
            m_previewEdit->setPlainText(applyPlaceholders(tpl.content, tpl.defaultName));
            return;
        }
    }
}

void FileTemplateDialog::updateFilenameSuggestion()
{
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (!cur)
        return;
    const QString id = cur->data(Qt::UserRole).toString();
    for (const Template &tpl : m_templates) {
        if (tpl.id == id) {
            // Only auto-fill if empty or looks like a previous suggestion (avoids
            // overwriting user-typed text).
            const QString currentText = m_filenameEdit->text().trimmed();
            bool looksAuto = currentText.isEmpty();
            for (const Template &other : m_templates) {
                const QString suggest = other.defaultName + other.extension;
                if (currentText == suggest) { looksAuto = true; break; }
            }
            if (looksAuto)
                m_filenameEdit->setText(tpl.defaultName + tpl.extension);
            return;
        }
    }
}

void FileTemplateDialog::onBrowseFolder()
{
    const QString start = m_folderEdit->text().isEmpty()
        ? QDir::homePath() : m_folderEdit->text();
    const QString chosen = QFileDialog::getExistingDirectory(
        this, tr("Select Folder"), start);
    if (!chosen.isEmpty())
        m_folderEdit->setText(chosen);
}

void FileTemplateDialog::updateCreateEnabled()
{
    const bool hasSel = m_listWidget->currentItem() != nullptr;
    const bool hasName = !m_filenameEdit->text().trimmed().isEmpty();
    const bool hasFolder = !m_folderEdit->text().trimmed().isEmpty();
    m_createBtn->setEnabled(hasSel && hasName && hasFolder);
}

void FileTemplateDialog::onCreateClicked()
{
    QListWidgetItem *cur = m_listWidget->currentItem();
    if (!cur)
        return;
    const QString id = cur->data(Qt::UserRole).toString();

    const Template *tpl = nullptr;
    for (const Template &t : m_templates) {
        if (t.id == id) { tpl = &t; break; }
    }
    if (!tpl) {
        QMessageBox::warning(this, tr("New File"), tr("Template not found."));
        return;
    }

    const QString folder = m_folderEdit->text().trimmed();
    const QString filename = m_filenameEdit->text().trimmed();
    if (folder.isEmpty() || filename.isEmpty())
        return;

    QDir dir(folder);
    if (!dir.exists()) {
        if (!QDir().mkpath(folder)) {
            QMessageBox::critical(this, tr("New File"),
                tr("Could not create folder:\n%1").arg(folder));
            return;
        }
    }

    const QString fullPath = dir.absoluteFilePath(filename);

    if (QFileInfo::exists(fullPath)) {
        const auto reply = QMessageBox::question(this, tr("File Exists"),
            tr("The file already exists:\n%1\n\nOverwrite?").arg(fullPath),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes)
            return;
    }

    const QString basename = QFileInfo(filename).completeBaseName();
    const QString content = applyPlaceholders(tpl->content, basename);

    QSaveFile out(fullPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("New File"),
            tr("Could not open file for writing:\n%1\n\n%2")
                .arg(fullPath, out.errorString()));
        return;
    }
    {
        QTextStream ts(&out);
        ts.setEncoding(QStringConverter::Utf8);
        ts << content;
    }
    if (!out.commit()) {
        QMessageBox::critical(this, tr("New File"),
            tr("Could not write file:\n%1\n\n%2")
                .arg(fullPath, out.errorString()));
        return;
    }

    m_createdFilePath = fullPath;
    emit templateSelected(fullPath, content);
    accept();
}
