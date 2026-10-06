// SPDX-License-Identifier: GPL-2.0-or-later
#include "TrafficPolicy.h"
#include "OcSettings.h"
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrl>

QJsonObject TrafficPolicy::toJson() const
{
    QJsonArray entries;
    for (const auto& rule : rules)
        entries.append(QJsonObject{{"kind", rule.kind}, {"target", rule.target},
            {"action", rule.action}, {"enabled", rule.enabled}});
    return {{"version", 1}, {"mode", mode}, {"rules", entries}};
}

bool TrafficPolicy::validateRule(TrafficRule& rule, QString& error)
{
    rule.target = rule.target.trimmed();
    if (rule.action != "vpn" && rule.action != "direct") {
        error = QObject::tr("Choose VPN or Direct for each rule.");
        return false;
    }
    if (rule.kind == "network") {
        QHostAddress host(rule.target);
        if (!host.isNull())
            rule.target = host.toString() + (host.protocol() == QAbstractSocket::IPv4Protocol ? "/32" : "/128");
        const auto network = QHostAddress::parseSubnet(rule.target);
        if (network.second < 1 || network.first.isNull() || network.first.isLoopback()
            || network.first.isMulticast() || network.first.isLinkLocal()) {
            error = QObject::tr("Enter a unicast IP or CIDR network. Use the routing mode for the default route.");
            return false;
        }
        rule.target = network.first.toString() + "/" + QString::number(network.second);
    } else if (rule.kind == "domain") {
        const QByteArray ace = QUrl::toAce(rule.target.toLower());
        static const QRegularExpression domain(QStringLiteral(
            "^(?=.{1,253}$)(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$"));
        rule.target = QString::fromLatin1(ace);
        if (!domain.match(rule.target).hasMatch() || !QHostAddress(rule.target).isNull()) {
            error = QObject::tr("Enter an exact hostname, such as example.com, without a URL or wildcard.");
            return false;
        }
    } else if (rule.kind == "application") {
        if (!rule.target.endsWith(".exe", Qt::CaseInsensitive) || !rule.target.contains(':')
            || rule.target.contains('\n') || rule.target.size() > 1024) {
            error = QObject::tr("Choose the full path of a Windows executable (.exe).");
            return false;
        }
        rule.target.replace('/', '\\');
    } else {
        error = QObject::tr("Unknown traffic rule type.");
        return false;
    }
    return true;
}

bool TrafficPolicy::fromJson(const QJsonObject& object, TrafficPolicy& result, QString& error)
{
    TrafficPolicy policy;
    if (object.value("version").toInt() != 1 || !object.value("rules").isArray()
        || object.value("rules").toArray().size() > 256) {
        error = QObject::tr("Unsupported policy format or too many rules (maximum 256).");
        return false;
    }
    policy.mode = object.value("mode").toString();
    if (policy.mode != "server" && policy.mode != "full" && policy.mode != "selected") {
        error = QObject::tr("Unknown routing mode.");
        return false;
    }
    QStringList keys;
    for (const auto& entry : object.value("rules").toArray()) {
        if (!entry.isObject()) { error = QObject::tr("Invalid traffic rule."); return false; }
        const auto value = entry.toObject();
        if (!value.value("enabled").isBool()) { error = QObject::tr("Invalid enabled flag."); return false; }
        TrafficRule rule{value.value("kind").toString(), value.value("target").toString(),
            value.value("action").toString(), value.value("enabled").toBool()};
        if (!validateRule(rule, error)) return false;
        if (policy.mode == "full" && rule.kind == "network" && QHostAddress::parseSubnet(rule.target).second < 2) {
            error = QObject::tr("Full-tunnel exceptions must use a prefix of /2 or longer."); return false;
        }
        const QString key = rule.kind + ":" + rule.target.toLower();
        if (keys.contains(key)) { error = QObject::tr("Duplicate rule: %1").arg(rule.target); return false; }
        keys.append(key);
        policy.rules.append(rule);
    }
    result = policy;
    return true;
}

TrafficPolicy TrafficPolicy::load(const QString& profile, QString* error)
{
    OcSettings settings;
    const QByteArray bytes = settings.value("server:" + profile + "/routing/policy").toByteArray();
    TrafficPolicy result;
    if (bytes.isEmpty()) return result;
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(bytes, &parseError);
    QString message;
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || !fromJson(document.object(), result, message)) {
        if (error) *error = message.isEmpty() ? QObject::tr("The saved routing policy is damaged.") : message;
    }
    return result;
}

bool TrafficPolicy::save(const QString& profile, QString& error) const
{
    TrafficPolicy validated;
    if (!fromJson(toJson(), validated, error)) return false;
    OcSettings settings;
    settings.setValue("server:" + profile + "/routing/policy", QJsonDocument(validated.toJson()).toJson(QJsonDocument::Compact));
    settings.sync();
    if (settings.status() != QSettings::NoError) { error = QObject::tr("Unable to save routing rules."); return false; }
    return true;
}
