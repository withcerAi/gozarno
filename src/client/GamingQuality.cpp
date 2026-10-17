// SPDX-License-Identifier: GPL-2.0-or-later
#include "GamingQuality.h"
#include "PinnedProbe.h"
#include <QElapsedTimer>
#include <QNetworkProxy>
#include <QTcpSocket>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#include <icmpapi.h>
#endif
namespace {
double percentile(QVector<double> values,double percentile) {
    if(values.isEmpty()) return -1;
    std::sort(values.begin(),values.end());
    if(percentile==.5 && values.size()%2==0) return (values[values.size()/2-1]+values[values.size()/2])/2.;
    return values[qBound(0,int(std::ceil(percentile*values.size()))-1,int(values.size())-1)];
}
}
QJsonObject GamingSummary::toJson() const {return {{"attempts",attempts},{"replies",replies},{"median_ms",median},{"p95_ms",p95},{"jitter_ms",jitter},{"failed_probe_percent",failedPercent},{"loaded_delta_ms",loadedDelta},{"loaded_comparison_valid",loadComparison}};}
namespace GamingQuality {
GamingSummary summarize(const QVector<GamingSample>& samples) {
    GamingSummary summary; summary.attempts=samples.size(); QVector<double> times,jitters,idle,loaded;
    double previous=-1;
    for(const auto& sample:samples) {if(sample.milliseconds<0 || !std::isfinite(sample.milliseconds)) {previous=-1; continue;}
        times.append(sample.milliseconds); (sample.loaded?loaded:idle).append(sample.milliseconds);
        if(previous>=0) jitters.append(std::abs(sample.milliseconds-previous));
        previous=sample.milliseconds;
    }
    summary.replies=times.size(); summary.median=percentile(times,.5); summary.p95=percentile(times,.95); summary.jitter=percentile(jitters,.5);
    if(summary.attempts) summary.failedPercent=100.*(summary.attempts-summary.replies)/summary.attempts;
    if(idle.size()>=8 && loaded.size()>=8) {summary.loadComparison=true; summary.loadedDelta=percentile(loaded,.5)-percentile(idle,.5);}
    return summary;
}
bool worse(const GamingSummary& before,const GamingSummary& after) {
    if(!before.comparable() || !after.comparable()) return false;
    return after.failedPercent>before.failedPercent+10 || after.median>before.median+qMax(10.,before.median*.2) || after.p95>before.p95+20;
}
GamingSample probe(const QString& tunnel,const GamingProfile& profile,unsigned packetSize) {
    GamingSample sample; const QHostAddress remote(profile.target); const auto iface=PinnedProbe::adapter(tunnel);
    if(remote.isNull() || !iface.isValid()) return sample;
    if(profile.probe=="tcp" && !packetSize) {
        QTcpSocket socket; socket.setProxy(QNetworkProxy::NoProxy); if(!PinnedProbe::bind(&socket,iface,remote)) return sample;
        QElapsedTimer timer; timer.start(); socket.connectToHost(remote,profile.port);
        if(socket.waitForConnected(800)) sample.milliseconds=timer.nsecsElapsed()/1000000.;
        return sample;
    }
#ifdef _WIN32
    QHostAddress source;
    for(const auto& entry:iface.addressEntries()) if(entry.ip().protocol()==remote.protocol() && !entry.ip().isLinkLocal() && !entry.ip().isLoopback()) {source=entry.ip(); break;}
    if(source.isNull()) return sample;
    SOCKADDR_INET from{},to{};
    const bool v6=remote.protocol()==QAbstractSocket::IPv6Protocol;
    from.si_family=to.si_family=v6?AF_INET6:AF_INET;
    if(v6) {const auto src=source.toIPv6Address(),dst=remote.toIPv6Address(); memcpy(&from.Ipv6.sin6_addr,src.c,16); memcpy(&to.Ipv6.sin6_addr,dst.c,16);}
    else {from.Ipv4.sin_addr.s_addr=htonl(source.toIPv4Address()); to.Ipv4.sin_addr.s_addr=htonl(remote.toIPv4Address());}
    // ICMP has no socket interface option. Refuse if Windows would route this
    // source/destination over another interface; never substitute a direct probe.
    MIB_IPFORWARD_ROW2 route{}; SOCKADDR_INET best{};
    if(GetBestRoute2(nullptr,0,&from,&to,0,&route,&best)!=NO_ERROR || route.InterfaceIndex!=NET_IFINDEX(iface.index())) return sample;
    if(v6 && packetSize) return sample; // No claim of UDP/IPv6 DPLPMTUD.
    const unsigned payloadSize=packetSize ? packetSize-28 : 32;
    if(payloadSize<16 || payloadSize>1472) return sample;
    QByteArray payload(int(payloadSize),char('G')); const quint64 token=QRandomGenerator::system()->generate64(); memcpy(payload.data(),&token,sizeof(token));
    IP_OPTION_INFORMATION options{}; options.Ttl=64; options.Flags=packetSize?IP_FLAG_DF:0;
    QByteArray reply(int(payloadSize)+int(sizeof(ICMP_ECHO_REPLY))+128,0);
    const HANDLE handle=v6?Icmp6CreateFile():IcmpCreateFile(); if(handle==INVALID_HANDLE_VALUE) return sample;
    DWORD count=0;
    if(v6) count=Icmp6SendEcho2(handle,nullptr,nullptr,nullptr,&from.Ipv6,&to.Ipv6,payload.data(),WORD(payload.size()),&options,reply.data(),DWORD(reply.size()),800);
    else count=IcmpSendEcho2Ex(handle,nullptr,nullptr,nullptr,from.Ipv4.sin_addr.s_addr,to.Ipv4.sin_addr.s_addr,payload.data(),WORD(payload.size()),&options,reply.data(),DWORD(reply.size()),800);
    if(count) {
        if(v6) {const auto* result=reinterpret_cast<const ICMPV6_ECHO_REPLY*>(reply.constData()); if(result->Status==IP_SUCCESS) sample.milliseconds=result->RoundTripTime;}
        else {const auto* result=reinterpret_cast<const ICMP_ECHO_REPLY*>(reply.constData());
            if(result->Status==IP_SUCCESS && result->Address==to.Ipv4.sin_addr.s_addr && result->DataSize==payload.size()
                && result->Data && memcmp(result->Data,payload.constData(),size_t(payload.size()))==0) sample.milliseconds=result->RoundTripTime;
        }
    }
    IcmpCloseHandle(handle);
#else
    Q_UNUSED(packetSize);
#endif
    return sample;
}
unsigned discoverMtu(const QString& tunnel,const GamingProfile& profile,unsigned upper,const std::atomic_bool& cancelled) {
    if(profile.probe!="icmp" || QHostAddress(profile.target).protocol()!=QAbstractSocket::IPv4Protocol || upper<1280) return 0;
    const QList<unsigned> candidates{qMin(upper,1500u),1400,1360,1320,1280}; unsigned previous=0;
    for(const auto size:candidates) {if(cancelled.load()) return 0; if(size>upper || size==previous) continue; previous=size;
        int success=0; for(int i=0;i<3 && !cancelled.load();++i) if(probe(tunnel,profile,size).milliseconds>=0) ++success;
        if(success==3 && !cancelled.load()) return size;
    }
    return 0;
}
unsigned tunnelMtu(const QString& tunnel) {
#ifdef _WIN32
    const auto iface=PinnedProbe::adapter(tunnel); MIB_IPINTERFACE_ROW row{}; InitializeIpInterfaceEntry(&row); row.InterfaceIndex=iface.index(); row.Family=AF_INET;
    if(iface.isValid() && GetIpInterfaceEntry(&row)==NO_ERROR) return row.NlMtu;
#else
    Q_UNUSED(tunnel);
#endif
    return 0;
}
quint64 uploadBytes(const QString& tunnel) {
#ifdef _WIN32
    const auto iface=PinnedProbe::adapter(tunnel); MIB_IF_ROW2 row{}; row.InterfaceIndex=iface.index();
    if(iface.isValid() && GetIfEntry2(&row)==NO_ERROR) return row.OutOctets;
#else
    Q_UNUSED(tunnel);
#endif
    return 0;
}
QString networkKey(const QString& tunnel) {
    QStringList parts; const auto vpn=PinnedProbe::adapter(tunnel);
    for(const auto& iface:QNetworkInterface::allInterfaces()) if(iface.index()!=vpn.index() && iface.flags().testFlag(QNetworkInterface::IsUp) && !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
        for(const auto& address:iface.addressEntries()) if(!address.ip().isLinkLocal()) parts.append(iface.name()+"="+address.ip().toString());
    }
    std::sort(parts.begin(),parts.end()); return QString::fromLatin1(QCryptographicHash::hash(parts.join('|').toUtf8(),QCryptographicHash::Sha256).toHex());
}
}
