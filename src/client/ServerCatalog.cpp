// SPDX-License-Identifier: GPL-2.0-or-later
#include "ServerCatalog.h"
#include "TrafficPolicy.h"
#include "OcSettings.h"
#include <QJsonArray>
#include <QUrl>
#include <QMap>
#include <QUuid>

bool ServerCatalog::validName(const QString& name)
{
    return !name.trimmed().isEmpty() && name == name.trimmed() && name.size() <= 128
        && !name.contains('/') && !name.contains('\\') && !name.contains(QChar('\0'))
        && !name.contains('\n') && !name.contains('\r');
}
bool ServerCatalog::validGateway(const QString& text, QString* normalized)
{
    QUrl url(text.contains("://") ? text.trimmed() : "https://" + text.trimmed(), QUrl::StrictMode);
    const bool valid = url.isValid() && url.scheme() == "https" && !url.host().isEmpty()
        && url.userInfo().isEmpty() && !url.hasFragment();
    if (valid && normalized) *normalized = url.toString();
    return valid;
}
QJsonDocument ServerCatalog::exportProfiles()
{
    OcSettings settings;
    QJsonArray servers;
    for (const auto& group : settings.childGroups()) {
        if (!group.startsWith("server:") || !settings.contains(group + "/server")) continue;
        settings.beginGroup(group);
        QJsonObject item{{"name", group.mid(7)}, {"gateway", settings.value("server").toString()},
            {"protocol", settings.value("protocol-name", "anyconnect").toString()},
            {"favorite", settings.value("catalog/favorite", false).toBool()},
            {"notes", settings.value("catalog/notes").toString()}};
        settings.endGroup();
        item.insert("routing", TrafficPolicy::load(group.mid(7)).toJson());
        servers.append(item);
    }
    return QJsonDocument(QJsonObject{{"format", "openconnect-client-catalog"}, {"version", 1}, {"servers", servers}});
}
bool ServerCatalog::importProfiles(const QJsonDocument& document, QStringList& names, QString& error)
{
    const auto root = document.object();
    if (root.value("format").toString() != "openconnect-client-catalog" || root.value("version").toInt() != 1
        || !root.value("servers").isArray() || root.value("servers").toArray().size() > 500) {
        error = QObject::tr("Invalid catalog format (maximum 500 servers)."); return false;
    }
    struct Entry { QString name, gateway, protocol, notes; bool favorite; TrafficPolicy policy; };
    QList<Entry> entries;
    QStringList reserved;
    OcSettings settings;
    const QStringList protocols{"anyconnect", "nc", "pulse", "gp", "f5", "fortinet", "array"};
    for (const auto& value : root.value("servers").toArray()) {
        if (!value.isObject()) { error = QObject::tr("Invalid catalog entry."); return false; }
        const auto object = value.toObject();
        Entry entry;
        entry.name = object.value("name").toString();
        entry.protocol = object.value("protocol").toString("anyconnect");
        entry.notes = object.value("notes").toString();
        entry.favorite = object.value("favorite").toBool();
        if (!validName(entry.name) || !validGateway(object.value("gateway").toString(), &entry.gateway)
            || !protocols.contains(entry.protocol) || entry.notes.size() > 4096) {
            error = QObject::tr("Invalid name, gateway, protocol or notes in a catalog entry."); return false;
        }
        if (object.contains("routing") && (!object.value("routing").isObject()
            || !TrafficPolicy::fromJson(object.value("routing").toObject(), entry.policy, error))) return false;
        const QString base = entry.name.left(110);
        int suffix = 1;
        while (reserved.contains(entry.name) || settings.contains("server:" + entry.name + "/server"))
            entry.name = base + QString(" (%1)").arg(++suffix);
        reserved.append(entry.name); entries.append(entry);
    }
    // Validate the entire input before modifying any profiles.
    QStringList written;
    for (const auto& entry : entries) {
        const QString group = "server:" + entry.name;
        settings.beginGroup(group);
        settings.setValue("server", entry.gateway);
        settings.setValue("protocol-name", entry.protocol);
        settings.setValue("remember-password", false);
        settings.setValue("catalog/favorite", entry.favorite);
        settings.setValue("catalog/notes", entry.notes);
        settings.setValue("routing/policy", QJsonDocument(entry.policy.toJson()).toJson(QJsonDocument::Compact));
        settings.endGroup(); written.append(entry.name);
    }
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        for (const auto& name : written) settings.remove("server:" + name);
        settings.sync(); error = QObject::tr("Unable to store the imported catalog."); return false;
    }
    names = written;
    return true;
}
bool ServerCatalog::duplicate(const QString& source, const QString& target, QString& error)
{
    OcSettings settings;
    if (!validName(target) || !settings.contains("server:" + source + "/server")
        || settings.contains("server:" + target + "/server")) {
        error = QObject::tr("Choose a unique valid name for the copied profile."); return false;
    }
    settings.beginGroup("server:" + source);
    QMap<QString, QVariant> values;
    for (const auto& key : settings.allKeys()) values.insert(key, settings.value(key));
    settings.endGroup();
    settings.beginGroup("server:" + target);
    for (auto it = values.cbegin(); it != values.cend(); ++it) settings.setValue(it.key(), it.value());
    settings.endGroup(); settings.sync();
    if (settings.status() != QSettings::NoError) { error = QObject::tr("Unable to copy profile."); return false; }
    return true;
}
