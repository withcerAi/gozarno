// SPDX-License-Identifier: GPL-2.0-or-later
#include "KillSwitch.h"
#include <QCoreApplication>
#include <QDir>
#include <QObject>
#ifdef _WIN32
#include "WindowsIpInterface.h"
#include <fwpmu.h>
#include <fwpmtypes.h>
#include "WindowsFilteringIds.h"
#endif
struct KillSwitch::Impl {
    bool armed=false;
#ifdef _WIN32
    HANDLE engine=nullptr;
    GUID sublayer{};
    QList<UINT64> tunnelFilters;
    DWORD add(const GUID& layer, FWP_ACTION_TYPE action, UINT8 weight,
        QList<FWPM_FILTER_CONDITION0> conditions, UINT64* id=nullptr) {
        FWPM_FILTER0 filter{};
        filter.displayData.name=const_cast<wchar_t*>(L"Gozarno temporary VPN protection");
        filter.layerKey=layer; filter.subLayerKey=sublayer;
        filter.action.type=action; filter.weight.type=FWP_UINT8; filter.weight.uint8=weight;
        filter.numFilterConditions=conditions.size(); filter.filterCondition=conditions.data();
        return FwpmFilterAdd0(engine,&filter,nullptr,id);
    }
#endif
};
KillSwitch::KillSwitch() : impl(new Impl) {}
KillSwitch::~KillSwitch() { QString error; release(error); }
bool KillSwitch::active() const { return impl->armed; }
bool KillSwitch::arm(const QList<QHostAddress>& gateways, QString& error)
{
    if(impl->armed) return true; // Keep protection across recovery attempts.
    if(gateways.isEmpty()) { error=QObject::tr("Cannot protect a connection without a resolved gateway."); return false; }
#ifdef _WIN32
    FWPM_SESSION0 session{}; session.flags=GozarnoWfp::DynamicSession;
    DWORD status=FwpmEngineOpen0(nullptr,RPC_C_AUTHN_WINNT,nullptr,&session,&impl->engine);
    if(!status) status=UuidCreate(&impl->sublayer);
    if(status==RPC_S_UUID_LOCAL_ONLY) status=0;
    if(!status) status=FwpmTransactionBegin0(impl->engine,0);
    FWPM_SUBLAYER0 layer{}; layer.subLayerKey=impl->sublayer; layer.weight=0x8000;
    layer.displayData.name=const_cast<wchar_t*>(L"Gozarno temporary VPN protection");
    if(!status) status=FwpmSubLayerAdd0(impl->engine,&layer,nullptr);
    FWP_BYTE_BLOB* app=nullptr;
    const auto path=QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    if(!status) status=FwpmGetAppIdFromFileName0(reinterpret_cast<const wchar_t*>(path.utf16()),&app);
    for(const auto& key:{GozarnoWfp::Connect4,GozarnoWfp::Connect6}) {
        if(status) break;
        status=impl->add(key,FWP_ACTION_BLOCK,0,{});
        FWPM_FILTER_CONDITION0 loop{}; loop.fieldKey=GozarnoWfp::Flags;
        loop.matchType=FWP_MATCH_FLAGS_ALL_SET; loop.conditionValue.type=FWP_UINT32;
        loop.conditionValue.uint32=FWP_CONDITION_FLAG_IS_LOOPBACK;
        if(!status) status=impl->add(key,FWP_ACTION_PERMIT,15,{loop});
        for(const auto& gateway:gateways) {
            const bool v6=gateway.protocol()==QAbstractSocket::IPv6Protocol;
            if(v6!=bool(IsEqualGUID(key,GozarnoWfp::Connect6))) continue;
            if(gateway.isNull() || gateway.isLoopback() || gateway.isMulticast() || gateway.isLinkLocal()) { status=ERROR_INVALID_PARAMETER; break; }
            FWPM_FILTER_CONDITION0 application{},destination{};
            application.fieldKey=GozarnoWfp::Application; application.matchType=FWP_MATCH_EQUAL;
            application.conditionValue.type=FWP_BYTE_BLOB_TYPE; application.conditionValue.byteBlob=app;
            destination.fieldKey=GozarnoWfp::RemoteAddress; destination.matchType=FWP_MATCH_EQUAL;
            FWP_BYTE_ARRAY16 bytes{};
            if(v6) { const auto ip=gateway.toIPv6Address(); memcpy(bytes.byteArray16,ip.c,16); destination.conditionValue.type=FWP_BYTE_ARRAY16_TYPE; destination.conditionValue.byteArray16=&bytes; }
            else { destination.conditionValue.type=FWP_UINT32; destination.conditionValue.uint32=gateway.toIPv4Address(); }
            if(!status) status=impl->add(key,FWP_ACTION_PERMIT,14,{application,destination});
        }
    }
    if(app) FwpmFreeMemory0(reinterpret_cast<void**>(&app));
    if(!status) status=FwpmTransactionCommit0(impl->engine);
    if(status) {
        if(impl->engine) { FwpmTransactionAbort0(impl->engine); FwpmEngineClose0(impl->engine); impl->engine=nullptr; }
        error=QObject::tr("Cannot enable kill switch (Windows error %1). Connection stopped.").arg(status); return false;
    }
    impl->armed=true; return true;
#else
    error=QObject::tr("Kill switch is currently supported on Windows only."); return false;
#endif
}
bool KillSwitch::allowTunnel(const QString& name, QString& error)
{
    if(!impl->armed) return true;
#ifdef _WIN32
    NET_LUID luid{};
    if(name.isEmpty() || ConvertInterfaceAliasToLuid(reinterpret_cast<const wchar_t*>(name.utf16()),&luid)!=NO_ERROR) {
        error=QObject::tr("The active VPN adapter is no longer available."); return false;
    }
    DWORD status=FwpmTransactionBegin0(impl->engine,0);
    if(status) { error=QObject::tr("Cannot start filtering transaction (error %1).").arg(status); return false; }
    for(const auto id:impl->tunnelFilters) { status=FwpmFilterDeleteById0(impl->engine,id); if(status) break; }
    QList<UINT64> next;
    FWPM_FILTER_CONDITION0 condition{}; condition.fieldKey=GozarnoWfp::LocalInterface;
    condition.matchType=FWP_MATCH_EQUAL; condition.conditionValue.type=FWP_UINT64;
    condition.conditionValue.uint64=&luid.Value;
    for(const auto& key:{GozarnoWfp::Connect4,GozarnoWfp::Connect6}) {
        UINT64 id=0; if(!status) status=impl->add(key,FWP_ACTION_PERMIT,13,{condition},&id); next.append(id);
    }
    if(!status) status=FwpmTransactionCommit0(impl->engine);
    if(status) { FwpmTransactionAbort0(impl->engine); error=QObject::tr("Cannot protect the VPN adapter (Windows error %1).").arg(status); return false; }
    impl->tunnelFilters=next; return true;
#else
    error=QObject::tr("Kill switch is currently supported on Windows only."); return false;
#endif
}
bool KillSwitch::release(QString& error)
{
#ifdef _WIN32
    if(impl->engine) {
        const auto status=FwpmEngineClose0(impl->engine);
        if(status) { error=QObject::tr("Cannot release kill switch (Windows error %1).").arg(status); return false; }
        impl->engine=nullptr; impl->tunnelFilters.clear();
    }
#else
    Q_UNUSED(error);
#endif
    impl->armed=false; return true;
}

bool KillSwitch::blockTunnel(QString& error)
{
    if (!impl->armed) return true;
#ifdef _WIN32
    DWORD status=FwpmTransactionBegin0(impl->engine,0);
    if (status) { error=QObject::tr("Cannot start filtering transaction (error %1).").arg(status); return false; }
    for (const auto id:impl->tunnelFilters) { status=FwpmFilterDeleteById0(impl->engine,id); if(status) break; }
    if (!status) status=FwpmTransactionCommit0(impl->engine);
    if (status) { FwpmTransactionAbort0(impl->engine); error=QObject::tr("Cannot protect the VPN adapter (Windows error %1).").arg(status); return false; }
    impl->tunnelFilters.clear(); return true;
#else
    Q_UNUSED(error); return false;
#endif
}
