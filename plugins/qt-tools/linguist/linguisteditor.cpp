// linguisteditor.cpp — Qt Linguist (.ts) translation editor implementation.
//
// Internals overview:
//
//   • TranslationModel — flat QAbstractTableModel over (contextIdx, msgIdx)
//     pairs into the underlying TsFile.  Uses Qt::EditRole on the translation
//     column for inline editing; emits dataChanged() when status changes so
//     the contexts list (left) and progress bar (bottom) refresh.
//
//   • A QSortFilterProxyModel wraps the model.  We override filterAcceptsRow
//     in a simple way by using QSortFilterProxyModel's filterRegularExpression
//     against a synthetic "search bag" column the model exposes via
//     Qt::UserRole on column 0.  Context filtering (clicking the contexts
//     list) is implemented as a custom string match against a second user
//     role on column 0 (UserRole+1).
//
//   • External tools (lupdate / lrelease) are launched via QProcess and their
//     stdout/stderr streamed into IOutputService when available; otherwise
//     into the editor's bottom status label.
//
// All UX shortcuts follow docs/ux-principles.md (keyboard-first, no modal
// dialogs for trivial actions, search at top of long lists).
#include "linguisteditor.h"

#include "tsxmlio.h"

#include <QAbstractTableModel>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextStream>
#include <QToolBar>
#include <QVBoxLayout>
#include <QFile>

namespace exo::forms::linguist {

// ── Status icons (procedural — avoids dragging in an icon resource) ─────────
static QIcon makeStatusIcon(const QColor &color, bool checkmark)
{
    QPixmap pm(16, 16);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(2, 2, 12, 12);
    if (checkmark) {
        QPen pen(Qt::white, 2.0);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        p.drawLine(5, 9, 7, 11);
        p.drawLine(7, 11, 11, 5);
    }
    return QIcon(pm);
}

static QIcon iconForStatus(const QString &status)
{
    if (status.isEmpty())
        return makeStatusIcon(QColor(QStringLiteral("#3fb950")), true);   // green check
    if (status == QStringLiteral("unfinished"))
        return makeStatusIcon(QColor(QStringLiteral("#d29922")), false);  // amber dot
    return makeStatusIcon(QColor(QStringLiteral("#6e7681")), false);      // gray dot (obsolete/vanished)
}

// ── Internal model ──────────────────────────────────────────────────────────
class TranslationModel : public QAbstractTableModel {
public:
    enum Column { ColStatus = 0, ColSource, ColTranslation, ColLocation, ColCount };
    enum Roles {
        SearchBagRole = Qt::UserRole + 0,    // concatenated source+translation+context for filtering
        ContextNameRole = Qt::UserRole + 1,  // context name for context filtering
        StatusRole      = Qt::UserRole + 2,  // raw status string
    };

    explicit TranslationModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setFile(TsFile *file)
    {
        beginResetModel();
        m_file = file;
        m_index.clear();
        if (m_file) {
            for (int c = 0; c < m_file->contexts.size(); ++c) {
                for (int m = 0; m < m_file->contexts[c].messages.size(); ++m) {
                    m_index.append({c, m});
                }
            }
        }
        endResetModel();
    }

    int rowCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : m_index.size();
    }
    int columnCount(const QModelIndex &parent = {}) const override
    {
        return parent.isValid() ? 0 : ColCount;
    }

    QVariant headerData(int section, Qt::Orientation o, int role = Qt::DisplayRole) const override
    {
        if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
        switch (section) {
            case ColStatus:      return QStringLiteral("");
            case ColSource:      return QStringLiteral("Source");
            case ColTranslation: return QStringLiteral("Translation");
            case ColLocation:    return QStringLiteral("Locations");
        }
        return {};
    }

    Qt::ItemFlags flags(const QModelIndex &idx) const override
    {
        if (!idx.isValid() || !m_file) return Qt::NoItemFlags;
        Qt::ItemFlags f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        if (idx.column() == ColTranslation) {
            const TsMessage *m = messageAt(idx.row());
            if (m && m->status != QStringLiteral("obsolete")
                  && m->status != QStringLiteral("vanished"))
                f |= Qt::ItemIsEditable;
        }
        return f;
    }

    QVariant data(const QModelIndex &idx, int role) const override
    {
        if (!idx.isValid() || !m_file) return {};
        const TsMessage *m = messageAt(idx.row());
        if (!m) return {};
        const auto &ctx = m_file->contexts[m_index[idx.row()].first];

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (idx.column()) {
                case ColSource:      return m->source;
                case ColTranslation: return m->translation;
                case ColLocation: {
                    QStringList parts;
                    for (const auto &loc : m->locations) {
                        if (!loc.first.isEmpty())
                            parts << QStringLiteral("%1:%2").arg(QFileInfo(loc.first).fileName())
                                                            .arg(loc.second);
                    }
                    return parts.join(QLatin1String(", "));
                }
                case ColStatus: return QString();
            }
        }
        if (role == Qt::DecorationRole && idx.column() == ColStatus) {
            return iconForStatus(m->status);
        }
        if (role == Qt::ToolTipRole) {
            if (idx.column() == ColStatus) {
                if (m->status.isEmpty())              return QStringLiteral("Finished");
                if (m->status == QStringLiteral("unfinished")) return QStringLiteral("Unfinished");
                return QStringLiteral("Obsolete");
            }
            if (idx.column() == ColSource && !m->comment.isEmpty())
                return QStringLiteral("%1\n— %2").arg(m->source, m->comment);
            return m->source;
        }
        if (role == SearchBagRole) {
            return ctx.name + QLatin1Char('\n') + m->source + QLatin1Char('\n') + m->translation;
        }
        if (role == ContextNameRole)  return ctx.name;
        if (role == StatusRole)       return m->status;
        return {};
    }

    bool setData(const QModelIndex &idx, const QVariant &value, int role) override
    {
        if (!idx.isValid() || !m_file || role != Qt::EditRole) return false;
        TsMessage *m = mutableMessageAt(idx.row());
        if (!m) return false;

        if (idx.column() == ColTranslation) {
            const QString newText = value.toString();
            if (newText == m->translation) return false;
            m->translation = newText;
            // If a translator typed text into a previously empty unfinished
            // entry, downgrade `unfinished` only when explicit.  Default Qt
            // Linguist behavior: typed text does NOT auto-mark finished —
            // user must explicitly mark it via Ctrl+Enter or the toolbar
            // button.  We mirror that.
            if (m->status != QStringLiteral("unfinished")
                && m->status != QStringLiteral("obsolete")
                && m->status != QStringLiteral("vanished")
                && !newText.isEmpty()) {
                // already finished, leave it
            } else if (m->status.isEmpty() && newText.isEmpty()) {
                m->status = QStringLiteral("unfinished");
            }
            emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole, SearchBagRole});
            // status icon column may want a redraw if we flipped status above
            const QModelIndex statusIdx = index(idx.row(), ColStatus);
            emit dataChanged(statusIdx, statusIdx, {Qt::DecorationRole, StatusRole, Qt::ToolTipRole});
            return true;
        }
        return false;
    }

    void setStatusForRow(int row, const QString &status)
    {
        TsMessage *m = mutableMessageAt(row);
        if (!m) return;
        if (m->status == status) return;
        m->status = status;
        const QModelIndex tl = index(row, 0);
        const QModelIndex br = index(row, ColCount - 1);
        emit dataChanged(tl, br, {Qt::DecorationRole, StatusRole, Qt::ToolTipRole});
    }

    QPair<int,int> mapping(int row) const
    {
        if (row < 0 || row >= m_index.size()) return {-1, -1};
        return m_index[row];
    }

    const TsMessage *messageAt(int row) const
    {
        if (!m_file || row < 0 || row >= m_index.size()) return nullptr;
        const auto p = m_index[row];
        if (p.first < 0 || p.first >= m_file->contexts.size()) return nullptr;
        const auto &c = m_file->contexts[p.first];
        if (p.second < 0 || p.second >= c.messages.size()) return nullptr;
        return &c.messages[p.second];
    }

private:
    TsMessage *mutableMessageAt(int row)
    {
        if (!m_file || row < 0 || row >= m_index.size()) return nullptr;
        const auto p = m_index[row];
        if (p.first < 0 || p.first >= m_file->contexts.size()) return nullptr;
        auto &c = m_file->contexts[p.first];
        if (p.second < 0 || p.second >= c.messages.size()) return nullptr;
        return &c.messages[p.second];
    }

    TsFile *m_file = nullptr;
    QList<QPair<int,int>> m_index;   // rowIdx → (contextIdx, msgIdx)
};

// ── Multi-condition proxy: search text + context filter ─────────────────────
class TranslationProxy : public QSortFilterProxyModel {
public:
    explicit TranslationProxy(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    void setSearchText(const QString &text)
    {
        m_search = text;
        invalidateRowsFilter();
    }
    void setContextFilter(const QString &name)
    {
        m_context = name;
        invalidateRowsFilter();
    }

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        const QModelIndex src = sourceModel()->index(row, 0, parent);

        if (!m_context.isEmpty()) {
            const QString ctx = src.data(TranslationModel::ContextNameRole).toString();
            if (ctx != m_context) return false;
        }
        if (!m_search.isEmpty()) {
            const QString bag = src.data(TranslationModel::SearchBagRole).toString();
            if (!bag.contains(m_search, Qt::CaseInsensitive)) return false;
        }
        return true;
    }

private:
    QString m_search;
    QString m_context;
};

// ── LinguistEditor ──────────────────────────────────────────────────────────
LinguistEditor::LinguistEditor(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
}

LinguistEditor::~LinguistEditor() = default;

void LinguistEditor::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    buildToolbar();
    root->addWidget(m_toolbar);

    // Contexts list (left)
    m_contextsList = new QListWidget(this);
    m_contextsList->setObjectName(QStringLiteral("LinguistContexts"));
    m_contextsList->setUniformItemSizes(true);
    m_contextsList->setAlternatingRowColors(true);
    connect(m_contextsList, &QListWidget::itemSelectionChanged,
            this, &LinguistEditor::onContextSelectionChanged);

    // Translation table (center)
    m_model = new TranslationModel(this);
    m_proxy = new TranslationProxy(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);

    m_table = new QTableView(this);
    m_table->setModel(m_proxy);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setAlternatingRowColors(true);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked
                           | QAbstractItemView::EditKeyPressed
                           | QAbstractItemView::AnyKeyPressed);
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(TranslationModel::ColStatus,      QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(TranslationModel::ColSource,      QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(TranslationModel::ColTranslation, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(TranslationModel::ColLocation,    QHeaderView::ResizeToContents);
    m_table->setColumnWidth(TranslationModel::ColStatus, 28);
    m_table->setWordWrap(false);

    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &LinguistEditor::onTableSelectionChanged);
    connect(m_model, &QAbstractItemModel::dataChanged,
            this, &LinguistEditor::onModelDataChanged);

    // Source preview (right)
    m_sourcePreview = new QPlainTextEdit(this);
    m_sourcePreview->setReadOnly(true);
    m_sourcePreview->setObjectName(QStringLiteral("LinguistSourcePreview"));
    m_sourcePreview->setPlaceholderText(QStringLiteral("Source preview — select a row to view its location"));
    m_sourcePreview->setLineWrapMode(QPlainTextEdit::NoWrap);

    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_contextsList);
    m_splitter->addWidget(m_table);
    m_splitter->addWidget(m_sourcePreview);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setSizes({200, 800, 250});
    root->addWidget(m_splitter, /*stretch*/ 1);

    // Bottom status row: progress bar + label
    auto *statusRow = new QWidget(this);
    auto *sLayout = new QHBoxLayout(statusRow);
    sLayout->setContentsMargins(8, 4, 8, 4);
    sLayout->setSpacing(8);
    m_progress = new QProgressBar(statusRow);
    m_progress->setTextVisible(true);
    m_progress->setRange(0, 100);
    m_progress->setMaximumHeight(14);
    m_status = new QLabel(QStringLiteral("Ready"), statusRow);
    sLayout->addWidget(m_progress, /*stretch*/ 1);
    sLayout->addWidget(m_status,   /*stretch*/ 2);
    root->addWidget(statusRow);

    // ── Widget-scoped shortcuts (keyboard-first, no leaks across tabs) ────
    auto addShortcut = [&](QKeySequence seq, void (LinguistEditor::*slot)()) {
        auto *a = new QAction(this);
        a->setShortcut(seq);
        a->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        connect(a, &QAction::triggered, this, slot);
        addAction(a);
    };
    addShortcut(QKeySequence(QKeySequence::Save),                 &LinguistEditor::save);
    addShortcut(QKeySequence(QKeySequence::Find),                 &LinguistEditor::focusSearch);
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_L),               &LinguistEditor::focusContextList);
    addShortcut(QKeySequence(Qt::Key_F5),                          &LinguistEditor::reload);
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return),          &LinguistEditor::markCurrentFinished);
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Enter),           &LinguistEditor::markCurrentFinished);

    setFocusProxy(m_table);
    updateProgressBar();
}

void LinguistEditor::buildToolbar()
{
    m_toolbar = new QToolBar(this);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setMovable(false);

    auto *langLabel = new QLabel(QStringLiteral("Language:"), m_toolbar);
    langLabel->setContentsMargins(4, 0, 4, 0);
    m_toolbar->addWidget(langLabel);

    m_languageEdit = new QLineEdit(m_toolbar);
    m_languageEdit->setPlaceholderText(QStringLiteral("e.g. en_US"));
    m_languageEdit->setMaximumWidth(120);
    m_languageEdit->setToolTip(QStringLiteral("Target locale code (Edit)"));
    connect(m_languageEdit, &QLineEdit::editingFinished,
            this, &LinguistEditor::onLanguageEdited);
    m_toolbar->addWidget(m_languageEdit);

    m_toolbar->addSeparator();

    m_searchEdit = new QLineEdit(m_toolbar);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search source / translation / context..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setToolTip(QStringLiteral("Filter rows (Ctrl+F)"));
    m_searchEdit->setMinimumWidth(280);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &LinguistEditor::onSearchTextChanged);
    m_toolbar->addWidget(m_searchEdit);

    m_toolbar->addSeparator();

    m_finishedButton = new QPushButton(QStringLiteral("Mark Finished"), m_toolbar);
    m_finishedButton->setToolTip(QStringLiteral("Mark current translation as finished (Ctrl+Enter)"));
    connect(m_finishedButton, &QPushButton::clicked,
            this, &LinguistEditor::markCurrentFinished);
    m_toolbar->addWidget(m_finishedButton);

    m_unfinishedButton = new QPushButton(QStringLiteral("Mark Unfinished"), m_toolbar);
    m_unfinishedButton->setToolTip(QStringLiteral("Mark current translation as unfinished"));
    connect(m_unfinishedButton, &QPushButton::clicked,
            this, &LinguistEditor::markCurrentUnfinished);
    m_toolbar->addWidget(m_unfinishedButton);

    m_toolbar->addSeparator();

    m_lupdateButton = new QPushButton(QStringLiteral("Run lupdate"), m_toolbar);
    m_lupdateButton->setToolTip(QStringLiteral("Extract translatable strings from source (lupdate)"));
    connect(m_lupdateButton, &QPushButton::clicked,
            this, &LinguistEditor::runLupdate);
    m_toolbar->addWidget(m_lupdateButton);

    m_lreleaseButton = new QPushButton(QStringLiteral("Run lrelease"), m_toolbar);
    m_lreleaseButton->setToolTip(QStringLiteral("Compile to .qm (lrelease)"));
    connect(m_lreleaseButton, &QPushButton::clicked,
            this, &LinguistEditor::runLrelease);
    m_toolbar->addWidget(m_lreleaseButton);
}

// ── Persistence ─────────────────────────────────────────────────────────────
bool LinguistEditor::loadFromFile(const QString &path)
{
    m_loading = true;
    QString err;
    TsFile parsed;
    const bool ok = TsXmlIO::load(path, parsed, &err);
    if (!ok) {
        m_status->setText(tr("Failed to load: %1").arg(err));
        m_loading = false;
        return false;
    }

    m_filePath = path;
    m_file = std::move(parsed);
    m_model->setFile(&m_file);

    if (m_languageEdit) m_languageEdit->setText(m_file.language);

    rebuildContextsList();
    updateProgressBar();
    m_status->setText(tr("Loaded %1 contexts, %2 messages")
                          .arg(m_file.contexts.size())
                          .arg(m_file.totalCount() + m_file.obsoleteCount()));
    setModified(false);
    m_loading = false;

    if (m_table->model()->rowCount() > 0) {
        const QModelIndex first = m_proxy->index(0, TranslationModel::ColTranslation);
        m_table->setCurrentIndex(first);
        m_table->scrollTo(first);
    }
    return true;
}

bool LinguistEditor::saveToFile(const QString &path)
{
    const QString target = path.isEmpty() ? m_filePath : path;
    if (target.isEmpty()) {
        m_status->setText(tr("No file path — cannot save"));
        return false;
    }
    QString err;
    if (!TsXmlIO::save(target, m_file, &err)) {
        m_status->setText(tr("Save failed: %1").arg(err));
        return false;
    }
    m_filePath = target;
    setModified(false);
    m_status->setText(tr("Saved %1").arg(QDir::toNativeSeparators(target)));
    return true;
}

void LinguistEditor::save()
{
    saveToFile(m_filePath);
}

void LinguistEditor::reload()
{
    if (m_filePath.isEmpty()) return;
    loadFromFile(m_filePath);
}

// ── Slots ───────────────────────────────────────────────────────────────────
void LinguistEditor::focusSearch()
{
    if (m_searchEdit) {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    }
}

void LinguistEditor::focusContextList()
{
    if (m_contextsList) m_contextsList->setFocus();
}

void LinguistEditor::onContextSelectionChanged()
{
    applyContextFilter();
}

void LinguistEditor::onSearchTextChanged(const QString &text)
{
    if (auto *p = static_cast<TranslationProxy*>(m_proxy))
        p->setSearchText(text);
}

void LinguistEditor::applyContextFilter()
{
    QString name;
    if (m_contextsList) {
        auto *cur = m_contextsList->currentItem();
        if (cur) name = cur->data(Qt::UserRole).toString();
    }
    if (auto *p = static_cast<TranslationProxy*>(m_proxy))
        p->setContextFilter(name);
}

void LinguistEditor::onLanguageEdited()
{
    if (m_loading) return;
    if (!m_languageEdit) return;
    const QString v = m_languageEdit->text().trimmed();
    if (v == m_file.language) return;
    m_file.language = v;
    setModified(true);
}

void LinguistEditor::onModelDataChanged()
{
    if (m_loading) return;
    setModified(true);
    rebuildContextsList();
    updateProgressBar();
}

void LinguistEditor::onTableSelectionChanged()
{
    const QModelIndex cur = m_table->currentIndex();
    if (!cur.isValid() || !m_sourcePreview) {
        if (m_sourcePreview) m_sourcePreview->clear();
        return;
    }
    const QModelIndex src = m_proxy->mapToSource(cur);
    const TsMessage *m = m_model->messageAt(src.row());
    if (!m) { m_sourcePreview->clear(); return; }

    QString text;
    text += QStringLiteral("Source:\n%1\n\n").arg(m->source);
    if (!m->comment.isEmpty())
        text += QStringLiteral("Comment:\n%1\n\n").arg(m->comment);

    if (!m->locations.isEmpty()) {
        text += QStringLiteral("Locations:\n");
        for (const auto &loc : m->locations)
            text += QStringLiteral("  %1:%2\n").arg(loc.first).arg(loc.second);

        // Best-effort: open the first existing location and show ±5 lines.
        for (const auto &loc : m->locations) {
            const QFileInfo tsFi(m_filePath);
            QString abs = QDir(tsFi.absolutePath()).absoluteFilePath(loc.first);
            QFile f(abs);
            if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QTextStream in(&f);
            QStringList lines;
            while (!in.atEnd()) lines << in.readLine();
            const int target = qBound(0, loc.second - 1, lines.size() - 1);
            const int from = qMax(0, target - 5);
            const int to   = qMin(lines.size() - 1, target + 5);
            text += QStringLiteral("\n— %1 —\n").arg(QFileInfo(abs).fileName());
            for (int i = from; i <= to; ++i) {
                const QString marker = (i == target) ? QStringLiteral("> ") : QStringLiteral("  ");
                text += QStringLiteral("%1%2: %3\n").arg(marker).arg(i + 1).arg(lines[i]);
            }
            break;
        }
    }
    m_sourcePreview->setPlainText(text);
}

// ── Status mutations ────────────────────────────────────────────────────────
void LinguistEditor::markCurrentFinished()
{
    const QModelIndex cur = m_table->currentIndex();
    if (!cur.isValid()) return;
    const QModelIndex src = m_proxy->mapToSource(cur);
    m_model->setStatusForRow(src.row(), QString());
    setModified(true);
    rebuildContextsList();
    updateProgressBar();

    // Move to next unfinished row.
    const int rows = m_proxy->rowCount();
    for (int delta = 1; delta <= rows; ++delta) {
        const int r = (cur.row() + delta) % rows;
        const QModelIndex p = m_proxy->index(r, TranslationModel::ColTranslation);
        const QString status = m_proxy->data(m_proxy->index(r, 0), TranslationModel::StatusRole).toString();
        if (status == QStringLiteral("unfinished")) {
            m_table->setCurrentIndex(p);
            m_table->scrollTo(p);
            return;
        }
    }
}

void LinguistEditor::markCurrentUnfinished()
{
    const QModelIndex cur = m_table->currentIndex();
    if (!cur.isValid()) return;
    const QModelIndex src = m_proxy->mapToSource(cur);
    m_model->setStatusForRow(src.row(), QStringLiteral("unfinished"));
    setModified(true);
    rebuildContextsList();
    updateProgressBar();
}

// ── External tools ──────────────────────────────────────────────────────────
QString LinguistEditor::currentSourceFile() const
{
    return m_filePath.isEmpty() ? QDir::currentPath() : QFileInfo(m_filePath).absolutePath();
}

void LinguistEditor::appendOutputLine(const QString &line)
{
    if (m_status) m_status->setText(line);
}

void LinguistEditor::runLupdate()
{
    if (m_filePath.isEmpty()) {
        appendOutputLine(tr("Save the .ts file first before running lupdate."));
        return;
    }
    appendOutputLine(tr("Running lupdate..."));

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(currentSourceFile());
    proc->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= 0x08000000;  // CREATE_NO_WINDOW — suppress console popup
    });
#endif

    // Heuristic: pass the .ts file directly.  If the user has a .pro/.cmake
    // with TRANSLATIONS = ... they can run lupdate from the terminal — this
    // is the keyboard-first inline path for the common case.
    const QStringList args{QStringLiteral("-locations"), QStringLiteral("absolute"),
                           m_filePath};

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString chunk = QString::fromLocal8Bit(proc->readAll());
        for (const QString &line : chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            appendOutputLine(QStringLiteral("[lupdate] ") + line);
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int code, QProcess::ExitStatus) {
                appendOutputLine(QStringLiteral("[lupdate] exit %1").arg(code));
                proc->deleteLater();
                if (code == 0 && !m_filePath.isEmpty()) reload();
            });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
        appendOutputLine(QStringLiteral("[lupdate] launch failed: %1").arg(proc->errorString()));
        proc->deleteLater();
    });

    proc->start(QStringLiteral("lupdate"), args);
}

void LinguistEditor::runLrelease()
{
    if (m_filePath.isEmpty()) {
        appendOutputLine(tr("Save the .ts file first before running lrelease."));
        return;
    }
    appendOutputLine(tr("Running lrelease..."));

    auto *proc = new QProcess(this);
    proc->setWorkingDirectory(currentSourceFile());
    proc->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= 0x08000000;  // CREATE_NO_WINDOW — suppress console popup
    });
#endif

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString chunk = QString::fromLocal8Bit(proc->readAll());
        for (const QString &line : chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            appendOutputLine(QStringLiteral("[lrelease] ") + line);
    });
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, proc](int code, QProcess::ExitStatus) {
                appendOutputLine(QStringLiteral("[lrelease] exit %1").arg(code));
                proc->deleteLater();
            });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError) {
        appendOutputLine(QStringLiteral("[lrelease] launch failed: %1").arg(proc->errorString()));
        proc->deleteLater();
    });

    proc->start(QStringLiteral("lrelease"), {m_filePath});
}

// ── Internal ────────────────────────────────────────────────────────────────
void LinguistEditor::rebuildContextsList()
{
    if (!m_contextsList) return;

    // Preserve current selection (by name) across rebuilds.
    QString prev;
    if (auto *cur = m_contextsList->currentItem()) prev = cur->data(Qt::UserRole).toString();

    m_contextsList->blockSignals(true);
    m_contextsList->clear();

    // "All contexts" pseudo-entry at the top.
    {
        const int total = m_file.totalCount();
        const int fin   = m_file.finishedCount();
        auto *all = new QListWidgetItem(
            QStringLiteral("All contexts (%1/%2)").arg(fin).arg(total));
        all->setData(Qt::UserRole, QString());
        m_contextsList->addItem(all);
    }

    m_ctxStats.clear();
    for (const auto &ctx : m_file.contexts) {
        int total = 0, fin = 0;
        for (const auto &m : ctx.messages) {
            if (m.status == QStringLiteral("obsolete")
                || m.status == QStringLiteral("vanished"))
                continue;
            ++total;
            if (m.status.isEmpty()) ++fin;
        }
        m_ctxStats.insert(ctx.name, qMakePair(fin, total));
        const QString label = QStringLiteral("%1 (%2/%3)").arg(ctx.name).arg(fin).arg(total);
        auto *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, ctx.name);
        m_contextsList->addItem(item);
    }

    // Restore selection if possible, otherwise default to "All".
    int restoreRow = 0;
    for (int i = 0; i < m_contextsList->count(); ++i) {
        if (m_contextsList->item(i)->data(Qt::UserRole).toString() == prev) {
            restoreRow = i; break;
        }
    }
    m_contextsList->setCurrentRow(restoreRow);
    m_contextsList->blockSignals(false);

    applyContextFilter();
}

void LinguistEditor::updateProgressBar()
{
    if (!m_progress) return;
    const int total = m_file.totalCount();
    const int fin   = m_file.finishedCount();
    const int unf   = m_file.unfinishedCount();
    const int obs   = m_file.obsoleteCount();
    const int pct   = total > 0 ? (fin * 100) / total : 0;
    m_progress->setValue(pct);
    m_progress->setFormat(QStringLiteral("%1% — %2/%3 finished, %4 unfinished, %5 obsolete")
                              .arg(pct).arg(fin).arg(total).arg(unf).arg(obs));
}

void LinguistEditor::setModified(bool modified)
{
    if (m_modified == modified) return;
    m_modified = modified;
    emit modificationChanged(modified);
}

} // namespace exo::forms::linguist
