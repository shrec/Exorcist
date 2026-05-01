#include "aboutdialog.h"

#include <QApplication>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSysInfo>
#include <QVBoxLayout>
#include <QtGlobal>

// Optional generated header containing GIT_REVISION / GIT_BRANCH macros.
// Generated at configure time by CMake (see add_custom_command for git_revision.h).
// If absent, fall back to "(dev)".
#if defined(__has_include)
#  if __has_include("git_revision.h")
#    include "git_revision.h"
#  endif
#endif

#ifndef EXORCIST_GIT_REVISION
#  define EXORCIST_GIT_REVISION "(dev)"
#endif

namespace {

QString compilerVersionString()
{
#if defined(__clang__)
    return QStringLiteral("Clang %1.%2.%3")
        .arg(__clang_major__)
        .arg(__clang_minor__)
        .arg(__clang_patchlevel__);
#elif defined(__GNUC__)
    return QStringLiteral("GCC %1.%2.%3")
        .arg(__GNUC__)
        .arg(__GNUC_MINOR__)
        .arg(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return QStringLiteral("MSVC %1").arg(_MSC_VER);
#else
    return QStringLiteral("Unknown compiler");
#endif
}

QString buildTypeString()
{
#ifdef QT_DEBUG
    return QStringLiteral("Debug");
#else
    return QStringLiteral("Release");
#endif
}

QPixmap loadAppIcon()
{
    // Prefer a dedicated app icon if present in resources, fall back to play.svg.
    if (QFile::exists(QStringLiteral(":/icons/exorcist.ico")))
        return QIcon(QStringLiteral(":/icons/exorcist.ico")).pixmap(64, 64);
    if (QFile::exists(QStringLiteral(":/icons/exorcist.png")))
        return QIcon(QStringLiteral(":/icons/exorcist.png")).pixmap(64, 64);
    if (QFile::exists(QStringLiteral(":/play.svg")))
        return QIcon(QStringLiteral(":/play.svg")).pixmap(64, 64);
    return QPixmap();
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Exorcist"));
    setModal(true);
    setFixedSize(460, 440);

    // VS dark theme styling
    setStyleSheet(QStringLiteral(
        "QDialog { background-color: #1e1e1e; }"
        "QLabel { color: #d4d4d4; }"
        "QLabel#titleLabel { color: #ffffff; }"
        "QLabel#tagLabel  { color: #9cdcfe; font-style: italic; }"
        "QLabel#buildLabel, QLabel#gitLabel { color: #888888; font-size: 11px; }"
        "QLabel#linkLabel  { color: #4ec9b0; }"
        "QLabel#licenseLabel { color: #888888; font-size: 11px; }"
        "QFrame#sep { background-color: #333333; max-height: 1px; }"
        "QPushButton {"
        "  background-color: #2d2d30; color: #d4d4d4;"
        "  border: 1px solid #3f3f46; padding: 6px 18px; border-radius: 2px;"
        "}"
        "QPushButton:hover  { background-color: #3e3e42; }"
        "QPushButton:default { border: 1px solid #007acc; }"
    ));

    auto *root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(24, 24, 24, 20);

    // ── Icon + title ──────────────────────────────────────────────────
    auto *header = new QHBoxLayout();
    header->setSpacing(14);
    header->addStretch();

    const QPixmap iconPix = loadAppIcon();
    if (!iconPix.isNull()) {
        auto *iconLabel = new QLabel(this);
        iconLabel->setPixmap(iconPix);
        iconLabel->setFixedSize(64, 64);
        header->addWidget(iconLabel, 0, Qt::AlignVCenter);
    }

    auto *titleBox = new QVBoxLayout();
    titleBox->setSpacing(2);

    auto *titleLabel = new QLabel(QStringLiteral("Exorcist IDE"), this);
    titleLabel->setObjectName(QStringLiteral("titleLabel"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleBox->addWidget(titleLabel);

    const QString version = QApplication::applicationVersion().isEmpty()
        ? QStringLiteral("1.0.0")
        : QApplication::applicationVersion();
    auto *versionLabel = new QLabel(tr("Version %1").arg(version), this);
    titleBox->addWidget(versionLabel);

    header->addLayout(titleBox);
    header->addStretch();
    root->addLayout(header);

    // ── Tagline ───────────────────────────────────────────────────────
    auto *tagLabel = new QLabel(tr("Lightweight. Modular. Yours."), this);
    tagLabel->setObjectName(QStringLiteral("tagLabel"));
    tagLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(tagLabel);

    // ── Separator ─────────────────────────────────────────────────────
    auto *sep1 = new QFrame(this);
    sep1->setObjectName(QStringLiteral("sep"));
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Plain);
    root->addWidget(sep1);

    // ── Build info ────────────────────────────────────────────────────
    auto *buildLabel = new QLabel(this);
    buildLabel->setObjectName(QStringLiteral("buildLabel"));
    buildLabel->setTextFormat(Qt::RichText);
    buildLabel->setText(
        tr("<b>Qt:</b> %1<br>"
           "<b>Compiler:</b> %2<br>"
           "<b>Build:</b> %3<br>"
           "<b>Platform:</b> %4")
            .arg(QString::fromLatin1(qVersion()),
                 compilerVersionString(),
                 buildTypeString(),
                 QSysInfo::prettyProductName()));
    buildLabel->setAlignment(Qt::AlignLeft);
    root->addWidget(buildLabel);

    // ── Git revision ──────────────────────────────────────────────────
    auto *gitLabel = new QLabel(
        tr("<b>Revision:</b> %1").arg(QStringLiteral(EXORCIST_GIT_REVISION)),
        this);
    gitLabel->setObjectName(QStringLiteral("gitLabel"));
    gitLabel->setTextFormat(Qt::RichText);
    root->addWidget(gitLabel);

    // ── Separator ─────────────────────────────────────────────────────
    auto *sep2 = new QFrame(this);
    sep2->setObjectName(QStringLiteral("sep"));
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Plain);
    root->addWidget(sep2);

    // ── Credits / link ────────────────────────────────────────────────
    auto *creditsLabel = new QLabel(
        tr("Built with Qt 6 and a lot of caffeine.\n"
           "Crafted by the Exorcist contributors."),
        this);
    creditsLabel->setAlignment(Qt::AlignCenter);
    creditsLabel->setWordWrap(true);
    root->addWidget(creditsLabel);

    auto *linkLabel = new QLabel(
        QStringLiteral(
            "<a href='https://github.com/shrec/Exorcist' "
            "style='color:#4ec9b0; text-decoration:none;'>"
            "https://github.com/shrec/Exorcist</a>"),
        this);
    linkLabel->setObjectName(QStringLiteral("linkLabel"));
    linkLabel->setOpenExternalLinks(true);
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    root->addWidget(linkLabel);

    auto *licenseLabel = new QLabel(tr("MIT License"), this);
    licenseLabel->setObjectName(QStringLiteral("licenseLabel"));
    licenseLabel->setAlignment(Qt::AlignCenter);
    root->addWidget(licenseLabel);

    root->addStretch();

    // ── Close button ──────────────────────────────────────────────────
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setDefault(true);
    closeBtn->setAutoDefault(true);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    root->addLayout(btnLayout);
}
