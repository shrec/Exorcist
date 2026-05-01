#include "newqtclasswizard.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

QString sanitizeIdentifier(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    for (QChar c : raw) {
        if (c.isLetterOrNumber() || c == QLatin1Char('_'))
            out.append(c);
    }
    if (out.isEmpty())
        out = QStringLiteral("MyClass");
    if (out.at(0).isDigit())
        out.prepend(QLatin1Char('C'));
    return out;
}

QString toUpperGuard(const QString &raw)
{
    QString out;
    out.reserve(raw.size() * 2);
    QChar prev;
    for (int i = 0; i < raw.size(); ++i) {
        QChar c = raw.at(i);
        if (c.isLetterOrNumber()) {
            if (i > 0 && c.isUpper() && prev.isLower())
                out.append(QLatin1Char('_'));
            out.append(c.toUpper());
        } else {
            out.append(QLatin1Char('_'));
        }
        prev = c;
    }
    return out;
}

bool writeTextFile(const QString &path, const QString &content)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    ts << content;
    return f.error() == QFile::NoError;
}

QString readTextFile(const QString &path, bool *ok = nullptr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok) *ok = false;
        return {};
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    if (ok) *ok = true;
    return ts.readAll();
}

} // namespace

// ── NewQtClassWizard ─────────────────────────────────────────────────────────

NewQtClassWizard::NewQtClassWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("New Qt Class"));
    setMinimumSize(620, 580);
    resize(720, 640);

    setStyleSheet(QStringLiteral(
        "NewQtClassWizard { background: #1e1e1e; }"
        "QLabel { color: #cccccc; font-size: 13px; }"
        "QLabel#titleLabel { font-size: 18px; font-weight: 300; }"
        "QLabel#hintLabel { color: #888; font-size: 12px; }"
        "QLineEdit { background: #3c3c3c; border: 1px solid #3e3e42; "
        "  color: #cccccc; padding: 6px; font-size: 13px; "
        "  selection-background-color: #094771; }"
        "QLineEdit:focus { border-color: #007acc; }"
        "QLineEdit:disabled { color: #666; background: #2d2d2d; }"
        "QComboBox { background: #3c3c3c; border: 1px solid #3e3e42; "
        "  color: #cccccc; padding: 5px 8px; font-size: 13px; min-height: 28px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: #252526; color: #cccccc; "
        "  selection-background-color: #094771; border: 1px solid #3e3e42; }"
        "QCheckBox { color: #cccccc; font-size: 13px; spacing: 6px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; "
        "  background: #3c3c3c; border: 1px solid #3e3e42; }"
        "QCheckBox::indicator:checked { background: #0e639c; border-color: #1177bb; }"
        "QListWidget { background: #252526; color: #cccccc; "
        "  border: 1px solid #3e3e42; font-size: 13px; }"
        "QListWidget::item { padding: 4px 8px; }"
        "QListWidget::item:selected { background: #094771; color: white; }"
        "QPushButton { background: #0e639c; color: white; border: none; "
        "  padding: 8px 20px; font-size: 13px; border-radius: 2px; }"
        "QPushButton:hover { background: #1177bb; }"
        "QPushButton:disabled { background: #3e3e42; color: #888; }"
        "QPushButton#browseBtn { background: #3c3c3c; color: #cccccc; "
        "  padding: 6px 12px; }"
        "QPushButton#browseBtn:hover { background: #505050; }"
        "QPushButton#cancelBtn { background: transparent; color: #cccccc; "
        "  border: 1px solid #3e3e42; }"
        "QPushButton#cancelBtn:hover { background: #2a2d2e; }"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    auto *title = new QLabel(tr("Create a new Qt class"), this);
    title->setObjectName(QStringLiteral("titleLabel"));
    mainLayout->addWidget(title);

    auto *subtitle = new QLabel(
        tr("Generates a Q_OBJECT-enabled header/source pair, with optional "
           ".ui form and CMakeLists.txt registration."), this);
    subtitle->setObjectName(QStringLiteral("hintLabel"));
    subtitle->setWordWrap(true);
    mainLayout->addWidget(subtitle);

    auto *form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_classNameEdit = new QLineEdit(this);
    m_classNameEdit->setPlaceholderText(QStringLiteral("MyWidget"));
    form->addRow(tr("Class name:"), m_classNameEdit);

    m_baseCombo = new QComboBox(this);
    m_baseCombo->addItem(QStringLiteral("QObject"));
    m_baseCombo->addItem(QStringLiteral("QWidget"));
    m_baseCombo->addItem(QStringLiteral("QDialog"));
    m_baseCombo->addItem(QStringLiteral("QMainWindow"));
    m_baseCombo->addItem(QStringLiteral("QFrame"));
    m_baseCombo->addItem(QStringLiteral("QPushButton"));
    m_baseCombo->addItem(QStringLiteral("QListView"));
    m_baseCombo->addItem(tr("Custom..."));
    form->addRow(tr("Base class:"), m_baseCombo);

    m_customBaseEdit = new QLineEdit(this);
    m_customBaseEdit->setPlaceholderText(QStringLiteral("e.g. QAbstractItemView"));
    m_customBaseEdit->setEnabled(false);
    form->addRow(tr("Custom base:"), m_customBaseEdit);

    m_guardEdit = new QLineEdit(this);
    m_guardEdit->setPlaceholderText(QStringLiteral("MYWIDGET_H"));
    form->addRow(tr("Header guard:"), m_guardEdit);

    auto *folderRow = new QHBoxLayout;
    folderRow->setSpacing(6);
    m_folderEdit = new QLineEdit(this);
    m_folderEdit->setPlaceholderText(tr("Output folder"));
    QString defaultParent = QStandardPaths::writableLocation(
                                QStandardPaths::DocumentsLocation);
    if (defaultParent.isEmpty())
        defaultParent = QDir::homePath();
    m_folderEdit->setText(defaultParent);
    m_browseBtn = new QPushButton(QStringLiteral("..."), this);
    m_browseBtn->setObjectName(QStringLiteral("browseBtn"));
    m_browseBtn->setFixedWidth(36);
    folderRow->addWidget(m_folderEdit, 1);
    folderRow->addWidget(m_browseBtn);
    form->addRow(tr("Output folder:"), folderRow);

    mainLayout->addLayout(form);

    // Options.
    auto *optsLayout = new QVBoxLayout;
    optsLayout->setSpacing(4);
    m_uiFileCheck   = new QCheckBox(tr("Include .ui file (Qt Designer form)"), this);
    m_addCMakeCheck = new QCheckBox(tr("Add to nearest CMakeLists.txt"), this);
    m_openCheck     = new QCheckBox(tr("Open header after create"), this);
    m_openCheck->setChecked(true);
    optsLayout->addWidget(m_uiFileCheck);
    optsLayout->addWidget(m_addCMakeCheck);
    optsLayout->addWidget(m_openCheck);
    mainLayout->addLayout(optsLayout);

    // Signal / slot picker.
    auto *listsRow = new QHBoxLayout;
    listsRow->setSpacing(8);

    auto *signalsCol = new QVBoxLayout;
    signalsCol->setSpacing(4);
    auto *signalsLabel = new QLabel(tr("Signals (toggle to include):"), this);
    m_signalsList = new QListWidget(this);
    m_signalsList->setSelectionMode(QAbstractItemView::NoSelection);
    auto addSignal = [this](const QString &sig) {
        auto *item = new QListWidgetItem(sig, m_signalsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    };
    addSignal(QStringLiteral("clicked()"));
    addSignal(QStringLiteral("textChanged(const QString &text)"));
    addSignal(QStringLiteral("destroyed()"));
    addSignal(QStringLiteral("valueChanged(int value)"));
    addSignal(QStringLiteral("toggled(bool checked)"));
    signalsCol->addWidget(signalsLabel);
    signalsCol->addWidget(m_signalsList, 1);

    auto *slotsCol = new QVBoxLayout;
    slotsCol->setSpacing(4);
    auto *slotsLabel = new QLabel(tr("Slots (toggle to include):"), this);
    m_slotsList = new QListWidget(this);
    m_slotsList->setSelectionMode(QAbstractItemView::NoSelection);
    auto addSlot = [this](const QString &slt) {
        auto *item = new QListWidgetItem(slt, m_slotsList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    };
    addSlot(QStringLiteral("onClicked()"));
    addSlot(QStringLiteral("onTextChanged(const QString &text)"));
    addSlot(QStringLiteral("refresh()"));
    addSlot(QStringLiteral("reset()"));
    addSlot(QStringLiteral("setEnabled(bool enabled)"));
    slotsCol->addWidget(slotsLabel);
    slotsCol->addWidget(m_slotsList, 1);

    listsRow->addLayout(signalsCol, 1);
    listsRow->addLayout(slotsCol, 1);
    mainLayout->addLayout(listsRow, 1);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName(QStringLiteral("hintLabel"));
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setText(tr("Files will be written directly into the output folder."));
    mainLayout->addWidget(m_hintLabel);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
    m_createBtn = new QPushButton(tr("Create"), this);
    m_createBtn->setDefault(true);
    buttonRow->addWidget(m_cancelBtn);
    buttonRow->addWidget(m_createBtn);
    mainLayout->addLayout(buttonRow);

    connect(m_browseBtn, &QPushButton::clicked,
            this, &NewQtClassWizard::onBrowseFolder);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_createBtn, &QPushButton::clicked,
            this, &NewQtClassWizard::onCreate);
    connect(m_classNameEdit, &QLineEdit::textChanged,
            this, &NewQtClassWizard::onClassNameChanged);
    connect(m_baseCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &NewQtClassWizard::onBaseClassChanged);
    connect(m_guardEdit, &QLineEdit::textEdited,
            this, [this](const QString &) { m_guardEditedManually = true; });
    connect(m_folderEdit, &QLineEdit::textChanged,
            this, &NewQtClassWizard::updateCreateButton);
    connect(m_customBaseEdit, &QLineEdit::textChanged,
            this, &NewQtClassWizard::updateCreateButton);

    updateCreateButton();
}

void NewQtClassWizard::onBrowseFolder()
{
    const QString start = m_folderEdit->text().isEmpty()
                              ? QDir::homePath()
                              : m_folderEdit->text();
    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose Output Folder"), start);
    if (!dir.isEmpty())
        m_folderEdit->setText(dir);
}

void NewQtClassWizard::onClassNameChanged(const QString &text)
{
    if (!m_guardEditedManually) {
        const QString sanitized = sanitizeIdentifier(text);
        m_guardEdit->setText(toUpperGuard(sanitized) + QStringLiteral("_H"));
    }
    updateCreateButton();
}

void NewQtClassWizard::onBaseClassChanged(int index)
{
    const bool isCustom = (index == m_baseCombo->count() - 1);
    m_customBaseEdit->setEnabled(isCustom);
    if (!isCustom)
        m_customBaseEdit->clear();
    updateCreateButton();
}

void NewQtClassWizard::updateCreateButton()
{
    QString err;
    m_createBtn->setEnabled(validate(&err));
}

bool NewQtClassWizard::validate(QString *errorOut) const
{
    auto fail = [&](const QString &msg) {
        if (errorOut) *errorOut = msg;
        return false;
    };
    const QString name = m_classNameEdit ? m_classNameEdit->text().trimmed() : QString();
    const QString folder = m_folderEdit ? m_folderEdit->text().trimmed() : QString();

    if (name.isEmpty())
        return fail(tr("Class name is required."));

    static const QRegularExpression nameRe(
        QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*$"));
    if (!nameRe.match(name).hasMatch())
        return fail(tr("Class name must be a valid C++ identifier."));

    const QString base = effectiveBaseClass();
    if (base.isEmpty())
        return fail(tr("Base class is required."));
    if (!nameRe.match(base).hasMatch())
        return fail(tr("Base class must be a valid C++ identifier."));

    if (folder.isEmpty())
        return fail(tr("Output folder is required."));
    if (!QFileInfo(folder).isDir())
        return fail(tr("Output folder does not exist."));
    return true;
}

QString NewQtClassWizard::effectiveBaseClass() const
{
    if (!m_baseCombo)
        return {};
    const int idx = m_baseCombo->currentIndex();
    const bool isCustom = (idx == m_baseCombo->count() - 1);
    if (isCustom)
        return m_customBaseEdit ? m_customBaseEdit->text().trimmed() : QString();
    return m_baseCombo->currentText();
}

bool NewQtClassWizard::baseIsWidget(const QString &base)
{
    static const QStringList widgetBases = {
        QStringLiteral("QWidget"),
        QStringLiteral("QDialog"),
        QStringLiteral("QMainWindow"),
        QStringLiteral("QFrame"),
        QStringLiteral("QPushButton"),
        QStringLiteral("QListView"),
        QStringLiteral("QAbstractButton"),
        QStringLiteral("QAbstractItemView"),
        QStringLiteral("QLabel"),
        QStringLiteral("QToolButton"),
        QStringLiteral("QGroupBox"),
        QStringLiteral("QScrollArea"),
        QStringLiteral("QTabWidget"),
    };
    return widgetBases.contains(base);
}

void NewQtClassWizard::onCreate()
{
    QString err;
    if (!validate(&err)) {
        QMessageBox::warning(this, tr("Invalid input"), err);
        return;
    }

    const QString className = m_classNameEdit->text().trimmed();
    const QString baseClass = effectiveBaseClass();
    QString guardPrefix = m_guardEdit->text().trimmed();
    if (guardPrefix.isEmpty())
        guardPrefix = toUpperGuard(className) + QStringLiteral("_H");
    const QString folder = m_folderEdit->text().trimmed();
    const bool includeUi = m_uiFileCheck->isChecked();
    const bool addToCMake = m_addCMakeCheck->isChecked();
    const bool openAfter = m_openCheck->isChecked();

    QStringList signals_;
    for (int i = 0; i < m_signalsList->count(); ++i) {
        QListWidgetItem *it = m_signalsList->item(i);
        if (it->checkState() == Qt::Checked)
            signals_ << it->text();
    }
    QStringList slots_;
    for (int i = 0; i < m_slotsList->count(); ++i) {
        QListWidgetItem *it = m_slotsList->item(i);
        if (it->checkState() == Qt::Checked)
            slots_ << it->text();
    }

    const QString stem = className.toLower();
    const QString headerName = stem + QStringLiteral(".h");
    const QString sourceName = stem + QStringLiteral(".cpp");
    const QString uiName = includeUi ? (stem + QStringLiteral(".ui")) : QString();

    QDir outDir(folder);
    const QString headerPath = outDir.absoluteFilePath(headerName);
    const QString sourcePath = outDir.absoluteFilePath(sourceName);
    const QString uiFilePath = includeUi ? outDir.absoluteFilePath(uiName) : QString();

    // Existence guard.
    QStringList exist;
    if (QFileInfo::exists(headerPath)) exist << headerName;
    if (QFileInfo::exists(sourcePath)) exist << sourceName;
    if (includeUi && QFileInfo::exists(uiFilePath)) exist << uiName;
    if (!exist.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, tr("Files exist"),
            tr("The following file(s) already exist and will be overwritten:\n\n%1\n\nContinue?")
                .arg(exist.join(QLatin1Char('\n'))));
        if (answer != QMessageBox::Yes)
            return;
    }

    const QString headerText = buildHeader(className, baseClass, guardPrefix,
                                           includeUi, signals_, slots_);
    const QString sourceText = buildSource(className, baseClass, includeUi, slots_);

    if (!writeTextFile(headerPath, headerText)) {
        QMessageBox::critical(this, tr("Create failed"),
                              tr("Could not write header: %1").arg(headerPath));
        return;
    }
    if (!writeTextFile(sourcePath, sourceText)) {
        QMessageBox::critical(this, tr("Create failed"),
                              tr("Could not write source: %1").arg(sourcePath));
        return;
    }
    if (includeUi) {
        const QString uiText = buildUiForm(className, baseClass);
        if (!writeTextFile(uiFilePath, uiText)) {
            QMessageBox::critical(this, tr("Create failed"),
                                  tr("Could not write .ui form: %1").arg(uiFilePath));
            return;
        }
    }

    m_headerPath = headerPath;
    m_sourcePath = sourcePath;
    m_uiPath     = includeUi ? uiFilePath : QString();

    QString cmakeNote;
    if (addToCMake) {
        const QString cmakePath = findNearestCMakeLists(folder);
        if (cmakePath.isEmpty()) {
            cmakeNote = tr("No CMakeLists.txt found in or above the output folder.");
        } else if (!patchCMakeLists(cmakePath, headerName, sourceName,
                                    includeUi ? uiName : QString())) {
            cmakeNote = tr("Could not patch %1 — please add the new files manually.")
                            .arg(cmakePath);
        } else {
            cmakeNote = tr("Updated %1.").arg(cmakePath);
        }
    }

    QString summary = tr("Class scaffolded:\n  %1\n  %2").arg(headerPath, sourcePath);
    if (includeUi)
        summary += tr("\n  %1").arg(uiFilePath);
    if (!cmakeNote.isEmpty())
        summary += QStringLiteral("\n\n") + cmakeNote;
    QMessageBox::information(this, tr("Class created"), summary);

    Q_UNUSED(openAfter);
    accept();
}

// ── Code generators ──────────────────────────────────────────────────────────

QString NewQtClassWizard::buildHeader(const QString &className,
                                      const QString &baseClass,
                                      const QString &guardPrefix,
                                      bool includeUi,
                                      const QStringList &signals_,
                                      const QStringList &slots_) const
{
    const bool isWidget = baseIsWidget(baseClass);
    const QString parentType = isWidget
        ? QStringLiteral("QWidget *parent = nullptr")
        : QStringLiteral("QObject *parent = nullptr");

    QString s;
    QTextStream ts(&s);
    ts << "// Generated by Exorcist NewQtClassWizard.\n";
    ts << "#ifndef " << guardPrefix << "\n";
    ts << "#define " << guardPrefix << "\n";
    ts << "\n";
    ts << "#include <" << baseClass << ">\n";

    if (includeUi) {
        ts << "\n";
        ts << "QT_BEGIN_NAMESPACE\n";
        ts << "namespace Ui { class " << className << "; }\n";
        ts << "QT_END_NAMESPACE\n";
    }

    ts << "\n";
    ts << "class " << className << " : public " << baseClass << "\n";
    ts << "{\n";
    ts << "    Q_OBJECT\n";
    ts << "\n";
    ts << "public:\n";
    ts << "    explicit " << className << "(" << parentType << ");\n";
    ts << "    ~" << className << "() override;\n";

    if (!signals_.isEmpty()) {
        ts << "\n";
        ts << "signals:\n";
        for (const QString &sig : signals_)
            ts << "    void " << sig << ";\n";
    }

    if (!slots_.isEmpty()) {
        ts << "\n";
        ts << "public slots:\n";
        for (const QString &slt : slots_)
            ts << "    void " << slt << ";\n";
    }

    if (includeUi) {
        ts << "\n";
        ts << "private:\n";
        ts << "    Ui::" << className << " *ui;\n";
    }

    ts << "};\n";
    ts << "\n";
    ts << "#endif // " << guardPrefix << "\n";
    return s;
}

QString NewQtClassWizard::buildSource(const QString &className,
                                      const QString &baseClass,
                                      bool includeUi,
                                      const QStringList &slots_) const
{
    const bool isWidget = baseIsWidget(baseClass);
    const QString parentType = isWidget
        ? QStringLiteral("QWidget *parent")
        : QStringLiteral("QObject *parent");

    QString s;
    QTextStream ts(&s);
    ts << "// Generated by Exorcist NewQtClassWizard.\n";
    ts << "#include \"" << className.toLower() << ".h\"\n";
    if (includeUi)
        ts << "#include \"ui_" << className.toLower() << ".h\"\n";
    ts << "\n";

    ts << className << "::" << className << "(" << parentType << ")\n";
    ts << "    : " << baseClass << "(parent)";
    if (includeUi)
        ts << ",\n      ui(new Ui::" << className << ")";
    ts << "\n{\n";
    if (includeUi)
        ts << "    ui->setupUi(this);\n";
    ts << "}\n";
    ts << "\n";

    ts << className << "::~" << className << "()\n";
    ts << "{\n";
    if (includeUi)
        ts << "    delete ui;\n";
    ts << "}\n";

    for (const QString &slt : slots_) {
        ts << "\n";
        ts << "void " << className << "::" << slt << "\n";
        ts << "{\n";
        ts << "    // TODO: implement\n";
        ts << "}\n";
    }

    return s;
}

QString NewQtClassWizard::buildUiForm(const QString &className,
                                      const QString &baseClass) const
{
    // Stub form — Qt Designer will accept and let user edit it further.
    QString s;
    QTextStream ts(&s);
    ts << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ts << "<ui version=\"4.0\">\n";
    ts << " <class>" << className << "</class>\n";
    ts << " <widget class=\"" << baseClass << "\" name=\"" << className << "\">\n";
    ts << "  <property name=\"geometry\">\n";
    ts << "   <rect>\n";
    ts << "    <x>0</x>\n";
    ts << "    <y>0</y>\n";
    ts << "    <width>400</width>\n";
    ts << "    <height>300</height>\n";
    ts << "   </rect>\n";
    ts << "  </property>\n";
    ts << "  <property name=\"windowTitle\">\n";
    ts << "   <string>" << className << "</string>\n";
    ts << "  </property>\n";
    ts << " </widget>\n";
    ts << " <resources/>\n";
    ts << " <connections/>\n";
    ts << "</ui>\n";
    return s;
}

QString NewQtClassWizard::findNearestCMakeLists(const QString &startDir)
{
    QDir dir(startDir);
    for (int depth = 0; depth < 16; ++depth) {
        const QString candidate = dir.absoluteFilePath(QStringLiteral("CMakeLists.txt"));
        if (QFileInfo::exists(candidate))
            return candidate;
        if (!dir.cdUp())
            break;
    }
    return {};
}

bool NewQtClassWizard::patchCMakeLists(const QString &cmakePath,
                                       const QString &headerName,
                                       const QString &sourceName,
                                       const QString &uiName) const
{
    bool ok = false;
    QString text = readTextFile(cmakePath, &ok);
    if (!ok)
        return false;

    // Compute the relative path prefix from the CMakeLists.txt directory to
    // the output folder so the listed files actually match where we wrote
    // them. If the user generated files alongside the CMakeLists.txt, the
    // prefix is empty.
    QString relPrefix;
    {
        const QFileInfo cmakeInfo(cmakePath);
        const QDir cmakeDir = cmakeInfo.absoluteDir();
        const QDir outDir(m_folderEdit->text().trimmed());
        const QString rel = cmakeDir.relativeFilePath(outDir.absolutePath());
        if (!rel.isEmpty() && rel != QLatin1String("."))
            relPrefix = rel + QLatin1Char('/');
    }
    const QString relHeader = relPrefix + headerName;
    const QString relSource = relPrefix + sourceName;
    const QString relUi     = uiName.isEmpty() ? QString() : (relPrefix + uiName);

    // Try to inject into the first add_executable(...) or add_library(...)
    // block. Multi-line, dot-all match for everything between (...) closing
    // paren.
    static const QRegularExpression targetRe(
        QStringLiteral("(add_executable|add_library)\\s*\\(([^)]*)\\)"),
        QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatch m = targetRe.match(text);
    if (m.hasMatch()) {
        const int blockStart = m.capturedStart(0);
        const int blockEnd = m.capturedEnd(0);
        QString block = text.mid(blockStart, blockEnd - blockStart);

        // Insert before the trailing ')'.
        const int closeParen = block.lastIndexOf(QLatin1Char(')'));
        if (closeParen < 0)
            return false;

        QString insertion;
        QTextStream ins(&insertion);
        ins << "\n    " << relHeader;
        ins << "\n    " << relSource;
        if (!relUi.isEmpty())
            ins << "\n    " << relUi;
        ins << "\n";

        block.insert(closeParen, insertion);
        text.replace(blockStart, blockEnd - blockStart, block);
    } else {
        // No add_executable/add_library found — append a target_sources call.
        QString tail;
        QTextStream ts(&tail);
        ts << "\n# Added by Exorcist NewQtClassWizard\n";
        ts << "if(TARGET ${PROJECT_NAME})\n";
        ts << "    target_sources(${PROJECT_NAME} PRIVATE\n";
        ts << "        " << relHeader << "\n";
        ts << "        " << relSource << "\n";
        if (!relUi.isEmpty())
            ts << "        " << relUi << "\n";
        ts << "    )\n";
        ts << "endif()\n";
        text.append(tail);
    }

    return writeTextFile(cmakePath, text);
}
