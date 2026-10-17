// SPDX-License-Identifier: GPL-2.0-or-later
#include "GamingProfile.h"
#include "OcSettings.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostAddress>
#include <QFileInfo>
#include <QDir>
#include <QUuid>
#include <QObject>
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#endif
QJsonObject GamingProfile::toJson() const {
    return {{"id",id},{"name",name},{"target",target},{"probe",probe},{"port",int(port)},
        {"applications",QJsonArray::fromStringList(applications)},{"background",QJsonArray::fromStringList(background)},
        {"route",route},{"automatic",automatic},{"auto_mtu",autoMtu},{"mtu",int(mtu)},
        {"upload_kbps",int(uploadKbps)},{"background_kbps",int(backgroundKbps)}};
}
bool GamingProfile::fromJson(const QJsonObject& o,GamingProfile& p,QString& error) {
    p.id=o.value("id").toString(); p.name=o.value("name").toString().trimmed(); p.target=o.value("target").toString().trimmed();
    p.probe=o.value("probe").toString("icmp"); p.route=o.value("route").toString("inherit");
    const int port=o.value("port").toInt(443), mtu=o.value("mtu").toInt(1280);
    const int upload=o.value("upload_kbps").toInt(), background=o.value("background_kbps").toInt(1024);
    if(QUuid(p.id).isNull() || p.name.isEmpty() || p.name.size()>80 || port<1 || port>65535 || mtu<1280 || mtu>1500
        || upload<0 || upload>1000000 || background<64 || background>1000000
        || !QStringList{"icmp","tcp"}.contains(p.probe) || !QStringList{"inherit","vpn","direct"}.contains(p.route)) {
        error=QObject::tr("Invalid gaming profile settings."); return false;
    }
    if(!p.target.isEmpty()) { const QHostAddress ip(p.target);
        if(ip.isNull() || ip.isLoopback() || ip.isMulticast() || ip.isLinkLocal() || ip==QHostAddress::Any || ip==QHostAddress::AnyIPv6) {
            error=QObject::tr("Enter a numeric game destination IP, or leave it empty. No arbitrary game UDP packets are sent."); return false;
        }
        p.target=ip.toString();
    }
    const auto paths=[&](const char* key,QStringList& out) {
        const auto array=o.value(key).toArray(); if(array.size()>16) return false;
        for(const auto& entry:array) { const QString path=QDir::cleanPath(entry.toString()); QFileInfo file(path);
            if(!file.isAbsolute() || file.suffix().compare("exe",Qt::CaseInsensitive)!=0 || path.size()>1024 || path.contains('*') || path.contains('?')) return false;
            if(!out.contains(path,Qt::CaseInsensitive)) out.append(path);
        }
        return true;
    };
    p.applications.clear(); p.background.clear();
    if(!paths("applications",p.applications) || !paths("background",p.background)) {error=QObject::tr("Choose absolute executable paths for gaming and background applications."); return false;}
    for(const auto& path:p.background) {
        const QString name=QFileInfo(path).fileName().toLower();
        if(p.applications.contains(path,Qt::CaseInsensitive) || QStringList{"gozarnovpn.exe","openconnect-gui.exe","proxifyre.exe","svchost.exe","system.exe"}.contains(name)) {
            error=QObject::tr("Do not throttle the game, VPN, routing helper or Windows network services."); return false;
        }
    }
    p.port=quint16(port); p.mtu=unsigned(mtu); p.uploadKbps=unsigned(upload); p.backgroundKbps=unsigned(background);
    p.automatic=o.value("automatic").toBool(); p.autoMtu=o.value("auto_mtu").toBool(true);
    if(p.automatic && p.applications.isEmpty()) {error=QObject::tr("Automatic game detection needs at least one game executable."); return false;}
    return true;
}
QList<GamingProfile> GamingProfile::load() {
    OcSettings s; QList<GamingProfile> result;
    const auto entries=QJsonDocument::fromJson(s.value("Client/gamingProfiles").toByteArray()).array();
    for(const auto& entry:entries) {GamingProfile p; QString error; if(result.size()<16 && fromJson(entry.toObject(),p,error)) result.append(p);}
    if(result.isEmpty()) {GamingProfile p; p.id="{161e25bb-c753-4eae-bd19-9b4d07bb041b}"; p.name=QObject::tr("Default game profile"); result.append(p);}
    return result;
}
bool GamingProfile::save(const QList<GamingProfile>& profiles,const QString& selected,QString& error) {
    if(profiles.isEmpty() || profiles.size()>16) {error=QObject::tr("Keep between one and sixteen gaming profiles."); return false;}
    QJsonArray entries; QStringList ids;
    for(const auto& p:profiles) {GamingProfile checked; if(!fromJson(p.toJson(),checked,error)) return false;
        if(ids.contains(checked.id)) {error=QObject::tr("Gaming profile IDs must be unique."); return false;} ids.append(checked.id); entries.append(checked.toJson());}
    if(!ids.contains(selected)) {error=QObject::tr("Select a gaming profile."); return false;}
    OcSettings s; s.setValue("Client/gamingProfiles",QJsonDocument(entries).toJson(QJsonDocument::Compact)); s.setValue("Client/gamingProfile",selected); s.sync();
    if(s.status()!=QSettings::NoError) {error=QObject::tr("Could not save gaming profiles."); return false;} return true;
}
GamingProfile GamingProfile::selected() {
    const auto entries=load(); OcSettings s; const QString id=s.value("Client/gamingProfile").toString();
    for(const auto& p:entries) if(p.id==id) return p;
    return entries.first();
}
GamingProfile GamingProfile::automaticPreset() {
    GamingProfile p; p.id="{f1ae214b-d419-4a19-9498-eb80d1e27076}";
    p.name=QObject::tr("Automatic gaming mode");
    p.target="1.1.1.1";
    // Use the client's existing connectivity destinations, never an old game
    // profile's route, executable list, manual MTU or upload limit.
    OcSettings s;
    for(const auto& candidate:s.value("Client/healthTargets",QStringList{"1.1.1.1","8.8.8.8"}).toStringList()) {
        const QHostAddress ip(candidate.trimmed());
        if(ip.protocol()==QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isMulticast() && !ip.isLinkLocal() && ip.toIPv4Address()!=0 && ip.toIPv4Address()!=0xffffffffu) {p.target=ip.toString(); break;}
    }
    return p;
}
TrafficPolicy GamingProfile::overlay(const TrafficPolicy& base,const GamingProfile& game) {
    TrafficPolicy result=base; if(game.route=="inherit") return result;
    for(const auto& path:game.applications) {
        for(int i=result.rules.size()-1;i>=0;--i) if(result.rules[i].kind=="application" && result.rules[i].target.compare(path,Qt::CaseInsensitive)==0) result.rules.removeAt(i);
        result.rules.append({"application",path,game.route,true});
    }
    return result;
}
bool GamingProfile::processesRunning(const QStringList& paths) {
#ifdef _WIN32
    if(paths.isEmpty()) return false;
    const HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); if(snapshot==INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry); bool found=false;
    if(Process32FirstW(snapshot,&entry)) do {
        bool candidate=false; const QString executable=QString::fromWCharArray(entry.szExeFile);
        for(const auto& path:paths) if(QFileInfo(path).fileName().compare(executable,Qt::CaseInsensitive)==0) {candidate=true; break;}
        if(!candidate) continue;
        const HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,entry.th32ProcessID); if(!process) continue;
        wchar_t buffer[32768]; DWORD size=32768;
        if(QueryFullProcessImageNameW(process,0,buffer,&size)) {const QString path=QDir::fromNativeSeparators(QString::fromWCharArray(buffer,int(size))); found=paths.contains(QDir::cleanPath(path),Qt::CaseInsensitive);}
        CloseHandle(process); if(found) break;
    } while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot); return found;
#else
    Q_UNUSED(paths); return false;
#endif
}
