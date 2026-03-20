#include "cppworkspacepanel.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QScrollBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

// ── VS2022 dark theme color constants ─────────────────────────────────────────
namespace VS {
    constexpr auto BgPanel       = "#1e1e1e";
    constexpr auto BgHeader      = "#2d2d30";
    constexpr auto BgHover       = "#3e3e40";
    constexpr auto BgSelected    = "#094771";
    constexpr auto BgStrip       = "#252526";
    constexpr auto TxtNormal     = "#c8c8c8";
    constexpr auto TxtDim        = "#848484";
    constexpr auto TxtHeader     = "#ffffff";
    constexpr auto TxtSelected   = "#ffffff";
    constexpr auto AccentBlue    = "#007acc";
    // file-type colors (match VS2022 token colors)
    constexpr auto TxtCpp        = "#9cdcfe";   // light blue — .cpp/.c
    constexpr auto TxtHeader_    = "#c586c0";   // purple     — .h/.hpp
    constexpr auto TxtCmake      = "#ce9178";   // orange     — CMakeLists.txt/.cmake
    constexpr auto TxtPython     = "#dcdcaa";   // yellow     — .py
    constexpr auto TxtJson       = "#d7ba7d";   // gold       — .json
}

// ── Helper: small flat VS-style tool button ────────────────────────────────────
static QToolButton *makeHeaderButton(const QString &text, const QString &tip,
                                     QWidget *parent)
{
    auto *btn = new QToolButton(parent);
    btn->setText(text);
    btn->setToolTip(tip);
    btn->setAutoRaise(true);
    btn->setFixedSize(22, 22);
    btn->setStyleSheet(QStringLiteral(
        "QToolButton {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  font-size: 12px;"
        "  padding: 0;"
        "}"
        "QToolButton:hover { background: %2; }"
        "QToolButton:pressed { background: %3; }"
    ).arg(VS::TxtNormal, VS::BgHover, VS::AccentBlue));
    return btn;
}

// ── File helpers ───────────────────────────────────────────────────────────────
bool CppWorkspacePanel::isRelevantFile(const QString &name)
{
    const QString ext = QFileInfo(name).suffix().toLower();
    static const QSet<QString> kExts = {
        "cpp", "cxx", "cc", "c",
        "h",   "hpp", "hxx", "inl",
        "cmake", "txt",  // CMakeLists.txt
        "py", "json", "toml", "yaml", "yml",
        "rs", "go", "ts", "js",
        "md", "sh", "ps1",
    };
    if (kExts.contains(ext)) return true;
    if (name.toLower() == "cmakelists.txt") return true;
    return false;
}

bool CppWorkspacePanel::isExcludedDir(const QString &name)
{
    static const QSet<QString> kExcl = {
        ".git", ".cache", ".vs", ".vscode",
        "build", "build-llvm", "build-msvc",
        "node_modules", "__pycache__",
        "CMakeFiles", "_deps", "Testing",
        ".mypy_cache", ".pytest_cache",
    };
    return kExcl.contains(name);
}

QColor CppWorkspacePanel::fileColor(const QString &name)
{
    const QString n   = name.toLower();
    const QString ext = QFileInfo(n).suffix();
    if (ext == "cpp" || ext == "cxx" || ext == "cc" || ext == "c")
        return QColor(VS::TxtCpp);
    if (ext == "h" || ext == "hpp" || ext == "hxx" || ext == "inl")
        return QColor(VS::TxtHeader_);
    if (ext == "cmake" || n == "cmakelists.txt")
        return QColor(VS::TxtCmake);
    if (ext == "py")
        return QColor(VS::TxtPython);
    if (ext == "json" || ext == "toml" || ext == "yaml" || ext == "yml")
        return QColor(VS::TxtJson);
    return QColor(VS::TxtNormal);
}

// ── Constructor ───────────────────────────────────────────────────────────────
CppWorkspacePanel::CppWorkspacePanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("cppWorkspacePanel"));
    setStyleSheet(QStringLiteral("QWidget#cppWorkspacePanel { background: %1; }")
                  .arg(VS::BgPanel));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header toolbar ────────────────────────────────────────────────────────
    auto *header = new QWidget(this);
    header->setFixedHeight(28);
    header->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid #3f3f46; }")
        .arg(VS::BgHeader));

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 0, 4, 0);
    headerLayout->setSpacing(2);

    m_titleLabel = new QLabel(tr("C++ Workspace"), header);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 12px; font-weight: 600;"
        "  background: transparent; border: none; }"
    ).arg(VS::TxtHeader));
    headerLayout->addWidget(m_titleLabel, 1);

    m_searchBtn = makeHeaderButton(QStringLiteral("\U0001F50D"), tr("Search (Ctrl+F)"), header);
    connect(m_searchBtn, &QToolButton::clicked, this, &CppWorkspacePanel::toggleFilterBar);
    headerLayout->addWidget(m_searchBtn);

    auto *collapseBtn = makeHeaderButton(QStringLiteral("\u2296"), tr("Collapse All"), header);
    connect(collapseBtn, &QToolButton::clicked, this, &CppWorkspacePanel::collapseAll);
    headerLayout->addWidget(collapseBtn);

    auto *refreshBtn = makeHeaderButton(QStringLiteral("\u21BB"), tr("Refresh"), header);
    connect(refreshBtn, &QToolButton::clicked, this, &CppWorkspacePanel::refreshTree);
    headerLayout->addWidget(refreshBtn);

    root->addWidget(header);

    // ── Filter bar (hidden by default) ────────────────────────────────────────
    m_filterBar = new QWidget(this);
    m_filterBar->setVisible(false);
    m_filterBar->setFixedHeight(28);
    m_filterBar->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid #3f3f46; }")
        .arg(VS::BgHeader));

    auto *filterLayout = new QHBoxLayout(m_filterBar);
    filterLayout->setContentsMargins(8, 2, 4, 2);
    filterLayout->setSpacing(4);

    auto *filterIcon = new QLabel(QStringLiteral("\U0001F50D"), m_filterBar);
    filterIcon->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(VS::TxtDim));
    filterLayout->addWidget(filterIcon);

    m_filterEdit = new QLineEdit(m_filterBar);
    m_filterEdit->setPlaceholderText(tr("Filter files..."));
    m_filterEdit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid #3f3f46;"
        "  border-radius: 2px;"
        "  padding: 1px 4px;"
        "  font-size: 11px;"
        "}"
        "QLineEdit:focus { border-color: %3; }"
    ).arg(VS::BgPanel, VS::TxtNormal, VS::AccentBlue));
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &CppWorkspacePanel::applyFilter);
    filterLayout->addWidget(m_filterEdit, 1);

    auto *clearFilter = makeHeaderButton(QStringLiteral("\u2715"), tr("Clear Filter"), m_filterBar);
    connect(clearFilter, &QToolButton::clicked, this, [this]() {
        m_filterEdit->clear();
        setFilterVisible(false);
    });
    filterLayout->addWidget(clearFilter);

    root->addWidget(m_filterBar);

    // ── File tree ─────────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setAnimated(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(false);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setIndentation(16);
    m_tree->setIconSize(QSize(16, 16));
    m_tree->verticalScrollBar()->setStyleSheet(QStringLiteral(
        "QScrollBar:vertical { width: 8px; background: %1; margin: 0; }"
        "QScrollBar::handle:vertical { background: #3f3f46; min-height: 20px; border-radius: 4px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(VS::BgPanel));

    m_tree->setStyleSheet(QStringLiteral(
        "QTreeWidget {"
        "  background-color: %1;"
        "  color: %2;"
        "  border: none;"
        "  outline: 0;"
        "  selection-background-color: transparent;"
        "  font-size: 12px;"
        "}"
        "QTreeWidget::item {"
        "  padding: 1px 0px;"
        "  min-height: 20px;"
        "  border: none;"
        "}"
        "QTreeWidget::item:hover {"
        "  background-color: %3;"
        "  color: %4;"
        "}"
        "QTreeWidget::item:selected {"
        "  background-color: %5;"
        "  color: %6;"
        "}"
        "QTreeWidget::item:selected:hover {"
        "  background-color: %5;"
        "}"
        "QTreeWidget::branch {"
        "  background: %1;"
        "}"
        "QTreeWidget::branch:has-children:!has-siblings:closed,"
        "QTreeWidget::branch:closed:has-children:has-siblings {"
        "  border-image: none;"
        "}"
        "QTreeWidget::branch:open:has-children:!has-siblings,"
        "QTreeWidget::branch:open:has-children:has-siblings {"
        "  border-image: none;"
        "}"
    ).arg(VS::BgPanel,    // 1 bg
         VS::TxtNormal,   // 2 text
         VS::BgHover,     // 3 hover bg
         VS::TxtHeader,   // 4 hover text
         VS::BgSelected,  // 5 selected bg
         VS::TxtSelected  // 6 selected text
    ));

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &CppWorkspacePanel::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &CppWorkspacePanel::onItemContextMenu);

    root->addWidget(m_tree, 1);

    // ── Status strip ──────────────────────────────────────────────────────────
    auto *strip = new QWidget(this);
    strip->setFixedHeight(26);
    strip->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-top: 1px solid #3f3f46; }")
        .arg(VS::BgStrip));

    auto *stripLayout = new QHBoxLayout(strip);
    stripLayout->setContentsMargins(8, 0, 8, 0);
    stripLayout->setSpacing(16);

    const QList<QPair<QString, QString>> statusDefs = {
        {QStringLiteral("language"), tr("clangd")},
        {QStringLiteral("build"),    tr("Build")},
        {QStringLiteral("tests"),    tr("Tests")},
    };
    for (const auto &[id, name] : statusDefs) {
        auto *lbl = new QLabel(
            QStringLiteral("\u25CF %1: \u2014").arg(name), strip);
        lbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 10px; background: transparent; }")
            .arg(VS::TxtDim));
        m_statusLabels.insert(id, lbl);
        stripLayout->addWidget(lbl);
    }
    stripLayout->addStretch();
    root->addWidget(strip);
}

// ── Public API ────────────────────────────────────────────────────────────────

void CppWorkspacePanel::setWorkspaceSummary(const QString &workspaceRoot,
                                            const QString &activeFile,
                                            const QString &buildDirectory,
                                            const QString & /*compileCommandsPath*/)
{
    m_activeFile = activeFile;

    if (workspaceRoot != m_workspaceRoot) {
        m_workspaceRoot = workspaceRoot;
        buildTree();

        const QString name = workspaceRoot.isEmpty()
            ? tr("(no workspace)")
            : QFileInfo(workspaceRoot).fileName();
        m_titleLabel->setText(tr("C++ Workspace — %1").arg(name));
    }

    Q_UNUSED(buildDirectory);
}

void CppWorkspacePanel::setCardStatus(const QString &cardId,
                                      const QString &title,
                                      const QString &detail)
{
    auto *lbl = m_statusLabels.value(cardId, nullptr);
    if (!lbl) return;

    // Pick a dot color based on title heuristic
    QColor dotColor(VS::AccentBlue);
    const QString tl = title.toLower();
    if (tl.contains("ready") || tl.contains("ok") || tl.contains("success"))
        dotColor = QColor("#4ec9b0");   // green-teal (VS ready color)
    else if (tl.contains("error") || tl.contains("fail"))
        dotColor = QColor("#f14c4c");   // red
    else if (tl.contains("build") || tl.contains("index") || tl.contains("start"))
        dotColor = QColor("#d7ba7d");   // yellow/amber

    const QString dot = QStringLiteral(
        "<span style='color:%1'>\u25CF</span> ").arg(dotColor.name());

    const QString label = cardId == "language" ? tr("clangd")
                        : cardId == "build"    ? tr("Build")
                        : cardId == "tests"    ? tr("Tests")
                        : cardId;

    lbl->setText(dot + label + QStringLiteral(": ") + title);
    lbl->setToolTip(detail);
    lbl->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 10px; background: transparent; }")
        .arg(VS::TxtNormal));
}

void CppWorkspacePanel::setActionEnabled(const QString &actionId, bool enabled)
{
    if (enabled)
        m_disabledActions.remove(actionId);
    else
        m_disabledActions.insert(actionId);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void CppWorkspacePanel::toggleFilterBar()
{
    setFilterVisible(!m_filterBar->isVisible());
}

void CppWorkspacePanel::setFilterVisible(bool visible)
{
    m_filterBar->setVisible(visible);
    if (visible) {
        m_filterEdit->setFocus();
        m_filterEdit->selectAll();
    } else {
        m_filterEdit->clear();
        // Show all items
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
            filterItems(m_tree->topLevelItem(i), {});
    }
}

void CppWorkspacePanel::applyFilter(const QString &text)
{
    const QString lower = text.trimmed().toLower();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        filterItems(m_tree->topLevelItem(i), lower);
}

void CppWorkspacePanel::filterItems(QTreeWidgetItem *item, const QString &lower)
{
    if (!item) return;
    bool anyChildVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        filterItems(item->child(i), lower);
        if (!item->child(i)->isHidden())
            anyChildVisible = true;
    }

    if (item->childCount() > 0) {
        // Folder: visible if any child visible or filter empty
        item->setHidden(!lower.isEmpty() && !anyChildVisible);
        if (anyChildVisible && !lower.isEmpty())
            item->setExpanded(true);
    } else {
        // Leaf (file)
        const bool match = lower.isEmpty()
                        || item->text(0).toLower().contains(lower);
        item->setHidden(!match);
    }
}

void CppWorkspacePanel::collapseAll()
{
    m_tree->collapseAll();
    // Keep root expanded
    if (m_tree->topLevelItemCount() > 0)
        m_tree->topLevelItem(0)->setExpanded(true);
}

void CppWorkspacePanel::refreshTree()
{
    buildTree();
}

void CppWorkspacePanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*col*/)
{
    if (!item) return;
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty()) return;
    if (QFileInfo(path).isFile()) {
        emit fileOpenRequested(path);
        emit commandRequested(QStringLiteral("cpp.openFile:") + path);
    }
}

void CppWorkspacePanel::onItemContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item) return;

    const QString path  = item->data(0, Qt::UserRole).toString();
    const bool isFile   = !path.isEmpty() && QFileInfo(path).isFile();
    const bool isDir    = !path.isEmpty() && QFileInfo(path).isDir();

    QMenu menu(this);
    menu.setStyleSheet(QStringLiteral(
        "QMenu {"
        "  background: #2d2d30;"
        "  color: #c8c8c8;"
        "  border: 1px solid #3f3f46;"
        "  padding: 2px;"
        "}"
        "QMenu::item { padding: 4px 24px 4px 16px; }"
        "QMenu::item:selected { background: #094771; color: #fff; }"
        "QMenu::separator { background: #3f3f46; height: 1px; margin: 2px 8px; }"
    ));

    if (isFile) {
        menu.addAction(tr("Open"), this, [this, path]() {
            emit fileOpenRequested(path);
        });

        const QString ext = QFileInfo(path).suffix().toLower();
        if (ext == "cpp" || ext == "h" || ext == "cxx" || ext == "hpp" || ext == "c") {
            menu.addSeparator();
            auto *def = menu.addAction(tr("Go to Definition  F12"), this, [this]() {
                emit commandRequested(QStringLiteral("cpp.goToDefinition"));
            });
            auto *refs = menu.addAction(tr("Find All References  Shift+F12"), this, [this]() {
                emit commandRequested(QStringLiteral("cpp.findReferences"));
            });
            auto *sw = menu.addAction(tr("Switch Header/Source  Alt+O"), this, [this]() {
                emit commandRequested(QStringLiteral("cpp.switchHeaderSource"));
            });
            if (m_disabledActions.contains("cpp.goToDefinition"))  def->setEnabled(false);
            if (m_disabledActions.contains("cpp.findReferences"))  refs->setEnabled(false);
            if (m_disabledActions.contains("cpp.switchHeaderSource")) sw->setEnabled(false);
        }

        menu.addSeparator();
        menu.addAction(tr("Copy Full Path"), this, [path]() {
            QApplication::clipboard()->setText(path);
        });
        menu.addAction(tr("Copy Relative Path"), this, [this, path]() {
            const QString rel = QDir(m_workspaceRoot).relativeFilePath(path);
            QApplication::clipboard()->setText(rel);
        });
    }

    if (isDir) {
        menu.addAction(tr("Find in Files…"), this, [this]() {
            emit commandRequested(QStringLiteral("cpp.focusSearch"));
        });
        menu.addAction(tr("Open Terminal Here"), this, [this]() {
            emit buildTerminalRequested();
        });
        menu.addSeparator();
        menu.addAction(tr("Refresh"), this, [this]() { buildTree(); });
    }

    if (!menu.isEmpty())
        menu.exec(m_tree->viewport()->mapToGlobal(pos));
}

// ── Tree building ─────────────────────────────────────────────────────────────

void CppWorkspacePanel::buildTree()
{
    m_tree->clear();
    if (m_workspaceRoot.isEmpty()) return;

    QFileIconProvider iconProvider;

    // Root item — "Solution 'ProjectName'"
    const QString solutionName = QFileInfo(m_workspaceRoot).fileName();
    auto *root = new QTreeWidgetItem(m_tree);
    root->setText(0, QStringLiteral("\U0001F4C2 Solution \u2018%1\u2019").arg(solutionName));
    root->setForeground(0, QColor(VS::TxtHeader));
    root->setData(0, Qt::UserRole, m_workspaceRoot);
    root->setFont(0, [&] {
        QFont f = m_tree->font();
        f.setWeight(QFont::DemiBold);
        return f;
    }());

    populateDirectory(root, m_workspaceRoot, 0);
    root->setExpanded(true);

    // Auto-expand the first level
    for (int i = 0; i < root->childCount(); ++i)
        root->child(i)->setExpanded(false);
}

void CppWorkspacePanel::populateDirectory(QTreeWidgetItem *parent,
                                           const QString   &dirPath,
                                           int              depth)
{
    if (depth > 6) return;

    QFileIconProvider iconProvider;
    QDir dir(dirPath);
    dir.setSorting(QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    const auto entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot);

    for (const QFileInfo &fi : entries) {
        const QString name = fi.fileName();

        if (fi.isDir()) {
            if (isExcludedDir(name)) continue;

            auto *item = new QTreeWidgetItem(parent);
            item->setText(0, name);
            item->setIcon(0, iconProvider.icon(QFileIconProvider::Folder));
            item->setForeground(0, QColor(VS::TxtNormal));
            item->setData(0, Qt::UserRole, fi.absoluteFilePath());
            populateDirectory(item, fi.absoluteFilePath(), depth + 1);

            // Prune empty directory items
            if (item->childCount() == 0) {
                delete parent->takeChild(parent->indexOfChild(item));
            }
        } else {
            if (!isRelevantFile(name)) continue;

            auto *item = new QTreeWidgetItem(parent);
            item->setText(0, name);
            item->setIcon(0, iconProvider.icon(fi));
            item->setForeground(0, fileColor(name));
            item->setData(0, Qt::UserRole, fi.absoluteFilePath());
            item->setToolTip(0, fi.absoluteFilePath());
        }
    }
}
