#include "qrceditorwidget.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QSet>
#include <QSplitter>
#include <QStyle>
#include <QSvgRenderer>
#include <QPainter>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

namespace {

// VS dark-modern palette — kept local to avoid coupling with chatthemetokens.
constexpr const char *kBackground    = "#1e1e1e";
constexpr const char *kStatusBg      = "#252526";
constexpr const char *kBorder        = "#3c3c3c";
constexpr const char *kForeground    = "#cccccc";
constexpr const char *kStatusFg      = "#9d9d9d";
constexpr const char *kToolbarBg     = "#2d2d30";
constexpr const char *kToolbarHover  = "#3e3e42";
constexpr const char *kTreeBg        = "#1e1e1e";
constexpr const char *kTreeAltBg     = "#252526";
constexpr const char *kTreeSelBg     = "#094771";
constexpr const char *kTreeSelFg     = "#ffffff";
constexpr const char *kTreeHeaderBg  = "#2d2d30";

// Item-data slots stored on QTreeWidgetItem.
constexpr int kRolePrefix      = Qt::UserRole + 1;   // QString — prefix value (prefix items)
constexpr int kRoleAlias       = Qt::UserRole + 2;   // QString — alias attr (file items)
constexpr int kRoleFilePath    = Qt::UserRole + 3;   // QString — file path  (file items)
constexpr int kRoleIsPrefix    = Qt::UserRole + 4;   // bool

QPixmap renderSvgPreview(const QString &filePath, const QSize &maxSize)
{
    QSvgRenderer renderer(filePath);
    if (!renderer.isValid())
        return {};
    QSize size = renderer.defaultSize();
    if (!size.isValid() || size.isEmpty())
        size = QSize(256, 256);
    size.scale(maxSize, Qt::KeepAspectRatio);
    QPixmap pix(size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    renderer.render(&p);
    return pix;
}

bool isImageExt(const QString &ext)
{
    static const QSet<QString> kImageExts = {
        QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"),
        QStringLiteral("bmp"),  QStringLiteral("gif"),  QStringLiteral("svg"),
        QStringLiteral("webp"), QStringLiteral("ico")
    };
    return kImageExts.contains(ext.toLower());
}

} // namespace

// ── Construction ───────────────────────────────────────────────────────────

QrcEditorWidget::QrcEditorWidget(QWidget *parent)
    : QWidget(parent)
{
    buildUi();
    applyTheme();
    updateStatus();
    updatePreview();
}

QrcEditorWidget::~QrcEditorWidget() = default;

// ── UI construction ────────────────────────────────────────────────────────

void QrcEditorWidget::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Toolbar ────────────────────────────────────────────────────────────
    m_toolbar = new QToolBar(this);
    m_toolbar->setMovable(false);
    m_toolbar->setIconSize(QSize(16, 16));
    m_toolbar->setObjectName(QStringLiteral("qrcEditorToolbar"));

    m_addPrefixAction = m_toolbar->addAction(tr("Add Prefix"));
    m_addPrefixAction->setToolTip(tr("Add a new <qresource> prefix entry"));
    connect(m_addPrefixAction, &QAction::triggered, this, &QrcEditorWidget::addPrefix);

    m_addFileAction = m_toolbar->addAction(tr("Add File"));
    m_addFileAction->setToolTip(tr("Add a <file> entry under the selected prefix"));
    connect(m_addFileAction, &QAction::triggered, this, &QrcEditorWidget::addFile);

    m_removeAction = m_toolbar->addAction(tr("Remove"));
    m_removeAction->setShortcut(QKeySequence::Delete);
    m_removeAction->setToolTip(tr("Remove the selected entry (Del)"));
    connect(m_removeAction, &QAction::triggered, this, &QrcEditorWidget::removeSelected);

    m_toolbar->addSeparator();

    m_saveAction = m_toolbar->addAction(tr("Save"));
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setToolTip(tr("Save the .qrc file (Ctrl+S)"));
    connect(m_saveAction, &QAction::triggered, this, &QrcEditorWidget::save);

    root->addWidget(m_toolbar);

    // ── Splitter: tree | preview ───────────────────────────────────────────
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->setHandleWidth(1);
    m_splitter->setChildrenCollapsible(false);

    m_tree = new QTreeWidget(m_splitter);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({ tr("Path / Prefix"), tr("Alias") });
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setEditTriggers(QAbstractItemView::DoubleClicked
                            | QAbstractItemView::SelectedClicked
                            | QAbstractItemView::EditKeyPressed);

    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &QrcEditorWidget::onSelectionChanged);
    connect(m_tree, &QTreeWidget::itemChanged,
            this, &QrcEditorWidget::onItemEdited);

    m_preview = new QLabel(m_splitter);
    m_preview->setAlignment(Qt::AlignCenter);
    m_preview->setMinimumWidth(180);
    m_preview->setWordWrap(true);
    m_preview->setText(tr("Select a file to preview"));

    m_splitter->addWidget(m_tree);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 3);
    m_splitter->setStretchFactor(1, 2);
    m_splitter->setSizes({ 600, 300 });

    root->addWidget(m_splitter, 1);

    // ── Status bar ─────────────────────────────────────────────────────────
    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("qrcEditorStatus"));
    m_status->setContentsMargins(8, 4, 8, 4);
    root->addWidget(m_status);
}

void QrcEditorWidget::applyTheme()
{
    setAutoFillBackground(true);
    setStyleSheet(QString::fromUtf8(
        "QrcEditorWidget { background: %1; }"
        "QToolBar#qrcEditorToolbar { background: %2; border: 0; border-bottom: 1px solid %3; "
        "  spacing: 2px; padding: 4px; }"
        "QToolBar#qrcEditorToolbar QToolButton { color: %4; background: transparent; "
        "  border: 1px solid transparent; padding: 4px 10px; border-radius: 2px; }"
        "QToolBar#qrcEditorToolbar QToolButton:hover { background: %5; border: 1px solid %3; }"
        "QToolBar#qrcEditorToolbar QToolButton:pressed { background: %3; }"
        "QLabel#qrcEditorStatus { background: %6; color: %7; "
        "  border-top: 1px solid %3; }"
        "QTreeWidget { background: %8; color: %4; "
        "  alternate-background-color: %9; border: 0; outline: 0; "
        "  selection-background-color: %10; selection-color: %11; }"
        "QTreeWidget::item { padding: 3px; }"
        "QTreeWidget::item:selected { background: %10; color: %11; }"
        "QHeaderView::section { background: %12; color: %4; padding: 4px 6px; "
        "  border: 0; border-right: 1px solid %3; border-bottom: 1px solid %3; }"
        "QSplitter::handle { background: %3; }"
        "QLabel { color: %4; }"
    ).arg(QString::fromUtf8(kBackground),
          QString::fromUtf8(kToolbarBg),
          QString::fromUtf8(kBorder),
          QString::fromUtf8(kForeground),
          QString::fromUtf8(kToolbarHover),
          QString::fromUtf8(kStatusBg),
          QString::fromUtf8(kStatusFg),
          QString::fromUtf8(kTreeBg),
          QString::fromUtf8(kTreeAltBg),
          QString::fromUtf8(kTreeSelBg))
        .arg(QString::fromUtf8(kTreeSelFg),
             QString::fromUtf8(kTreeHeaderBg)));
}

// ── Modification tracking ──────────────────────────────────────────────────

void QrcEditorWidget::setModified(bool modified)
{
    if (m_loading) return;
    if (m_modified == modified) return;
    m_modified = modified;
    emit modificationChanged(m_modified);
    updateStatus();
}

// ── Status bar ─────────────────────────────────────────────────────────────

QString QrcEditorWidget::formatFileSize(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("—");
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    const double kb = bytes / 1024.0;
    if (kb < 1024.0) return QStringLiteral("%1 KB").arg(kb, 0, 'f', 1);
    const double mb = kb / 1024.0;
    if (mb < 1024.0) return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
    const double gb = mb / 1024.0;
    return QStringLiteral("%1 GB").arg(gb, 0, 'f', 2);
}

void QrcEditorWidget::updateStatus()
{
    int prefixCount = 0;
    int fileCount   = 0;
    if (m_tree) {
        prefixCount = m_tree->topLevelItemCount();
        for (int i = 0; i < prefixCount; ++i)
            fileCount += m_tree->topLevelItem(i)->childCount();
    }

    qint64 fileSize = -1;
    if (!m_filePath.isEmpty()) {
        QFileInfo fi(m_filePath);
        if (fi.exists())
            fileSize = fi.size();
    }

    QString modMark = m_modified ? QStringLiteral("  *  ") : QStringLiteral("    ");
    QString pathPart = m_filePath.isEmpty()
                       ? tr("(unsaved)")
                       : QDir::toNativeSeparators(m_filePath);
    QString sizePart = (fileSize >= 0)
                       ? formatFileSize(fileSize)
                       : QStringLiteral("—");

    m_status->setText(tr("%1%2  •  %3 prefix(es), %4 file(s)  •  %5")
                          .arg(pathPart, modMark)
                          .arg(prefixCount)
                          .arg(fileCount)
                          .arg(sizePart));
}

// ── Preview ────────────────────────────────────────────────────────────────

void QrcEditorWidget::updatePreview()
{
    if (!m_preview) return;

    auto items = m_tree ? m_tree->selectedItems() : QList<QTreeWidgetItem*>();
    if (items.isEmpty()) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("Select a file to preview"));
        return;
    }

    QTreeWidgetItem *item = items.first();
    const bool isPrefix = item->data(0, kRoleIsPrefix).toBool();
    if (isPrefix) {
        const QString prefix = item->data(0, kRolePrefix).toString();
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("Prefix: %1\n\n%2 file(s)")
                               .arg(prefix.isEmpty() ? QStringLiteral("/") : prefix)
                               .arg(item->childCount()));
        return;
    }

    const QString rel = item->data(0, kRoleFilePath).toString();
    QString abs = rel;
    if (!QFileInfo(abs).isAbsolute() && !m_filePath.isEmpty())
        abs = QDir(QFileInfo(m_filePath).absolutePath()).absoluteFilePath(rel);

    QFileInfo fi(abs);
    if (!fi.exists()) {
        m_preview->setPixmap(QPixmap());
        m_preview->setText(tr("File not found:\n%1").arg(rel));
        return;
    }

    const QString ext = fi.suffix().toLower();
    const QSize maxPreview = m_preview->size().isEmpty()
                             ? QSize(240, 240)
                             : (m_preview->size() - QSize(16, 80));

    QPixmap pix;
    if (ext == QStringLiteral("svg")) {
        pix = renderSvgPreview(abs, maxPreview);
    } else if (isImageExt(ext)) {
        QPixmap raw(abs);
        if (!raw.isNull())
            pix = raw.scaled(maxPreview, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    if (!pix.isNull()) {
        m_preview->setPixmap(pix);
        return;
    }

    m_preview->setPixmap(QPixmap());
    m_preview->setText(tr("%1\n%2\n%3")
                           .arg(fi.fileName(),
                                ext.isEmpty() ? tr("(no extension)") : ext.toUpper(),
                                formatFileSize(fi.size())));
}

// ── Tree helpers ───────────────────────────────────────────────────────────

QTreeWidgetItem *QrcEditorWidget::createPrefixItem(const QString &prefix)
{
    auto *item = new QTreeWidgetItem(m_tree);
    item->setText(0, prefix.isEmpty() ? QStringLiteral("/") : prefix);
    item->setData(0, kRoleIsPrefix, true);
    item->setData(0, kRolePrefix, prefix);
    item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    item->setExpanded(true);
    return item;
}

QTreeWidgetItem *QrcEditorWidget::createFileItem(QTreeWidgetItem *prefixItem,
                                                 const QString &filePath,
                                                 const QString &alias)
{
    auto *item = new QTreeWidgetItem(prefixItem);
    item->setText(0, filePath);
    item->setText(1, alias);
    item->setData(0, kRoleIsPrefix, false);
    item->setData(0, kRoleFilePath, filePath);
    item->setData(0, kRoleAlias, alias);
    item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    item->setFlags(item->flags() | Qt::ItemIsEditable);
    return item;
}

QTreeWidgetItem *QrcEditorWidget::prefixItemFor(QTreeWidgetItem *item) const
{
    if (!item) return nullptr;
    if (item->data(0, kRoleIsPrefix).toBool())
        return item;
    return item->parent();
}

// ── Load ───────────────────────────────────────────────────────────────────

bool QrcEditorWidget::loadFromFile(const QString &path)
{
    m_loading = true;
    m_tree->clear();

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_filePath = path;
        m_loading  = false;
        m_modified = false;
        emit modificationChanged(false);
        updateStatus();
        return false;
    }

    QXmlStreamReader xml(&f);
    QTreeWidgetItem *currentPrefix = nullptr;

    while (!xml.atEnd() && !xml.hasError()) {
        const auto tt = xml.readNext();
        if (tt != QXmlStreamReader::StartElement) continue;

        const QString name = xml.name().toString();
        if (name.compare(QStringLiteral("qresource"), Qt::CaseInsensitive) == 0) {
            const QString prefix = xml.attributes().value(QStringLiteral("prefix")).toString();
            currentPrefix = createPrefixItem(prefix.isEmpty() ? QStringLiteral("/") : prefix);
        } else if (name.compare(QStringLiteral("file"), Qt::CaseInsensitive) == 0) {
            const QString alias = xml.attributes().value(QStringLiteral("alias")).toString();
            // readElementText() consumes through the matching EndElement.
            const QString filePath = xml.readElementText().trimmed();
            if (currentPrefix && !filePath.isEmpty())
                createFileItem(currentPrefix, filePath, alias);
        }
    }

    f.close();

    m_filePath = path;
    m_modified = false;
    m_loading  = false;
    emit modificationChanged(false);
    updateStatus();
    updatePreview();
    return !xml.hasError();
}

// ── Save ───────────────────────────────────────────────────────────────────

bool QrcEditorWidget::saveToFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;

    QXmlStreamWriter xml(&f);
    xml.setAutoFormatting(true);
    xml.setAutoFormattingIndent(2);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("RCC"));

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *p = m_tree->topLevelItem(i);
        QString prefix = p->data(0, kRolePrefix).toString();
        if (prefix.isEmpty())
            prefix = p->text(0);
        if (prefix.isEmpty())
            prefix = QStringLiteral("/");

        xml.writeStartElement(QStringLiteral("qresource"));
        xml.writeAttribute(QStringLiteral("prefix"), prefix);

        for (int j = 0; j < p->childCount(); ++j) {
            QTreeWidgetItem *c = p->child(j);
            const QString alias = c->text(1);
            const QString filePath = c->text(0);
            if (filePath.isEmpty()) continue;

            xml.writeStartElement(QStringLiteral("file"));
            if (!alias.isEmpty())
                xml.writeAttribute(QStringLiteral("alias"), alias);
            xml.writeCharacters(filePath);
            xml.writeEndElement(); // file
        }

        xml.writeEndElement(); // qresource
    }

    xml.writeEndElement();   // RCC
    xml.writeEndDocument();

    const bool ok = (f.error() == QFile::NoError);
    f.close();

    if (ok) {
        m_filePath = path;
        m_modified = false;
        emit modificationChanged(false);
        updateStatus();
    }
    return ok;
}

// ── Slots ──────────────────────────────────────────────────────────────────

void QrcEditorWidget::addPrefix()
{
    bool ok = false;
    const QString prefix = QInputDialog::getText(
        this,
        tr("Add Prefix"),
        tr("Prefix (e.g. /, /icons, /qml):"),
        QLineEdit::Normal,
        QStringLiteral("/"),
        &ok);
    if (!ok) return;

    QTreeWidgetItem *item = createPrefixItem(prefix.isEmpty() ? QStringLiteral("/") : prefix);
    m_tree->setCurrentItem(item);
    setModified(true);
    updateStatus();
    updatePreview();
}

void QrcEditorWidget::addFile()
{
    QTreeWidgetItem *sel = m_tree->currentItem();
    QTreeWidgetItem *prefix = prefixItemFor(sel);
    if (!prefix) {
        if (m_tree->topLevelItemCount() > 0) {
            prefix = m_tree->topLevelItem(0);
        } else {
            // Auto-create a default "/" prefix so the user can add files even
            // if the .qrc is empty.
            prefix = createPrefixItem(QStringLiteral("/"));
        }
    }

    const QString baseDir = m_filePath.isEmpty()
                            ? QDir::currentPath()
                            : QFileInfo(m_filePath).absolutePath();

    const QStringList files = QFileDialog::getOpenFileNames(
        this,
        tr("Add File(s) to Resource"),
        baseDir);
    if (files.isEmpty()) return;

    for (const QString &abs : files) {
        QString rel = abs;
        if (!m_filePath.isEmpty()) {
            const QDir base(QFileInfo(m_filePath).absolutePath());
            rel = base.relativeFilePath(abs);
        }
        createFileItem(prefix, rel, QString());
    }
    prefix->setExpanded(true);

    setModified(true);
    updateStatus();
    updatePreview();
}

void QrcEditorWidget::removeSelected()
{
    QList<QTreeWidgetItem *> items = m_tree->selectedItems();
    if (items.isEmpty()) return;

    bool didRemove = false;
    for (QTreeWidgetItem *item : items) {
        if (!item) continue;
        if (item->data(0, kRoleIsPrefix).toBool()) {
            // Top-level prefix — remove from tree.
            const int idx = m_tree->indexOfTopLevelItem(item);
            if (idx >= 0) {
                delete m_tree->takeTopLevelItem(idx);
                didRemove = true;
            }
        } else {
            QTreeWidgetItem *parent = item->parent();
            if (parent) {
                parent->removeChild(item);
                delete item;
                didRemove = true;
            }
        }
    }

    if (didRemove) {
        setModified(true);
        updateStatus();
        updatePreview();
    }
}

void QrcEditorWidget::save()
{
    QString target = m_filePath;
    if (target.isEmpty()) {
        target = QFileDialog::getSaveFileName(
            this,
            tr("Save Qt Resource File"),
            QDir::currentPath(),
            tr("Qt Resource Files (*.qrc);;All Files (*)"));
        if (target.isEmpty()) return;
    }

    if (!saveToFile(target)) {
        QMessageBox::warning(this,
                             tr("Save Failed"),
                             tr("Could not write to %1").arg(QDir::toNativeSeparators(target)));
    }
}

void QrcEditorWidget::onSelectionChanged()
{
    updatePreview();
}

void QrcEditorWidget::onItemEdited(QTreeWidgetItem *item, int column)
{
    if (!item || m_loading) return;

    // Keep the role data in sync with the displayed text so save() round-trips
    // user edits correctly.
    if (item->data(0, kRoleIsPrefix).toBool()) {
        if (column == 0)
            item->setData(0, kRolePrefix, item->text(0));
    } else {
        if (column == 0)
            item->setData(0, kRoleFilePath, item->text(0));
        else if (column == 1)
            item->setData(0, kRoleAlias, item->text(1));
    }

    setModified(true);
    if (column == 0)
        updatePreview();
}
