#include "searchpanel.h"

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStringList>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "searchmatch.h"
#include "searchquery.h"
#include "searchservice.h"

namespace {

// QSettings keys
constexpr auto kSettingsGroup    = "SearchPanel";
constexpr auto kKeyCase          = "caseSensitive";
constexpr auto kKeyWord          = "wholeWord";
constexpr auto kKeyRegex         = "regex";
constexpr auto kKeyReplaceMode   = "replaceMode";
constexpr auto kKeyInclude       = "includePatterns";
constexpr auto kKeyExclude       = "excludePatterns";

// Split a comma- or semicolon-separated glob string into trimmed entries.
QStringList splitGlobs(const QString &raw)
{
    QStringList out;
    const QStringList parts = raw.split(QRegularExpression(QStringLiteral("[,;]")),
                                        Qt::SkipEmptyParts);
    out.reserve(parts.size());
    for (const QString &p : parts) {
        const QString t = p.trimmed();
        if (!t.isEmpty())
            out.append(t);
    }
    return out;
}

// HTML-escape preview text.
QString htmlEscape(const QString &s)
{
    QString out = s;
    out.replace('&',  QStringLiteral("&amp;"));
    out.replace('<',  QStringLiteral("&lt;"));
    out.replace('>',  QStringLiteral("&gt;"));
    out.replace('"',  QStringLiteral("&quot;"));
    return out;
}

// Renders match column (column 2) as rich-text so the matched substring
// can be highlighted. Required because QTreeWidget treats item text as
// plain text by default.
class RichTextDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);

        const QString html = index.data(Qt::DisplayRole).toString();
        if (!html.contains('<')) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        opt.text.clear();
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        QTextDocument doc;
        doc.setDefaultFont(opt.font);
        doc.setDocumentMargin(0);
        doc.setHtml(html);

        QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText,
                                               &opt, opt.widget);
        painter->save();
        painter->translate(textRect.topLeft());
        painter->setClipRect(textRect.translated(-textRect.topLeft()));

        QAbstractTextDocumentLayout::PaintContext ctx;
        ctx.palette = opt.palette;
        if (opt.state & QStyle::State_Selected) {
            ctx.palette.setColor(QPalette::Text,
                                 opt.palette.color(QPalette::HighlightedText));
        }
        doc.documentLayout()->draw(painter, ctx);
        painter->restore();
    }
};

} // namespace

// ─────────────────────────────────────────────────────────────────────────
//  SearchPanel
// ─────────────────────────────────────────────────────────────────────────

SearchPanel::SearchPanel(SearchService *service, QWidget *parent)
    : QWidget(parent),
      m_rootPath(QString{}),
      m_service(service),
      m_input(new QLineEdit(this)),
      m_replaceInput(new QLineEdit(this)),
      m_includeFilter(new QLineEdit(this)),
      m_excludeFilter(new QLineEdit(this)),
      m_caseBtn(makeToggleButton(QStringLiteral("Aa"), tr("Match case"))),
      m_wordBtn(makeToggleButton(QStringLiteral("Ww"), tr("Match whole word"))),
      m_regexBtn(makeToggleButton(QStringLiteral(".*"), tr("Use regular expression"))),
      m_replaceModeBtn(makeToggleButton(QStringLiteral("↪"),
                                        tr("Toggle Replace mode"))),
      m_searchButton(new QPushButton(tr("Search"), this)),
      m_replaceAllButton(new QPushButton(tr("Replace All"), this)),
      m_results(new QTreeWidget(this)),
      m_statusLabel(new QLabel(this))
{
    buildUi();
    applyTheme();
    loadSettings();

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_searchButton, &QPushButton::clicked, this, &SearchPanel::startSearch);
    connect(m_input, &QLineEdit::returnPressed, this, &SearchPanel::startSearch);
    connect(m_replaceModeBtn, &QToolButton::toggled,
            this, &SearchPanel::toggleReplaceMode);
    connect(m_replaceAllButton, &QPushButton::clicked,
            this, &SearchPanel::runReplaceAll);

    // Persist toggles whenever they change.
    auto persist = [this]() { saveSettings(); };
    connect(m_caseBtn,  &QToolButton::toggled, this, persist);
    connect(m_wordBtn,  &QToolButton::toggled, this, persist);
    connect(m_regexBtn, &QToolButton::toggled, this, persist);
    connect(m_replaceModeBtn, &QToolButton::toggled, this, persist);
    connect(m_includeFilter, &QLineEdit::editingFinished, this, persist);
    connect(m_excludeFilter, &QLineEdit::editingFinished, this, persist);

    connect(m_service, &SearchService::matchFound, this,
            [this](const SearchMatch &match) {
                addMatch(match.filePath, match.line, match.column, match.preview);
            });
    connect(m_service, &SearchService::searchFinished, this, [this]() {
        m_searchButton->setEnabled(true);
        m_replaceAllButton->setEnabled(m_replaceModeBtn->isChecked()
                                       && m_matchCount > 0);
        QString status = tr("Matching lines: %1  Matching files: %2")
                             .arg(m_matchCount).arg(m_fileCount);
        if (m_matchCount >= 5000)
            status += tr("  (results capped at 5000)");
        m_statusLabel->setText(status);
    });

    connect(m_results, &QTreeWidget::itemActivated,
            this, &SearchPanel::onItemActivated);
    connect(m_results, &QTreeWidget::itemClicked,
            this, &SearchPanel::onItemClicked);
}

SearchPanel::~SearchPanel()
{
    saveSettings();
}

// ─────────────────────────────────────────────────────────────────────────
//  UI construction
// ─────────────────────────────────────────────────────────────────────────

QToolButton *SearchPanel::makeToggleButton(const QString &label,
                                           const QString &tooltip)
{
    auto *btn = new QToolButton(this);
    btn->setText(label);
    btn->setToolTip(tooltip);
    btn->setCheckable(true);
    btn->setAutoRaise(false);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setFixedSize(22, 22);
    return btn;
}

void SearchPanel::buildUi()
{
    // ── Toolbar above query input: toggle buttons ─────────────────────────
    auto *toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(2);
    toolbar->addWidget(m_replaceModeBtn);
    auto *toolbarSep = new QLabel(QStringLiteral("│"), this);
    toolbarSep->setStyleSheet(QStringLiteral("color:#3e3e42;"));
    toolbar->addWidget(toolbarSep);
    toolbar->addWidget(m_caseBtn);
    toolbar->addWidget(m_wordBtn);
    toolbar->addWidget(m_regexBtn);
    toolbar->addStretch();

    // ── Query input ───────────────────────────────────────────────────────
    m_input->setPlaceholderText(tr("Search pattern..."));
    m_input->setClearButtonEnabled(true);

    // ── Replace input (hidden until Replace mode toggled) ─────────────────
    m_replaceInput->setPlaceholderText(tr("Replace with..."));
    m_replaceInput->setClearButtonEnabled(true);
    m_replaceInput->setVisible(false);
    m_replaceAllButton->setVisible(false);
    m_replaceAllButton->setEnabled(false);

    auto *replaceRow = new QHBoxLayout();
    replaceRow->setContentsMargins(0, 0, 0, 0);
    replaceRow->setSpacing(4);
    replaceRow->addWidget(m_replaceInput, 1);
    replaceRow->addWidget(m_replaceAllButton);

    // Wrap replaceRow in a QWidget so we can show/hide it as a unit.
    auto *replaceWrap = new QWidget(this);
    replaceWrap->setLayout(replaceRow);
    replaceWrap->setVisible(false);
    replaceWrap->setObjectName(QStringLiteral("replaceWrap"));

    // ── File filters ──────────────────────────────────────────────────────
    m_includeFilter->setPlaceholderText(tr("files to include (e.g. *.cpp, *.h)"));
    m_includeFilter->setClearButtonEnabled(true);
    m_includeFilter->setToolTip(tr("Comma-separated globs of files to search "
                                   "(e.g. *.cpp, *.h)"));

    m_excludeFilter->setPlaceholderText(tr("files to exclude (e.g. build/, *.o)"));
    m_excludeFilter->setClearButtonEnabled(true);
    m_excludeFilter->setToolTip(tr("Comma-separated globs of files / folders "
                                   "to skip (e.g. build/, *.o)"));

    // Action row (Search button)
    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->addStretch();
    actionRow->addWidget(m_searchButton);

    // ── Results tree ──────────────────────────────────────────────────────
    m_results->setHeaderLabels({tr("File"), tr("Line"), tr("Text")});
    m_results->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_results->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_results->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_results->setRootIsDecorated(true);
    m_results->setUniformRowHeights(false);   // allow rich-text height
    m_results->setAlternatingRowColors(true);
    m_results->setItemDelegateForColumn(2, new RichTextDelegate(m_results));

    // ── Status bar ────────────────────────────────────────────────────────
    m_statusLabel->setText(tr("Ready"));

    // ── Compose layout ────────────────────────────────────────────────────
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addLayout(toolbar);
    layout->addWidget(m_input);
    layout->addWidget(replaceWrap);   // shown when Replace mode is active
    layout->addWidget(m_includeFilter);
    layout->addWidget(m_excludeFilter);
    layout->addLayout(actionRow);
    layout->addWidget(m_results, 1);
    layout->addWidget(m_statusLabel);

    // Stash the wrapper so toggleReplaceMode() can show/hide it.
    m_replaceInput->setProperty("_wrap",
                                QVariant::fromValue<QWidget *>(replaceWrap));
}

void SearchPanel::applyTheme()
{
    setStyleSheet(QStringLiteral(
        "SearchPanel { background: #1e1e1e; }"
        "QLineEdit { background: #3c3c3c; color: #d4d4d4; border: 1px solid #555558;"
        "  padding: 3px 6px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #007acc; }"
        "QToolButton { color: #d4d4d4; background: #2d2d30;"
        "  border: 1px solid #3e3e42; border-radius: 3px;"
        "  font-size: 11px; font-weight: bold; padding: 0; }"
        "QToolButton:hover { background: #3e3e42; border-color: #007acc; }"
        "QToolButton:checked { background: #094771; border-color: #007acc;"
        "  color: #ffffff; }"
        "QToolButton:checked:hover { background: #0a5ea0; }"
        "QPushButton { color: #d4d4d4; background: #3c3c3c; border: 1px solid #555558;"
        "  padding: 3px 12px; font-size: 12px; }"
        "QPushButton:hover { background: #3e3e42; border-color: #007acc; }"
        "QPushButton:pressed { background: #094771; }"
        "QPushButton:disabled { color: #6a6a6a; background: #2d2d30;"
        "  border-color: #3e3e42; }"
        "QLabel { color: #858585; font-size: 11px; background: transparent; }"
    ));

    m_results->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: #1e1e1e; color: #d4d4d4; border: none;"
        "  font-size: 12px; outline: 0; }"
        "QTreeWidget::item { padding: 2px 0; border: none; }"
        "QTreeWidget::item:alternate { background: #252526; }"
        "QTreeWidget::item:hover { background: #2a2d2e; }"
        "QTreeWidget::item:selected { background: #094771; color: #ffffff; }"
        "QHeaderView::section { background: #252526; color: #858585; border: none;"
        "  border-right: 1px solid #3e3e42; border-bottom: 1px solid #3e3e42;"
        "  padding: 3px 6px; font-size: 11px; }"
        "QScrollBar:vertical { background: #1e1e1e; width: 10px; border: none; }"
        "QScrollBar::handle:vertical { background: #424242; min-height: 20px;"
        "  border-radius: 5px; }"
        "QScrollBar::handle:vertical:hover { background: #686868; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
}

// ─────────────────────────────────────────────────────────────────────────
//  Public slots
// ─────────────────────────────────────────────────────────────────────────

void SearchPanel::activateSearch()
{
    m_input->setFocus();
    m_input->selectAll();
}

void SearchPanel::setRootPath(const QString &path)
{
    m_rootPath = path;
}

// ─────────────────────────────────────────────────────────────────────────
//  Search lifecycle
// ─────────────────────────────────────────────────────────────────────────

SearchQuery SearchPanel::buildQuery() const
{
    SearchQuery q;
    q.pattern = m_input->text().trimmed();
    q.mode = m_regexBtn->isChecked() ? SearchMode::Regex : SearchMode::Literal;
    q.caseSensitive = m_caseBtn->isChecked();
    q.wholeWord = m_wordBtn->isChecked();
    q.includePatterns = splitGlobs(m_includeFilter->text());
    q.excludePatterns = splitGlobs(m_excludeFilter->text());
    return q;
}

void SearchPanel::startSearch()
{
    const QString pattern = m_input->text().trimmed();
    if (pattern.isEmpty())
        return;
    if (m_rootPath.isEmpty()) {
        m_statusLabel->setText(tr("No workspace folder open"));
        return;
    }

    m_searchButton->setEnabled(false);
    m_replaceAllButton->setEnabled(false);
    clearResults();

    m_statusLabel->setText(tr("Searching..."));

    m_service->cancel();
    m_service->startSearch(m_rootPath, buildQuery());
}

void SearchPanel::clearResults()
{
    m_results->clear();
    m_fileItems.clear();
    m_matchCount = 0;
    m_fileCount  = 0;
}

void SearchPanel::addMatch(const QString &filePath, int line, int column,
                           const QString &preview)
{
    ++m_matchCount;

    const QDir root(m_rootPath);
    const QString relPath = root.relativeFilePath(filePath);

    // O(1) lookup of the file group.
    QTreeWidgetItem *parentItem = m_fileItems.value(filePath, nullptr);
    if (!parentItem) {
        ++m_fileCount;
        parentItem = new QTreeWidgetItem(m_results);
        parentItem->setData(0, Qt::UserRole, filePath);
        QFont bold = parentItem->font(0);
        bold.setBold(true);
        parentItem->setFont(0, bold);
        parentItem->setFirstColumnSpanned(true);
        parentItem->setExpanded(true);
        m_fileItems.insert(filePath, parentItem);
    }

    const int childCount = parentItem->childCount() + 1;
    parentItem->setText(0, tr("%1  (%2 %3)")
                                .arg(relPath)
                                .arg(childCount)
                                .arg(childCount == 1 ? tr("match") : tr("matches")));

    auto *child = new QTreeWidgetItem(parentItem);
    child->setText(1, QString::number(line));
    child->setText(2, renderPreview(preview, column));
    child->setData(0, Qt::UserRole, filePath);
    child->setData(0, Qt::UserRole + 1, line);
    child->setData(0, Qt::UserRole + 2, column);

    if (m_matchCount % 50 == 0) {
        m_statusLabel->setText(tr("Searching... %1 matches in %2 files")
                                   .arg(m_matchCount).arg(m_fileCount));
    }
}

void SearchPanel::onItemActivated(QTreeWidgetItem *item, int /*column*/)
{
    if (!item)
        return;
    const QString path = item->data(0, Qt::UserRole).toString();
    if (path.isEmpty())
        return;
    const int line = item->data(0, Qt::UserRole + 1).toInt();
    const int col  = item->data(0, Qt::UserRole + 2).toInt();
    emit resultActivated(path, qMax(line, 1), col);
}

void SearchPanel::onItemClicked(QTreeWidgetItem *item, int column)
{
    // Single-click on a child row jumps to file:line.
    // For top-level (file header) rows we only toggle expansion,
    // which QTreeWidget already handles, so do nothing extra.
    if (!item || item->childCount() > 0)
        return;
    onItemActivated(item, column);
}

void SearchPanel::toggleReplaceMode(bool on)
{
    auto *wrap = m_replaceInput->property("_wrap").value<QWidget *>();
    if (wrap)
        wrap->setVisible(on);
    m_replaceInput->setVisible(on);
    m_replaceAllButton->setVisible(on);
    m_replaceAllButton->setEnabled(on && m_matchCount > 0);
    if (on)
        m_replaceInput->setFocus();
}

void SearchPanel::runReplaceAll()
{
    if (!m_replaceModeBtn->isChecked())
        return;
    if (m_matchCount == 0) {
        m_statusLabel->setText(tr("Run a search before replacing"));
        return;
    }

    const QString pattern = m_input->text();
    const QString replacement = m_replaceInput->text();
    if (pattern.isEmpty())
        return;

    const auto button = QMessageBox::warning(
        this, tr("Replace All"),
        tr("Replace %1 match(es) in %2 file(s) with \"%3\"?\n\n"
           "This rewrites files on disk and cannot be undone in-IDE.")
            .arg(m_matchCount).arg(m_fileCount).arg(replacement),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (button != QMessageBox::Ok)
        return;

    const SearchQuery q = buildQuery();
    QRegularExpression rx;
    if (q.mode == SearchMode::Regex) {
        QRegularExpression::PatternOptions opts;
        if (!q.caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        rx = QRegularExpression(q.pattern, opts);
        if (!rx.isValid()) {
            m_statusLabel->setText(tr("Invalid regex — replace aborted"));
            return;
        }
    } else if (q.wholeWord) {
        QRegularExpression::PatternOptions opts;
        if (!q.caseSensitive)
            opts |= QRegularExpression::CaseInsensitiveOption;
        rx = QRegularExpression(
            QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(q.pattern)),
            opts);
    }
    const Qt::CaseSensitivity cs = q.caseSensitive
                                       ? Qt::CaseSensitive
                                       : Qt::CaseInsensitive;

    int filesChanged = 0;
    int totalReplacements = 0;

    const QStringList files = m_fileItems.keys();
    for (const QString &path : files) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        const QByteArray raw = f.readAll();
        f.close();

        QString content = QString::fromUtf8(raw);
        QString updated = content;
        int countHere = 0;

        if (q.mode == SearchMode::Literal && !q.wholeWord) {
            int idx = 0;
            QString out;
            out.reserve(content.size());
            while (idx < content.size()) {
                int hit = content.indexOf(q.pattern, idx, cs);
                if (hit < 0) {
                    out.append(content.mid(idx));
                    break;
                }
                out.append(content.mid(idx, hit - idx));
                out.append(replacement);
                idx = hit + q.pattern.size();
                ++countHere;
            }
            updated = out;
        } else if (rx.isValid()) {
            // Count manually for accurate stats.
            auto it = rx.globalMatch(content);
            while (it.hasNext()) { it.next(); ++countHere; }
            updated = content;
            updated.replace(rx, replacement);
        }

        if (countHere > 0 && updated != content) {
            QFile out(path);
            if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                out.write(updated.toUtf8());
                out.close();
                ++filesChanged;
                totalReplacements += countHere;
            }
        }
    }

    m_statusLabel->setText(tr("Replaced %1 occurrence(s) in %2 file(s)")
                               .arg(totalReplacements).arg(filesChanged));

    // Re-run the search so results reflect the new file state.
    if (totalReplacements > 0)
        startSearch();
}

// ─────────────────────────────────────────────────────────────────────────
//  Match-preview rendering
// ─────────────────────────────────────────────────────────────────────────

QString SearchPanel::renderPreview(const QString &previewIn, int matchColumn) const
{
    // Trim leading whitespace but remember the offset so the column lines up.
    int leading = 0;
    while (leading < previewIn.size() && previewIn.at(leading).isSpace())
        ++leading;
    const QString preview = previewIn.mid(leading);

    // SearchWorker reports column with 1-based offset relative to the
    // *original* line, then trims the line for the preview. Adjust for that.
    int idx = matchColumn - 1 - leading;
    if (idx < 0) idx = 0;

    // Recover the matched substring length by re-running the active query
    // against the preview text — preview is already trimmed by the worker.
    const QString pattern = m_input->text();
    int matchLen = 0;
    if (!pattern.isEmpty() && idx < preview.size()) {
        if (m_regexBtn->isChecked()) {
            QRegularExpression::PatternOptions opts;
            if (!m_caseBtn->isChecked())
                opts |= QRegularExpression::CaseInsensitiveOption;
            QRegularExpression rx(pattern, opts);
            if (rx.isValid()) {
                const auto m = rx.match(preview, idx);
                if (m.hasMatch() && m.capturedStart() == idx)
                    matchLen = m.capturedLength();
            }
        } else {
            matchLen = pattern.length();
            if (idx + matchLen > preview.size())
                matchLen = preview.size() - idx;
        }
    }

    if (matchLen <= 0)
        return htmlEscape(preview);

    const QString before = htmlEscape(preview.left(idx));
    const QString hit    = htmlEscape(preview.mid(idx, matchLen));
    const QString after  = htmlEscape(preview.mid(idx + matchLen));
    return QStringLiteral(
               "%1<span style=\"background:#613a00;color:#ffd479;\">%2</span>%3")
        .arg(before, hit, after);
}

// ─────────────────────────────────────────────────────────────────────────
//  Persistence
// ─────────────────────────────────────────────────────────────────────────

void SearchPanel::loadSettings()
{
    QSettings s;
    s.beginGroup(kSettingsGroup);
    m_caseBtn->setChecked(s.value(kKeyCase, false).toBool());
    m_wordBtn->setChecked(s.value(kKeyWord, false).toBool());
    m_regexBtn->setChecked(s.value(kKeyRegex, false).toBool());
    const bool replaceOn = s.value(kKeyReplaceMode, false).toBool();
    m_replaceModeBtn->setChecked(replaceOn);
    m_includeFilter->setText(s.value(kKeyInclude).toString());
    m_excludeFilter->setText(s.value(kKeyExclude).toString());
    s.endGroup();

    // Apply replace-mode visibility from persisted state.
    toggleReplaceMode(replaceOn);
}

void SearchPanel::saveSettings()
{
    QSettings s;
    s.beginGroup(kSettingsGroup);
    s.setValue(kKeyCase,        m_caseBtn->isChecked());
    s.setValue(kKeyWord,        m_wordBtn->isChecked());
    s.setValue(kKeyRegex,       m_regexBtn->isChecked());
    s.setValue(kKeyReplaceMode, m_replaceModeBtn->isChecked());
    s.setValue(kKeyInclude,     m_includeFilter->text());
    s.setValue(kKeyExclude,     m_excludeFilter->text());
    s.endGroup();
}
