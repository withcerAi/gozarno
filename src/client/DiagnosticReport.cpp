// SPDX-License-Identifier: GPL-2.0-or-later
#include "DiagnosticReport.h"
#include <QJsonObject>
#include <QDateTime>
#include <QCoreApplication>
#include <QSysInfo>
#include <QSet>
DiagnosticReport::DiagnosticReport() { elapsed.start(); }
void DiagnosticReport::record(const QString& event,const QJsonObject& values) {
    static const QSet<QString> accepted{"connect","connected","disconnected","manual_stop","health","recovery","fallback","environment","failure","protection_test"};
    if(!accepted.contains(event)) return;
    QJsonObject entry{{"event",event},{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"elapsed_ms",elapsed.elapsed()}};
    static const QSet<QString> numbers{"attempt","error_code","state","profile_index"};
    static const QSet<QString> flags{"kill_switch","ipv4","ipv6","passed","available"};
    static const QSet<QString> codes{"healthy","network_unavailable","tunnel_unavailable","tunnel_unreachable","dns_unavailable","dns_failed","authentication","cancelled","configuration","transport","resume","network_changed","not_run","skipped","failed","passed"};
    for(auto it=values.begin();it!=values.end();++it) {
        static const QSet<QString> tests{"temporary_filter_setup","ipv4_direct_tcp","ipv6_direct_tcp","direct_dns","tunnel_tcp","interrupted_tunnel","recovery","release","vpn_dns","connection"};
        if(it.key()=="test" && tests.contains(it.value().toString())) { entry.insert(it.key(),it.value()); continue; }
        if(numbers.contains(it.key()) && it.value().isDouble()) entry.insert(it.key(),it.value());
        else if(flags.contains(it.key()) && it.value().isBool()) entry.insert(it.key(),it.value());
        else if((it.key()=="reason" || it.key()=="result") && codes.contains(it.value().toString())) entry.insert(it.key(),it.value());
    }
    events.append(entry); while(events.size()>500) events.removeAt(0);
}
QJsonDocument DiagnosticReport::document() const {
    return QJsonDocument(QJsonObject{{"schema","gozarno-diagnostics-v1"},{"application_version",QCoreApplication::applicationVersion()},
        {"qt_version",QT_VERSION_STR},{"os",QSysInfo::productType()},{"os_version",QSysInfo::productVersion()},
        {"privacy","Structured events only. Credentials, raw logs, server/profile names, addresses and user paths are excluded."},
        {"events",events}});
}
