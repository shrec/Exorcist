// signalsloteditor.h — Designer-style signal/slot connection editor.
//
// Phase 2 ships a lightweight QDialog (modeless, can be left open while
// editing the form) with a 4-column QTableWidget:
//
//     Sender | Signal | Receiver | Slot
//
// Add row → 4 dropdowns: pick widget from form, pick signal from sender's
// metaObject, pick receiver, pick slot from receiver's metaObject.  Search
// boxes filter signal/slot lists (Principle 2: Search-everywhere).
//
// Connections are STORED, not executed: the editor doesn't actually call
// QObject::connect() on the live form (we don't want runtime side effects
// during design).  Instead we keep the list of FormConnection records on
// FormCanvas, and uixmlio.cpp emits them in the .ui's <connections> section
// when saving.  Loading via QUiLoader rehydrates them automatically.
//
// Open shortcuts (wired in UiFormEditor):
//   Ctrl+Alt+S  → open editor as a modeless dialog
//   Right-click a widget → context menu "Add Signal-Slot Connection..."
//
// UX
//   - Modeless: editing the form should still be possible while editor is
//     visible (Principle 3: Inline > Modal, applied as far as a multi-row
//     editor allows).
//   - When sender combo changes, briefly highlight that widget on the
//     canvas to give the user spatial confirmation.
//   - Search filter inside the signal/slot dropdowns (combobox lineEdit).
#pragma once

#include <QDialog>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>

class QComboBox;
class QPushButton;
class QTableWidget;
class QTableWidgetItem;

namespace exo::forms {

class FormCanvas;

// Persistent record of a connection.  Stored on FormCanvas and serialized
// in uixmlio.cpp inside the <connections> tag.  Names rather than pointers
// because the form is reloaded across save/load round trips.
struct FormConnection {
    QString sender;     // sender objectName
    QString signal_;    // signal signature without parameters listed in slot,
                        // e.g. "clicked()"  (matches Designer output)
    QString receiver;   // receiver objectName
    QString slot_;      // e.g. "close()"
};

class SignalSlotEditor : public QDialog {
    Q_OBJECT
public:
    explicit SignalSlotEditor(FormCanvas *canvas, QWidget *parent = nullptr);

    // Pre-select a sender (used when invoked from widget context menu).
    void presetSender(QWidget *sender);

    // Refresh table from canvas connection list (call after canvas reload).
    void rebuildFromCanvas();

private slots:
    void onAddRow();
    void onRemoveRow();
    void onSenderChanged(int row);
    void onReceiverChanged(int row);
    void writeConnectionsToCanvas();

private:
    void buildUi();
    QComboBox *makeWidgetCombo();
    QComboBox *makeSearchableCombo();
    void populateSignalCombo(QComboBox *combo, QWidget *sender);
    void populateSlotCombo(QComboBox *combo, QWidget *receiver);
    void highlightCanvasWidget(QWidget *w);

    FormCanvas    *m_canvas = nullptr;
    QTableWidget  *m_table  = nullptr;
    QPushButton   *m_btnAdd = nullptr;
    QPushButton   *m_btnDel = nullptr;
};

} // namespace exo::forms
