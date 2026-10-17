// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "TrafficPolicy.h"
#include <QStringList>
struct GamingProfile {
    QString id, name, target;
    QString probe = "icmp";
    quint16 port = 443;
    QStringList applications, background;
    QString route = "inherit";
    bool automatic = false, autoMtu = true;
    unsigned mtu = 1280;
    unsigned uploadKbps = 0, backgroundKbps = 1024;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject&,GamingProfile&,QString& error);
    static QList<GamingProfile> load();
    static bool save(const QList<GamingProfile>&,const QString& selected,QString& error);
    static GamingProfile selected();
    static GamingProfile automaticPreset();
    static TrafficPolicy overlay(const TrafficPolicy&,const GamingProfile&);
    static bool processesRunning(const QStringList& paths);
};
