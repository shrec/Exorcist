#include "searchservice.h"

#include <QThread>

#include "searchworker.h"

SearchService::SearchService(QObject *parent)
    : QObject(parent),
      m_thread(new QThread(this)),
      m_worker(new SearchWorker())
{
    qRegisterMetaType<SearchQuery>("SearchQuery");
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &SearchWorker::matchFound, this, &SearchService::matchFound);
    connect(m_worker, &SearchWorker::finished, this, &SearchService::searchFinished);
}

SearchService::~SearchService()
{
    cancel();
    m_thread->quit();
    m_thread->wait();
}

void SearchService::startSearch(const QString &rootPath, const SearchQuery &query)
{
    if (!m_thread->isRunning()) {
        m_thread->start();
    }
    QMetaObject::invokeMethod(m_worker, "run", Qt::QueuedConnection,
                              Q_ARG(QString, rootPath),
                              Q_ARG(SearchQuery, query));
}

void SearchService::cancel()
{
    if (m_worker) {
        m_worker->requestCancel();
    }
}
