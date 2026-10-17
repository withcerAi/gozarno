// SPDX-License-Identifier: GPL-2.0-or-later
#include "ProxyStack.h"
#include "SocksBridge.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QEvent>
#include <QElapsedTimer>
#include <QPointer>
#include <QQueue>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QNetworkProxy>
#include <QTimer>
#include <QRandomGenerator>
#include <QUrl>
#include <QHash>
#include <functional>
#include <mutex>
#include <chrono>
extern "C" {
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/ip.h"
#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/timeouts.h"
}
extern "C" uint32_t gozarno_stack_random() {return QRandomGenerator::system()->generate();}
extern "C" u32_t sys_now() {
    using namespace std::chrono;
    return u32_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
namespace {
std::mutex stackMutex;
bool stackInitialized=false;
constexpr int BufferLimit=256*1024;
bool usable(const QHostAddress& ip) {
    return ip.protocol()==QAbstractSocket::IPv4Protocol && ip.toIPv4Address()!=0
        && ip.toIPv4Address()!=0xffffffff && !ip.isLoopback() && !ip.isMulticast() && !ip.isLinkLocal();
}
ip_addr_t stackAddress(const QHostAddress& ip) {ip_addr_t a; ip_addr_set_ip4_u32(&a,lwip_htonl(ip.toIPv4Address())); return a;}
QByteArray header(const ip_addr_t* ip,quint16 port) {
    QByteArray b(1,char(1)); quint32 value=lwip_ntohl(ip4_addr_get_u32(ip_2_ip4(ip)));
    for(int i=24;i>=0;i-=8) b.append(char(value>>i));
    b.append(char(port>>8)); b.append(char(port)); return b;
}
int word(const QByteArray& b,int at) {return (quint8(b[at])<<8)|quint8(b[at+1]);}
bool skipName(const QByteArray& b,int& at) {
    int cursor=at,end=-1,total=0;
    for(int hop=0;hop<128 && cursor<b.size();++hop) {
        int len=quint8(b[cursor]);
        if((len&0xc0)==0xc0) {
            if(cursor+1>=b.size()) return false;
            int target=((len&0x3f)<<8)|quint8(b[cursor+1]);
            if(target<12 || target>=cursor) return false;
            if(end<0) end=cursor+2;
            cursor=target; continue;
        }
        if(len&0xc0) return false;
        if(!len) {at=end<0?cursor+1:end;return true;}
        total+=len+1; if(total>255 || cursor+len>=b.size()) return false;
        cursor+=len+1;
    }
    return false;
}
class DnsQuery final:public QObject {
public:
    udp_pcb* pcb=nullptr;
    QByteArray question,query;
    int attempts=0;
    quint16 id=0;
    std::function<void(QHostAddress)> done;
    QTimer deadline;
    DnsQuery(const QString& host,const QHostAddress& resolver,std::function<void(QHostAddress)> cb,QObject* parent)
        :QObject(parent),done(std::move(cb)) {
        deadline.setSingleShot(true); deadline.setInterval(1200);
        QObject::connect(&deadline,&QTimer::timeout,this,[this] {if(attempts<2)transmit();else finish({});});
        deadline.start();
        const auto ace=QUrl::toAce(host);
        if(ace.isEmpty() || ace.size()>253) {QTimer::singleShot(0,this,[this]{finish({});});return;}
        for(const auto& label:ace.split('.')) {
            if(label.isEmpty() || label.size()>63) {QTimer::singleShot(0,this,[this]{finish({});});return;}
            question.append(char(label.size()));question+=label;
        }
        question+=QByteArray::fromHex("0000010001");
        id=quint16(QRandomGenerator::system()->generate());
        query=QByteArray::fromHex("000001000001000000000000")+question;
        query[0]=char(id>>8);query[1]=char(id);
        pcb=udp_new();
        auto address=stackAddress(resolver);
        if(!pcb || udp_connect(pcb,&address,53)!=ERR_OK) {QTimer::singleShot(0,this,[this]{finish({});});return;}
        udp_recv(pcb,[](void* arg,udp_pcb*,pbuf* p,const ip_addr_t*,u16_t) {
            auto* self=static_cast<DnsQuery*>(arg);
            QByteArray b(p->tot_len,Qt::Uninitialized);pbuf_copy_partial(p,b.data(),p->tot_len,0);pbuf_free(p);
            if(b.size()<12 || word(b,0)!=self->id || (quint8(b[2])&0xfa)!=0x80 || (quint8(b[3])&15)
                || word(b,4)!=1 || b.mid(12,self->question.size())!=self->question) return;
            int at=12+self->question.size(),count=word(b,6);
            if(count>128) return;
            for(int i=0;i<count;++i) {
                if(!skipName(b,at) || at+10>b.size()) return;
                int type=word(b,at),klass=word(b,at+2),len=word(b,at+8); at+=10;
                if(at+len>b.size()) return;
                if(type==1 && klass==1 && len==4) {
                    quint32 value=0;for(int k=0;k<4;++k)value=(value<<8)|quint8(b[at+k]);
                    QHostAddress answer(value);if(usable(answer)){self->finish(answer);return;}
                }
                at+=len;
            }
            self->finish({});
        },this);
        transmit();
    }
    void transmit() {
        if(!pcb || !done)return;
        ++attempts;
        auto* p=pbuf_alloc(PBUF_TRANSPORT,u16_t(query.size()),PBUF_RAM);
        if(p){pbuf_take(p,query.constData(),u16_t(query.size()));udp_send(pcb,p);pbuf_free(p);}
        deadline.start();
    }
    ~DnsQuery() override {if(pcb)udp_remove(pcb);}
    void finish(const QHostAddress& ip) {
        if(!done)return;
        deadline.stop();if(pcb){udp_remove(pcb);pcb=nullptr;}
        auto callback=std::move(done);done={};deleteLater();callback(ip);
    }
};
struct ProxyCounters {
    int accepted=0,requests=0,dns=0,tx=0,rx=0;
    QString lastError;
    std::function<void(QString)> notice;
    void report(const QString& code,const QString& message) {if(lastError==code)return;lastError=code;if(notice)notice(message);}
};
class ProxySession final:public QObject {
public:
    QTcpSocket* client;
    ProxySettings settings;
    ProxyCounters* counters;
    QStringList dns;
    QByteArray buffer;
    tcp_pcb* tcp=nullptr;
    QUdpSocket* relay=nullptr;
    QHash<QString,udp_pcb*> flows;
    QHash<QString,qint64> lastUsed;
    QElapsedTimer clock;
    quint16 clientUdpPort=0;
    int stage=0,lookups=0;
    QTimer deadline,reaper;
    bool dead=false;
    ProxySession(QTcpSocket* socket,const ProxySettings& config,const QStringList& resolvers,ProxyCounters* counts,QObject* parent)
        :QObject(parent),client(socket),settings(config),counters(counts),dns(resolvers) {
        client->setParent(this);client->setReadBufferSize(BufferLimit);clock.start();
        deadline.setSingleShot(true);deadline.start(15000);
        connect(&deadline,&QTimer::timeout,this,[this]{close();});
        connect(client,&QTcpSocket::disconnected,this,[this]{close();});
        connect(client,&QTcpSocket::readyRead,this,[this]{receive();});
        reaper.setInterval(30000);connect(&reaper,&QTimer::timeout,this,[this] {
            for(const auto& key:lastUsed.keys()) if(clock.elapsed()-lastUsed.value(key)>120000) {
                udp_remove(flows.take(key));lastUsed.remove(key);
            }
        });reaper.start();
        if(client->bytesAvailable())QTimer::singleShot(0,this,[this]{receive();});
    }
    ~ProxySession() override {
        if(tcp){tcp_arg(tcp,nullptr);tcp_err(tcp,nullptr);tcp_recv(tcp,nullptr);tcp_sent(tcp,nullptr);tcp_abort(tcp);}
        for(auto* pcb:flows)udp_remove(pcb);
    }
    void close() {if(dead)return;dead=true;client->abort();deleteLater();}
    void fail(char code) {
        if(dead)return;
        dead=true;
        QByteArray reply=QByteArray::fromHex("05000001000000000000");reply[1]=code;
        client->write(reply);client->disconnectFromHost();deadline.start(1000);
        // disconnected schedules deletion even though dead is already set.
        connect(client,&QTcpSocket::disconnected,this,&QObject::deleteLater);
        QTimer::singleShot(1000,this,&QObject::deleteLater);
    }
    void resolve(const QString& host,std::function<void(QHostAddress)> cb) {
        QHostAddress literal(host);
        if(!literal.isNull()){cb(literal);return;}
        if(lookups>=8 || dns.isEmpty()){cb({});return;}
        ++lookups;
        QStringList candidates=dns.mid(0,2);
        if(!candidates.contains("1.1.1.1"))candidates.append("1.1.1.1");
        else if(candidates.size()<3 && !candidates.contains("8.8.8.8"))candidates.append("8.8.8.8");
        resolveNext(host,candidates,0,std::move(cb));
    }
    void resolveNext(const QString& host,const QStringList& candidates,int index,std::function<void(QHostAddress)> cb) {
        if(index>=candidates.size()){--lookups;cb({});return;}
        ++counters->dns;QPointer<ProxySession> guard(this);
        new DnsQuery(host,QHostAddress(candidates[index]),[guard,host,candidates,index,cb](QHostAddress ip) {
            if(!guard || guard->dead)return;
            if(!ip.isNull()){--guard->lookups;cb(ip);}
            else guard->resolveNext(host,candidates,index+1,cb);
        },this);
    }
    void receive() {
        if(dead)return;
        if(stage==4){pump();return;}
        if(stage==5){if(client->bytesAvailable()>0)client->readAll();return;}
        if(stage==3)return;
        buffer+=client->readAll();if(buffer.size()>65536){close();return;}
        if(stage==0) {
            if(buffer.size()<2)return;
            int n=quint8(buffer[1]);if(quint8(buffer[0])!=5 || !n){
                counters->report("not_socks5",tr("This port uses SOCKS5. Select SOCKS5, not HTTP or HTTPS, in your app."));close();return;
            }
            if(buffer.size()<2+n)return;
            char method=settings.password.isEmpty()?0:2;
            if(!buffer.mid(2,n).contains(method)){
                counters->report("auth_method",settings.password.isEmpty()?tr("No proxy password is configured. Leave proxy username and password empty in your app."):tr("This SOCKS5 proxy requires its separate proxy username and password."));
                client->write(QByteArray::fromHex("05ff"));client->disconnectFromHost();return;}
            buffer.remove(0,2+n);client->write(QByteArray(1,char(5))+QByteArray(1,method));stage=method?1:2;
        }
        if(stage==1) {
            if(buffer.size()<2)return;
            int u=quint8(buffer[1]);
            if(quint8(buffer[0])!=1 || !u){close();return;}
            if(buffer.size()<3+u)return;
            int p=quint8(buffer[2+u]);
            if(buffer.size()<3+u+p)return;
            const auto provided=buffer.mid(3+u,p),expected=settings.password.toUtf8();
            unsigned mismatch=unsigned(provided.size()^expected.size());
            for(int i=0;i<expected.size();++i)mismatch|=quint8(expected[i])^quint8(i<provided.size()?provided[i]:0);
            if(buffer.mid(2,u)!=settings.username.toUtf8() || mismatch || !p){counters->report("authentication_failed",tr("The SOCKS5 proxy username or password is incorrect."));client->write(QByteArray::fromHex("0101"));client->disconnectFromHost();return;}
            buffer.remove(0,3+u+p);client->write(QByteArray::fromHex("0100"));stage=2;
        }
        if(stage==2) {
            if(buffer.size()<4)return;
            if(quint8(buffer[0])!=5 || buffer[2]){fail(1);return;}
            const int command=quint8(buffer[1]);QString host;quint16 port;
            const int used=readSocksTarget(buffer,3,host,port,command==3);
            if(!used)return;
            if(used<0){fail(8);return;}
            buffer.remove(0,3+used);stage=3;++counters->requests;
            if(command==1)resolve(host,[this,port](QHostAddress address){connectRemote(address,port);});
            else if(command==3)startUdp();else fail(7);
        }
    }
    void pump() {
        if(dead || !tcp || stage!=4)return;
        while(!buffer.isEmpty() || client->bytesAvailable()) {
            const int capacity=tcp_sndbuf(tcp);if(!capacity)break;
            if(buffer.isEmpty())buffer=client->read(qMin(capacity,16384));
            const int n=qMin(capacity,int(buffer.size()));if(!n)break;
            if(tcp_write(tcp,buffer.constData(),u16_t(n),TCP_WRITE_FLAG_COPY)!=ERR_OK)break;
            buffer.remove(0,n);
        }
        tcp_output(tcp);
    }
    void connectRemote(const QHostAddress& destination,quint16 port) {
        if(!usable(destination)){
            counters->report("destination_unavailable",destination.protocol()==QAbstractSocket::IPv6Protocol?tr("This SOCKS tunnel supports IPv4. Enable remote DNS through SOCKS5 in your app."):tr("The SOCKS destination could not be resolved through the tunnel."));
            fail(4);return;
        }
        tcp=tcp_new();if(!tcp){fail(1);return;}
        tcp_arg(tcp,this);
        tcp_err(tcp,[](void* arg,err_t){auto* s=static_cast<ProxySession*>(arg);if(s){s->tcp=nullptr;if(s->stage!=4)s->fail(5);else s->close();}});
        tcp_sent(tcp,[](void* arg,tcp_pcb*,u16_t)->err_t{static_cast<ProxySession*>(arg)->pump();return ERR_OK;});
        tcp_recv(tcp,[](void* arg,tcp_pcb* pcb,pbuf* p,err_t)->err_t {
            auto* s=static_cast<ProxySession*>(arg);
            if(!p){s->client->disconnectFromHost();return ERR_OK;}
            if(s->dead || s->client->bytesToWrite()+p->tot_len>BufferLimit)return ERR_MEM;
            QByteArray b(p->tot_len,Qt::Uninitialized);pbuf_copy_partial(p,b.data(),p->tot_len,0);
            if(s->client->write(b)!=b.size())return ERR_MEM;
            tcp_recved(pcb,p->tot_len);pbuf_free(p);return ERR_OK;
        });
        auto address=stackAddress(destination);
        if(tcp_connect(tcp,&address,port,[](void* arg,tcp_pcb* pcb,err_t error)->err_t {
            auto* s=static_cast<ProxySession*>(arg);if(error!=ERR_OK){s->fail(5);return ERR_OK;}
            s->client->write(QByteArray::fromHex("050000")+header(&pcb->local_ip,pcb->local_port));
            s->deadline.stop();s->stage=4;s->pump();return ERR_OK;
        })!=ERR_OK)fail(3);
    }
    void startUdp() {
        relay=new QUdpSocket(this);relay->setProxy(QNetworkProxy::NoProxy);
        if(!relay->bind(client->localAddress(),0)){fail(1);return;}
        auto address=stackAddress(client->localAddress());
        client->write(QByteArray::fromHex("050000")+header(&address,relay->localPort()));stage=5;deadline.stop();
        connect(relay,&QUdpSocket::readyRead,this,[this] {
            int budget=32;
            while(budget-- && relay->hasPendingDatagrams()) {
                const auto packet=relay->receiveDatagram(65536);
                if(packet.senderAddress()!=client->peerAddress() || !settings.permits(packet.senderAddress()))continue;
                if(clientUdpPort && clientUdpPort!=packet.senderPort())continue;
                auto b=packet.data();if(b.size()<4 || b[0] || b[1] || b[2])continue;
                QString host;quint16 port;int used=readSocksTarget(b,3,host,port);if(used<=0)continue;
                clientUdpPort=packet.senderPort();const auto payload=b.mid(3+used);
                if(payload.size()>65507)continue;
                resolve(host,[this,port,payload](QHostAddress destination) {
                    if(!usable(destination))return;
                    const QString key=destination.toString()+":"+QString::number(port);
                    auto* pcb=flows.value(key);
                    if(!pcb) {
                        if(flows.size()>=64)return;
                        pcb=udp_new();if(!pcb)return;
                        auto address=stackAddress(destination);
                        if(udp_connect(pcb,&address,port)!=ERR_OK){udp_remove(pcb);return;}
                        flows.insert(key,pcb);
                        udp_recv(pcb,[](void* arg,udp_pcb* flow,pbuf* p,const ip_addr_t* remote,u16_t remotePort) {
                            auto* s=static_cast<ProxySession*>(arg);
                            if(!s->dead && s->relay && s->clientUdpPort) {
                                QByteArray b(p->tot_len,Qt::Uninitialized);pbuf_copy_partial(p,b.data(),p->tot_len,0);
                                s->relay->writeDatagram(QByteArray(3,char(0))+header(remote,remotePort)+b,s->client->peerAddress(),s->clientUdpPort);
                                for(auto it=s->flows.cbegin();it!=s->flows.cend();++it)if(it.value()==flow)s->lastUsed[it.key()]=s->clock.elapsed();
                            }
                            pbuf_free(p);
                        },this);
                    }
                    lastUsed[key]=clock.elapsed();auto* p=pbuf_alloc(PBUF_TRANSPORT,u16_t(payload.size()),PBUF_RAM);if(!p)return;
                    pbuf_take(p,payload.constData(),u16_t(payload.size()));udp_send(pcb,p);pbuf_free(p);
                });
            }
        });
    }
};
}
struct ProxyStack::Impl:public QObject {
    std::unique_lock<std::mutex> lock;
    netif iface{};
    bool attached=false;
    ProxyCounters counters;
    QTcpServer local,lan;
    QQueue<QByteArray> packets;
    ProxySettings settings;
    QStringList dns;
    QString addresses;
    QElapsedTimer events,lanCheck;
    Impl():lock(stackMutex,std::defer_lock) {local.setProxy(QNetworkProxy::NoProxy);lan.setProxy(QNetworkProxy::NoProxy);}
    ~Impl() override {
        local.close();lan.close();
        const auto childrenCopy=children();for(auto* child:childrenCopy)delete child;
        if(attached){netif_set_down(&iface);netif_remove(&iface);}
    }
    void accept(QTcpServer& server) {
        int budget=16;
        while(budget-- && server.hasPendingConnections()) {
            auto* socket=server.nextPendingConnection();
            if(!settings.permits(socket->peerAddress()) || children().size()>=128){socket->abort();socket->deleteLater();continue;}
            ++counters.accepted;
            new ProxySession(socket,settings,dns,&counters,this);
        }
    }
};
ProxyStack::ProxyStack():impl(new Impl){}
ProxyStack::~ProxyStack()=default;
bool ProxyStack::start(const QString& address,int mtu,const QStringList& resolvers,const ProxySettings& settings,QString& error) {
    if(impl->attached || (!impl->lock.owns_lock() && !impl->lock.try_lock())){error=QObject::tr("Another proxy session is running.");return false;}
    if(!settings.validate(error))return false;
    QHostAddress ip(address.section('/',0,0));
    if(!usable(ip) || mtu<576 || mtu>65535){error=QObject::tr("SOCKS mode requires a VPN IPv4 address and a valid tunnel MTU.");return false;}
    if(!stackInitialized){lwip_init();stackInitialized=true;}
    impl->settings=settings;
    for(const auto& item:resolvers)if(usable(QHostAddress(item)) && !impl->dns.contains(item))impl->dns.append(item);
    if(impl->dns.isEmpty())impl->dns.append("1.1.1.1");
    ip4_addr_t local,mask,gw;ip4_addr_set_u32(&local,lwip_htonl(ip.toIPv4Address()));ip4_addr_set_u32(&mask,0xffffffff);ip4_addr_set_zero(&gw);
    impl->iface.mtu=u16_t(mtu);
    if(!netif_add(&impl->iface,&local,&mask,&gw,impl.get(),[](netif* n)->err_t {
        n->name[0]='s';n->name[1]='x';
        n->output=[](netif* n,pbuf* p,const ip4_addr_t*)->err_t {
            auto* self=static_cast<Impl*>(n->state);
            if(self->packets.size()>=512)return ERR_MEM;
            if(p->tot_len>n->mtu)return ERR_IF;
            QByteArray b(p->tot_len,Qt::Uninitialized);pbuf_copy_partial(p,b.data(),p->tot_len,0);self->packets.enqueue(b);++self->counters.tx;return ERR_OK;
        };return ERR_OK;
    },ip_input)){error=QObject::tr("Could not initialize the private proxy stack.");return false;}
    impl->iface.mtu=u16_t(mtu);
    impl->attached=true;netif_set_default(&impl->iface);netif_set_up(&impl->iface);netif_set_link_up(&impl->iface);
    QObject::connect(&impl->local,&QTcpServer::newConnection,impl.get(),[this]{impl->accept(impl->local);});
    QObject::connect(&impl->lan,&QTcpServer::newConnection,impl.get(),[this]{impl->accept(impl->lan);});
    if(!impl->local.listen(QHostAddress::LocalHost,quint16(settings.port))){error=impl->local.errorString();return false;}
    impl->addresses=QString("127.0.0.1:%1").arg(settings.port);
    if(settings.lan) {
        if(!impl->lan.listen(QHostAddress(settings.lanAddress),quint16(settings.port))){error=impl->lan.errorString();return false;}
        impl->addresses+="  |  "+settings.lanAddress+":"+QString::number(settings.port);
    }
    impl->events.start();impl->lanCheck.start();return true;
}
int ProxyStack::readPacket(unsigned char* data,int capacity) {
    if(!impl->attached)return -1;
    if(impl->events.elapsed()>=5) {
        QCoreApplication::processEvents(QEventLoop::AllEvents,2);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        sys_check_timeouts();impl->events.restart();
        if(impl->settings.lan && impl->lanCheck.elapsed()>=1000) {
            impl->lanCheck.restart();
            if(!impl->settings.permits(QHostAddress(impl->settings.lanAddress))) {
            impl->lan.close();
            for(auto* child:impl->children())if(auto* s=dynamic_cast<ProxySession*>(child))if(!s->client->peerAddress().isLoopback())s->close();
            }
        }
    }
    if(impl->packets.isEmpty())return 0;
    const auto b=impl->packets.dequeue();if(b.size()>capacity)return -1;
    memcpy(data,b.constData(),size_t(b.size()));return int(b.size());
}
int ProxyStack::writePacket(const unsigned char* data,int length) {
    if(!impl->attached)return -1;
    if(length<20 || length>65535 || (data[0]>>4)!=4)return 0;
    ++impl->counters.rx;
    auto* p=pbuf_alloc(PBUF_RAW,u16_t(length),PBUF_RAM);if(!p)return 1;
    pbuf_take(p,data,u16_t(length));if(impl->iface.input(p,&impl->iface)!=ERR_OK)pbuf_free(p);return 0;
}
QString ProxyStack::endpoints() const {return impl->addresses;}

QJsonObject ProxyStack::diagnostics() const {const auto& c=impl->counters;return {{"accepted",c.accepted},{"requests",c.requests},{"dns",c.dns},{"tx",c.tx},{"rx",c.rx},{"last_error",c.lastError}};}

void ProxyStack::setNoticeHandler(std::function<void(QString)> handler) {impl->counters.notice=std::move(handler);}
