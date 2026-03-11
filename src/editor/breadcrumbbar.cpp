#include "breadcrumbbar.h"

#include <QComboBox>
#include <QDir>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>

#include <memory>


BreadcrumbBar::BreadcrumbBar(QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(6, 2, 6, 2);
    m_layout->setSpacing(0);
    m_layout->addStretch();

    m_symbolCombo = new QComboBox(this);
    m_symbolCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_symbolCombo->setMinimumWidth(180);
    m_symbolCombo->setMaximumWidth(350);
    m_symbolCombo->setMaxVisibleItems(25);
    m_symbolCombo->setPlaceholderText(tr("Symbols…"));
    m_symbolCombo->setStyleSheet(QStringLiteral(
        "QComboBox { background: transparent; border: 1px solid rgba(255,255,255,25);"
        " border-radius: 3px; color: #cccccc; font-size: 12px; padding: 1px 6px; }"
        "QComboBox:hover { border-color: rgba(255,255,255,55); }"
        "QComboBox::drop-down { border: none; width: 14px; }"
        "QComboBox QAbstractItemView { background: #252526; color: #cccccc;"
        " selection-background-color: #094771; border: 1px solid #3c3c3c; }"));
    m_layout->addWidget(m_symbolCombo);

    connect(m_symbolCombo, &QComboBox::activated,
            this, [this](int index) {
        if (index >= 0 && index < m_symbolEntries.size()) {
            const auto &e = m_symbolEntries[index];
            emit symbolActivated(e.line, e.col);
        }
    });

    setFixedHeight(24);
    setStyleSheet(QStringLiteral(
        "BreadcrumbBar { background: rgba(255,255,255,8); }"
        "QPushButton { border:none; padding:0 4px; color:#cccccc; font-size:12px; }"
        "QPushButton:hover { color:#ffffff; text-decoration:underline; }"
        "QLabel { color:#666666; font-size:12px; padding:0 2px; }"));
}

void BreadcrumbBar::setFilePath(const QString &absPath, const QString &rootPath)
{
    clear();

    QString rel = absPath;
    if (!rootPath.isEmpty())
        rel = QDir(rootPath).relativeFilePath(absPath);

    const QStringList parts = rel.split('/', Qt::SkipEmptyParts);

    // Build cumulative paths for each segment
    QString cumulative = rootPath.isEmpty() ? QString() : rootPath;

    for (int i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            auto *sep = new QLabel(QStringLiteral("\u203A"), this);  // ›
            m_layout->insertWidget(m_layout->count() - 2, sep);
        }

        if (!cumulative.isEmpty())
            cumulative += '/';
        cumulative += parts[i];

        auto *btn = new QPushButton(parts[i], this);
        btn->setCursor(Qt::PointingHandCursor);
        const QString dirPath = cumulative;
        connect(btn, &QPushButton::clicked, this, [this, dirPath]() {
            emit segmentClicked(dirPath);
        });
        m_layout->insertWidget(m_layout->count() - 2, btn);
    }
}

void BreadcrumbBar::clear()
{
    while (m_layout->count() > 2) {  // keep stretch + symbolCombo
        auto *item = m_layout->takeAt(0);
        if (auto *w = item->widget()) {
            m_layout->removeWidget(w);
            w->deleteLater();
        }
        auto guard = std::unique_ptr<QLayoutItem>(item);
    }
}

// ── Symbol navigation ────────────────────────────────────────────────────────

void BreadcrumbBar::setSymbols(const QJsonArray &symbols)
{
    m_symbolCombo->blockSignals(true);
    m_symbolCombo->clear();
    m_symbolEntries.clear();

    flattenSymbols(symbols, QString());

    m_symbolCombo->blockSignals(false);
}

void BreadcrumbBar::clearSymbols()
{
    m_symbolCombo->blockSignals(true);
    m_symbolCombo->clear();
    m_symbolEntries.clear();
    m_symbolCombo->blockSignals(false);
}

void BreadcrumbBar::flattenSymbols(const QJsonArray &symbols,
                                   const QString &prefix)
{
    for (const QJsonValue &v : symbols) {
        const QJsonObject sym = v.toObject();
        const QString name    = sym[QLatin1String("name")].toString();
        const int kind        = sym[QLatin1String("kind")].toInt();

        QJsonObject range;
        if (sym.contains(QLatin1String("selectionRange")))
            range = sym[QLatin1String("selectionRange")].toObject();
        else if (sym.contains(QLatin1String("location")))
            range = sym[QLatin1String("location")].toObject()
                        [QLatin1String("range")].toObject();
        else
            range = sym[QLatin1String("range")].toObject();

        const int line = range[QLatin1String("start")].toObject()
                             [QLatin1String("line")].toInt();
        const int col  = range[QLatin1String("start")].toObject()
                             [QLatin1String("character")].toInt();

        const QString qualifiedName = prefix.isEmpty()
            ? name
            : prefix + QLatin1String("::") + name;

        const QString label =
            QStringLiteral("[%1] %2").arg(kindLabel(kind), qualifiedName);

        m_symbolCombo->addItem(label);
        m_symbolEntries.append({line, col});

        if (sym.contains(QLatin1String("children")))
            flattenSymbols(sym[QLatin1String("children")].toArray(),
                           qualifiedName);
    }
}

QString BreadcrumbBar::kindLabel(int kind)
{
    switch (kind) {
    case  2: return tr("mod");
    case  3: return tr("ns");
    case  5: return tr("class");
    case  6: return tr("method");
    case  9: return tr("ctor");
    case 10: return tr("enum");
    case 11: return tr("iface");
    case 12: return tr("fn");
    case 13: return tr("var");
    case 14: return tr("const");
    case 22: return tr("member");
    case 23: return tr("struct");
    default: return QStringLiteral("\u00B7");
    }
}
