#pragma once

#include <QJsonArray>
#include <QVector>
#include <QWidget>

class QComboBox;
class QHBoxLayout;

class BreadcrumbBar : public QWidget
{
    Q_OBJECT
public:
    explicit BreadcrumbBar(QWidget *parent = nullptr);

    void setFilePath(const QString &absPath, const QString &rootPath);
    void clear();

    void setSymbols(const QJsonArray &symbols);
    void clearSymbols();

signals:
    void segmentClicked(const QString &dirPath);
    void symbolActivated(int line, int character);

private:
    struct SymbolEntry {
        int line;
        int col;
    };

    void flattenSymbols(const QJsonArray &symbols, const QString &prefix);

    static QString kindLabel(int kind);

    QHBoxLayout         *m_layout      = nullptr;
    QComboBox           *m_symbolCombo = nullptr;
    QVector<SymbolEntry> m_symbolEntries;
};
