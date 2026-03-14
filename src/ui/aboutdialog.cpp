#include "aboutdialog.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("About Exorcist"));
    setFixedSize(420, 300);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(12);
    layout->setContentsMargins(24, 24, 24, 24);

    auto *titleLabel = new QLabel(QStringLiteral("<h2>Exorcist IDE</h2>"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    const QString version = QApplication::applicationVersion().isEmpty()
        ? QStringLiteral("dev")
        : QApplication::applicationVersion();

    auto *versionLabel = new QLabel(
        tr("Version %1").arg(version), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    auto *descLabel = new QLabel(
        tr("A fast, lightweight, cross-platform IDE built with Qt 6.\n"
           "Visual Studio-level capabilities, minimal footprint."), this);
    descLabel->setWordWrap(true);
    descLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(descLabel);

    auto *buildLabel = new QLabel(
        tr("Built with Qt %1\nCompiler: %2\nPlatform: %3")
            .arg(QString::fromLatin1(qVersion()),
                 QStringLiteral(
#if defined(Q_CC_CLANG)
                     "Clang " QT_STRINGIFY(__clang_major__) "." QT_STRINGIFY(__clang_minor__)
#elif defined(Q_CC_GNU)
                     "GCC " QT_STRINGIFY(__GNUC__) "." QT_STRINGIFY(__GNUC_MINOR__)
#elif defined(Q_CC_MSVC)
                     "MSVC " QT_STRINGIFY(_MSC_VER)
#else
                     "Unknown"
#endif
                 ),
                 QSysInfo::prettyProductName()),
        this);
    buildLabel->setAlignment(Qt::AlignCenter);
    buildLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 11px;"));
    layout->addWidget(buildLabel);

    layout->addStretch();

    auto *licenseLabel = new QLabel(
        QStringLiteral("<a href='https://github.com/shrec/Exorcist'>GitHub</a>"
                       " · MIT License"), this);
    licenseLabel->setOpenExternalLinks(true);
    licenseLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(licenseLabel);

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *okBtn = new QPushButton(tr("OK"), this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(okBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
}
