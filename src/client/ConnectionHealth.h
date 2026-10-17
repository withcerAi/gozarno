// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QUdpSocket>
#include "RecoveryPolicy.h"
class ConnectionHealth : public QObject {
    Q_OBJECT
public:
    explicit ConnectionHealth(QObject* parent = nullptr);
    void start(const QString& interfaceName,const QStringList& dns={});
    void stop();
    void recheck();
    static QString classify(bool network,bool tunnel,bool data,bool hasDns,bool dns);
signals:
    void healthChanged(bool healthy);
    void recoveryRequested();
    void recoveryExhausted();
    void diagnosisChanged(QString code,QString message);
private slots:
    void probe();
private:
    void finish();
    void clearProbes();
    QTimer interval, deadline;
    QElapsedTimer clock;
    QList<QTcpSocket*> sockets;
    QList<QUdpSocket*> dnsSockets;
    QStringList dnsServers;
    QString interfaceName;
    RecoveryPolicy policy;
    bool running = false;
    bool networkAvailable=false, tunnelAvailable=false, dataHealthy=false, dnsHealthy=false, hasDns=false;
    bool exhaustionNotified=false;
};
