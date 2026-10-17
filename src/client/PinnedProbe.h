// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QAbstractSocket>
#include <QNetworkInterface>
#ifdef _WIN32
#include "WindowsIpInterface.h"
#endif
namespace PinnedProbe {
inline QNetworkInterface adapter(const QString& name) {
#ifdef _WIN32
    NET_LUID luid{}; NET_IFINDEX index=0;
    if(ConvertInterfaceAliasToLuid(reinterpret_cast<const wchar_t*>(name.utf16()),&luid)==NO_ERROR
        && ConvertInterfaceLuidToIndex(&luid,&index)==NO_ERROR) return QNetworkInterface::interfaceFromIndex(index);
#endif
    return QNetworkInterface::interfaceFromName(name);
}
inline bool bind(QAbstractSocket* socket,const QNetworkInterface& iface,const QHostAddress& remote) {
    if(!iface.isValid()) return false;
    QHostAddress source;
    for(const auto& entry:iface.addressEntries())
        if(entry.ip().protocol()==remote.protocol() && !entry.ip().isLinkLocal() && !entry.ip().isLoopback()) { source=entry.ip(); break; }
    if(source.isNull() || !socket->bind(source,0)) return false;
#ifdef _WIN32
    const bool v6=remote.protocol()==QAbstractSocket::IPv6Protocol;
    const DWORD value=v6 ? iface.index() : htonl(iface.index());
    return setsockopt(SOCKET(socket->socketDescriptor()),v6 ? IPPROTO_IPV6 : IPPROTO_IP,
        v6 ? IPV6_UNICAST_IF : IP_UNICAST_IF,reinterpret_cast<const char*>(&value),sizeof(value))==0;
#else
    return false;
#endif
}
inline QByteArray dnsQuery(quint16 id) {
    QByteArray query=QByteArray::fromHex("000001000001000000000000076578616d706c6503636f6d0000010001");
    query[0]=char(id>>8); query[1]=char(id); return query;
}
inline bool dnsAnswer(const QByteArray& bytes,quint16 id) {
    if(bytes.size()<12 || quint8(bytes[0])!=quint8(id>>8) || quint8(bytes[1])!=quint8(id)
        || !(quint8(bytes[2])&0x80) || (quint8(bytes[2])&0x7a) || (quint8(bytes[3])&0x0f)) return false;
    const auto word=[&](int at) { return (quint16(quint8(bytes[at]))<<8)|quint8(bytes[at+1]); };
    const auto question=dnsQuery(id).mid(12);
    if(word(4)!=1 || bytes.mid(12,question.size())!=question) return false;
    const int count=word(6); if(count<1 || count>64) return false;
    int position=12+question.size();
    const auto name=[&](int& at) {
        int cursor=at, end=-1, total=0;
        for(int hops=0;hops<128 && cursor<bytes.size();++hops) {
            const auto length=quint8(bytes[cursor]);
            if((length&0xc0)==0xc0) {
                if(cursor+1>=bytes.size()) return false;
                const int target=((length&0x3f)<<8)|quint8(bytes[cursor+1]);
                if(target<12 || target>=cursor) return false;
                if(end<0) end=cursor+2;
                cursor=target; continue;
            }
            if(length&0xc0) return false;
            if(!length) { at=end<0 ? cursor+1 : end; return true; }
            total+=length+1; if(total>255 || cursor+length>=bytes.size()) return false;
            cursor+=length+1;
        }
        return false;
    };
    for(int i=0;i<count;++i) {
        if(!name(position) || position+10>bytes.size()) return false;
        const int type=word(position), klass=word(position+2), length=word(position+8);
        position+=10; if(position+length>bytes.size()) return false;
        if(type==1 && klass==1 && length==4) return true;
        position+=length;
    }
    return false;
}
}
