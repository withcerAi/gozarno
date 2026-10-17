// SPDX-License-Identifier: GPL-2.0-or-later
#include "ProtectionValidation.h"
#include "KillSwitch.h"
#include "PinnedProbe.h"
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkProxy>
#include <QRandomGenerator>
namespace {
struct Outcome { bool success=false, denied=false; };
QNetworkInterface physical(const QString& tunnel,bool v6) {
#ifdef _WIN32
    PMIB_IPFORWARD_TABLE2 routes=nullptr; QNetworkInterface result; ULONG metric=ULONG_MAX;
    if(GetIpForwardTable2(v6 ? AF_INET6 : AF_INET,&routes)!=NO_ERROR) return {};
    const auto vpn=PinnedProbe::adapter(tunnel);
    for(ULONG i=0;i<routes->NumEntries;++i) {
        const auto& row=routes->Table[i]; const auto iface=QNetworkInterface::interfaceFromIndex(row.InterfaceIndex);
        if(row.DestinationPrefix.PrefixLength==0 && !row.Loopback && iface.index()!=vpn.index()
            && iface.isValid() && iface.flags().testFlag(QNetworkInterface::IsUp) && row.Metric<metric) {
            result=iface; metric=row.Metric;
        }
    }
    FreeMibTable(routes); return result;
#else
    Q_UNUSED(tunnel); Q_UNUSED(v6); return {};
#endif
}
Outcome tcp(const QNetworkInterface& iface,const QHostAddress& address) {
    QTcpSocket socket; socket.setProxy(QNetworkProxy::NoProxy);
    if(!PinnedProbe::bind(&socket,iface,address)) return {};
    socket.connectToHost(address,443);
    const bool success=socket.waitForConnected(1800);
    return {success,socket.error()==QAbstractSocket::SocketAccessError};
}
Outcome dns(const QNetworkInterface& iface,const QHostAddress& resolver) {
    QUdpSocket socket; socket.setProxy(QNetworkProxy::NoProxy);
    if(!PinnedProbe::bind(&socket,iface,resolver)) return {};
    const auto id=quint16(QRandomGenerator::global()->generate()); socket.connectToHost(resolver,53);
    const auto written=socket.write(PinnedProbe::dnsQuery(id));
    if(written<0) return {false,socket.error()==QAbstractSocket::SocketAccessError};
    if(!socket.waitForReadyRead(1500)) return {false,socket.error()==QAbstractSocket::SocketAccessError};
    QByteArray response; response.resize(qMin<qint64>(socket.pendingDatagramSize(),4096));
    socket.readDatagram(response.data(),response.size());
    return {PinnedProbe::dnsAnswer(response,id),false};
}
QString blockResult(const Outcome& baseline,const Outcome& protectedProbe) {
    if(protectedProbe.success) return "failed";
    if(protectedProbe.denied) return "passed";
    // Silence or an unavailable baseline cannot establish WFP enforcement.
    Q_UNUSED(baseline); return "skipped";
}
}
QJsonObject ProtectionValidation::run(const QString& tunnel,const QList<QHostAddress>& gateways,const QStringList& resolvers,
    const std::function<void()>& reconnect,const std::function<bool()>& cancelled) {
    QJsonObject results;
    const auto stopped=[&] { return cancelled && cancelled(); };
    const auto vpn=PinnedProbe::adapter(tunnel);
    const QHostAddress destination4("1.1.1.1"),destination6("2606:4700:4700::1111");
    const auto direct4=physical(tunnel,false), direct6=physical(tunnel,true);
    const auto before4=tcp(direct4,destination4);
    if(stopped()) return results;
    const auto before6=tcp(direct6,destination6);
    if(stopped()) return results;
    const auto beforeDns=dns(direct4,destination4);
    if(stopped()) return results;
    const auto beforeTunnel=tcp(vpn,destination4);
    if(stopped()) return results;
    KillSwitch protection; QString error;
    if(!protection.arm(gateways,error) || !protection.allowTunnel(tunnel,error)) {
        results.insert("temporary_filter_setup","failed");
        for(const auto& key:{"ipv4_direct_tcp","ipv6_direct_tcp","direct_dns","tunnel_tcp","interrupted_tunnel","recovery","release"}) results.insert(key,"skipped");
        return results;
    }
    results.insert("temporary_filter_setup","passed");
    // The application exception is limited to VPN peers; check targets must not
    // coincide with a peer or the direct test would exercise an intended permit.
    results.insert("ipv4_direct_tcp",gateways.contains(destination4) || !direct4.isValid() ? "skipped" : blockResult(before4,tcp(direct4,destination4)));
    if(stopped()) return results;
    results.insert("ipv6_direct_tcp",gateways.contains(destination6) || !direct6.isValid() ? "skipped" : blockResult(before6,tcp(direct6,destination6)));
    if(stopped()) return results;
    results.insert("direct_dns",gateways.contains(destination4) || !direct4.isValid() ? "skipped" : blockResult(beforeDns,dns(direct4,destination4)));
    if(stopped()) return results;
    results.insert("tunnel_tcp",!beforeTunnel.success ? "skipped" : tcp(vpn,destination4).success ? "passed" : "failed");
    if(stopped()) return results;
    if(!protection.blockTunnel(error)) results.insert("interrupted_tunnel","failed");
    else results.insert("interrupted_tunnel",blockResult(beforeTunnel,tcp(vpn,destination4)));
    if(stopped()) return results;
    if(!protection.allowTunnel(tunnel,error)) results.insert("recovery","failed");
    else {
        if(reconnect) reconnect();
        bool restored=false;
        for(int i=0;i<5 && !restored && !stopped();++i) restored=tcp(vpn,destination4).success;
        results.insert("recovery",!beforeTunnel.success ? "skipped" : restored ? "passed" : "failed");
    }
    if(stopped()) return results;
    // Test the provider's resolver through the VPN separately from physical DNS.
    if(!resolvers.isEmpty()) {
        const auto answer=dns(vpn,QHostAddress(resolvers.first()));
        results.insert("vpn_dns",answer.success ? "passed" : "skipped");
    } else results.insert("vpn_dns","skipped");
    if(!protection.release(error)) results.insert("release","failed");
    else results.insert("release",!before4.success ? "skipped" : tcp(direct4,destination4).success ? "passed" : "failed");
    return results;
}
