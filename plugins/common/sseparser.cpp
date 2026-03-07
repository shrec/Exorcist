#include "sseparser.h"

SseParser::SseParser(QObject *parent)
    : QObject(parent)
{
}

void SseParser::feed(const QByteArray &chunk)
{
    m_buffer += chunk;

    while (true) {
        const int nl = m_buffer.indexOf('\n');
        if (nl < 0)
            break;

        QByteArray line = m_buffer.left(nl);
        m_buffer = m_buffer.mid(nl + 1);

        if (line.endsWith('\r'))
            line.chop(1);

        if (line.isEmpty()) {
            // Empty line dispatches the accumulated event
            if (!m_data.isEmpty()) {
                if (m_data == QLatin1String("[DONE]"))
                    emit done();
                else
                    emit eventReceived(m_eventType, m_data);
                m_eventType.clear();
                m_data.clear();
            }
            continue;
        }

        if (line.startsWith("data: ")) {
            const QString d = QString::fromUtf8(line.mid(6));
            m_data = m_data.isEmpty() ? d : (m_data + QLatin1Char('\n') + d);
        } else if (line.startsWith("event: ")) {
            m_eventType = QString::fromUtf8(line.mid(7));
        }
        // Ignore other fields (id:, retry:, comments starting with :)
    }
}

void SseParser::reset()
{
    m_buffer.clear();
    m_eventType.clear();
    m_data.clear();
}
