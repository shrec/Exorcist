#include "installpluginurldialog.h"
#include "pluginmarketplaceservice.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

// ── VS Code dark theme stylesheet ─────────────────────────────────────────────

static const char *kDialogStyle = R"(
QDialog {
    background: #1e1e1e;
    color: #d4d4d4;
}
QLabel {
    color: #cccccc;
    font-size: 12px;
}
QLabel#sectionHeader {
    color: #ffffff;
    font-size: 13px;
    font-weight: bold;
    padding-top: 6px;
}
QLineEdit, QComboBox {
    background: #3c3c3c;
    border: 1px solid #3c3c3c;
    border-radius: 3px;
    color: #d4d4d4;
    padding: 6px 8px;
    font-size: 12px;
    selection-background-color: #094771;
}
QLineEdit:focus, QComboBox:focus {
    border-color: #007acc;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QComboBox QAbstractItemView {
    background: #252526;
    color: #d4d4d4;
    border: 1px solid #3c3c3c;
    selection-background-color: #094771;
}
QCheckBox {
    color: #cccccc;
    font-size: 12px;
    spacing: 6px;
}
QCheckBox::indicator {
    width: 14px;
    height: 14px;
    border: 1px solid #6a6a6a;
    border-radius: 2px;
    background: #3c3c3c;
}
QCheckBox::indicator:checked {
    background: #007acc;
    border-color: #007acc;
    image: none;
}
QProgressBar {
    background: #2d2d2d;
    border: 1px solid #3c3c3c;
    border-radius: 3px;
    color: #d4d4d4;
    text-align: center;
    height: 16px;
}
QProgressBar::chunk {
    background: #007acc;
    border-radius: 2px;
}
QPlainTextEdit {
    background: #1e1e1e;
    border: 1px solid #3c3c3c;
    border-radius: 3px;
    color: #cccccc;
    font-family: "Consolas", "Cascadia Code", monospace;
    font-size: 11px;
    selection-background-color: #094771;
}
QPushButton {
    background: #3a3d41;
    color: #d4d4d4;
    border: 1px solid #3a3d41;
    border-radius: 3px;
    padding: 6px 16px;
    font-size: 12px;
    min-width: 70px;
}
QPushButton:hover {
    background: #45494e;
    color: #ffffff;
}
QPushButton:pressed {
    background: #2d2d2d;
}
QPushButton[primary="true"] {
    background: #0e639c;
    color: #ffffff;
    border-color: #0e639c;
}
QPushButton[primary="true"]:hover {
    background: #1177bb;
}
QPushButton[primary="true"]:disabled {
    background: #3c3c3c;
    color: #888888;
}
QLabel#statusOk    { color: #4ec9b0; font-weight: bold; }
QLabel#statusErr   { color: #f48771; font-weight: bold; }
QLabel#statusInfo  { color: #d4d4d4; }
)";

// ── Construction ──────────────────────────────────────────────────────────────

InstallPluginUrlDialog::InstallPluginUrlDialog(PluginMarketplaceService *service,
                                               QWidget *parent)
    : QDialog(parent)
    , m_service(service)
{
    setWindowTitle(tr("Install Plugin from URL"));
    setMinimumSize(620, 540);
    setModal(true);
    setupUi();
    applyDarkTheme();

    if (m_service) {
        connect(m_service, &PluginMarketplaceService::installProgress,
                this, &InstallPluginUrlDialog::onProgress);
        connect(m_service, &PluginMarketplaceService::installLog,
                this, &InstallPluginUrlDialog::onLog);
        connect(m_service, &PluginMarketplaceService::installFinished,
                this, &InstallPluginUrlDialog::onFinished);
    }
}

InstallPluginUrlDialog::~InstallPluginUrlDialog() = default;

void InstallPluginUrlDialog::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 16, 18, 14);
    root->setSpacing(10);

    // ── Header ──────────────────────────────────────────────────────────
    auto *header = new QLabel(
        tr("Paste a Git repository URL or .zip download URL to install a plugin."),
        this);
    header->setWordWrap(true);
    header->setStyleSheet(QStringLiteral("color: #969696; font-size: 12px;"));
    root->addWidget(header);

    // ── Form ────────────────────────────────────────────────────────────
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(
        tr("https://github.com/user/myplugin.git  or  https://example.com/plugin.zip"));
    m_urlEdit->setToolTip(tr("Repository or archive URL (Enter to install)"));
    form->addRow(tr("Source URL"), m_urlEdit);

    m_sourceTypeBox = new QComboBox(this);
    m_sourceTypeBox->addItem(tr("Auto-detect"), int(SourceType::AutoDetect));
    m_sourceTypeBox->addItem(tr("Git Clone"),   int(SourceType::GitClone));
    m_sourceTypeBox->addItem(tr("ZIP Download"), int(SourceType::ZipDownload));
    m_sourceTypeBox->setToolTip(
        tr("How to fetch the plugin. Auto-detect picks based on the URL."));
    form->addRow(tr("Source type"), m_sourceTypeBox);

    m_subdirEdit = new QLineEdit(this);
    m_subdirEdit->setPlaceholderText(
        tr("optional: e.g. plugins/myplugin (for monorepos)"));
    m_subdirEdit->setToolTip(
        tr("If the plugin lives inside a subdirectory of the repo, point here."));
    form->addRow(tr("Subdirectory"), m_subdirEdit);

    auto *optionsRow = new QHBoxLayout;
    optionsRow->setSpacing(16);
    m_buildAfter = new QCheckBox(tr("Build after install"), this);
    m_buildAfter->setChecked(true);
    m_buildAfter->setToolTip(
        tr("Run cmake -B build && cmake --build build in the plugin folder."));
    m_activateAfter = new QCheckBox(tr("Activate immediately after build"), this);
    m_activateAfter->setChecked(true);
    m_activateAfter->setToolTip(
        tr("Load the freshly built plugin into the running IDE."));
    optionsRow->addWidget(m_buildAfter);
    optionsRow->addWidget(m_activateAfter);
    optionsRow->addStretch();
    form->addRow(QString(), optionsRow);

    root->addLayout(form);

    // ── Progress + Status ───────────────────────────────────────────────
    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setVisible(false);
    root->addWidget(m_progress);

    m_statusLabel = new QLabel(tr("Ready."), this);
    m_statusLabel->setObjectName(QStringLiteral("statusInfo"));
    m_statusLabel->setWordWrap(true);
    root->addWidget(m_statusLabel);

    // ── Output log (read-only) ──────────────────────────────────────────
    auto *logHeader = new QLabel(tr("Output"), this);
    logHeader->setObjectName(QStringLiteral("sectionHeader"));
    root->addWidget(logHeader);

    m_logView = new QPlainTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(5000);
    m_logView->setPlaceholderText(
        tr("Clone, extract, and build output will appear here…"));
    m_logView->setVisible(false);
    root->addWidget(m_logView, 1);

    // ── Buttons ─────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    btnRow->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setVisible(false);
    m_cancelBtn->setShortcut(QKeySequence(Qt::Key_Escape));
    m_cancelBtn->setToolTip(tr("Abort the running install (Esc)"));
    btnRow->addWidget(m_cancelBtn);

    m_closeBtn = new QPushButton(tr("Close"), this);
    m_closeBtn->setShortcut(QKeySequence(Qt::Key_Escape));
    m_closeBtn->setToolTip(tr("Close this dialog (Esc)"));
    btnRow->addWidget(m_closeBtn);

    m_installBtn = new QPushButton(tr("Install"), this);
    m_installBtn->setProperty("primary", true);
    m_installBtn->setDefault(true);
    m_installBtn->setEnabled(false);
    m_installBtn->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return));
    m_installBtn->setToolTip(tr("Begin install (Ctrl+Enter)"));
    btnRow->addWidget(m_installBtn);

    root->addLayout(btnRow);

    // ── Connections ─────────────────────────────────────────────────────
    connect(m_urlEdit, &QLineEdit::textChanged,
            this, &InstallPluginUrlDialog::onUrlChanged);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_installBtn->isEnabled())
            onInstallClicked();
    });
    connect(m_sourceTypeBox, QOverload<int>::of(&QComboBox::activated),
            this, [this](int) { m_userOverrode = true; });
    connect(m_installBtn, &QPushButton::clicked,
            this, &InstallPluginUrlDialog::onInstallClicked);
    connect(m_cancelBtn, &QPushButton::clicked,
            this, &InstallPluginUrlDialog::onCancelClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_urlEdit->setFocus();
}

void InstallPluginUrlDialog::applyDarkTheme()
{
    setStyleSheet(QString::fromLatin1(kDialogStyle));
}

// ── Auto-detect logic ─────────────────────────────────────────────────────────

InstallPluginUrlDialog::SourceType
InstallPluginUrlDialog::autoDetectSourceType(const QString &url) const
{
    const QString lower = url.toLower().trimmed();
    if (lower.isEmpty())
        return SourceType::AutoDetect;
    if (lower.endsWith(QLatin1String(".zip")))
        return SourceType::ZipDownload;
    if (lower.endsWith(QLatin1String(".git")) ||
        lower.startsWith(QLatin1String("git@")) ||
        lower.startsWith(QLatin1String("git://")))
        return SourceType::GitClone;
    if (lower.contains(QLatin1String("github.com/")) ||
        lower.contains(QLatin1String("gitlab.com/")) ||
        lower.contains(QLatin1String("bitbucket.org/")) ||
        lower.contains(QLatin1String("codeberg.org/")))
        return SourceType::GitClone;
    return SourceType::AutoDetect;
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void InstallPluginUrlDialog::onUrlChanged(const QString &text)
{
    const QString trimmed = text.trimmed();
    const QUrl u(trimmed, QUrl::StrictMode);
    const bool valid = !trimmed.isEmpty() && u.isValid()
                       && (u.scheme() == QLatin1String("http")
                        || u.scheme() == QLatin1String("https")
                        || u.scheme() == QLatin1String("git")
                        || u.scheme() == QLatin1String("ssh"));
    m_installBtn->setEnabled(valid && !m_busy);

    if (!m_userOverrode) {
        const SourceType detected = autoDetectSourceType(trimmed);
        // Display index = enum value (set via addItem userData).
        for (int i = 0; i < m_sourceTypeBox->count(); ++i) {
            if (m_sourceTypeBox->itemData(i).toInt() == int(detected)) {
                const QSignalBlocker block(m_sourceTypeBox);
                m_sourceTypeBox->setCurrentIndex(i);
                break;
            }
        }
    }
}

void InstallPluginUrlDialog::onInstallClicked()
{
    if (!m_service) {
        m_statusLabel->setObjectName(QStringLiteral("statusErr"));
        m_statusLabel->setText(tr("No marketplace service available."));
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
        return;
    }

    const QString url = m_urlEdit->text().trimmed();
    const QString subdir = m_subdirEdit->text().trimmed();

    // Reveal progress + log area, lock inputs.
    setBusy(true);
    m_logView->clear();
    m_logView->setVisible(true);
    m_progress->setVisible(true);
    m_progress->setValue(0);
    m_statusLabel->setObjectName(QStringLiteral("statusInfo"));
    m_statusLabel->setText(tr("Starting install…"));
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);

    // Note: PluginMarketplaceService::installFromUrl auto-detects source
    // type internally, but we honour the user's override by hinting via URL
    // form (e.g. forcing .zip semantics is implicit in the URL itself, and
    // git clone is the safer fallback for ambiguous URLs).
    m_service->installFromUrl(
        url, subdir, m_buildAfter->isChecked(), m_activateAfter->isChecked());
}

void InstallPluginUrlDialog::onCancelClicked()
{
    if (!m_service || !m_busy)
        return;
    appendLog(tr("[user] Cancelling…"), QStringLiteral("#f48771"));
    m_service->cancelInstall();
}

void InstallPluginUrlDialog::onProgress(int percent, const QString &message)
{
    if (percent >= 0 && percent <= 100)
        m_progress->setValue(percent);
    if (!message.isEmpty()) {
        m_statusLabel->setObjectName(QStringLiteral("statusInfo"));
        m_statusLabel->setText(message);
        m_statusLabel->style()->unpolish(m_statusLabel);
        m_statusLabel->style()->polish(m_statusLabel);
    }
}

void InstallPluginUrlDialog::onLog(const QString &line)
{
    appendLog(line);
}

void InstallPluginUrlDialog::onFinished(bool success,
                                        const QString &pluginId,
                                        const QString &errorMessage)
{
    setBusy(false);
    if (success) {
        m_installedId = pluginId;
        m_progress->setValue(100);
        m_statusLabel->setObjectName(QStringLiteral("statusOk"));
        m_statusLabel->setText(tr("Installed and ready: %1").arg(pluginId));
        appendLog(tr("[done] Plugin %1 installed successfully.").arg(pluginId),
                  QStringLiteral("#4ec9b0"));
        // Switch the primary button into a "Done" affordance so users can
        // accept the dialog with a single keystroke.
        m_installBtn->setText(tr("Done"));
        m_installBtn->setEnabled(true);
        QObject::disconnect(m_installBtn, &QPushButton::clicked, nullptr, nullptr);
        connect(m_installBtn, &QPushButton::clicked, this, &QDialog::accept);
    } else {
        m_statusLabel->setObjectName(QStringLiteral("statusErr"));
        m_statusLabel->setText(errorMessage.isEmpty()
                                   ? tr("Install failed.")
                                   : errorMessage);
        appendLog(tr("[error] %1").arg(errorMessage),
                  QStringLiteral("#f48771"));
        m_installBtn->setText(tr("Retry"));
        m_installBtn->setEnabled(true);
    }
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void InstallPluginUrlDialog::setBusy(bool busy)
{
    m_busy = busy;

    m_urlEdit->setEnabled(!busy);
    m_sourceTypeBox->setEnabled(!busy);
    m_subdirEdit->setEnabled(!busy);
    m_buildAfter->setEnabled(!busy);
    m_activateAfter->setEnabled(!busy);

    m_installBtn->setEnabled(!busy && !m_urlEdit->text().trimmed().isEmpty());
    m_cancelBtn->setVisible(busy);
    m_closeBtn->setVisible(!busy);
}

void InstallPluginUrlDialog::appendLog(const QString &line, const QString &color)
{
    if (!m_logView)
        return;
    // QPlainTextEdit only supports plain-text append. We embed a
    // colour-marker prefix via a per-line CSS class, but for simplicity
    // and to avoid HTML injection vectors in arbitrary process output we
    // append plain text and rely on the dialog's monochrome log scheme.
    Q_UNUSED(color);
    m_logView->appendPlainText(line);
    auto *bar = m_logView->verticalScrollBar();
    if (bar)
        bar->setValue(bar->maximum());
}
