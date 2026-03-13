#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class CitationManager : public QObject
{
    Q_OBJECT

public:
    explicit CitationManager(QObject *parent = nullptr);

    struct Citation {
        int    number;
        QString url;
        QString title;
        QString snippet;
    };

    int addCitation(const QString &url, const QString &title = {},
                    const QString &snippet = {});

    QList<Citation> citations() const { return m_citations; }

    QString toHtml() const;
    QString toMarkdown() const;
    void clear();

    static bool hasCitationMarkers(const QString &text);

private:
    QList<Citation> m_citations;
};
