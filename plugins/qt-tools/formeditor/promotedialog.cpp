#include "promotedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace exo::forms {

namespace {

// "MyButton" → "mybutton.h".  Strip any leading uppercase Q (so QPushButton
// would suggest "qpushbutton.h" — fine; the user typically chooses their
// own class name, not Qt's).  We don't try to be clever about acronyms.
QString suggestHeaderFor(const QString &cls) {
    if (cls.isEmpty()) return {};
    return cls.toLower() + QStringLiteral(".h");
}

QString capitalizeFirst(const QString &s) {
    if (s.isEmpty()) return s;
    QString out = s;
    out[0] = out[0].toUpper();
    return out;
}

} // namespace

PromoteDialog::PromoteDialog(const QString &baseClass,
                             const QString &initialClassName,
                             const QString &initialHeader,
                             bool           initialGlobal,
                             QWidget       *parent)
    : QDialog(parent), m_baseClass(baseClass) {
    setWindowTitle(tr("Promote to Custom Widget"));
    setModal(true);
    setMinimumWidth(380);

    // VS dark theme on the dialog frame and every input.  Keeps the dialog
    // visually consistent with the editor; native Qt-look stands out badly
    // against the rest of the IDE shell.
    setStyleSheet(
        "QDialog{background:#252526;color:#d4d4d4;}"
        "QLabel{color:#d4d4d4;}"
        "QLineEdit{background:#1e1e1e;color:#d4d4d4;border:1px solid #3c3c3c;"
        " padding:3px 6px;selection-background-color:#094771;}"
        "QLineEdit:focus{border:1px solid #007acc;}"
        "QCheckBox{color:#d4d4d4;}"
        "QCheckBox::indicator{width:14px;height:14px;background:#1e1e1e;"
        " border:1px solid #3c3c3c;}"
        "QCheckBox::indicator:checked{background:#007acc;}"
        "QPushButton{background:#0e639c;color:#ffffff;border:1px solid #007acc;"
        " padding:5px 16px;min-width:72px;}"
        "QPushButton:hover{background:#1177bb;}"
        "QPushButton:default{background:#0e639c;}"
        "QPushButton[secondary=\"1\"]{background:#3a3d41;border:1px solid #3c3c3c;}"
        "QPushButton[secondary=\"1\"]:hover{background:#4a4d51;}");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 14, 14, 12);
    root->setSpacing(10);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setSpacing(8);

    // Base class — read-only display so the user can see what they're
    // extending.  Designer prints this as plain text; we go further and
    // colour the value subtly so it reads as informational, not editable.
    m_baseLabel = new QLabel(QStringLiteral("<span style='color:#9cdcfe;'>%1</span>")
                                 .arg(baseClass.isEmpty() ? tr("QWidget") : baseClass),
                             this);
    m_baseLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(tr("Base class:"), m_baseLabel);

    m_className = new QLineEdit(initialClassName, this);
    m_className->setPlaceholderText(tr("MyButton"));
    m_className->setToolTip(tr("Promoted class name (e.g. MyButton)"));
    form->addRow(tr("Promoted class &name:"), m_className);

    m_headerFile = new QLineEdit(initialHeader, this);
    m_headerFile->setPlaceholderText(tr("mybutton.h"));
    m_headerFile->setToolTip(tr("Header file containing the promoted class declaration"));
    form->addRow(tr("&Header file:"), m_headerFile);

    m_globalInc = new QCheckBox(tr("Global include  (use <header> instead of \"header\")"), this);
    m_globalInc->setChecked(initialGlobal);
    m_globalInc->setToolTip(tr("Designer emits <include location=\"global\"> when checked"));
    form->addRow(QString(), m_globalInc);

    root->addLayout(form);

    auto *hint = new QLabel(
        tr("The .ui file will gain a <customwidgets> entry that Qt Designer recognizes."),
        this);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#888888;font-size:10px;");
    root->addWidget(hint);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    if (auto *cancel = m_buttons->button(QDialogButtonBox::Cancel))
        cancel->setProperty("secondary", "1");
    auto *okBtn = m_buttons->button(QDialogButtonBox::Ok);
    if (okBtn) {
        okBtn->setText(tr("Promote"));
        okBtn->setDefault(true);
    }
    root->addWidget(m_buttons);

    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_className, &QLineEdit::textEdited,
            this, &PromoteDialog::onClassNameEdited);
    connect(m_headerFile, &QLineEdit::textEdited,
            this, &PromoteDialog::onHeaderManuallyEdited);

    // Initial enable/disable: OK is only meaningful when class name is not
    // empty.  Using textChanged so we react to programmatic resets too.
    auto refreshOk = [this, okBtn]() {
        if (!okBtn) return;
        const QString cls = m_className->text().trimmed();
        const QString hdr = m_headerFile->text().trimmed();
        // Class name must be a plausible C++ identifier (no spaces, doesn't
        // start with a digit).  We don't lint exhaustively — the IDE will
        // reject malformed XML at load time.
        bool ok = !cls.isEmpty() && !hdr.isEmpty();
        if (ok && cls[0].isDigit()) ok = false;
        if (ok) {
            for (QChar c : cls) {
                if (!(c.isLetterOrNumber() || c == QLatin1Char('_'))) { ok = false; break; }
            }
        }
        okBtn->setEnabled(ok);
    };
    connect(m_className, &QLineEdit::textChanged, this, refreshOk);
    connect(m_headerFile, &QLineEdit::textChanged, this, refreshOk);
    refreshOk();

    // Auto-uppercase first letter once the user finishes typing (textEdited
    // fires per keystroke; we wait for editing finished to avoid fighting
    // the cursor while they type).
    connect(m_className, &QLineEdit::editingFinished, this, [this](){
        const QString cur = m_className->text();
        const QString cap = capitalizeFirst(cur);
        if (cap != cur) m_className->setText(cap);
    });

    m_className->setFocus();
}

QString PromoteDialog::className()     const { return m_className->text().trimmed(); }
QString PromoteDialog::headerFile()    const { return m_headerFile->text().trimmed(); }
bool    PromoteDialog::globalInclude() const { return m_globalInc->isChecked(); }

void PromoteDialog::onClassNameEdited(const QString &text) {
    if (m_headerEdited) return;   // user already manually customized; don't clobber
    m_headerFile->setText(suggestHeaderFor(text));
}

void PromoteDialog::onHeaderManuallyEdited() {
    m_headerEdited = true;
}

} // namespace exo::forms
