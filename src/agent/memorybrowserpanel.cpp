#include "memorybrowserpanel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

MemoryBrowserPanel::MemoryBrowserPanel(const QString &basePath, QWidget *parent)
    : QWidget(parent)
    , m_basePath(basePath)
    , m_tree(new QTreeWidget(this))
    , m_editor(new QTextEdit(this))
    , m_saveBtn(new QToolButton(this))
    , m_deleteBtn(new QToolButton(this))
    , m_newBtn(new QToolButton(this))
    , m_refreshBtn(new QToolButton(this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    m_newBtn->setText(tr("New"));
    m_newBtn->setToolTip(tr("Create a new memory file"));
    m_refreshBtn->setText(tr("Refresh"));
    m_saveBtn->setText(tr("Save"));
    m_saveBtn->setEnabled(false);
    m_deleteBtn->setText(tr("Delete"));
    m_deleteBtn->setEnabled(false);

    toolbar->addWidget(m_newBtn);
    toolbar->addWidget(m_refreshBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_saveBtn);
    toolbar->addWidget(m_deleteBtn);
    root->addLayout(toolbar);

    // Splitter: tree (left/top) + editor (right/bottom)
    auto *splitter = new QSplitter(Qt::Vertical, this);

    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    splitter->addWidget(m_tree);

    m_editor->setReadOnly(true);
    m_editor->setPlaceholderText(tr("Select a memory file to view/edit..."));
    m_editor->setStyleSheet(QStringLiteral(
        "QTextEdit { font-family: 'Consolas','Courier New',monospace; "
        "font-size: 12px; background: #1e1e1e; color: #d4d4d4; "
        "border: 1px solid #3c3c3c; }"));
    splitter->addWidget(m_editor);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    root->addWidget(splitter);

    connect(m_tree, &QTreeWidget::itemClicked, this, &MemoryBrowserPanel::onItemClicked);
    connect(m_saveBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::saveCurrentFile);
    connect(m_deleteBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::deleteCurrentFile);
    connect(m_newBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::createNewFile);
    connect(m_refreshBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::refresh);

    // Ensure base directory exists
    QDir().mkpath(m_basePath);
    QDir().mkpath(m_basePath + QStringLiteral("/session"));
    QDir().mkpath(m_basePath + QStringLiteral("/repo"));

    refresh();
}

void MemoryBrowserPanel::refresh()
{
    m_tree->clear();
    m_editor->clear();
    m_editor->setReadOnly(true);
    m_saveBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_currentFilePath.clear();

    auto *rootItem = new QTreeWidgetItem(m_tree, {tr("memories")});
    rootItem->setExpanded(true);
    populateTree(m_basePath, rootItem);
}

void MemoryBrowserPanel::populateTree(const QString &dirPath, QTreeWidgetItem *parentItem)
{
    QDir dir(dirPath);
    // Directories first
    for (const QString &subDir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        auto *item = new QTreeWidgetItem(parentItem, {subDir + QStringLiteral("/")});
        item->setData(0, Qt::UserRole, dir.absoluteFilePath(subDir));
        item->setExpanded(true);
        populateTree(dir.absoluteFilePath(subDir), item);
    }
    // Files
    for (const QString &file : dir.entryList(QDir::Files, QDir::Name)) {
        auto *item = new QTreeWidgetItem(parentItem, {file});
        item->setData(0, Qt::UserRole, dir.absoluteFilePath(file));
    }
}

void MemoryBrowserPanel::onItemClicked(QTreeWidgetItem *item)
{
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;

    QFileInfo fi(path);
    if (fi.isDir()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    m_currentFilePath = path;
    m_editor->setPlainText(QString::fromUtf8(f.readAll()));
    m_editor->setReadOnly(false);
    m_saveBtn->setEnabled(true);
    m_deleteBtn->setEnabled(true);
}

void MemoryBrowserPanel::saveCurrentFile()
{
    if (m_currentFilePath.isEmpty()) return;

    QFile f(m_currentFilePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_editor->toPlainText().toUtf8());
    }
}

void MemoryBrowserPanel::deleteCurrentFile()
{
    if (m_currentFilePath.isEmpty()) return;

    if (QMessageBox::question(this, tr("Delete Memory File"),
            tr("Delete \"%1\"?").arg(QFileInfo(m_currentFilePath).fileName()),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        QFile::remove(m_currentFilePath);
        refresh();
    }
}

void MemoryBrowserPanel::createNewFile()
{
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("New Memory File"),
        tr("File name (e.g. notes.md):"), QLineEdit::Normal, QString(), &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // Sanitize: no path separators
    QString safe = name;
    safe.replace(QLatin1Char('/'), QLatin1Char('-'));
    safe.replace(QLatin1Char('\\'), QLatin1Char('-'));

    const QString fullPath = m_basePath + QLatin1Char('/') + safe;
    if (QFile::exists(fullPath)) {
        QMessageBox::information(this, tr("File Exists"),
            tr("A file named \"%1\" already exists.").arg(safe));
        return;
    }

    QFile f(fullPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write("# " + safe.toUtf8() + "\n\n");
    }
    refresh();
}
