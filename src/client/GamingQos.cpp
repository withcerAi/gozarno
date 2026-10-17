// SPDX-License-Identifier: GPL-2.0-or-later
#include "GamingQos.h"
#include "OcSettings.h"
#include <QJsonDocument>
#include <QUuid>
#include <QRegularExpression>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
QMutex serialization;
bool execute(const QJsonArray& entries,bool remove,QString& error) {
    if(entries.isEmpty()) return true;
#ifdef _WIN32
    QProcess process; const QString source=GamingQos::script(remove);
    const QByteArray utf16(reinterpret_cast<const char*>(source.utf16()),source.size()*2);
    auto environment=QProcessEnvironment::systemEnvironment();
    environment.insert("GOZARNO_QOS_PLAN",QString::fromLatin1(QJsonDocument(entries).toJson(QJsonDocument::Compact).toBase64()));
    process.setProcessEnvironment(environment);
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {args->flags|=CREATE_NO_WINDOW;});
    wchar_t systemDirectory[MAX_PATH]; const UINT length=GetSystemDirectoryW(systemDirectory,MAX_PATH);
    if(!length || length>=MAX_PATH) {error=QObject::tr("Gaming upload limits require Windows."); return false;}
    const QString executable=QString::fromWCharArray(systemDirectory)+"/WindowsPowerShell/v1.0/powershell.exe";
    process.start(executable,{"-NoLogo","-NoProfile","-NonInteractive","-EncodedCommand",QString::fromLatin1(utf16.toBase64())});
    if(!process.waitForStarted(3000) || !process.waitForFinished(20000)) {
        process.kill(); process.waitForFinished(3000); error=QObject::tr("Gaming upload policy timed out. Recovery information has been kept."); return false;
    }
    if(process.exitStatus()!=QProcess::NormalExit || process.exitCode()!=0) {
        error=QObject::tr("Windows could not apply or remove gaming upload limits. Administrator access and NetQos support are required."); return false;
    }
    return true;
#else
    Q_UNUSED(remove); error=QObject::tr("Gaming upload limits require Windows."); return false;
#endif
}
bool restoreLocked(QString& error) {
    OcSettings settings; const auto values=QJsonDocument::fromJson(settings.value("Client/gamingQosJournal").toByteArray()).array();
    QJsonArray owned; for(const auto& value:values) if(GamingQos::validOwnedName(value.toObject().value("name").toString())) owned.append(value);
    if(!execute(owned,true,error)) return false;
    settings.remove("Client/gamingQosJournal"); settings.sync(); return true;
}
}
namespace GamingQos {
bool validOwnedName(const QString& name) {return QRegularExpression("^GozarnoGaming-[0-9a-f]{32}-[0-9]{1,2}$").match(name).hasMatch();}
QJsonArray plan(const GamingProfile& profile) {
    GamingProfile checked; QString error; if(!GamingProfile::fromJson(profile.toJson(),checked,error)) return {};
    QJsonArray entries; QString id=QUuid::createUuid().toString(QUuid::Id128); int index=0;
    for(const auto& path:checked.background) entries.append(QJsonObject{{"name","GozarnoGaming-"+id+"-"+QString::number(index++)},
        {"path",QDir::toNativeSeparators(path)},{"bits",qint64(checked.backgroundKbps)*1000}});
    return entries;
}
QString script(bool remove) {
    const QString prefix=QStringLiteral("$ErrorActionPreference='Stop'; try { Import-Module (Join-Path $PSHOME 'Modules\\NetQos\\NetQos.psd1') -ErrorAction Stop; $plan=[Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($env:GOZARNO_QOS_PLAN)) | ConvertFrom-Json; foreach($entry in $plan) { if($entry.name -notmatch '^GozarnoGaming-[0-9a-f]{32}-[0-9]{1,2}$') { throw 'invalid owner' }; ");
    const QString operation=remove
        ? QStringLiteral("$policy=Get-NetQosPolicy -Name $entry.name -PolicyStore ActiveStore -ErrorAction SilentlyContinue; if($policy) { if($policy.AppPathName -ne $entry.path) {throw 'policy ownership mismatch'}; Remove-NetQosPolicy -Name $entry.name -PolicyStore ActiveStore -Confirm:$false -ErrorAction Stop }; ")
        : QStringLiteral("New-NetQosPolicy -Name $entry.name -AppPathNameMatchCondition $entry.path -ThrottleRateActionBitsPerSecond ([UInt64]$entry.bits) -PolicyStore ActiveStore -NetworkProfile All -ErrorAction Stop | Out-Null; ");
    return prefix+operation+QStringLiteral("}; exit 0 } catch { exit 1 }");
}
bool restore(QString& error) {QMutexLocker lock(&serialization); return restoreLocked(error);}
bool apply(const GamingProfile& profile,const std::atomic_bool& cancelled,QString& error) {
    QMutexLocker lock(&serialization);
    if(!restoreLocked(error)) return false;
    if(cancelled.load()) return true;
    const auto entries=plan(profile); if(entries.isEmpty()) return true;
    OcSettings settings; settings.setValue("Client/gamingQosJournal",QJsonDocument(entries).toJson(QJsonDocument::Compact)); settings.sync();
    if(settings.status()!=QSettings::NoError) {error=QObject::tr("Cannot save gaming upload recovery information."); return false;}
    const bool success=execute(entries,false,error);
    if(!success || cancelled.load()) {QString cleanup; if(!restoreLocked(cleanup)) error=cleanup; return false;}
    return true;
}
}
