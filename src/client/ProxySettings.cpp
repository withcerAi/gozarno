// SPDX-License-Identifier: GPL-2.0-or-later

#include "ProxySettings.h"

#include "OcSettings.h"

#include "cryptdata.h"

#include <QNetworkInterface>

#include <QObject>
#include <QCoreApplication>

ProxySettings ProxySettings::load() {

    OcSettings s; ProxySettings p;

    p.enabled=s.value("Client/proxyMode",false).toBool();

    p.lan=s.value("Client/proxyLan",false).toBool();

    p.port=s.value("Client/proxyPort",1080).toInt();

    p.lanAddress=s.value("Client/proxyLanAddress").toString();

    p.username=s.value("Client/proxyUser","client").toString();

    QString entropy="Gozarno SOCKS credentials";

    const auto cipher=s.value("Client/proxyPassword").toByteArray();

    if(!cipher.isEmpty()) CryptData::decode(entropy,cipher,p.password);

    if(QCoreApplication::instance()->property("socksValidation").toBool()) {
        p.enabled=true;p.lan=false;p.password.clear();p.port=QCoreApplication::instance()->property("socksValidationPort").toInt();
    }
    return p;

}

bool ProxySettings::validate(QString& error) const {

    if(port<1 || port>65535) {error=QObject::tr("Choose a proxy port between 1 and 65535."); return false;}

    if(username.toUtf8().size()>255 || password.toUtf8().size()>255 || (!password.isEmpty() && username.isEmpty())) {

        error=QObject::tr("Proxy credentials must fit SOCKS5 limits (1–255 bytes)."); return false;

    }

    if(lan && password.size()<8) {error=QObject::tr("LAN sharing requires a separate proxy password of at least 8 characters."); return false;}

    if(lan) {

        const QHostAddress address(lanAddress);

        for(const auto& iface:QNetworkInterface::allInterfaces()) {

            if(!iface.flags().testFlag(QNetworkInterface::IsUp) || iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;

            for(const auto& entry:iface.addressEntries()) if(entry.ip()==address && address.protocol()==QAbstractSocket::IPv4Protocol && !address.isLinkLocal()) return true;

        }

        error=QObject::tr("Select an active LAN IPv4 address."); return false;

    }

    return true;

}

bool ProxySettings::save(QString& error) const {

    if(!validate(error)) return false;

    QString entropy="Gozarno SOCKS credentials";

    const auto cipher=password.isEmpty()? QByteArray():CryptData::encode(entropy,password);

    if(!password.isEmpty() && cipher.isEmpty()) {error=QObject::tr("Windows could not encrypt the proxy password."); return false;}

    OcSettings s;

    s.setValue("Client/proxyMode",enabled); s.setValue("Client/proxyLan",lan);

    s.setValue("Client/proxyPort",port); s.setValue("Client/proxyLanAddress",lanAddress);

    s.setValue("Client/proxyUser",username); s.setValue("Client/proxyPassword",cipher); s.sync();

    if(s.status()!=QSettings::NoError) {error=QObject::tr("Could not save proxy settings."); return false;}

    return true;

}

bool ProxySettings::permits(const QHostAddress& peer) const {

    if(peer.isLoopback()) return true;

    if(!lan || peer.protocol()!=QAbstractSocket::IPv4Protocol) return false;

    const QHostAddress local(lanAddress);

    for(const auto& iface:QNetworkInterface::allInterfaces()) {

        if(!iface.flags().testFlag(QNetworkInterface::IsUp)) continue;

        for(const auto& entry:iface.addressEntries())

            if(entry.ip()==local && entry.prefixLength()>0 && peer.isInSubnet(local,entry.prefixLength())) return true;

    }

    return false;

}

