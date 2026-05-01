#include "signalsloteditor.h"

#include "formcanvas.h"

#include <QComboBox>
#include <QCompleter>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMetaMethod>
#include <QMetaObject>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace exo::forms {

namespace {

// Collect every named widget reachable from the form root.  Designer treats
// the root form widget as a valid sender/receiver too.
QList<QWidget *> collectNamedWidgets(QWidget *root) {
    QList<QWidget *> out;
    if (!root) return out;
    out << root;
    const QList<QWidget *> all = root->findChildren<QWidget *>();
    for (QWidget *w : all) {
        if (w->property("exo_isHandle").toBool()) continue;
        if (w->property("exo_isOverlay").toBool()) continue;
        if (w->objectName().isEmpty()) continue;
        out << w;
    }
    return out;
}

// Render a QMetaMethod as Designer's signature form: "clicked()" or
// "valueChanged(int)" — the exact string Designer writes into the
// <connections><signal>...</signal> block.
QString methodSignature(const QMetaMethod &m) {
    return QString::fromLatin1(m.methodSignature());
}

} // namespace

// ── ctor / build ─────────────────────────────────────────────────────────────

SignalSlotEditor::SignalSlotEditor(FormCanvas *canvas, QWidget *parent)
    : QDialog(parent), m_canvas(canvas) {
    setWindowTitle(tr("Signal/Slot Editor"));
    setModal(false);
    resize(720, 360);
    setStyleSheet(
        "QDialog{background:#1e1e1e;color:#d4d4d4;}"
        "QLabel{color:#d4d4d4;}"
        "QPushButton{background:#2d2d30;color:#d4d4d4;border:1px solid #3c3c3c;"
        " padding:4px 12px;border-radius:2px;}"
        "QPushButton:hover{background:#3e3e42;}"
        "QPushButton:pressed{background:#094771;}"
        "QTableWidget{background:#1e1e1e;color:#d4d4d4;border:1px solid #3c3c3c;"
        " gridline-color:#3c3c3c;alternate-background-color:#252526;}"
        "QHeaderView::section{background:#2d2d30;color:#d4d4d4;border:none;"
        " padding:4px 8px;border-bottom:1px solid #3c3c3c;}"
        "QComboBox{background:#252526;color:#d4d4d4;border:1px solid #3c3c3c;"
        " padding:2px 6px;border-radius:2px;}"
        "QComboBox:focus{border:1px solid #007acc;}"
        "QComboBox QAbstractItemView{background:#1e1e1e;color:#d4d4d4;"
        " selection-background-color:#094771;border:1px solid #3c3c3c;}");
    buildUi();
    rebuildFromCanvas();
}

void SignalSlotEditor::buildUi() {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    auto *header = new QLabel(tr("Define connections (Ctrl+Alt+S to toggle)"), this);
    header->setStyleSheet("QLabel{color:#9da0a4;font-style:italic;}");
    root->addWidget(header);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({tr("Sender"), tr("Signal"),
                                        tr("Receiver"), tr("Slot")});
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setAlternatingRowColors(true);
    m_table->setShowGrid(false);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    for (int i = 0; i < 4; ++i)
        m_table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Stretch);
    root->addWidget(m_table, 1);

    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(6);
    m_btnAdd = new QPushButton(tr("Add Connection"), this);
    m_btnAdd->setToolTip(tr("Add new signal-slot row"));
    m_btnDel = new QPushButton(tr("Remove"), this);
    m_btnDel->setToolTip(tr("Remove selected connection"));
    btnRow->addWidget(m_btnAdd);
    btnRow->addWidget(m_btnDel);
    btnRow->addStretch(1);
    auto *btnClose = new QPushButton(tr("Close"), this);
    btnClose->setToolTip(tr("Close (Esc)"));
    btnRow->addWidget(btnClose);
    root->addLayout(btnRow);

    connect(m_btnAdd,  &QPushButton::clicked, this, &SignalSlotEditor::onAddRow);
    connect(m_btnDel,  &QPushButton::clicked, this, &SignalSlotEditor::onRemoveRow);
    connect(btnClose,  &QPushButton::clicked, this, &QDialog::close);
}

// ── Populate ─────────────────────────────────────────────────────────────────

void SignalSlotEditor::rebuildFromCanvas() {
    m_table->setRowCount(0);
    if (!m_canvas) return;
    const QList<FormConnection> existing = m_canvas->connections();
    for (const FormConnection &c : existing) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        QComboBox *sender = makeWidgetCombo();
        QComboBox *signal = makeSearchableCombo();
        QComboBox *recv   = makeWidgetCombo();
        QComboBox *slot   = makeSearchableCombo();

        // Find sender index.
        const int sIdx = sender->findData(c.sender);
        if (sIdx >= 0) sender->setCurrentIndex(sIdx);
        // Populate signals from sender, set current.
        if (QWidget *sw = m_canvas->findNamedWidget(c.sender)) populateSignalCombo(signal, sw);
        const int sigIdx = signal->findText(c.signal_);
        if (sigIdx >= 0) signal->setCurrentIndex(sigIdx);
        // Receiver
        const int rIdx = recv->findData(c.receiver);
        if (rIdx >= 0) recv->setCurrentIndex(rIdx);
        if (QWidget *rw = m_canvas->findNamedWidget(c.receiver)) populateSlotCombo(slot, rw);
        const int slIdx = slot->findText(c.slot_);
        if (slIdx >= 0) slot->setCurrentIndex(slIdx);

        m_table->setCellWidget(row, 0, sender);
        m_table->setCellWidget(row, 1, signal);
        m_table->setCellWidget(row, 2, recv);
        m_table->setCellWidget(row, 3, slot);

        connect(sender, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int){ onSenderChanged(row); writeConnectionsToCanvas(); });
        connect(signal, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ writeConnectionsToCanvas(); });
        connect(recv, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, row](int){ onReceiverChanged(row); writeConnectionsToCanvas(); });
        connect(slot, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ writeConnectionsToCanvas(); });
    }
}

QComboBox *SignalSlotEditor::makeWidgetCombo() {
    auto *cb = new QComboBox;
    cb->setEditable(false);
    if (!m_canvas) return cb;
    const QList<QWidget *> widgets = collectNamedWidgets(m_canvas->root());
    for (QWidget *w : widgets) {
        const QString cls = QString::fromLatin1(w->metaObject()->className());
        cb->addItem(QStringLiteral("%1 (%2)").arg(w->objectName(), cls),
                    w->objectName());
    }
    return cb;
}

QComboBox *SignalSlotEditor::makeSearchableCombo() {
    auto *cb = new QComboBox;
    cb->setEditable(true);
    cb->setInsertPolicy(QComboBox::NoInsert);
    cb->lineEdit()->setPlaceholderText(tr("Search..."));
    cb->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    if (auto *c = cb->completer()) {
        c->setCaseSensitivity(Qt::CaseInsensitive);
        c->setFilterMode(Qt::MatchContains);
    }
    return cb;
}

void SignalSlotEditor::populateSignalCombo(QComboBox *combo, QWidget *sender) {
    combo->clear();
    if (!sender) return;
    const QMetaObject *mo = sender->metaObject();
    QStringList sigs;
    for (int i = 0; i < mo->methodCount(); ++i) {
        const QMetaMethod m = mo->method(i);
        if (m.methodType() != QMetaMethod::Signal) continue;
        if (m.access() == QMetaMethod::Private) continue;
        sigs << methodSignature(m);
    }
    sigs.removeDuplicates();
    sigs.sort();
    combo->addItems(sigs);
}

void SignalSlotEditor::populateSlotCombo(QComboBox *combo, QWidget *receiver) {
    combo->clear();
    if (!receiver) return;
    const QMetaObject *mo = receiver->metaObject();
    QStringList slots_;
    for (int i = 0; i < mo->methodCount(); ++i) {
        const QMetaMethod m = mo->method(i);
        if (m.methodType() != QMetaMethod::Slot) continue;
        if (m.access() == QMetaMethod::Private) continue;
        slots_ << methodSignature(m);
    }
    slots_.removeDuplicates();
    slots_.sort();
    combo->addItems(slots_);
}

// ── Add / remove ─────────────────────────────────────────────────────────────

void SignalSlotEditor::onAddRow() {
    const int row = m_table->rowCount();
    m_table->insertRow(row);
    QComboBox *sender = makeWidgetCombo();
    QComboBox *signal = makeSearchableCombo();
    QComboBox *recv   = makeWidgetCombo();
    QComboBox *slot   = makeSearchableCombo();
    m_table->setCellWidget(row, 0, sender);
    m_table->setCellWidget(row, 1, signal);
    m_table->setCellWidget(row, 2, recv);
    m_table->setCellWidget(row, 3, slot);

    // Initial population using whatever's currently selected.
    if (sender->count() > 0) {
        if (QWidget *sw = m_canvas->findNamedWidget(sender->currentData().toString()))
            populateSignalCombo(signal, sw);
    }
    if (recv->count() > 0) {
        if (QWidget *rw = m_canvas->findNamedWidget(recv->currentData().toString()))
            populateSlotCombo(slot, rw);
    }

    connect(sender, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, row](int){ onSenderChanged(row); writeConnectionsToCanvas(); });
    connect(signal, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ writeConnectionsToCanvas(); });
    connect(recv, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this, row](int){ onReceiverChanged(row); writeConnectionsToCanvas(); });
    connect(slot, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ writeConnectionsToCanvas(); });

    writeConnectionsToCanvas();
}

void SignalSlotEditor::onRemoveRow() {
    const int row = m_table->currentRow();
    if (row < 0) return;
    m_table->removeRow(row);
    writeConnectionsToCanvas();
}

void SignalSlotEditor::onSenderChanged(int row) {
    auto *senderCombo = qobject_cast<QComboBox *>(m_table->cellWidget(row, 0));
    auto *signalCombo = qobject_cast<QComboBox *>(m_table->cellWidget(row, 1));
    if (!senderCombo || !signalCombo || !m_canvas) return;
    QWidget *sw = m_canvas->findNamedWidget(senderCombo->currentData().toString());
    populateSignalCombo(signalCombo, sw);
    highlightCanvasWidget(sw);
}

void SignalSlotEditor::onReceiverChanged(int row) {
    auto *recvCombo = qobject_cast<QComboBox *>(m_table->cellWidget(row, 2));
    auto *slotCombo = qobject_cast<QComboBox *>(m_table->cellWidget(row, 3));
    if (!recvCombo || !slotCombo || !m_canvas) return;
    QWidget *rw = m_canvas->findNamedWidget(recvCombo->currentData().toString());
    populateSlotCombo(slotCombo, rw);
    highlightCanvasWidget(rw);
}

void SignalSlotEditor::writeConnectionsToCanvas() {
    if (!m_canvas) return;
    QList<FormConnection> connections;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        auto *sender = qobject_cast<QComboBox *>(m_table->cellWidget(r, 0));
        auto *signal = qobject_cast<QComboBox *>(m_table->cellWidget(r, 1));
        auto *recv   = qobject_cast<QComboBox *>(m_table->cellWidget(r, 2));
        auto *slot   = qobject_cast<QComboBox *>(m_table->cellWidget(r, 3));
        if (!sender || !signal || !recv || !slot) continue;
        FormConnection c;
        c.sender   = sender->currentData().toString();
        c.signal_  = signal->currentText();
        c.receiver = recv->currentData().toString();
        c.slot_    = slot->currentText();
        if (c.sender.isEmpty() || c.receiver.isEmpty()) continue;
        if (c.signal_.isEmpty() || c.slot_.isEmpty()) continue;
        connections << c;
    }
    m_canvas->setConnections(connections);
}

void SignalSlotEditor::presetSender(QWidget *sender) {
    if (!sender || sender->objectName().isEmpty()) return;
    onAddRow();
    const int row = m_table->rowCount() - 1;
    auto *senderCombo = qobject_cast<QComboBox *>(m_table->cellWidget(row, 0));
    if (!senderCombo) return;
    const int idx = senderCombo->findData(sender->objectName());
    if (idx >= 0) senderCombo->setCurrentIndex(idx);
}

void SignalSlotEditor::highlightCanvasWidget(QWidget *w) {
    if (!w || !m_canvas) return;
    m_canvas->flashHighlight(w);
}

} // namespace exo::forms
