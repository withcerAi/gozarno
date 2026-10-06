// SPDX-License-Identifier: GPL-2.0-or-later
#include "InstalledComponents.h"
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QVersionNumber>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <winsvc.h>
#endif

namespace {
QString fileVersion(const QString& path) {
#ifdef _WIN32
    const auto native=QDir::toNativeSeparators(path).toStdWString();
    const DWORD size=GetFileVersionInfoSizeW(native.c_str(),nullptr);
    if(size==0 || size>1024*1024) return {};
    std::vector<char> data(size);
    if(!GetFileVersionInfoW(native.c_str(),0,size,data.data())) return {};
    VS_FIXEDFILEINFO* info=nullptr; UINT length=0;
    if(!VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&info),&length) || length<sizeof(VS_FIXEDFILEINFO) || info->dwSignature!=0xfeef04bd) return {};
    return QString("%1.%2.%3.%4").arg(HIWORD(info->dwFileVersionMS)).arg(LOWORD(info->dwFileVersionMS)).arg(HIWORD(info->dwFileVersionLS)).arg(LOWORD(info->dwFileVersionLS));
#else
    Q_UNUSED(path); return {};
#endif
}
#ifdef _WIN32
bool registryNumber(const wchar_t* key,const wchar_t* name,DWORD& result,DWORD view=RRF_SUBKEY_WOW6464KEY) {
    DWORD length=sizeof(result);
    return RegGetValueW(HKEY_LOCAL_MACHINE,key,name,RRF_RT_REG_DWORD|view,nullptr,&result,&length)==ERROR_SUCCESS;
}
InstalledComponent packetFilter() {
    InstalledComponent item{"Windows Packet Filter",QObject::tr("Windows driver"),{}, {},InstalledComponent::Unknown};
    SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT);
    SC_HANDLE service=manager ? OpenServiceW(manager,L"ndisrd",SERVICE_QUERY_STATUS|SERVICE_QUERY_CONFIG) : nullptr;
    const auto error=GetLastError();
    if(!service) {
        item.state=error==ERROR_SERVICE_DOES_NOT_EXIST ? InstalledComponent::Missing : InstalledComponent::Unknown;
        item.status=item.state==InstalledComponent::Missing ? QObject::tr("Not installed") : QObject::tr("Unable to inspect");
        if(manager) CloseServiceHandle(manager);
        return item;
    }
    DWORD needed=0; QueryServiceConfigW(service,nullptr,0,&needed);
    if(needed>0 && needed<1024*1024) {
        std::vector<char> data(needed); auto* config=reinterpret_cast<QUERY_SERVICE_CONFIGW*>(data.data());
        if(QueryServiceConfigW(service,config,needed,&needed)) {
            QString path=QString::fromWCharArray(config->lpBinaryPathName).trimmed();
            if(path.startsWith('"') && path.endsWith('"')) path=path.mid(1,path.size()-2);
            wchar_t windows[MAX_PATH]; const auto length=GetWindowsDirectoryW(windows,MAX_PATH);
            if(length && length<MAX_PATH) path.replace("\\SystemRoot",QString::fromWCharArray(windows),Qt::CaseInsensitive);
            path.remove("\\??\\"); item.version=fileVersion(path);
        }
    }
    SERVICE_STATUS status{};
    if(QueryServiceStatus(service,&status)) {
        const auto version=QVersionNumber::fromString(item.version);
        if(!version.isNull() && (version.majorVersion()!=3 || version.minorVersion()<6)) {
            item.status=QObject::tr("Needs a compatible version"); item.state=InstalledComponent::Attention;
        } else {
            item.state=status.dwCurrentState==SERVICE_RUNNING ? InstalledComponent::Ready : InstalledComponent::Attention;
            item.status=status.dwCurrentState==SERVICE_RUNNING ? QObject::tr("Running") : QObject::tr("Installed · stopped");
        }
    } else item.status=QObject::tr("Unable to inspect");
    CloseServiceHandle(service); CloseServiceHandle(manager); return item;
}
#endif
}
QVector<InstalledComponent> InstalledComponents::inspect(const QString& directory) {
    QVector<InstalledComponent> result;
    const QDir root(directory);
    const auto bundle=[&](const QString& name,const QString& file,const QString& knownVersion={}) {
        const auto path=root.filePath(file); const bool exists=QFileInfo(path).isFile();
        QString version=exists ? fileVersion(path) : QString(); if(version.isEmpty() && exists) version=knownVersion;
        result.append({name,QObject::tr("Inside Gozarno"),exists ? QObject::tr("Files available") : QObject::tr("Not found"),version,exists ? InstalledComponent::Ready : InstalledComponent::Missing});
    };
    bundle("OpenConnect","libopenconnect-5.dll",qApp->property("openConnectVersion").toString());
    bundle("Wintun","wintun.dll");
    bundle("ProxiFyre","backend/ProxiFyre.exe");
    bundle("Qt","Qt6Core.dll",QString::fromLatin1(qVersion()));
    bundle("GnuTLS","libgnutls-30.dll",qApp->property("gnutlsVersion").toString());
#ifdef _WIN32
    result.append(packetFilter());
    DWORD release=0;
    bool framework=registryNumber(L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full",L"Release",release,RRF_SUBKEY_WOW6432KEY);
    if(!framework) framework=registryNumber(L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full",L"Release",release);
    QString version=release>=533320 ? "4.8.1" : release>=528040 ? "4.8" : release>=461808 ? "4.7.2" : release>=461308 ? "4.7.1" : release>=460798 ? "4.7" : framework ? QObject::tr("Older than 4.7.2") : QString();
    result.append({".NET Framework",QObject::tr("Windows runtime"),release>=461808 ? QObject::tr("Ready") : framework ? QObject::tr("Needs update") : QObject::tr("Not installed"),version,release>=461808 ? InstalledComponent::Ready : framework ? InstalledComponent::Attention : InstalledComponent::Missing});
    const wchar_t* key=L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64";
    DWORD major=0,minor=0,build=0,revision=0,installed=0;
    DWORD view=RRF_SUBKEY_WOW6464KEY;
    if(!registryNumber(key,L"Installed",installed,view) || !installed) { view=RRF_SUBKEY_WOW6432KEY; registryNumber(key,L"Installed",installed,view); }
    registryNumber(key,L"Major",major,view); registryNumber(key,L"Minor",minor,view); registryNumber(key,L"Bld",build,view); registryNumber(key,L"Rbld",revision,view);
    const bool ready=installed && major>=14 && minor>=44;
    result.append({"Visual C++ Runtime x64",QObject::tr("Windows runtime"),ready ? QObject::tr("Ready") : installed ? QObject::tr("Needs update") : QObject::tr("Not installed"),installed ? QString("%1.%2.%3.%4").arg(major).arg(minor).arg(build).arg(revision) : QString(),ready ? InstalledComponent::Ready : installed ? InstalledComponent::Attention : InstalledComponent::Missing});
#else
    for(const auto& name:{"Windows Packet Filter",".NET Framework","Visual C++ Runtime x64"}) result.append({QString::fromLatin1(name),QObject::tr("Windows"),QObject::tr("Windows only"),{},InstalledComponent::Unknown});
#endif
    return result;
}
