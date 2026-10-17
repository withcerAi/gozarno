// SPDX-License-Identifier: GPL-2.0-or-later
#include "ConnectionHealth.h"
#include "OcSettings.h"
#include "PinnedProbe.h"
#include "NetworkEvents.h"
#include <QNetworkProxy>
#include <QRandomGenerator>
ConnectionHealth::ConnectionHealth(QObject* parent) : QObject(parent) {
    interval.setInterval(15000); deadline.setSingleShot(true); deadline.setInterval(5000);
    connect(&interval,&QTimer::timeout,this,&ConnectionHealth::probe);
    connect(&deadline,&QTimer::timeout,this,&ConnectionHealth::finish);
}
void ConnectionHealth::start(const QString& name,const QStringList& dns) {
    stop(); interfaceName=name; dnsServers=dns; running=true; clock.start(); interval.start();
}
void ConnectionHealth::clearProbes() {
    deadline.stop();
    const auto tcp=sockets; sockets.clear(); const auto udp=dnsSockets; dnsSockets.clear();
    for(auto* socket:tcp) { socket->disconnect(this); socket->abort(); socket->deleteLater(); }
    for(auto* socket:udp) { socket->disconnect(this); socket->abort(); socket->deleteLater(); }
}
void ConnectionHealth::stop() { running=false; interval.stop(); clearProbes(); policy.reset(); exhaustionNotified=false; }
void ConnectionHealth::recheck() {
    if(!running) return;
    clearProbes(); policy.failures=qMax(policy.failures,2); policy.nextAttempt=0;
    probe();
}
QString ConnectionHealth::classify(bool network,bool tunnel,bool data,bool hasDns,bool dns) {
    if(!network) return "network_unavailable";
    if(!tunnel) return "tunnel_unavailable";
    if(!data) return "tunnel_unreachable";
    if(hasDns && !dns) return "dns_failed";
    return "healthy";
}
void ConnectionHealth::probe() {
    if(!running || deadline.isActive()) return;
    OcSettings settings;
    if(!settings.value("Client/healthCheck",true).toBool() && policy.failures<2) return;
    networkAvailable=NetworkEvents::physicalNetworkAvailable(interfaceName);
    const auto iface=PinnedProbe::adapter(interfaceName);
    tunnelAvailable=iface.isValid() && iface.flags().testFlag(QNetworkInterface::IsUp);
    dataHealthy=dnsHealthy=hasDns=false;
    if(!networkAvailable || !tunnelAvailable) { finish(); return; }
    deadline.start();
    const auto targets=settings.value("Client/healthTargets",QStringList{"1.1.1.1","8.8.8.8"}).toStringList();
    for(const auto& target:targets) {
        const QHostAddress remote(target.trimmed());
        if(remote.isNull() || remote.isLoopback() || remote.isMulticast() || remote.isLinkLocal()) continue;
        auto* socket=new QTcpSocket(this); socket->setProxy(QNetworkProxy::NoProxy);
        if(!PinnedProbe::bind(socket,iface,remote)) { socket->deleteLater(); continue; }
        sockets.append(socket);
        connect(socket,&QTcpSocket::connected,this,[this] { dataHealthy=true; if(dnsHealthy || !hasDns) finish(); });
        socket->connectToHost(remote,443);
    }
    for(const auto& value:dnsServers) {
        const QHostAddress resolver(value.trimmed());
        if(resolver.isNull() || resolver.isLoopback() || resolver.isMulticast()) continue;
        auto* socket=new QUdpSocket(this); socket->setProxy(QNetworkProxy::NoProxy);
        if(!PinnedProbe::bind(socket,iface,resolver)) { socket->deleteLater(); continue; }
        hasDns=true; dnsSockets.append(socket);
        const auto id=quint16(QRandomGenerator::global()->generate());
        socket->connectToHost(resolver,53);
        connect(socket,&QUdpSocket::readyRead,this,[this,socket,id] {
            while(socket->hasPendingDatagrams()) {
                QByteArray response; response.resize(qMin<qint64>(socket->pendingDatagramSize(),4096));
                socket->readDatagram(response.data(),response.size());
                if(PinnedProbe::dnsAnswer(response,id)) { dnsHealthy=true; if(dataHealthy) { finish(); return; } }
            }
        });
        socket->write(PinnedProbe::dnsQuery(id));
    }
}
void ConnectionHealth::finish() {
    if(!running) return;
    clearProbes();
    const QString code=classify(networkAvailable,tunnelAvailable,dataHealthy,hasDns,dnsHealthy);
    const bool healthy=code=="healthy";
    OcSettings settings;
    const bool recover=policy.result(healthy,clock.elapsed(),settings.value("Client/autoReconnect",true).toBool());
    if(healthy) exhaustionNotified=false;
    if(healthy || policy.failures>=3) {
        QString message;
        if(code=="network_unavailable") message=tr("Local network or default route is unavailable. Waiting for the network.");
        else if(code=="tunnel_unavailable") message=tr("The VPN adapter is unavailable. Rebuilding the connection may be required.");
        else if(code=="tunnel_unreachable") message=tr("The tunnel cannot reach the check addresses. The server or internet path may be interrupted.");
        else if(code=="dns_failed") message=tr("Tunnel traffic works, but the VPN DNS check did not resolve a name.");
        else message=hasDns ? tr("Tunnel and VPN DNS checks passed.") : tr("Tunnel check passed. No usable VPN DNS resolver was supplied for testing.");
        emit healthChanged(healthy); emit diagnosisChanged(code,message);
    }
    if(recover && networkAvailable) {
        if(policy.attempts>=3 && !exhaustionNotified) { exhaustionNotified=true; emit recoveryExhausted(); }
        emit recoveryRequested();
    }
}
