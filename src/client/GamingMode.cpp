// SPDX-License-Identifier: GPL-2.0-or-later
#include "GamingMode.h"
#include "OcSettings.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#endif
struct GamingMode::Impl {
    bool active = false;
#ifdef _WIN32
    struct Change { NET_LUID luid; ADDRESS_FAMILY family; ULONG before, after; };
    QList<Change> changes;
    bool journal() const {
        QJsonArray entries;
        for (const auto& change : changes)
            entries.append(QJsonObject{{"luid", QString::number(change.luid.Value)}, {"family", int(change.family)},
                {"before", int(change.before)}, {"after", int(change.after)}});
        OcSettings settings; settings.setValue("Client/gamingJournal", QJsonDocument(entries).toJson()); settings.sync();
        return settings.status() == QSettings::NoError;
    }
#endif
};
GamingMode::GamingMode() : impl(new Impl)
{
#ifdef _WIN32
    // Recover only the exact adapter/MTU changed by a previous Gozarno session.
    OcSettings settings;
    const auto pending = QJsonDocument::fromJson(settings.value("Client/gamingJournal").toByteArray()).array();
    for (const auto& value : pending) {
        const auto object = value.toObject(); NET_LUID luid{};
        luid.Value = object.value("luid").toString().toULongLong();
        impl->changes.append({luid, ADDRESS_FAMILY(object.value("family").toInt()),
            ULONG(object.value("before").toInt()), ULONG(object.value("after").toInt())});
    }
    disable();
#endif
}
GamingMode::~GamingMode() { disable(); }
bool GamingMode::active() const { return impl->active; }
bool GamingMode::enable(const QString& tunnelName, unsigned mtu, QString& error)
{
    if (mtu < 1280 || mtu > 1500 || tunnelName.isEmpty()) {
        error = QObject::tr("Gaming mode requires an active VPN adapter and an MTU between 1280 and 1500."); return false;
    }
#ifdef _WIN32
    if (!disable(&error)) return false;
    NET_LUID luid{};
    if (ConvertInterfaceAliasToLuid(reinterpret_cast<const wchar_t*>(tunnelName.utf16()), &luid) != NO_ERROR) {
        error = QObject::tr("The active VPN adapter is no longer available."); return false;
    }
    for (const auto family : {AF_INET, AF_INET6}) {
        MIB_IPINTERFACE_ROW row{}; InitializeIpInterfaceEntry(&row);
        row.InterfaceLuid = luid; row.Family = family;
        if (GetIpInterfaceEntry(&row) != NO_ERROR || !row.Connected) continue;
        const ULONG next = qMin(row.NlMtu, ULONG(mtu));
        if (next == row.NlMtu) continue; // Never exceed the server-negotiated packet size.
        impl->changes.append({luid, ADDRESS_FAMILY(family), row.NlMtu, next});
        if (!impl->journal()) {
            impl->changes.removeLast();
            error = QObject::tr("Cannot save gaming recovery information.");
            disable(); return false;
        }
        row.NlMtu = next;
        const ULONG status = SetIpInterfaceEntry(&row);
        if (status != NO_ERROR) {
            error = QObject::tr("Cannot enable gaming mode (Windows error %1).").arg(status);
            disable(); return false;
        }
    }
    impl->active = true; return true;
#else
    error = QObject::tr("Gaming mode is currently supported on Windows only."); return false;
#endif
}
bool GamingMode::disable(QString* error)
{
#ifdef _WIN32
    QList<Impl::Change> pending;
    for (const auto& change : impl->changes) {
        MIB_IPINTERFACE_ROW row{}; InitializeIpInterfaceEntry(&row);
        row.InterfaceLuid = change.luid; row.Family = change.family;
        const ULONG lookup = GetIpInterfaceEntry(&row);
        if (lookup == ERROR_NOT_FOUND || lookup == ERROR_FILE_NOT_FOUND) continue;
        if (lookup != NO_ERROR) { pending.append(change); continue; }
        if (row.NlMtu != change.after) continue; // A later administrator change takes precedence.
        row.NlMtu = change.before;
        if (SetIpInterfaceEntry(&row) != NO_ERROR) pending.append(change);
    }
    impl->changes = pending; impl->journal();
    if (!pending.isEmpty()) {
        if (error) *error = QObject::tr("Gaming settings could not be fully restored. Restart Gozarno as administrator to retry recovery.");
        return false;
    }
#endif
    impl->active = false; return true;
}
