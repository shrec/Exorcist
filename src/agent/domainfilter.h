#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>

class DomainFilter : public QObject
{
    Q_OBJECT

public:
    enum class Mode {
        AllowAll,
        DenyAll,
    };

    explicit DomainFilter(QObject *parent = nullptr);

    void setMode(Mode mode) { m_mode = mode; }
    Mode mode() const { return m_mode; }

    void allowDomain(const QString &domain);
    void denyDomain(const QString &domain);
    void removeAllow(const QString &domain);
    void removeDeny(const QString &domain);

    bool isAllowed(const QUrl &url) const;
    bool isAllowed(const QString &urlStr) const;

    QStringList allowList() const;
    QStringList denyList() const;

    void clear();

private:
    static bool isDomainInSet(const QString &host, const QSet<QString> &set);

    Mode m_mode = Mode::AllowAll;
    QSet<QString> m_allowList;
    QSet<QString> m_denyList;
};
