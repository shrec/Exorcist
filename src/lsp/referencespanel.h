#pragma once

#include <QJsonArray>
#include <QWidget>

class QLabel;
class QListWidget;

// ── ReferencesPanel ───────────────────────────────────────────────────────────
//
// Bottom dock panel that lists textDocument/references results.
// Each row shows "filename:line:col"; double-click navigates there.

class ReferencesPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ReferencesPanel(QWidget *parent = nullptr);

    void showReferences(const QString &symbol, const QJsonArray &locations);
    void clear();

signals:
    void referenceActivated(const QString &filePath, int line, int character);

private:
    QLabel       *m_header;
    QListWidget  *m_list;

    struct Ref { QString path; int line; int col; };
    QVector<Ref>  m_refs;
};
