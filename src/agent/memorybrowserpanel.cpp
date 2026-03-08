#include "memorybrowserpanel.h"
#include "projectbrainservice.h"
#include "projectbraintypes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QUuid>
#include <QVBoxLayout>

MemoryBrowserPanel::MemoryBrowserPanel(const QString &basePath, QWidget *parent)
    : QWidget(parent)
    , m_basePath(basePath)
    , m_tabWidget(new QTabWidget(this))
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    root->addWidget(m_tabWidget);

    setupFilesTab();
    setupFactsTab();
    setupRulesTab();
    setupSessionsTab();

    // Ensure base directory exists
    QDir().mkpath(m_basePath);
    QDir().mkpath(m_basePath + QStringLiteral("/session"));
    QDir().mkpath(m_basePath + QStringLiteral("/repo"));

    refresh();
}

// ── Files Tab (original memory browser) ──────────────────────────────────────

void MemoryBrowserPanel::setupFilesTab()
{
    m_filesTab = new QWidget(this);
    auto *lay = new QVBoxLayout(m_filesTab);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    // Toolbar
    auto *toolbar = new QHBoxLayout;
    m_newBtn = new QToolButton(m_filesTab);
    m_refreshBtn = new QToolButton(m_filesTab);
    m_saveBtn = new QToolButton(m_filesTab);
    m_deleteBtn = new QToolButton(m_filesTab);

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
    lay->addLayout(toolbar);

    // Splitter: tree + editor
    auto *splitter = new QSplitter(Qt::Vertical, m_filesTab);

    m_tree = new QTreeWidget(m_filesTab);
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(14);
    splitter->addWidget(m_tree);

    m_editor = new QTextEdit(m_filesTab);
    m_editor->setReadOnly(true);
    m_editor->setPlaceholderText(tr("Select a memory file to view/edit..."));
    m_editor->setStyleSheet(QStringLiteral(
        "QTextEdit { font-family: 'Consolas','Courier New',monospace; "
        "font-size: 12px; background: #1e1e1e; color: #d4d4d4; "
        "border: 1px solid #3c3c3c; }"));
    splitter->addWidget(m_editor);

    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    lay->addWidget(splitter);

    connect(m_tree, &QTreeWidget::itemClicked, this, &MemoryBrowserPanel::onItemClicked);
    connect(m_saveBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::saveCurrentFile);
    connect(m_deleteBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::deleteCurrentFile);
    connect(m_newBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::createNewFile);
    connect(m_refreshBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::refresh);

    m_tabWidget->addTab(m_filesTab, tr("Files"));
}

// ── Facts Tab ───────────────────────────────────────────────────────────────

void MemoryBrowserPanel::setupFactsTab()
{
    m_factsTab = new QWidget(this);
    auto *lay = new QVBoxLayout(m_factsTab);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    auto *toolbar = new QHBoxLayout;
    auto *addBtn = new QToolButton(m_factsTab);
    addBtn->setText(tr("Add Fact"));
    auto *delBtn = new QToolButton(m_factsTab);
    delBtn->setText(tr("Delete"));
    auto *refBtn = new QToolButton(m_factsTab);
    refBtn->setText(tr("Refresh"));

    toolbar->addWidget(addBtn);
    toolbar->addStretch();
    toolbar->addWidget(delBtn);
    toolbar->addWidget(refBtn);
    lay->addLayout(toolbar);

    m_factsTable = new QTableWidget(0, 4, m_factsTab);
    m_factsTable->setHorizontalHeaderLabels({tr("Category"), tr("Key"), tr("Value"), tr("Confidence")});
    m_factsTable->horizontalHeader()->setStretchLastSection(true);
    m_factsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_factsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_factsTable->verticalHeader()->hide();
    lay->addWidget(m_factsTable);

    connect(addBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::addFact);
    connect(delBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::deleteFact);
    connect(refBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::refreshFacts);

    m_tabWidget->addTab(m_factsTab, tr("Facts"));
}

// ── Rules Tab ───────────────────────────────────────────────────────────────

void MemoryBrowserPanel::setupRulesTab()
{
    m_rulesTab = new QWidget(this);
    auto *lay = new QVBoxLayout(m_rulesTab);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    auto *toolbar = new QHBoxLayout;
    auto *addBtn = new QToolButton(m_rulesTab);
    addBtn->setText(tr("Add Rule"));
    auto *delBtn = new QToolButton(m_rulesTab);
    delBtn->setText(tr("Delete"));
    auto *refBtn = new QToolButton(m_rulesTab);
    refBtn->setText(tr("Refresh"));

    toolbar->addWidget(addBtn);
    toolbar->addStretch();
    toolbar->addWidget(delBtn);
    toolbar->addWidget(refBtn);
    lay->addLayout(toolbar);

    m_rulesTable = new QTableWidget(0, 4, m_rulesTab);
    m_rulesTable->setHorizontalHeaderLabels({tr("Category"), tr("Severity"), tr("Text"), tr("Scope")});
    m_rulesTable->horizontalHeader()->setStretchLastSection(true);
    m_rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_rulesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_rulesTable->verticalHeader()->hide();
    lay->addWidget(m_rulesTable);

    connect(addBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::addRule);
    connect(delBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::deleteRule);
    connect(refBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::refreshRules);

    m_tabWidget->addTab(m_rulesTab, tr("Rules"));
}

// ── Sessions Tab ────────────────────────────────────────────────────────────

void MemoryBrowserPanel::setupSessionsTab()
{
    m_sessionsTab = new QWidget(this);
    auto *lay = new QVBoxLayout(m_sessionsTab);
    lay->setContentsMargins(4, 4, 4, 4);
    lay->setSpacing(4);

    auto *refBtn = new QToolButton(m_sessionsTab);
    refBtn->setText(tr("Refresh"));
    auto *toolbar = new QHBoxLayout;
    toolbar->addStretch();
    toolbar->addWidget(refBtn);
    lay->addLayout(toolbar);

    m_sessionsTable = new QTableWidget(0, 4, m_sessionsTab);
    m_sessionsTable->setHorizontalHeaderLabels(
        {tr("Title"), tr("Objective"), tr("Result"), tr("Summary")});
    m_sessionsTable->horizontalHeader()->setStretchLastSection(true);
    m_sessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_sessionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_sessionsTable->verticalHeader()->hide();
    lay->addWidget(m_sessionsTable);

    connect(refBtn, &QToolButton::clicked, this, &MemoryBrowserPanel::refreshSessions);

    m_tabWidget->addTab(m_sessionsTab, tr("Sessions"));
}

// ── setBrainService ─────────────────────────────────────────────────────────

void MemoryBrowserPanel::setBrainService(ProjectBrainService *brain)
{
    m_brain = brain;
    if (m_brain) {
        connect(m_brain, &ProjectBrainService::factsChanged,
                this, &MemoryBrowserPanel::refreshFacts);
        connect(m_brain, &ProjectBrainService::rulesChanged,
                this, &MemoryBrowserPanel::refreshRules);
        connect(m_brain, &ProjectBrainService::summariesChanged,
                this, &MemoryBrowserPanel::refreshSessions);
        refreshFacts();
        refreshRules();
        refreshSessions();
    }
}

// ── Refresh ────────────────────────────────────────────────────────────────

void MemoryBrowserPanel::refresh()
{
    // Refresh files tab
    m_tree->clear();
    m_editor->clear();
    m_editor->setReadOnly(true);
    m_saveBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);
    m_currentFilePath.clear();

    auto *rootItem = new QTreeWidgetItem(m_tree, {tr("memories")});
    rootItem->setExpanded(true);
    populateTree(m_basePath, rootItem);

    // Refresh brain tabs
    refreshFacts();
    refreshRules();
    refreshSessions();
}

void MemoryBrowserPanel::refreshFacts()
{
    m_factsTable->setRowCount(0);
    if (!m_brain) return;

    const auto &facts = m_brain->facts();
    m_factsTable->setRowCount(facts.size());
    for (int i = 0; i < facts.size(); ++i) {
        const auto &f = facts[i];
        m_factsTable->setItem(i, 0, new QTableWidgetItem(f.category));
        m_factsTable->setItem(i, 1, new QTableWidgetItem(f.key));
        m_factsTable->setItem(i, 2, new QTableWidgetItem(f.value));
        m_factsTable->setItem(i, 3, new QTableWidgetItem(
            QString::number(f.confidence, 'f', 1)));
        // Store ID in first column item
        m_factsTable->item(i, 0)->setData(Qt::UserRole, f.id);
    }
}

void MemoryBrowserPanel::refreshRules()
{
    m_rulesTable->setRowCount(0);
    if (!m_brain) return;

    const auto &rules = m_brain->rules();
    m_rulesTable->setRowCount(rules.size());
    for (int i = 0; i < rules.size(); ++i) {
        const auto &r = rules[i];
        m_rulesTable->setItem(i, 0, new QTableWidgetItem(r.category));
        m_rulesTable->setItem(i, 1, new QTableWidgetItem(r.severity));
        m_rulesTable->setItem(i, 2, new QTableWidgetItem(r.text));
        m_rulesTable->setItem(i, 3, new QTableWidgetItem(r.scope.join(QStringLiteral(", "))));
        m_rulesTable->item(i, 0)->setData(Qt::UserRole, r.id);
    }
}

void MemoryBrowserPanel::refreshSessions()
{
    m_sessionsTable->setRowCount(0);
    if (!m_brain) return;

    const auto &sessions = m_brain->summaries();
    m_sessionsTable->setRowCount(sessions.size());
    for (int i = 0; i < sessions.size(); ++i) {
        const auto &s = sessions[i];
        m_sessionsTable->setItem(i, 0, new QTableWidgetItem(s.title));
        m_sessionsTable->setItem(i, 1, new QTableWidgetItem(s.objective));
        m_sessionsTable->setItem(i, 2, new QTableWidgetItem(s.result));
        m_sessionsTable->setItem(i, 3, new QTableWidgetItem(s.summary));
    }
}

// ── Facts CRUD ──────────────────────────────────────────────────────────────

void MemoryBrowserPanel::addFact()
{
    if (!m_brain) return;

    bool ok = false;
    const QString key = QInputDialog::getText(this, tr("Add Memory Fact"),
        tr("Fact key (short name):"), QLineEdit::Normal, QString(), &ok);
    if (!ok || key.trimmed().isEmpty()) return;

    const QString value = QInputDialog::getMultiLineText(this, tr("Add Memory Fact"),
        tr("Fact value:"), QString(), &ok);
    if (!ok || value.trimmed().isEmpty()) return;

    MemoryFact fact;
    fact.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    fact.category = QStringLiteral("user");
    fact.key = key.trimmed();
    fact.value = value.trimmed();
    fact.confidence = 1.0;
    fact.source = QStringLiteral("user_confirmed");
    fact.updatedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    m_brain->addFact(fact);
}

void MemoryBrowserPanel::deleteFact()
{
    if (!m_brain) return;
    const int row = m_factsTable->currentRow();
    if (row < 0) return;

    const QString id = m_factsTable->item(row, 0)->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    if (QMessageBox::question(this, tr("Delete Fact"),
            tr("Delete fact \"%1\"?").arg(m_factsTable->item(row, 1)->text()),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_brain->forgetFact(id);
    }
}

// ── Rules CRUD ──────────────────────────────────────────────────────────────

void MemoryBrowserPanel::addRule()
{
    if (!m_brain) return;

    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Add Project Rule"),
        tr("Rule text:"), QString(), &ok);
    if (!ok || text.trimmed().isEmpty()) return;

    ProjectRule rule;
    rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rule.category = QStringLiteral("custom");
    rule.text = text.trimmed();
    rule.severity = QStringLiteral("should");
    m_brain->addRule(rule);
}

void MemoryBrowserPanel::deleteRule()
{
    if (!m_brain) return;
    const int row = m_rulesTable->currentRow();
    if (row < 0) return;

    const QString id = m_rulesTable->item(row, 0)->data(Qt::UserRole).toString();
    if (id.isEmpty()) return;

    if (QMessageBox::question(this, tr("Delete Rule"),
            tr("Delete this rule?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        m_brain->removeRule(id);
    }
}

// ── Files Tab methods (unchanged) ──────────────────────────────────────────

void MemoryBrowserPanel::populateTree(const QString &dirPath, QTreeWidgetItem *parentItem)
{
    QDir dir(dirPath);
    for (const QString &subDir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        auto *item = new QTreeWidgetItem(parentItem, {subDir + QStringLiteral("/")});
        item->setData(0, Qt::UserRole, dir.absoluteFilePath(subDir));
        item->setExpanded(true);
        populateTree(dir.absoluteFilePath(subDir), item);
    }
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
