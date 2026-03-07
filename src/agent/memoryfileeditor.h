#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QListWidget>
#include <QObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWidget>

// ─────────────────────────────────────────────────────────────────────────────
// MemoryFileEditor — UI for viewing and editing memory files.
//
// Shows a file browser for /memories/ directory with user, session,
// and repo scopes. Supports create/edit/delete operations.
// ─────────────────────────────────────────────────────────────────────────────

class MemoryFileEditor : public QWidget
{
    Q_OBJECT

public:
    explicit MemoryFileEditor(QWidget *parent = nullptr) : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(4, 4, 4, 4);

        auto *title = new QLabel(tr("Memory Files"), this);
        title->setStyleSheet(QStringLiteral("font-weight: bold;"));
        layout->addWidget(title);

        auto *splitter = new QSplitter(Qt::Horizontal, this);

        // File list
        auto *leftPanel = new QWidget(this);
        auto *leftLayout = new QVBoxLayout(leftPanel);
        leftLayout->setContentsMargins(0, 0, 0, 0);

        m_fileList = new QListWidget(leftPanel);
        leftLayout->addWidget(m_fileList);

        auto *btnRow = new QHBoxLayout;
        auto *newBtn = new QPushButton(tr("New"), leftPanel);
        connect(newBtn, &QPushButton::clicked, this, &MemoryFileEditor::createFile);
        btnRow->addWidget(newBtn);
        auto *delBtn = new QPushButton(tr("Delete"), leftPanel);
        connect(delBtn, &QPushButton::clicked, this, &MemoryFileEditor::deleteFile);
        btnRow->addWidget(delBtn);
        auto *refreshBtn = new QPushButton(tr("Refresh"), leftPanel);
        connect(refreshBtn, &QPushButton::clicked, this, &MemoryFileEditor::refresh);
        btnRow->addWidget(refreshBtn);
        leftLayout->addLayout(btnRow);
        splitter->addWidget(leftPanel);

        // Editor
        m_editor = new QPlainTextEdit(this);
        m_editor->setFont(QFont(QStringLiteral("Consolas"), 11));
        splitter->addWidget(m_editor);
        splitter->setSizes({200, 400});
        layout->addWidget(splitter);

        // Save button
        auto *saveBtn = new QPushButton(tr("Save"), this);
        connect(saveBtn, &QPushButton::clicked, this, &MemoryFileEditor::saveFile);
        layout->addWidget(saveBtn);

        connect(m_fileList, &QListWidget::currentItemChanged, this,
                [this](QListWidgetItem *current, QListWidgetItem *) {
            if (!current) return;
            loadFile(current->data(Qt::UserRole).toString());
        });
    }

    void setMemoryRoot(const QString &root)
    {
        m_memoryRoot = root;
        refresh();
    }

signals:
    void fileChanged(const QString &path);

private slots:
    void refresh()
    {
        m_fileList->clear();
        if (m_memoryRoot.isEmpty()) return;
        scanDir(QDir(m_memoryRoot), QString());
    }

    void createFile()
    {
        const QString name = QStringLiteral("notes_%1.md")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
        const QString path = m_memoryRoot + QStringLiteral("/") + name;
        QFile file(path);
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (file.open(QIODevice::WriteOnly)) {
            file.write("# Notes\n\n");
            file.close();
            refresh();
            emit fileChanged(path);
        }
    }

    void deleteFile()
    {
        auto *item = m_fileList->currentItem();
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        QFile::remove(path);
        refresh();
        m_editor->clear();
        emit fileChanged(path);
    }

    void saveFile()
    {
        auto *item = m_fileList->currentItem();
        if (!item) return;
        const QString path = item->data(Qt::UserRole).toString();
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(m_editor->toPlainText().toUtf8());
            emit fileChanged(path);
        }
    }

private:
    void loadFile(const QString &path)
    {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            m_editor->setPlainText(QString::fromUtf8(file.readAll()));
        else
            m_editor->clear();
    }

    void scanDir(const QDir &dir, const QString &prefix)
    {
        for (const auto &entry : dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                                     QDir::DirsFirst | QDir::Name)) {
            if (entry.isDir()) {
                scanDir(QDir(entry.filePath()), prefix + entry.fileName() + QStringLiteral("/"));
            } else {
                const QString label = prefix + entry.fileName();
                auto *item = new QListWidgetItem(label);
                item->setData(Qt::UserRole, entry.filePath());
                m_fileList->addItem(item);
            }
        }
    }

    QString m_memoryRoot;
    QListWidget *m_fileList;
    QPlainTextEdit *m_editor;
};
