// SPDX-License-Identifier: GPL-2.0-or-later
#include "PublicIpLookup.h"
#include "PinnedProbe.h"
#include <QSslSocket>
#include <QHostInfo>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QElapsedTimer>
namespace PublicIpLookup {
QJsonObject parse(const QByteArray& body) {
    if(body.size()>32768) return {};
    const auto object=QJsonDocument::fromJson(body).object();
    const QHostAddress address(object.value("ip").toString());
    const QString country=object.value("country").toString();
    if(address.isNull() || address.isLoopback() || address.isLinkLocal() || address.isMulticast()
        || !QRegularExpression("^[A-Z]{2}$").match(country).hasMatch()) return {};
    QString city=object.value("city").toString().left(80);
    city.remove(QRegularExpression("[\\x00-\\x1f\\x7f]"));
    return {{"ip",address.toString()},{"country",country},{"city",city}};
}
QJsonObject fetch(const QString& tunnel,const std::shared_ptr<std::atomic_bool>& cancelled) {
    if(cancelled->load()) return {};
    QNetworkInterface iface;
    if(!tunnel.isEmpty()) iface=PinnedProbe::adapter(tunnel);
#ifdef _WIN32
    else {
        PMIB_IPFORWARD_TABLE2 routes=nullptr; ULONG metric=ULONG_MAX;
        if(GetIpForwardTable2(AF_INET,&routes)==NO_ERROR) {
            for(ULONG i=0;i<routes->NumEntries;++i) {
                const auto& row=routes->Table[i];
                const auto candidate=QNetworkInterface::interfaceFromIndex(row.InterfaceIndex);
                if(row.DestinationPrefix.PrefixLength==0 && !row.Loopback && row.Metric<metric
                    && candidate.flags().testFlag(QNetworkInterface::IsUp)) {iface=candidate; metric=row.Metric;}
            }
            FreeMibTable(routes);
        }
    }
#endif
    if(!iface.isValid()) return {};
    const auto addresses=QHostInfo::fromName("api.country.is").addresses();
    QElapsedTimer elapsed; elapsed.start();
    for(const auto& address:addresses) {
        if(cancelled->load() || elapsed.elapsed()>10000) break;
        QSslSocket socket; socket.setProxy(QNetworkProxy::NoProxy);
        socket.setProtocol(QSsl::TlsV1_2OrLater); socket.setPeerVerifyMode(QSslSocket::VerifyPeer);
        if(!PinnedProbe::bind(&socket,iface,address)) continue;
        socket.connectToHostEncrypted(address.toString(),443,"api.country.is");
        if(!socket.waitForEncrypted(4000)) continue;
        if(cancelled->load()) return {};
        socket.write("GET /?fields=city HTTP/1.1\r\nHost: api.country.is\r\nAccept: application/json\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n");
        QByteArray bytes;
        while(!cancelled->load() && bytes.size()<=40000 && elapsed.elapsed()<10000) {
            bytes+=socket.readAll();
            if(socket.state()==QAbstractSocket::UnconnectedState) break;
            socket.waitForReadyRead(500);
        }
        bytes+=socket.readAll();
        if(cancelled->load()) return {};
        const int boundary=bytes.indexOf("\r\n\r\n");
        if(boundary<0 || bytes.size()>40000) return {};
        const auto headers=bytes.left(boundary).toLower();
        if(!headers.startsWith("http/1.1 200 ") && !headers.startsWith("http/1.0 200 ")) return {};
        QByteArray body=bytes.mid(boundary+4);
        if(headers.contains("transfer-encoding: chunked")) {
            QByteArray decoded;
            for(int pos=0;;) {
                const int end=body.indexOf("\r\n",pos); if(end<0) return {};
                bool ok=false; const auto length=body.mid(pos,end-pos).split(';').first().toUInt(&ok,16);
                if(!ok || length>32768 || decoded.size()+length>32768) return {};
                if(!length) {body=decoded; break;}
                pos=end+2; if(pos+length+2>quint32(body.size()) || body.mid(pos+length,2)!="\r\n") return {};
                decoded+=body.mid(pos,length); pos+=length+2;
            }
        }
        return parse(body);
    }
    return {};
}
}
