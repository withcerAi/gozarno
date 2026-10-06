#include "logger.h"

#include <QDateTime>
#include <QThread>
#include <algorithm>

void Logger::addMessage(const QString& message, const MessageType& messageType, const ComponentType& componentType)
{
    Message tmp{ QDateTime::currentMSecsSinceEpoch(),
        messageType,
        componentType,
        message.left(8192),
        0,
        QThread::currentThreadId() };
    {
        QWriteLocker lock(&m_lock);
        tmp.id=++m_messageCounter;
        m_messages.push_back(tmp);
        if(m_messages.size()>4096) m_messages.pop_front();
    }
    // Receivers may query the history: never emit while holding its lock.
    emit newLogMessage(tmp);
}

QVector<Logger::Message> Logger::getMessages(int lastKnownId) const
{
    QReadLocker lock(&m_lock);

    QVector<Message> result;
    const auto first=std::upper_bound(m_messages.begin(),m_messages.end(),lastKnownId,
        [](int id,const Message& message) { return id<message.id; });
    result.reserve(std::distance(first,m_messages.end()));
    for(auto it=first;it!=m_messages.end();++it) result.append(*it);
    return result;
}

void Logger::clear()
{
    QWriteLocker lock(&m_lock);
    m_messages.clear();
}

Logger::Logger(QObject* parent)
    : QObject(parent)
    , m_messageCounter{ -1 }
    , m_lock{ QReadWriteLock::Recursive }
{
}
