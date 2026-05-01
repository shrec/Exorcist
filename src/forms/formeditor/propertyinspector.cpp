#include "propertyinspector.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QPushButton>
#include <QRect>
#include <QSize>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QVariant>

namespace exo::forms {

PropertyInspector::PropertyInspector(QWidget *parent) : QWidget(parent) {
    buildUi();
}

void PropertyInspector::buildUi() {
    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(6, 6, 6, 6);
    col->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search properties..."));
    m_search->setClearButtonEnabled(true);
    m_search->setStyleSheet(
        "QLineEdit{background:#252526;color:#d4d4d4;border:1px solid #3c3c3c;"
        "padding:3px 6px;border-radius:2px;}"
        "QLineEdit:focus{border:1px solid #007acc;}");
    col->addWidget(m_search);

    auto *toggleRow = new QHBoxLayout;
    toggleRow->setSpacing(4);
    m_btnAll = new QPushButton(tr("All"), this);
    m_btnMod = new QPushButton(tr("Modified"), this);
    for (auto *b : {m_btnAll, m_btnMod}) {
        b->setCheckable(true);
        b->setStyleSheet(
            "QPushButton{background:#2d2d30;color:#d4d4d4;border:1px solid #3c3c3c;"
            "padding:3px 10px;border-radius:2px;}"
            "QPushButton:checked{background:#094771;border:1px solid #007acc;color:white;}");
    }
    m_btnAll->setChecked(true);
    auto *grp = new QButtonGroup(this);
    grp->setExclusive(true);
    grp->addButton(m_btnAll); grp->addButton(m_btnMod);
    toggleRow->addWidget(m_btnAll);
    toggleRow->addWidget(m_btnMod);
    toggleRow->addStretch(1);
    col->addLayout(toggleRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(2);
    m_table->setHorizontalHeaderLabels({tr("Property"), tr("Value")});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setStyleSheet(
        "QTableWidget{background:#1e1e1e;color:#d4d4d4;border:1px solid #3c3c3c;"
        "alternate-background-color:#252526;gridline-color:#3c3c3c;}"
        "QHeaderView::section{background:#2d2d30;color:#d4d4d4;border:none;"
        "padding:3px 6px;border-bottom:1px solid #3c3c3c;}"
        "QTableWidget::item{padding:2px 4px;}"
        "QTableWidget::item:selected{background:#094771;color:white;}");
    col->addWidget(m_table, 1);

    connect(m_search, &QLineEdit::textChanged, this, [this](){ applyFilter(); });
    connect(m_btnAll, &QPushButton::clicked, this,
            [this](){ m_filter = ShowAll; applyFilter(); });
    connect(m_btnMod, &QPushButton::clicked, this,
            [this](){ m_filter = ShowModified; applyFilter(); });

    setStyleSheet("QWidget{background:#1e1e1e;}");
}

void PropertyInspector::setTarget(QWidget *target) {
    m_target = target;
    rebuild();
}

void PropertyInspector::rebuild() {
    m_table->clearContents();
    m_table->setRowCount(0);
    if (!m_target) return;

    const QMetaObject *mo = m_target->metaObject();
    for (int i = 0; i < mo->propertyCount(); ++i) {
        const QMetaProperty p = mo->property(i);
        if (!p.isReadable() || !p.isWritable()) continue;
        if (p.isConstant()) continue;
        // Skip the noisy/derived ones.  These tend to be huge enums
        // (palette, font subprops) or things that must not be edited
        // textually (children, layout).
        const QString name = QString::fromLatin1(p.name());
        static const QSet<QString> kSkip = {
            "objectName",  // shown as breadcrumb instead
            "windowFlags", "windowFilePath",
            "modal", "windowModality",
            "geometry",    // edited via canvas drag/resize, not text
        };
        if (kSkip.contains(name)) continue;

        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto *labelItem = new QTableWidgetItem(name);
        labelItem->setFlags(Qt::ItemIsEnabled);
        const bool modified = m_target->property(
            (QByteArray("exo_modified_") + p.name()).constData()).toBool();
        if (modified) {
            QFont f = labelItem->font(); f.setBold(true);
            labelItem->setFont(f);
            labelItem->setForeground(QColor("#d4d4d4"));
        } else {
            labelItem->setForeground(QColor("#9da0a4"));
        }
        m_table->setItem(row, 0, labelItem);

        const QVariant val = p.read(m_target);
        QWidget *editor = createEditorForCell(row, p.name(), val);
        if (editor) m_table->setCellWidget(row, 1, editor);
        else {
            auto *vItem = new QTableWidgetItem(val.toString());
            vItem->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(row, 1, vItem);
        }
    }
    m_table->resizeColumnToContents(0);
    applyFilter();
}

void PropertyInspector::applyFilter() {
    if (!m_target) return;
    const QString needle = m_search->text().trimmed().toLower();
    for (int r = 0; r < m_table->rowCount(); ++r) {
        QTableWidgetItem *l = m_table->item(r, 0);
        if (!l) continue;
        const QString name = l->text();
        bool show = needle.isEmpty() || name.toLower().contains(needle);
        if (show && m_filter == ShowModified) {
            const bool mod = m_target->property(
                (QByteArray("exo_modified_") + name.toLatin1()).constData()).toBool();
            show = mod;
        }
        m_table->setRowHidden(r, !show);
    }
}

void PropertyInspector::writeProperty(const char *propName, const QVariant &value) {
    if (!m_target) return;
    m_target->setProperty(propName, value);
    m_target->setProperty((QByteArray("exo_modified_") + propName).constData(), true);
    if (auto *l = m_table->item(0, 0)) Q_UNUSED(l);  // noop; refresh below
    // Bold the label row to reflect the new "modified" state.
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (auto *li = m_table->item(r, 0)) {
            if (li->text() == QString::fromLatin1(propName)) {
                QFont f = li->font(); f.setBold(true);
                li->setFont(f);
                li->setForeground(QColor("#d4d4d4"));
                break;
            }
        }
    }
    emit propertyChanged(m_target, QString::fromLatin1(propName));
}

// ── Per-type editor factory ──────────────────────────────────────────────────
//
// Each branch returns a QWidget whose value-changed signal is connected to
// writeProperty().  The widget is parented to the table cell automatically
// when QTableWidget::setCellWidget() is called by the caller.

QWidget *PropertyInspector::createEditorForCell(int /*row*/,
                                                const char *propName,
                                                const QVariant &value) {
    const QByteArray name(propName);

    auto styleEditor = [](QWidget *w){
        w->setStyleSheet(
            "QWidget{background:#252526;color:#d4d4d4;border:none;}"
            "QLineEdit,QSpinBox,QDoubleSpinBox,QComboBox{"
            " background:#252526;color:#d4d4d4;border:1px solid #3c3c3c;"
            " padding:1px 4px;border-radius:2px;}"
            "QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus,QComboBox:focus{"
            " border:1px solid #007acc;}");
    };

    switch (value.metaType().id()) {
    case QMetaType::Bool: {
        auto *cb = new QCheckBox;
        cb->setChecked(value.toBool());
        styleEditor(cb);
        connect(cb, &QCheckBox::toggled, this, [this, name](bool v){
            writeProperty(name.constData(), v);
        });
        return cb;
    }
    case QMetaType::Int:
    case QMetaType::Long: {
        auto *sb = new QSpinBox;
        sb->setRange(-1000000, 1000000);
        sb->setValue(value.toInt());
        styleEditor(sb);
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, name](int v){ writeProperty(name.constData(), v); });
        return sb;
    }
    case QMetaType::Double:
    case QMetaType::Float: {
        auto *sb = new QDoubleSpinBox;
        sb->setRange(-1e9, 1e9);
        sb->setDecimals(3);
        sb->setValue(value.toDouble());
        styleEditor(sb);
        connect(sb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, name](double v){ writeProperty(name.constData(), v); });
        return sb;
    }
    case QMetaType::QString: {
        auto *le = new QLineEdit(value.toString());
        styleEditor(le);
        connect(le, &QLineEdit::editingFinished, this, [this, name, le](){
            writeProperty(name.constData(), le->text());
        });
        return le;
    }
    case QMetaType::QSize: {
        const QSize sz = value.toSize();
        auto *host = new QWidget; styleEditor(host);
        auto *row = new QHBoxLayout(host);
        row->setContentsMargins(0,0,0,0);
        row->setSpacing(2);
        auto *w = new QSpinBox; w->setRange(0, 100000); w->setValue(sz.width());  styleEditor(w);
        auto *h = new QSpinBox; h->setRange(0, 100000); h->setValue(sz.height()); styleEditor(h);
        row->addWidget(w); row->addWidget(h);
        auto commit = [this, name, w, h](){
            writeProperty(name.constData(), QSize(w->value(), h->value()));
        };
        connect(w, QOverload<int>::of(&QSpinBox::valueChanged), this, commit);
        connect(h, QOverload<int>::of(&QSpinBox::valueChanged), this, commit);
        return host;
    }
    case QMetaType::QRect: {
        const QRect r = value.toRect();
        auto *host = new QWidget; styleEditor(host);
        auto *row = new QHBoxLayout(host);
        row->setContentsMargins(0,0,0,0); row->setSpacing(2);
        QSpinBox *boxes[4];
        const int vals[4] = {r.x(), r.y(), r.width(), r.height()};
        for (int i = 0; i < 4; ++i) {
            boxes[i] = new QSpinBox;
            boxes[i]->setRange(-100000, 100000);
            boxes[i]->setValue(vals[i]);
            styleEditor(boxes[i]);
            row->addWidget(boxes[i]);
        }
        auto commit = [this, name, boxes](){
            writeProperty(name.constData(),
                QRect(boxes[0]->value(), boxes[1]->value(),
                      boxes[2]->value(), boxes[3]->value()));
        };
        for (auto *b : boxes)
            connect(b, QOverload<int>::of(&QSpinBox::valueChanged), this, commit);
        return host;
    }
    case QMetaType::QColor: {
        const QColor c = value.value<QColor>();
        auto *btn = new QPushButton;
        btn->setText(c.name());
        btn->setStyleSheet(QString(
            "QPushButton{background:%1;color:%2;border:1px solid #3c3c3c;"
            "padding:2px 8px;text-align:left;}").arg(
                c.name(),
                c.lightness() > 127 ? "#000000" : "#ffffff"));
        connect(btn, &QPushButton::clicked, this, [this, name, btn]() {
            QColorDialog dlg(this);
            dlg.setOption(QColorDialog::DontUseNativeDialog, false);
            if (dlg.exec() == QDialog::Accepted) {
                const QColor picked = dlg.currentColor();
                btn->setText(picked.name());
                btn->setStyleSheet(QString(
                    "QPushButton{background:%1;color:%2;border:1px solid #3c3c3c;"
                    "padding:2px 8px;text-align:left;}").arg(
                        picked.name(),
                        picked.lightness() > 127 ? "#000000" : "#ffffff"));
                writeProperty(name.constData(), picked);
            }
        });
        return btn;
    }
    default:
        // Enum: Qt reports enums as the underlying int type via QMetaType but
        // the QMetaProperty lets us discover the enumerator names.  Build a
        // QComboBox.  This handles things like Qt::Alignment, Qt::Orientation,
        // QFrame::Shape, etc.
        if (m_target) {
            const QMetaObject *mo = m_target->metaObject();
            const int idx = mo->indexOfProperty(propName);
            if (idx >= 0) {
                const QMetaProperty p = mo->property(idx);
                if (p.isEnumType()) {
                    QMetaEnum me = p.enumerator();
                    auto *cb = new QComboBox;
                    int currentIndex = 0;
                    for (int i = 0; i < me.keyCount(); ++i) {
                        cb->addItem(QString::fromLatin1(me.key(i)), me.value(i));
                        if (me.value(i) == value.toInt()) currentIndex = i;
                    }
                    cb->setCurrentIndex(currentIndex);
                    styleEditor(cb);
                    connect(cb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                            this, [this, name, cb](int){
                                writeProperty(name.constData(), cb->currentData());
                            });
                    return cb;
                }
            }
        }
        return nullptr;
    }
}

} // namespace exo::forms
