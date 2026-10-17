// SPDX-License-Identifier: GPL-2.0-or-later
#include "NetworkEvents.h"
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QDateTime>
#ifdef _WIN32
#include "WindowsIpInterface.h"
#endif
namespace {
bool physical(const QNetworkInterface& iface, const QString& tunnel) {
    return iface.isValid() && iface.name()!=tunnel && iface.humanReadableName()!=tunnel
        && !iface.humanReadableName().startsWith("gozarno",Qt::CaseInsensitive)
        && iface.flags().testFlag(QNetworkInterface::IsUp)
        && iface.flags().testFlag(QNetworkInterface::IsRunning)
        && !iface.flags().testFlag(QNetworkInterface::IsLoopBack);
}
}
NetworkEvents::NetworkEvents(QObject* parent) : QObject(parent) {
    lastTick=QDateTime::currentMSecsSinceEpoch(); previous=fingerprint();
    settle.setSingleShot(true); settle.setInterval(1500);
    connect(&settle,&QTimer::timeout,this,[this] { const auto reason=pending; pending.clear(); emit environmentChanged(reason); });
    poll.setInterval(3000); connect(&poll,&QTimer::timeout,this,&NetworkEvents::check); poll.start();
    QCoreApplication::instance()->installNativeEventFilter(this);
}
NetworkEvents::~NetworkEvents() { QCoreApplication::instance()->removeNativeEventFilter(this); }
void NetworkEvents::setTunnel(const QString& name) { tunnel=name; previous=fingerprint(); }
QString NetworkEvents::fingerprint() const {
    QStringList parts;
    for(const auto& iface:QNetworkInterface::allInterfaces()) if(physical(iface,tunnel)) {
        for(const auto& address:iface.addressEntries())
            if(!address.ip().isLoopback() && !address.ip().isLinkLocal())
                parts.append(QString::number(iface.index())+":"+address.ip().toString());
    }
#ifdef _WIN32
    PMIB_IPFORWARD_TABLE2 routes=nullptr;
    if(GetIpForwardTable2(AF_UNSPEC,&routes)==NO_ERROR) {
        for(ULONG i=0;i<routes->NumEntries;++i) {
            const auto& row=routes->Table[i];
            if(row.DestinationPrefix.PrefixLength!=0 || row.Loopback
                || !physical(QNetworkInterface::interfaceFromIndex(row.InterfaceIndex),tunnel)) continue;
            const auto hop=row.NextHop.si_family==AF_INET
                ? QHostAddress(ntohl(row.NextHop.Ipv4.sin_addr.s_addr)).toString()
                : QHostAddress(reinterpret_cast<const quint8*>(&row.NextHop.Ipv6.sin6_addr)).toString();
            parts.append("route:"+QString::number(row.InterfaceIndex)+":"+hop+":"+QString::number(row.Metric));
        }
        FreeMibTable(routes);
    }
#endif
    parts.sort(); return parts.join('|'); // Kept in memory; never exported.
}
void NetworkEvents::schedule(const QString& reason) {
    if(pending!="resume") pending=reason;
    settle.start();
}
void NetworkEvents::check() {
    const auto now=QDateTime::currentMSecsSinceEpoch();
    if(now-lastTick>12000) schedule("resume");
    lastTick=now;
    const auto next=fingerprint();
    if(next!=previous) { previous=next; schedule("network_changed"); }
}
bool NetworkEvents::nativeEventFilter(const QByteArray&,void* raw,qintptr*) {
#ifdef _WIN32
    const auto* message=static_cast<MSG*>(raw);
    if(message && message->message==WM_POWERBROADCAST
        && (message->wParam==PBT_APMRESUMEAUTOMATIC || message->wParam==PBT_APMRESUMESUSPEND))
        schedule("resume");
#else
    Q_UNUSED(raw);
#endif
    return false;
}
bool NetworkEvents::physicalNetworkAvailable(const QString& tunnel) {
#ifdef _WIN32
    PMIB_IPFORWARD_TABLE2 routes=nullptr;
    if(GetIpForwardTable2(AF_UNSPEC,&routes)!=NO_ERROR) return false;
    bool found=false;
    for(ULONG i=0;i<routes->NumEntries;++i) {
        const auto& row=routes->Table[i];
        if(row.DestinationPrefix.PrefixLength==0 && !row.Loopback
            && physical(QNetworkInterface::interfaceFromIndex(row.InterfaceIndex),tunnel)) { found=true; break; }
    }
    FreeMibTable(routes); return found;
#else
    for(const auto& iface:QNetworkInterface::allInterfaces()) if(physical(iface,tunnel) && !iface.addressEntries().isEmpty()) return true;
    return false;
#endif
}
