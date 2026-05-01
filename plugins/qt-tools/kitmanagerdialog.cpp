#include "kitmanagerdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

namespace {

constexpr auto kSettingsOrg     = "Exorcist";
constexpr auto kSettingsApp     = "Exorcist";
constexpr auto kKitsGroup       = "kits";
constexpr auto kDefaultKitKey   = "kits/default";

QString detectedDefaultBase()
{
#ifdef Q_OS_WIN
    return QStringLiteral("C:/Qt");
#else
    return QStringLiteral("/opt/Qt");
#endif
}

} // namespace

// ─── ctor / dtor ────────────────────────────────────────────────────────────

KitManagerDialog::KitManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Kit Manager"));
    setModal(true);
    resize(900, 520);

    buildUi();
    applyVsDarkTheme();
    loadKitsFromSettings();

    // First-run convenience: if no kits exist, run detection automatically.
    if (m_kits.isEmpty())
        onAutoDetect();
    else
        refreshKitList();

    if (!m_kits.isEmpty()) {
        m_kitList->setCurrentRow(0);
    } else {
        populateForm({});
    }
}

KitManagerDialog::~KitManagerDialog() = default;

// ─── UI construction ────────────────────────────────────────────────────────

void KitManagerDialog::buildUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(8);

    auto *header = new QLabel(
        tr("Manage Qt kits — pairings of a Qt version with a C++ toolchain."), this);
    header->setStyleSheet(QStringLiteral("color: #9cdcfe;"));
    root->addWidget(header);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);

    // ── Left: kit list ──
    auto *leftWrap = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(leftWrap);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    auto *kitsLabel = new QLabel(tr("Kits"), leftWrap);
    kitsLabel->setStyleSheet(QStringLiteral("color: #d4d4d4; font-weight: bold;"));
    leftLayout->addWidget(kitsLabel);

    m_kitList = new QListWidget(leftWrap);
    m_kitList->setSelectionMode(QAbstractItemView::SingleSelection);
    leftLayout->addWidget(m_kitList, 1);

    auto *leftBtnRow = new QHBoxLayout();
    m_addBtn     = new QPushButton(tr("Add Kit"), leftWrap);
    m_removeBtn  = new QPushButton(tr("Remove"), leftWrap);
    m_defaultBtn = new QPushButton(tr("Set Default"), leftWrap);
    leftBtnRow->addWidget(m_addBtn);
    leftBtnRow->addWidget(m_removeBtn);
    leftBtnRow->addWidget(m_defaultBtn);
    leftLayout->addLayout(leftBtnRow);

    auto *detectRow = new QHBoxLayout();
    m_detectBtn = new QPushButton(tr("Auto-detect"), leftWrap);
    detectRow->addWidget(m_detectBtn);
    detectRow->addStretch();
    leftLayout->addLayout(detectRow);

    splitter->addWidget(leftWrap);

    // ── Right: form ──
    auto *rightWrap = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(rightWrap);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(6);

    auto *detailsLabel = new QLabel(tr("Kit details"), rightWrap);
    detailsLabel->setStyleSheet(QStringLiteral("color: #d4d4d4; font-weight: bold;"));
    rightLayout->addWidget(detailsLabel);

    auto *form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setFormAlignment(Qt::AlignTop);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    m_nameEdit = new QLineEdit(rightWrap);
    m_nameEdit->setPlaceholderText(tr("e.g. Qt 6.7.0 MSVC2022"));
    form->addRow(tr("Name:"), m_nameEdit);

    auto makeBrowseRow = [rightWrap](QWidget *editor, const QString &btnText,
                                     QPushButton **outBtn) {
        auto *row = new QWidget(rightWrap);
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(4);
        h->addWidget(editor, 1);
        auto *btn = new QPushButton(btnText, row);
        btn->setFixedWidth(80);
        h->addWidget(btn);
        if (outBtn) *outBtn = btn;
        return row;
    };

    m_qtCombo = new QComboBox(rightWrap);
    m_qtCombo->setEditable(true);
    QPushButton *qtBrowseBtn = nullptr;
    form->addRow(tr("Qt (qmake):"), makeBrowseRow(m_qtCombo, tr("Browse..."), &qtBrowseBtn));

    m_compilerCombo = new QComboBox(rightWrap);
    m_compilerCombo->setEditable(true);
    QPushButton *compBrowseBtn = nullptr;
    form->addRow(tr("C++ compiler:"), makeBrowseRow(m_compilerCombo, tr("Browse..."), &compBrowseBtn));

    m_cmakeEdit = new QLineEdit(rightWrap);
    QPushButton *cmakeBrowseBtn = nullptr;
    form->addRow(tr("CMake:"), makeBrowseRow(m_cmakeEdit, tr("Browse..."), &cmakeBrowseBtn));

    m_debuggerEdit = new QLineEdit(rightWrap);
    QPushButton *dbgBrowseBtn = nullptr;
    form->addRow(tr("Debugger:"), makeBrowseRow(m_debuggerEdit, tr("Browse..."), &dbgBrowseBtn));

    m_sysrootEdit = new QLineEdit(rightWrap);
    QPushButton *sysBrowseBtn = nullptr;
    form->addRow(tr("Sysroot:"), makeBrowseRow(m_sysrootEdit, tr("Browse..."), &sysBrowseBtn));

    m_defaultCheck = new QCheckBox(tr("Default kit"), rightWrap);
    form->addRow(QString(), m_defaultCheck);

    rightLayout->addLayout(form);
    rightLayout->addStretch();

    m_statusLabel = new QLabel(rightWrap);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #9cdcfe; font-size: 11px;"));
    m_statusLabel->setWordWrap(true);
    rightLayout->addWidget(m_statusLabel);

    splitter->addWidget(rightWrap);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setSizes({280, 600});

    root->addWidget(splitter, 1);

    // ── Bottom: Save / Cancel ──
    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_saveBtn   = new QPushButton(tr("Save"), this);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_saveBtn->setDefault(true);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_saveBtn);
    root->addLayout(btnRow);

    // ── Wire signals ──
    connect(m_kitList,   &QListWidget::currentRowChanged, this,
            [this](int) { onKitSelectionChanged(); });
    connect(m_addBtn,    &QPushButton::clicked, this, &KitManagerDialog::onAddKit);
    connect(m_removeBtn, &QPushButton::clicked, this, &KitManagerDialog::onRemoveKit);
    connect(m_defaultBtn,&QPushButton::clicked, this, &KitManagerDialog::onSetDefault);
    connect(m_detectBtn, &QPushButton::clicked, this, &KitManagerDialog::onAutoDetect);
    connect(m_saveBtn,   &QPushButton::clicked, this, &KitManagerDialog::onSave);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    connect(qtBrowseBtn,    &QPushButton::clicked, this, &KitManagerDialog::onBrowseQt);
    connect(compBrowseBtn,  &QPushButton::clicked, this, &KitManagerDialog::onBrowseCompiler);
    connect(cmakeBrowseBtn, &QPushButton::clicked, this, &KitManagerDialog::onBrowseCMake);
    connect(dbgBrowseBtn,   &QPushButton::clicked, this, &KitManagerDialog::onBrowseDebugger);
    connect(sysBrowseBtn,   &QPushButton::clicked, this, &KitManagerDialog::onBrowseSysroot);

    // Live-update of current kit when form fields change.
    auto liveUpdate = [this]() {
        if (!m_loadingForm) commitFormToCurrentKit();
    };
    connect(m_nameEdit,     &QLineEdit::textEdited, this, [this, liveUpdate](const QString &) {
        liveUpdate();
        if (m_currentIndex >= 0 && m_currentIndex < m_kits.size()) {
            auto *item = m_kitList->item(m_currentIndex);
            if (item) item->setText(m_kits[m_currentIndex].name);
        }
    });
    connect(m_qtCombo,       &QComboBox::editTextChanged, this, liveUpdate);
    connect(m_compilerCombo, &QComboBox::editTextChanged, this, liveUpdate);
    connect(m_cmakeEdit,     &QLineEdit::textEdited,      this, liveUpdate);
    connect(m_debuggerEdit,  &QLineEdit::textEdited,      this, liveUpdate);
    connect(m_sysrootEdit,   &QLineEdit::textEdited,      this, liveUpdate);
    connect(m_defaultCheck,  &QCheckBox::toggled, this, [this](bool on) {
        if (m_loadingForm || m_currentIndex < 0 || m_currentIndex >= m_kits.size()) return;
        if (on) {
            for (int i = 0; i < m_kits.size(); ++i)
                m_kits[i].isDefault = (i == m_currentIndex);
        } else {
            m_kits[m_currentIndex].isDefault = false;
        }
        refreshKitList();
        m_kitList->setCurrentRow(m_currentIndex);
    });
}

void KitManagerDialog::applyVsDarkTheme()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #252526; }"
        "QLabel  { color: #d4d4d4; }"
        "QListWidget {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; border-radius: 2px;"
        "  outline: none;"
        "}"
        "QListWidget::item { padding: 4px 6px; }"
        "QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
        "QListWidget::item:hover    { background-color: #2a2d2e; }"
        "QLineEdit, QComboBox {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 3px 5px; border-radius: 2px;"
        "  selection-background-color: #264f78;"
        "}"
        "QLineEdit:focus, QComboBox:focus { border: 1px solid #007acc; }"
        "QComboBox QAbstractItemView {"
        "  background-color: #1e1e1e; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; selection-background-color: #094771;"
        "}"
        "QCheckBox { color: #d4d4d4; }"
        "QPushButton {"
        "  background-color: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 5px 14px; border-radius: 2px;"
        "}"
        "QPushButton:hover    { background-color: #3e3e42; }"
        "QPushButton:default  { border: 1px solid #007acc; }"
        "QPushButton:disabled { color: #6d6d6d; border-color: #2d2d30; }"
        "QSplitter::handle { background-color: #2d2d30; }"
    ));
}

// ─── Persistence ────────────────────────────────────────────────────────────

void KitManagerDialog::loadKitsFromSettings()
{
    m_kits.clear();
    QSettings s(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));
    const QString defaultId = s.value(QString::fromLatin1(kDefaultKitKey)).toString();

    s.beginGroup(QString::fromLatin1(kKitsGroup));
    const QStringList ids = s.childGroups();
    for (const QString &id : ids) {
        s.beginGroup(id);
        Kit k;
        k.id        = id;
        k.name      = s.value(QStringLiteral("name"),     id).toString();
        k.qtPath    = s.value(QStringLiteral("qtPath")).toString();
        k.compiler  = s.value(QStringLiteral("compiler")).toString();
        k.cmake     = s.value(QStringLiteral("cmake")).toString();
        k.debugger  = s.value(QStringLiteral("debugger")).toString();
        k.sysroot   = s.value(QStringLiteral("sysroot")).toString();
        k.isDefault = (id == defaultId);
        m_kits.append(k);
        s.endGroup();
    }
    s.endGroup();
}

void KitManagerDialog::saveKitsToSettings()
{
    commitFormToCurrentKit();

    QSettings s(QString::fromLatin1(kSettingsOrg), QString::fromLatin1(kSettingsApp));

    // Wipe & re-write so removed kits don't linger.
    s.remove(QString::fromLatin1(kKitsGroup));

    QString defaultId;
    s.beginGroup(QString::fromLatin1(kKitsGroup));
    for (const Kit &k : std::as_const(m_kits)) {
        s.beginGroup(k.id);
        s.setValue(QStringLiteral("name"),     k.name);
        s.setValue(QStringLiteral("qtPath"),   k.qtPath);
        s.setValue(QStringLiteral("compiler"), k.compiler);
        s.setValue(QStringLiteral("cmake"),    k.cmake);
        s.setValue(QStringLiteral("debugger"), k.debugger);
        s.setValue(QStringLiteral("sysroot"),  k.sysroot);
        s.endGroup();
        if (k.isDefault) defaultId = k.id;
    }
    s.endGroup();

    s.setValue(QString::fromLatin1(kDefaultKitKey), defaultId);
    s.sync();

    // TODO: bridge persisted kits to ToolchainManager once Kit↔Toolchain mapping
    //       is finalized. Currently this dialog only persists; nothing in the
    //       running app reads "kits/<id>/*" yet.
}

// ─── Form / list synchronization ────────────────────────────────────────────

void KitManagerDialog::refreshKitList()
{
    const int prev = m_currentIndex;
    m_kitList->blockSignals(true);
    m_kitList->clear();
    for (const Kit &k : std::as_const(m_kits)) {
        QString label = k.name.isEmpty() ? k.id : k.name;
        if (k.isDefault) label += tr("  (default)");
        m_kitList->addItem(label);
    }
    m_kitList->blockSignals(false);
    if (prev >= 0 && prev < m_kits.size())
        m_kitList->setCurrentRow(prev);
}

void KitManagerDialog::populateForm(const Kit &kit)
{
    m_loadingForm = true;
    m_nameEdit->setText(kit.name);

    m_qtCombo->clear();
    m_qtCombo->addItems(m_detectedQt);
    m_qtCombo->setCurrentText(kit.qtPath);

    m_compilerCombo->clear();
    m_compilerCombo->addItems(m_detectedCompilers);
    m_compilerCombo->setCurrentText(kit.compiler);

    m_cmakeEdit->setText(kit.cmake);
    m_debuggerEdit->setText(kit.debugger);
    m_sysrootEdit->setText(kit.sysroot);
    m_defaultCheck->setChecked(kit.isDefault);
    m_loadingForm = false;
}

void KitManagerDialog::commitFormToCurrentKit()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_kits.size()) return;
    Kit &k = m_kits[m_currentIndex];
    k.name      = m_nameEdit->text().trimmed();
    k.qtPath    = m_qtCombo->currentText().trimmed();
    k.compiler  = m_compilerCombo->currentText().trimmed();
    k.cmake     = m_cmakeEdit->text().trimmed();
    k.debugger  = m_debuggerEdit->text().trimmed();
    k.sysroot   = m_sysrootEdit->text().trimmed();
    k.isDefault = m_defaultCheck->isChecked();
}

void KitManagerDialog::onKitSelectionChanged()
{
    // Save current edits before switching.
    commitFormToCurrentKit();

    const int row = m_kitList->currentRow();
    m_currentIndex = row;
    if (row < 0 || row >= m_kits.size()) {
        populateForm({});
        return;
    }
    populateForm(m_kits[row]);
}

// ─── Slots: kit operations ──────────────────────────────────────────────────

QString KitManagerDialog::makeUniqueKitId(const QString &base) const
{
    QString slug = base.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    if (slug.isEmpty()) slug = QStringLiteral("kit");
    QString candidate = slug;
    int n = 2;
    auto exists = [this](const QString &id) {
        for (const Kit &k : m_kits)
            if (k.id == id) return true;
        return false;
    };
    while (exists(candidate)) {
        candidate = QStringLiteral("%1-%2").arg(slug).arg(n++);
    }
    return candidate;
}

void KitManagerDialog::onAddKit()
{
    commitFormToCurrentKit();
    Kit k;
    k.name = tr("New Kit");
    k.id   = makeUniqueKitId(k.name);
    if (m_kits.isEmpty()) k.isDefault = true;
    m_kits.append(k);
    m_currentIndex = m_kits.size() - 1;
    refreshKitList();
    m_kitList->setCurrentRow(m_currentIndex);
    m_nameEdit->setFocus();
    m_nameEdit->selectAll();
}

void KitManagerDialog::onRemoveKit()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_kits.size()) return;
    const QString name = m_kits[m_currentIndex].name;
    const auto reply = QMessageBox::question(
        this, tr("Remove Kit"),
        tr("Remove kit \"%1\"?").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    const bool wasDefault = m_kits[m_currentIndex].isDefault;
    m_kits.removeAt(m_currentIndex);
    if (wasDefault && !m_kits.isEmpty())
        m_kits[0].isDefault = true;
    if (m_currentIndex >= m_kits.size())
        m_currentIndex = m_kits.size() - 1;
    refreshKitList();
    if (m_currentIndex >= 0)
        m_kitList->setCurrentRow(m_currentIndex);
    else
        populateForm({});
}

void KitManagerDialog::onSetDefault()
{
    if (m_currentIndex < 0 || m_currentIndex >= m_kits.size()) return;
    for (int i = 0; i < m_kits.size(); ++i)
        m_kits[i].isDefault = (i == m_currentIndex);
    m_defaultCheck->blockSignals(true);
    m_defaultCheck->setChecked(true);
    m_defaultCheck->blockSignals(false);
    refreshKitList();
    m_kitList->setCurrentRow(m_currentIndex);
}

void KitManagerDialog::onAutoDetect()
{
    detectQtInstallations();
    detectCompilers();

    // Pre-populate kit list from cartesian-ish defaults if empty: one kit per
    // detected Qt, with the first detected compiler as a sensible default.
    if (m_kits.isEmpty() && !m_detectedQt.isEmpty()) {
        for (const QString &qmake : std::as_const(m_detectedQt)) {
            Kit k;
            // Name from path: ".../<version>/<compiler>/bin/qmake.exe"
            const QFileInfo qfi(qmake);
            const QString compilerDir = qfi.absoluteDir().absolutePath();
            const QFileInfo cdi(compilerDir);
            QString cName = cdi.fileName();        // e.g. "msvc2022_64" or "bin"
            if (cName == QLatin1String("bin")) {
                cName = cdi.dir().dirName();
            }
            const QString version = cdi.dir().dirName();
            k.name = tr("Qt %1 %2").arg(version, cName);
            k.id   = makeUniqueKitId(k.name);
            k.qtPath = qmake;
            if (!m_detectedCompilers.isEmpty())
                k.compiler = m_detectedCompilers.first();
            m_kits.append(k);
        }
        if (!m_kits.isEmpty()) m_kits.first().isDefault = true;
    }

    // Refresh combo boxes for the currently shown kit.
    Kit current;
    if (m_currentIndex >= 0 && m_currentIndex < m_kits.size())
        current = m_kits[m_currentIndex];
    else if (!m_kits.isEmpty()) {
        m_currentIndex = 0;
        current = m_kits.first();
    }

    refreshKitList();
    if (!m_kits.isEmpty())
        m_kitList->setCurrentRow(m_currentIndex >= 0 ? m_currentIndex : 0);
    else
        populateForm({});

    m_statusLabel->setText(tr("Detected %1 Qt installation(s) and %2 compiler(s).")
                               .arg(m_detectedQt.size())
                               .arg(m_detectedCompilers.size()));
}

void KitManagerDialog::onSave()
{
    saveKitsToSettings();
    accept();
}

// ─── Slots: file/path browse helpers ────────────────────────────────────────

void KitManagerDialog::onBrowseQt()
{
    const QString start = m_qtCombo->currentText().isEmpty() ? detectedDefaultBase()
                                                             : m_qtCombo->currentText();
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select qmake binary"), start,
#ifdef Q_OS_WIN
        tr("qmake (qmake.exe);;All files (*.*)"));
#else
        tr("qmake (qmake);;All files (*)"));
#endif
    if (!path.isEmpty()) {
        m_qtCombo->setCurrentText(path);
    }
}

void KitManagerDialog::onBrowseCompiler()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select C++ compiler"), m_compilerCombo->currentText(),
#ifdef Q_OS_WIN
        tr("Executables (*.exe);;All files (*.*)"));
#else
        tr("All files (*)"));
#endif
    if (!path.isEmpty()) m_compilerCombo->setCurrentText(path);
}

void KitManagerDialog::onBrowseCMake()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select CMake binary"), m_cmakeEdit->text(),
#ifdef Q_OS_WIN
        tr("Executables (*.exe);;All files (*.*)"));
#else
        tr("All files (*)"));
#endif
    if (!path.isEmpty()) m_cmakeEdit->setText(path);
}

void KitManagerDialog::onBrowseDebugger()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Select debugger binary"), m_debuggerEdit->text(),
#ifdef Q_OS_WIN
        tr("Executables (*.exe);;All files (*.*)"));
#else
        tr("All files (*)"));
#endif
    if (!path.isEmpty()) m_debuggerEdit->setText(path);
}

void KitManagerDialog::onBrowseSysroot()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, tr("Select sysroot directory"), m_sysrootEdit->text());
    if (!path.isEmpty()) m_sysrootEdit->setText(path);
}

// ─── Detection ──────────────────────────────────────────────────────────────

void KitManagerDialog::detectQtInstallations()
{
    m_detectedQt.clear();

    QStringList roots;
#ifdef Q_OS_WIN
    roots << QStringLiteral("C:/Qt")
          << QStringLiteral("D:/Qt")
          << QStringLiteral("E:/Qt");
    const QString home = QDir::homePath() + QStringLiteral("/Qt");
    if (QFileInfo::exists(home)) roots << home;
#else
    roots << QStringLiteral("/opt/Qt")
          << QStringLiteral("/usr/local/Qt")
          << QDir::homePath() + QStringLiteral("/Qt");
#endif

    static const QRegularExpression versionRe(QStringLiteral("^\\d+\\.\\d+(\\.\\d+)?$"));

#ifdef Q_OS_WIN
    const QString qmakeName = QStringLiteral("qmake.exe");
#else
    const QString qmakeName = QStringLiteral("qmake");
#endif

    for (const QString &root : std::as_const(roots)) {
        QDir rootDir(root);
        if (!rootDir.exists()) continue;

        // Iterate <root>/<version>/<compiler>/bin/qmake.
        const QStringList versionDirs = rootDir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &vd : versionDirs) {
            if (!versionRe.match(vd).hasMatch()) continue;
            QDir versionDir(rootDir.filePath(vd));
            const QStringList compilerDirs = versionDir.entryList(
                QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString &cd : compilerDirs) {
                const QString candidate =
                    versionDir.filePath(cd) + QStringLiteral("/bin/") + qmakeName;
                if (QFileInfo::exists(candidate))
                    m_detectedQt.append(QDir::toNativeSeparators(candidate));
            }
        }
    }

    m_detectedQt.removeDuplicates();
}

void KitManagerDialog::detectCompilers()
{
    m_detectedCompilers.clear();

    QStringList probe;
#ifdef Q_OS_WIN
    probe << QStringLiteral("cl") << QStringLiteral("clang") << QStringLiteral("clang++")
          << QStringLiteral("g++") << QStringLiteral("gcc")
          << QStringLiteral("mingw32-gcc") << QStringLiteral("x86_64-w64-mingw32-g++");
    const QString whereTool = QStringLiteral("where");
#else
    probe << QStringLiteral("clang") << QStringLiteral("clang++")
          << QStringLiteral("g++") << QStringLiteral("gcc");
    const QString whereTool = QStringLiteral("which");
#endif

    for (const QString &name : std::as_const(probe)) {
        QProcess p;
        p.setProgram(whereTool);
        p.setArguments(QStringList() << name);
        p.start();
        if (!p.waitForFinished(2500)) {
            p.kill();
            continue;
        }
        const QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
        const QStringList lines = out.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                            Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty() && QFileInfo::exists(trimmed))
                m_detectedCompilers.append(QDir::toNativeSeparators(trimmed));
        }
    }

    // Also probe well-known Qt-bundled MinGW location on Windows.
#ifdef Q_OS_WIN
    QStringList bundled = {
        QStringLiteral("C:/Qt/Tools/mingw1310_64/bin/g++.exe"),
        QStringLiteral("C:/Qt/Tools/mingw1120_64/bin/g++.exe"),
        QStringLiteral("C:/Qt/Tools/llvm-mingw1706_64/bin/g++.exe"),
    };
    for (const QString &p : std::as_const(bundled)) {
        if (QFileInfo::exists(p))
            m_detectedCompilers.append(QDir::toNativeSeparators(p));
    }
#endif

    m_detectedCompilers.removeDuplicates();
}
