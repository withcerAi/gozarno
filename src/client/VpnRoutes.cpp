// SPDX-License-Identifier: GPL-2.0-or-later
#include "VpnRoutes.h"
#include "OcSettings.h"
#include "logger.h"
#include <QHostInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QNetworkInterface>
#include <functional>
#include <algorithm>
#include <climits>
#include <QMap>
#include <thread>
#include <mutex>
#include <condition_variable>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#endif

struct VpnRoutes::Impl {
    TrafficPolicy policy;
    std::thread refresher;
    std::mutex mutex;
    std::condition_variable wake;
    bool stopped = false;
    bool applied = false;
#ifdef _WIN32
    HANDLE ownership = nullptr;
    MIB_IPFORWARD_ROW2 direct4{}, direct6{};
    bool hasDirect4 = false, hasDirect6 = false;
    NET_LUID tunnel{};
    QList<MIB_IPFORWARD_ROW2> added, removed, domainRoutes;
    QStringList dns;
    QList<QHostAddress> peers;

    static SOCKADDR_INET address(const QHostAddress& value) {
        SOCKADDR_INET result{};
        if (value.protocol() == QAbstractSocket::IPv4Protocol) {
            result.Ipv4.sin_family = AF_INET;
            result.Ipv4.sin_addr.s_addr = htonl(value.toIPv4Address());
        } else {
            result.Ipv6.sin6_family = AF_INET6;
            const auto bytes = value.toIPv6Address();
            memcpy(&result.Ipv6.sin6_addr, bytes.c, 16);
        }
        return result;
    }
    static QHostAddress address(const SOCKADDR_INET& value) {
        if (value.si_family == AF_INET) return QHostAddress(ntohl(value.Ipv4.sin_addr.s_addr));
        return QHostAddress(reinterpret_cast<const quint8*>(&value.Ipv6.sin6_addr));
    }
    static QJsonObject json(const MIB_IPFORWARD_ROW2& row) {
        return {{"luid", QString::number(row.InterfaceLuid.Value)},
            {"prefix", address(row.DestinationPrefix.Prefix).toString()},
            {"length", row.DestinationPrefix.PrefixLength},
            {"hop", address(row.NextHop).toString()}, {"metric", int(row.Metric)}};
    }
    static MIB_IPFORWARD_ROW2 rowFromJson(const QJsonObject& value) {
        MIB_IPFORWARD_ROW2 row{};
        InitializeIpForwardEntry(&row);
        row.InterfaceLuid.Value = value.value("luid").toString().toULongLong();
        row.DestinationPrefix.Prefix = address(QHostAddress(value.value("prefix").toString()));
        row.DestinationPrefix.PrefixLength = value.value("length").toInt();
        row.NextHop = address(QHostAddress(value.value("hop").toString()));
        row.Metric = value.value("metric").toInt();
        row.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
        return row;
    }
    bool journal() {
        QJsonArray rows;
        for (const auto& row : added) rows.append(json(row));
        OcSettings settings;
        settings.setValue("Client/routeJournal", QJsonDocument(rows).toJson(QJsonDocument::Compact));
        settings.sync();
        return settings.status() == QSettings::NoError;
    }
    bool add(const QString& cidr, bool viaTunnel, QString& error, QList<MIB_IPFORWARD_ROW2>* domains = nullptr, ULONG metric = 0) {
        const auto subnet = QHostAddress::parseSubnet(cidr);
        if (subnet.second < 0) { error = QObject::tr("Invalid network: %1").arg(cidr); return false; }
        const bool ipv6 = subnet.first.protocol() == QAbstractSocket::IPv6Protocol;
        if (!viaTunnel && !(ipv6 ? hasDirect6 : hasDirect4)) {
            error = QObject::tr("No direct %1 route is available for %2.").arg(ipv6 ? "IPv6" : "IPv4", cidr);
            return false;
        }
        MIB_IPFORWARD_ROW2 row{};
        InitializeIpForwardEntry(&row);
        row.InterfaceLuid = viaTunnel ? tunnel : (ipv6 ? direct6.InterfaceLuid : direct4.InterfaceLuid);
        row.DestinationPrefix.Prefix = address(subnet.first);
        row.DestinationPrefix.PrefixLength = subnet.second;
        row.NextHop = viaTunnel ? address(QHostAddress(ipv6 ? "::" : "0.0.0.0")) : (ipv6 ? direct6.NextHop : direct4.NextHop);
        row.Metric = metric;
        row.Protocol = static_cast<NL_ROUTE_PROTOCOL>(MIB_IPPROTO_NETMGMT);
        // Durable intent before mutation allows cleanup after a crash at any point.
        MIB_IPFORWARD_ROW2 existingRoute = row;
        if (GetIpForwardEntry2(&existingRoute) == NO_ERROR)
            return true; // An existing route is never journaled as ours.
        added.append(row);
        if (!journal()) {
            added.removeLast();
            error = QObject::tr("Cannot save route recovery information. No route was added.");
            return false;
        }
        const ULONG status = CreateIpForwardEntry2(&row);
        if (status != NO_ERROR) {
            added.removeLast();
            journal();
            // Never claim ownership of an existing route.
            if (status == ERROR_OBJECT_ALREADY_EXISTS) {
                MIB_IPFORWARD_ROW2 existing = row;
                if (GetIpForwardEntry2(&existing) == NO_ERROR && existing.InterfaceLuid.Value == row.InterfaceLuid.Value)
                    return true;
            }
            error = QObject::tr("Cannot apply route %1 (Windows error %2).").arg(cidr).arg(status);
            return false;
        }
        if (domains) domains->append(row);
        return true;
    }
    bool resolveDomains(QString& error, bool first) {
        QList<MIB_IPFORWARD_ROW2> next;
        QMap<QString, QString> destinations;
        for (const auto& rule : policy.rules) {
            if (!rule.enabled || rule.kind != "domain") continue;
            const auto lookup = QHostInfo::fromName(rule.target);
            if (lookup.error() != QHostInfo::NoError || lookup.addresses().isEmpty()) {
                error = QObject::tr("Cannot resolve domain %1: %2").arg(rule.target, lookup.errorString());
                return false; // Preserve previous routes on a refresh failure.
            }
            for (const auto& host : lookup.addresses()) {
                if (host.isLoopback() || host.isLinkLocal() || host.isMulticast()) continue;
                if (host.protocol() == QAbstractSocket::IPv6Protocol && !hasDirect6
                    && source(true, true).isNull()) continue;
                const QString cidr = host.toString() + (host.protocol() == QAbstractSocket::IPv4Protocol ? "/32" : "/128");
                if (destinations.contains(cidr) && destinations.value(cidr) != rule.action) {
                    error = QObject::tr("Domain rules conflict on shared address %1.").arg(host.toString());
                    return false;
                }
                if (peers.contains(host) && rule.action == "vpn") {
                    error = QObject::tr("A domain rule would route the VPN gateway into its own tunnel."); return false;
                }
                destinations.insert(cidr, rule.action);
            }
        }
        for (auto it = destinations.cbegin(); it != destinations.cend(); ++it) {
            bool retained = false;
            for (const auto& previous : domainRoutes) {
                const QString cidr = address(previous.DestinationPrefix.Prefix).toString()
                    + "/" + QString::number(previous.DestinationPrefix.PrefixLength);
                if (cidr == it.key()) { next.append(previous); retained = true; break; }
            }
            if (!retained && !add(it.key(), it.value() == "vpn", error, &next)) {
                // Roll back only new domain entries; leave the old working policy intact.
                for (const auto& row : next) {
                    bool old = false;
                    for (const auto& prior : domainRoutes)
                        if (json(prior) == json(row)) old = true;
                    if (!old) { DeleteIpForwardEntry2(&row); removeOwned(row); }
                }
                journal();
                return false;
            }
        }
        if (!first) for (const auto& previous : domainRoutes) {
            bool retained = false;
            for (const auto& row : next) if (json(previous) == json(row)) retained = true;
            if (!retained) { DeleteIpForwardEntry2(&previous); removeOwned(previous); }
        }
        domainRoutes = next;
        journal();
        return true;
    }
    void removeOwned(const MIB_IPFORWARD_ROW2& row) {
        for (int i = added.size() - 1; i >= 0; --i)
            if (json(added.at(i)) == json(row)) added.removeAt(i);
    }
    QHostAddress source(bool vpn, bool ipv6) const {
        const auto luid = vpn ? tunnel : (ipv6 ? direct6.InterfaceLuid : direct4.InterfaceLuid);
        NET_IFINDEX index = 0;
        if (ConvertInterfaceLuidToIndex(&luid, &index) != NO_ERROR) return {};
        const auto iface = QNetworkInterface::interfaceFromIndex(index);
        for (const auto& entry : iface.addressEntries())
            if (entry.ip().protocol() == (ipv6 ? QAbstractSocket::IPv6Protocol : QAbstractSocket::IPv4Protocol)
                && !entry.ip().isLinkLocal() && !entry.ip().isLoopback()) return entry.ip();
        return {};
    }
#endif
};

VpnRoutes::VpnRoutes(TrafficPolicy policy) : impl(new Impl) { impl->policy = std::move(policy); }
VpnRoutes::~VpnRoutes() { restore(); }

bool VpnRoutes::prepare(const QString& gateway, QString& error)
{
    if (!impl->policy.active()) return true;
#ifdef _WIN32
    impl->ownership = CreateMutexW(nullptr, FALSE, L"Local\\GozarnoVPN_ClientRouting");
    if (!impl->ownership) { error = QObject::tr("Cannot acquire client routing ownership."); return false; }
    const DWORD acquired = WaitForSingleObject(impl->ownership, 0);
    if (acquired != WAIT_OBJECT_0 && acquired != WAIT_ABANDONED) {
        CloseHandle(impl->ownership); impl->ownership = nullptr;
        error = QObject::tr("Another client is already managing traffic rules."); return false;
    }
    OcSettings settings;
    const auto previous = QJsonDocument::fromJson(settings.value("Client/routeJournal").toByteArray()).array();
    QJsonArray pending;
    for (const auto& entry : previous) {
        const auto row = Impl::rowFromJson(entry.toObject());
        MIB_IPFORWARD_ROW2 existing = row;
        if (GetIpForwardEntry2(&existing) == NO_ERROR && existing.Metric == row.Metric
            && existing.Protocol == MIB_IPPROTO_NETMGMT) {
            const ULONG status = DeleteIpForwardEntry2(&row);
            if (status != NO_ERROR && status != ERROR_NOT_FOUND) pending.append(entry);
        }
    }
    settings.setValue("Client/routeJournal", QJsonDocument(pending).toJson());
    settings.sync();
    if (!pending.isEmpty()) { error = QObject::tr("Previous client routes could not be recovered. Run the client as administrator."); return false; }
    for (const auto family : {AF_INET, AF_INET6}) {
        PMIB_IPFORWARD_TABLE2 table = nullptr;
        if (GetIpForwardTable2(family, &table) != NO_ERROR) continue;
        ULONG best = ULONG_MAX;
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const auto& row = table->Table[i];
            if (row.DestinationPrefix.PrefixLength != 0 || row.Loopback) continue;
            MIB_IPINTERFACE_ROW iface{};
            InitializeIpInterfaceEntry(&iface);
            iface.Family = family; iface.InterfaceLuid = row.InterfaceLuid;
            if (GetIpInterfaceEntry(&iface) != NO_ERROR || !iface.Connected) continue;
            if (row.Metric + iface.Metric < best) {
                best = row.Metric + iface.Metric;
                if (family == AF_INET) { impl->direct4 = row; impl->hasDirect4 = true; }
                else { impl->direct6 = row; impl->hasDirect6 = true; }
            }
        }
        FreeMibTable(table);
    }
    const auto lookup = QHostInfo::fromName(gateway);
    impl->peers = lookup.addresses();
    if (impl->peers.isEmpty()) { error = QObject::tr("Cannot resolve the VPN gateway before applying routing rules."); return false; }
    return true;
#else
    Q_UNUSED(gateway);
    error = QObject::tr("Custom routing is currently supported on Windows only."); return false;
#endif
}

bool VpnRoutes::apply(const QString& interfaceName, const QStringList& dns, QString& error)
{
    if (!impl->policy.active()) return true;
#ifdef _WIN32
    if (impl->applied) {
        error = QObject::tr("The tunnel was reconfigured. Reconnect to safely reapply traffic rules.");
        return false;
    }
    if (ConvertInterfaceAliasToLuid(reinterpret_cast<const wchar_t*>(interfaceName.utf16()), &impl->tunnel) != NO_ERROR) {
        error = QObject::tr("Cannot identify the VPN network adapter."); return false;
    }
    impl->dns = dns;
    // Pin gateway destinations to their original physical route to avoid a routing loop.
    for (const auto& peer : impl->peers) {
        const QString cidr = peer.toString() + (peer.protocol() == QAbstractSocket::IPv4Protocol ? "/32" : "/128");
        if (!impl->add(cidr, false, error)) return false;
    }
    if (impl->policy.mode != "server") {
        PMIB_IPFORWARD_TABLE2 table = nullptr;
        if (GetIpForwardTable2(AF_UNSPEC, &table) != NO_ERROR) { error = QObject::tr("Cannot inspect tunnel routes."); return false; }
        for (ULONG i = 0; i < table->NumEntries; ++i) {
            const auto& row = table->Table[i];
            if (row.InterfaceLuid.Value != impl->tunnel.Value || row.Protocol == MIB_IPPROTO_LOCAL || row.Loopback) continue;
            const ULONG status = DeleteIpForwardEntry2(&row);
            if (status != NO_ERROR) { FreeMibTable(table); error = QObject::tr("Cannot replace server routes (Windows error %1).").arg(status); return false; }
            impl->removed.append(row);
        }
        FreeMibTable(table);
    }
    if (impl->policy.mode == "full") {
        if (!impl->add("0.0.0.0/1", true, error) || !impl->add("128.0.0.0/1", true, error)) return false;
        if (!impl->source(true, true).isNull()) {
            if (!impl->add("::/1", true, error) || !impl->add("8000::/1", true, error)) return false;
        } else if (impl->hasDirect6) {
            error = QObject::tr("Full tunnel requires VPN IPv6 support while the direct connection has IPv6. Use server routing or selected destinations.");
            return false; // Do not silently promise full coverage while IPv6 bypasses it.
        }
    }
    // The upstream script configures the server DNS; keep those resolvers reachable.
    if (impl->policy.mode == "selected") for (const auto& item : dns) {
        const QHostAddress host(item);
        if (!host.isNull() && !impl->add(host.toString() + (host.protocol() == QAbstractSocket::IPv4Protocol ? "/32" : "/128"), true, error)) return false;
    }
    // Adapter-pinned application sockets need a tunnel default route. Give it a
    // high metric so unmatched applications keep the physical default route.
    if (impl->policy.mode == "selected") for (const auto& rule : impl->policy.rules) {
        if (rule.enabled && rule.kind == "application" && rule.action == "vpn") {
            if (!impl->add("0.0.0.0/0", true, error, nullptr, 100000)) return false;
            if (!impl->source(true, true).isNull() && !impl->add("::/0", true, error, nullptr, 100000)) return false;
            break;
        }
    }
    for (const auto& rule : impl->policy.rules) {
        if (!rule.enabled || rule.kind != "network") continue;
        const auto subnet = QHostAddress::parseSubnet(rule.target);
        for (const auto& peer : impl->peers)
            if (peer.isInSubnet(subnet) && rule.action == "vpn") {
                error = QObject::tr("Rule %1 includes the VPN gateway. Use a narrower network.").arg(rule.target); return false;
            }
        if (!impl->add(rule.target, rule.action == "vpn", error)) return false;
    }
    if (!impl->resolveDomains(error, true)) return false;
    impl->applied = true;
    const bool domains = std::any_of(impl->policy.rules.cbegin(), impl->policy.rules.cend(),
        [](const TrafficRule& rule) { return rule.enabled && rule.kind == "domain"; });
    if (domains) impl->refresher = std::thread([this]() {
        std::unique_lock<std::mutex> lock(impl->mutex);
        while (!impl->wake.wait_for(lock, std::chrono::seconds(60), [this]() { return impl->stopped; })) {
            QString message;
            if (!impl->resolveDomains(message, false)) Logger::instance().addMessage(message);
        }
    });
    Logger::instance().addMessage(QObject::tr("Client traffic rules applied to %1.").arg(interfaceName));
    return true;
#else
    Q_UNUSED(interfaceName); Q_UNUSED(dns);
    error = QObject::tr("Custom routing is currently supported on Windows only."); return false;
#endif
}

void VpnRoutes::restore()
{
    { std::lock_guard<std::mutex> lock(impl->mutex); impl->stopped = true; }
    impl->wake.notify_all();
    if (impl->refresher.joinable()) impl->refresher.join();
#ifdef _WIN32
    if (!impl->ownership) return;
    QList<MIB_IPFORWARD_ROW2> pending;
    for (auto it = impl->added.crbegin(); it != impl->added.crend(); ++it) {
        const ULONG status = DeleteIpForwardEntry2(&*it);
        if (status != NO_ERROR && status != ERROR_NOT_FOUND) pending.append(*it);
    }
    impl->added = pending;
    impl->journal();
    // Upstream removes its tunnel on free; restore suspended entries only if still present.
    for (const auto& row : impl->removed) {
        MIB_IPINTERFACE_ROW iface{};
        InitializeIpInterfaceEntry(&iface);
        iface.InterfaceLuid = row.InterfaceLuid; iface.Family = row.DestinationPrefix.Prefix.si_family;
        if (GetIpInterfaceEntry(&iface) == NO_ERROR && iface.Connected) CreateIpForwardEntry2(&row);
    }
    impl->removed.clear();
    ReleaseMutex(impl->ownership); CloseHandle(impl->ownership); impl->ownership = nullptr;
#endif
}

quint32 VpnRoutes::interfaceIndex(bool tunnel, bool ipv6) const
{
#ifdef _WIN32
    const auto luid = tunnel ? impl->tunnel : (ipv6 ? impl->direct6.InterfaceLuid : impl->direct4.InterfaceLuid);
    NET_IFINDEX index = 0;
    ConvertInterfaceLuidToIndex(&luid, &index);
    return index;
#else
    Q_UNUSED(tunnel); Q_UNUSED(ipv6); return 0;
#endif
}
QHostAddress VpnRoutes::sourceAddress(bool tunnel, bool ipv6) const
{
#ifdef _WIN32
    return impl->source(tunnel, ipv6);
#else
    Q_UNUSED(tunnel); Q_UNUSED(ipv6); return {};
#endif
}
