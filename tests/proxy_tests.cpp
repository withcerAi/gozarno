// SPDX-License-Identifier: GPL-2.0-or-later
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QNetworkProxy>
#include <QElapsedTimer>
#include <QNetworkInterface>
#include <QSettings>
#include <QtConcurrent>
#include <atomic>
#include <QEventLoop>
#include "client/ProxyStack.h"
#include "client/WindowsIpInterface.h"
namespace {
quint16 word(const QByteArray& b,int n){return (quint8(b[n])<<8)|quint8(b[n+1]);}
quint32 dword(const QByteArray& b,int n){return (quint32(word(b,n))<<16)|word(b,n+2);}
void put16(QByteArray& b,int n,quint16 v){b[n]=char(v>>8);b[n+1]=char(v);}
void put32(QByteArray& b,int n,quint32 v){put16(b,n,quint16(v>>16));put16(b,n+2,quint16(v));}
quint16 checksum(const QByteArray& b){quint32 sum=0;for(int i=0;i<b.size();i+=2)sum+=(quint16(quint8(b[i]))<<8)|(i+1<b.size()?quint8(b[i+1]):0);while(sum>>16)sum=(sum&65535)+(sum>>16);return quint16(~sum);}
QByteArray ipv4Reply(const QByteArray& original,const QByteArray& payload,int proto){
    QByteArray b(20,char(0));b[0]=char(0x45);put16(b,2,quint16(20+payload.size()));b[8]=64;b[9]=char(proto);
    b.replace(12,4,original.mid(16,4));b.replace(16,4,original.mid(12,4));put16(b,10,checksum(b));return b+payload;
}
QByteArray tcpReply(const QByteArray& original,quint32 seq,quint32 ack,quint8 flags,const QByteArray& payload={}){
    int at=(quint8(original[0])&15)*4;QByteArray t(20,char(0));put16(t,0,word(original,at+2));put16(t,2,word(original,at));
    put32(t,4,seq);put32(t,8,ack);t[12]=char(0x50);t[13]=char(flags);put16(t,14,65535);t+=payload;
    QByteArray pseudo=original.mid(16,4)+original.mid(12,4)+QByteArray::fromHex("0006");pseudo.append(char(t.size()>>8));pseudo.append(char(t.size()));
    put16(t,16,checksum(pseudo+t));return ipv4Reply(original,t,6);
}
QByteArray udpReply(const QByteArray& original,const QByteArray& payload){
    int at=(quint8(original[0])&15)*4;QByteArray u(8,char(0));put16(u,0,word(original,at+2));put16(u,2,word(original,at));put16(u,4,quint16(payload.size()+8));return ipv4Reply(original,u+payload,17);
}
QByteArray networkSnapshot(){
    QStringList rows;
#ifdef _WIN32
    PMIB_IPFORWARD_TABLE2 table=nullptr;
    if(GetIpForwardTable2(AF_UNSPEC,&table)==NO_ERROR){
        for(ULONG i=0;i<table->NumEntries;++i){const auto& r=table->Table[i];
            rows.append(QString::number(r.InterfaceLuid.Value)+":"+QString::fromLatin1(reinterpret_cast<const char*>(&r.DestinationPrefix),sizeof(r.DestinationPrefix)).toUtf8().toHex()+":"+QString::number(r.Metric));}
        FreeMibTable(table);
    }
    ULONG size=16384;QByteArray buffer(int(size),Qt::Uninitialized);auto* adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
    auto status=GetAdaptersAddresses(AF_UNSPEC,0,nullptr,adapters,&size);
    if(status==ERROR_BUFFER_OVERFLOW){buffer.resize(int(size));adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());status=GetAdaptersAddresses(AF_UNSPEC,0,nullptr,adapters,&size);}
    if(status==NO_ERROR)for(auto* a=adapters;a;a=a->Next){
        rows.append(QString("adapter:%1:%2").arg(a->IfIndex).arg(a->Mtu));
        for(auto* d=a->FirstDnsServerAddress;d;d=d->Next)rows.append(QString("dns:%1:").arg(a->IfIndex)+QByteArray(d->Address.lpSockaddr?reinterpret_cast<const char*>(d->Address.lpSockaddr):"",d->Address.iSockaddrLength).toHex());
    }
#endif
#ifdef _WIN32
    QSettings proxy("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",QSettings::NativeFormat);
    for(const auto& key:{"ProxyEnable","ProxyServer","AutoConfigURL","ProxyOverride"})rows.append(QString(key)+":"+proxy.value(key).toString());
#endif
    rows.sort();return rows.join("\n").toUtf8();
}
struct Harness {
    ProxyStack stack;QList<QByteArray> packets;ProxySettings settings;
    Harness(){QTcpServer available;available.listen(QHostAddress::LocalHost,0);settings.port=available.serverPort();settings.enabled=true;}
    bool start(QString& error){return stack.start("10.77.0.2",1300,{"10.77.0.1"},settings,error);}
    void tick(){unsigned char bytes[65536];int n=stack.readPacket(bytes,sizeof(bytes));if(n>0)packets.append(QByteArray(reinterpret_cast<const char*>(bytes),n));QCoreApplication::processEvents();}
    bool wait(std::function<bool()> ready,int timeout=3000){QElapsedTimer clock;clock.start();do{tick();if(ready())return true;QTest::qWait(2);}while(clock.elapsed()<timeout);return false;}
    QByteArray take(int protocol){for(int i=0;i<packets.size();++i)if(quint8(packets[i][9])==protocol)return packets.takeAt(i);return {};}
    bool has(int protocol)const{for(const auto& p:packets)if(quint8(p[9])==protocol)return true;return false;}
    void inject(const QByteArray& packet){stack.writePacket(reinterpret_cast<const unsigned char*>(packet.constData()),int(packet.size()));}
    bool greet(QTcpSocket& client){client.setProxy(QNetworkProxy::NoProxy);client.connectToHost(QHostAddress::LocalHost,quint16(settings.port));if(!wait([&]{return client.state()==QAbstractSocket::ConnectedState;}))return false;client.write(QByteArray::fromHex("050100"));return wait([&]{return client.bytesAvailable()>=2;}) && client.readAll()==QByteArray::fromHex("0500");}
};
}
class ProxyTests:public QObject {
    Q_OBJECT
private slots:
    void proxyDispatchesSocketsOnVpnWorkerThread(){
        std::atomic<bool> ready{false},stop{false},emitted{false};std::atomic<int> port{0};
        auto future=QtConcurrent::run([&]{QEventLoop dispatcher;Harness h;QString error;if(!h.start(error)){ready=true;return;}port=h.settings.port;ready=true;
            while(!stop){h.tick();if(h.has(6))emitted=true;QThread::msleep(1);}
        });
        QElapsedTimer clock;clock.start();while(!ready && clock.elapsed()<3000)QTest::qWait(2);
        QTcpSocket client;client.setProxy(QNetworkProxy::NoProxy);client.connectToHost(QHostAddress::LocalHost,quint16(port.load()));
        bool connected=client.waitForConnected(2000);bool greeted=false;
        if(connected){client.write(QByteArray::fromHex("050100"));client.flush();greeted=client.waitForReadyRead(2000) && client.readAll()==QByteArray::fromHex("0500");}
        if(greeted){client.write(QByteArray::fromHex("05010001c63364140050"));client.flush();clock.restart();while(!emitted && clock.elapsed()<2000)QTest::qWait(2);}
        stop=true;future.waitForFinished();QVERIFY(connected);QVERIFY(greeted);QVERIFY(emitted);
    }
    void numericTcpUsesPrivatePacketsAndKeepsDirectSockets(){
        const auto before=networkSnapshot();QTcpServer direct;QVERIFY(direct.listen(QHostAddress::LocalHost,0));QTcpSocket existing;existing.setProxy(QNetworkProxy::NoProxy);existing.connectToHost(QHostAddress::LocalHost,direct.serverPort());QVERIFY(existing.waitForConnected());QVERIFY(direct.waitForNewConnection());auto* peer=direct.nextPendingConnection();
        {
            Harness h;QString error;QVERIFY2(h.start(error),qPrintable(error));QTcpSocket client;QVERIFY(h.greet(client));
            client.write(QByteArray::fromHex("05010001c63364140050"));QVERIFY(h.wait([&]{return h.has(6);}));auto syn=h.take(6);int at=(quint8(syn[0])&15)*4;QVERIFY(quint8(syn[at+13])&2);QCOMPARE(syn.mid(12,4).toHex(),QByteArray("0a4d0002"));
            quint32 sequence=dword(syn,at+4);h.inject(tcpReply(syn,50000,sequence+1,0x12));QVERIFY(h.wait([&]{return client.bytesAvailable()>=10;}));QCOMPARE(client.readAll().left(2),QByteArray::fromHex("0500"));
            h.inject(tcpReply(syn,50001,sequence+1,0x18,"PRIVATE"));QVERIFY(h.wait([&]{return client.bytesAvailable()>=7;}));QCOMPARE(client.readAll(),QByteArray("PRIVATE"));
            client.write("UPLOAD");
            QVERIFY(h.wait([&]{for(const auto& packet:h.packets)if(packet.endsWith("UPLOAD"))return true;return false;}));
            existing.write("DIRECT");QVERIFY(h.wait([&]{return peer->bytesAvailable()>=6;}));QCOMPARE(peer->readAll(),QByteArray("DIRECT"));QCOMPARE(existing.state(),QAbstractSocket::ConnectedState);
            QCOMPARE(networkSnapshot(),before);
        }
        existing.write("ALIVE");existing.flush();QVERIFY(peer->waitForReadyRead(1000) || peer->bytesAvailable());QCOMPARE(peer->readAll(),QByteArray("ALIVE"));QCOMPARE(networkSnapshot(),before);
    }
    void dnsIsAnInternalTunnelPacket(){
        Harness h;QString error;QVERIFY2(h.start(error),qPrintable(error));QTcpSocket client;QVERIFY(h.greet(client));
        QByteArray host="example.com";client.write(QByteArray::fromHex("05010003")+QByteArray(1,char(host.size()))+host+QByteArray::fromHex("0050"));
        QVERIFY(h.wait([&]{return h.has(17);}));auto request=h.take(17);int at=(quint8(request[0])&15)*4;QCOMPARE(word(request,at+2),quint16(53));QCOMPARE(request.mid(16,4).toHex(),QByteArray("0a4d0001"));QVERIFY(!h.has(6));
        auto response=request.mid(at+8);response[2]=char(0x81);response[3]=char(0x80);put16(response,6,1);response+=QByteArray::fromHex("c00c000100010000003c0004c6336414");h.inject(udpReply(request,response));
        QVERIFY(h.wait([&]{return h.has(6);}));QCOMPARE(h.take(6).mid(16,4).toHex(),QByteArray("c6336414"));
    }
    void dnsRetriesAndFallsBackOnlyInsideTunnel(){
        const auto before=networkSnapshot();Harness h;QString error;QVERIFY2(h.start(error),qPrintable(error));QTcpSocket client;QVERIFY(h.greet(client));
        QByteArray host="example.com";client.write(QByteArray::fromHex("05010003")+QByteArray(1,char(host.size()))+host+QByteArray::fromHex("0050"));
        QByteArray fallback;
        QVERIFY(h.wait([&]{for(int i=0;i<h.packets.size();++i){const auto& p=h.packets[i];if(quint8(p[9])==17 && p.mid(16,4).toHex()=="01010101"){fallback=h.packets.takeAt(i);return true;}}return false;},6000));
        int primary=0;for(const auto& p:h.packets)if(quint8(p[9])==17 && p.mid(16,4).toHex()=="0a4d0001")++primary;QVERIFY(primary>=2);
        int at=(quint8(fallback[0])&15)*4;auto response=fallback.mid(at+8);response[2]=char(0x81);response[3]=char(0x80);put16(response,6,1);response+=QByteArray::fromHex("c00c000100010000003c0004c6336414");h.inject(udpReply(fallback,response));
        QVERIFY(h.wait([&]{return h.has(6);}));QCOMPARE(h.take(6).mid(16,4).toHex(),QByteArray("c6336414"));QCOMPARE(networkSnapshot(),before);
    }
    void udpAssociateUsesPrivatePackets(){
        Harness h;QString error;QVERIFY2(h.start(error),qPrintable(error));QTcpSocket client;QVERIFY(h.greet(client));client.write(QByteArray::fromHex("05030001000000000000"));
        QVERIFY(h.wait([&]{return client.bytesAvailable()>=10;}));auto reply=client.readAll();QCOMPARE(reply.left(2),QByteArray::fromHex("0500"));quint16 port=word(reply,8);
        QUdpSocket udp;udp.setProxy(QNetworkProxy::NoProxy);QVERIFY(udp.bind(QHostAddress::LocalHost,0));udp.writeDatagram(QByteArray::fromHex("00000001c63364140035")+"PAYLOAD",QHostAddress::LocalHost,port);
        QVERIFY(h.wait([&]{return h.has(17);}));auto request=h.take(17);QCOMPARE(request.mid(28),QByteArray("PAYLOAD"));h.inject(udpReply(request,"ANSWER"));
        QVERIFY(h.wait([&]{return udp.hasPendingDatagrams();}));QCOMPARE(udp.receiveDatagram().data().mid(10),QByteArray("ANSWER"));
    }
    void authenticationAndUnavailableFamilyFailClosed(){
        Harness h;h.settings.password="separate-secret";QString error;QVERIFY2(h.start(error),qPrintable(error));QTcpSocket client;client.setProxy(QNetworkProxy::NoProxy);client.connectToHost(QHostAddress::LocalHost,quint16(h.settings.port));
        QVERIFY(h.wait([&]{return client.state()==QAbstractSocket::ConnectedState;}));client.write(QByteArray::fromHex("050100"));QVERIFY(h.wait([&]{return client.bytesAvailable()>=2;}));QCOMPARE(client.readAll(),QByteArray::fromHex("05ff"));QVERIFY(!h.has(6));QVERIFY(!h.has(17));
    }
    void authenticatedClientsCanUseOnlyPrivateStack(){
        Harness h;h.settings.password="separate-secret";QString error;QVERIFY2(h.start(error),qPrintable(error));
        QTcpSocket client;client.setProxy(QNetworkProxy::NoProxy);client.connectToHost(QHostAddress::LocalHost,quint16(h.settings.port));
        QVERIFY(h.wait([&]{return client.state()==QAbstractSocket::ConnectedState;}));client.write(QByteArray::fromHex("050102"));
        QVERIFY(h.wait([&]{return client.bytesAvailable()>=2;}));QCOMPARE(client.readAll(),QByteArray::fromHex("0502"));
        const auto user=h.settings.username.toUtf8(),pass=h.settings.password.toUtf8();
        client.write(QByteArray(1,char(1))+QByteArray(1,char(user.size()))+user+QByteArray(1,char(pass.size()))+pass);
        QVERIFY(h.wait([&]{return client.bytesAvailable()>=2;}));QCOMPARE(client.readAll(),QByteArray::fromHex("0100"));
        client.write(QByteArray::fromHex("05010001c63364140050"));QVERIFY(h.wait([&]{return h.has(6);}));QCOMPARE(h.take(6).mid(16,4).toHex(),QByteArray("c6336414"));
    }
    void wrongPasswordDoesNotCreateTunnelTraffic(){
        Harness h;h.settings.password="correct-secret";QString error;QVERIFY2(h.start(error),qPrintable(error));QTcpSocket client;
        client.setProxy(QNetworkProxy::NoProxy);client.connectToHost(QHostAddress::LocalHost,quint16(h.settings.port));QVERIFY(h.wait([&]{return client.state()==QAbstractSocket::ConnectedState;}));
        client.write(QByteArray::fromHex("050102"));QVERIFY(h.wait([&]{return client.bytesAvailable()>=2;}));client.readAll();
        client.write(QByteArray::fromHex("0106636c69656e740577726f6e67"));QVERIFY(h.wait([&]{return client.bytesAvailable()>=2;}));QCOMPARE(client.readAll(),QByteArray::fromHex("0101"));QVERIFY(h.packets.isEmpty());
    }
    void ipv6DestinationFailsWithoutDirectFallback(){
        Harness h;QString error;QVERIFY2(h.start(error),qPrintable(error));QTcpSocket client;QVERIFY(h.greet(client));
        client.write(QByteArray::fromHex("050100042001486048600000000000000000888801bb"));QVERIFY(h.wait([&]{return client.bytesAvailable()>=10;}));QCOMPARE(quint8(client.readAll()[1]),quint8(4));QVERIFY(h.packets.isEmpty());
    }
    void portIsReleasedAfterStackStops(){
        int port;{Harness h;QString error;QVERIFY2(h.start(error),qPrintable(error));port=h.settings.port;}
        QTcpServer listener;QVERIFY(listener.listen(QHostAddress::LocalHost,quint16(port)));
    }
    void httpProxyConfigurationProducesClearSocksNotice(){
        Harness h;QString error,notice;h.stack.setNoticeHandler([&](const QString& text){notice=text;});QVERIFY2(h.start(error),qPrintable(error));
        QTcpSocket client;client.setProxy(QNetworkProxy::NoProxy);client.connectToHost(QHostAddress::LocalHost,quint16(h.settings.port));QVERIFY(h.wait([&]{return client.state()==QAbstractSocket::ConnectedState;}));
        client.write("CONNECT example.com:443 HTTP/1.1\r\n\r\n");QVERIFY(h.wait([&]{return !notice.isEmpty();}));QVERIFY(notice.contains("SOCKS5"));QVERIFY(h.packets.isEmpty());QCOMPARE(h.stack.diagnostics().value("last_error").toString(),QString("not_socks5"));
    }
    void settingsRejectUnsafeLanAndPort(){ProxySettings s;QString error;s.port=0;QVERIFY(!s.validate(error));s.port=1080;s.lan=true;s.lanAddress="0.0.0.0";QVERIFY(!s.validate(error));s.password="long-enough";QVERIFY(!s.validate(error));s.lan=false;QVERIFY(s.validate(error));QVERIFY(s.permits(QHostAddress::LocalHost));QVERIFY(!s.permits(QHostAddress("192.168.1.20")));}
    void occupiedPortFailsWithoutNetworkChanges(){const auto before=networkSnapshot();Harness h;QTcpServer occupied;QVERIFY(occupied.listen(QHostAddress::LocalHost,quint16(h.settings.port)));QString error;QVERIFY(!h.start(error));QVERIFY(!error.isEmpty());QCOMPARE(networkSnapshot(),before);}
};
QTEST_GUILESS_MAIN(ProxyTests)
#include "proxy_tests.moc"
