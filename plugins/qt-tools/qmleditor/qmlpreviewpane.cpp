#include "qmlpreviewpane.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickWidget>
#include <QResizeEvent>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

namespace exo::forms {

QmlPreviewPane::QmlPreviewPane(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_quick = new QQuickWidget(this);
    m_quick->setResizeMode(QQuickWidget::SizeRootObjectToView);
    // VS dark theme background — visible while QML loads / on transparent roots.
    m_quick->setClearColor(QColor(0x1e, 0x1e, 0x1e));
    m_quick->setAttribute(Qt::WA_AcceptTouchEvents, false);
    layout->addWidget(m_quick);

    // Error overlay: floats on top of QQuickWidget, hidden until needed.
    m_errorOverlay = new QLabel(this);
    m_errorOverlay->setWordWrap(true);
    m_errorOverlay->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_errorOverlay->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_errorOverlay->setMargin(8);
    m_errorOverlay->setStyleSheet(
        QStringLiteral(
            "QLabel { "
            "  background-color: rgba(40, 0, 0, 220); "
            "  color: #ff6b6b; "
            "  font-family: Consolas, 'Courier New', monospace; "
            "  font-size: 10pt; "
            "  border: 1px solid #5a1f1f; "
            "  border-radius: 2px; "
            "}"));
    m_errorOverlay->hide();
    m_errorOverlay->raise();

    // Plumb QQuickWidget status changes into our error overlay + signal.
    connect(m_quick, &QQuickWidget::statusChanged, this,
            [this](QQuickWidget::Status status) {
                if (status == QQuickWidget::Error) {
                    QStringList lines;
                    const auto errs = m_quick->errors();
                    lines.reserve(errs.size());
                    for (const QQmlError &e : errs)
                        lines << e.toString();
                    const QString joined = lines.join(QLatin1Char('\n'));
                    showError(joined);
                    emit errorMessage(joined);
                } else if (status == QQuickWidget::Ready) {
                    hideError();
                    emit previewReloaded();
                }
            });

    setMinimumSize(120, 80);
}

QmlPreviewPane::~QmlPreviewPane()
{
    // Best-effort cleanup of the temp file we own.
    if (!m_tempFilePath.isEmpty())
        QFile::remove(m_tempFilePath);
}

void QmlPreviewPane::setQmlSource(const QString &qml)
{
    m_lastQml = qml;
    if (qml.trimmed().isEmpty()) {
        clear();
        return;
    }
    writeAndLoad(qml);
}

void QmlPreviewPane::reload()
{
    if (m_lastQml.isEmpty()) {
        clear();
        return;
    }
    // Drop QML engine caches so re-loading the same path picks up new bytes
    // (QQuickWidget::setSource caches per-URL).
    m_quick->engine()->clearComponentCache();
    writeAndLoad(m_lastQml);
}

void QmlPreviewPane::clear()
{
    m_quick->setSource(QUrl());
    hideError();
}

void QmlPreviewPane::writeAndLoad(const QString &qml)
{
    if (m_tempFilePath.isEmpty()) {
        const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QDir().mkpath(tmpDir);
        // Use a stable per-instance filename so the QML engine caches sensibly,
        // and so relative imports resolve against the same parent directory.
        m_tempFilePath = QDir(tmpDir).filePath(
            QStringLiteral("exorcist_qml_preview_%1.qml")
                .arg(reinterpret_cast<quintptr>(this), 0, 16));
    }

    QFile f(m_tempFilePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QString err = tr("Failed to open temp file for QML preview: %1")
                                .arg(QDir::toNativeSeparators(m_tempFilePath));
        showError(err);
        emit errorMessage(err);
        return;
    }
    f.write(qml.toUtf8());
    f.close();

    // clearComponentCache so editing the same file path re-parses contents.
    m_quick->engine()->clearComponentCache();
    m_quick->setSource(QUrl::fromLocalFile(m_tempFilePath));
}

void QmlPreviewPane::showError(const QString &msg)
{
    m_errorOverlay->setText(msg);
    // Place overlay across the top portion of the pane.
    const int h = qMax(60, height() / 3);
    m_errorOverlay->setGeometry(0, 0, width(), h);
    m_errorOverlay->show();
    m_errorOverlay->raise();
}

void QmlPreviewPane::hideError()
{
    m_errorOverlay->hide();
}

void QmlPreviewPane::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    if (m_errorOverlay && m_errorOverlay->isVisible()) {
        const int h = qMax(60, height() / 3);
        m_errorOverlay->setGeometry(0, 0, width(), h);
    }
}

} // namespace exo::forms
