#include "FileLogger.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QMessageBox>
#include <QTimer>

#include <spdlog/spdlog.h>

namespace spd = spdlog;

FileLogger::FileLogger(QObject* parent, const QString& logPath, const size_t logSize, const size_t logCount)
    : QObject(parent)
    , m_logPath{ logPath }
    , m_logSize{ logSize }
    , m_logCount{ logCount }
{

    QDir dir;
    dir.mkpath(m_logPath);
    try {
        auto sink=std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
#ifdef Q_OS_WIN
            QString("%1/%2.log").arg(m_logPath).arg(qApp->applicationName()).toStdWString(),
#else
            QString("%1/%2.log").arg(m_logPath).arg(qApp->applicationName()).toStdString(),
#endif
            logSize,
            logCount);
        m_pool=std::make_shared<spdlog::details::thread_pool>(2048,1);
        m_logger=std::make_shared<spdlog::async_logger>("gozarno-file",sink,m_pool,spdlog::async_overflow_policy::overrun_oldest);
        m_logger->set_pattern("%v");
        m_logger->flush_on(spdlog::level::warn);
        auto* flushTimer=new QTimer(this); flushTimer->setInterval(3000);
        connect(flushTimer,&QTimer::timeout,this,[this]{ m_logger->flush(); }); flushTimer->start();
    } catch (const spd::spdlog_ex& ex) {
        QMessageBox::critical(nullptr,
            tr("Log file init failed"),
            QString(ex.what()));
        throw;
    }

    connect(&Logger::instance(), &Logger::newLogMessage, this, &FileLogger::addLogMessage,Qt::DirectConnection);
}

FileLogger::~FileLogger()
{
    Logger::instance().addMessage(QString("...logging finished"));
    disconnect(&Logger::instance(),nullptr,this,nullptr);
    m_logger->flush();
    // The owned worker drains its bounded queue before the sink is destroyed.
    m_pool.reset();
}

void FileLogger::addLogMessage(const Logger::Message& message)
{
    QDateTime dt;
    dt.setMSecsSinceEpoch(message.timeStamp);

    const auto level=message.messageType==Logger::MessageType::CRITICAL ? spdlog::level::err : message.messageType==Logger::MessageType::WARNING ? spdlog::level::warn : spdlog::level::info;
    m_logger->log(level,
        "{:<24} | {:>4} | {}",
        dt.toString("yyyy-MM-dd hh:mm:ss.ms").toStdString(),
        QString::number((long long)message.threadId, 16).toStdString(),
        message.text.toStdString());
}
