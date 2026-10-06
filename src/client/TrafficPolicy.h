// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <QList>
#include <QString>

struct TrafficRule {
    QString kind; // network, domain, application
    QString target;
    QString action; // vpn, direct
    bool enabled = true;
};

struct TrafficPolicy {
    QString mode = "server"; // server, full, selected
    QList<TrafficRule> rules;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& object, TrafficPolicy& result, QString& error);
    static bool validateRule(TrafficRule& rule, QString& error);
    static TrafficPolicy load(const QString& profile, QString* error = nullptr);
    bool save(const QString& profile, QString& error) const;
    bool active() const { return mode != "server" || !rules.isEmpty(); }
};
