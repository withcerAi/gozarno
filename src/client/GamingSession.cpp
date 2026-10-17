// SPDX-License-Identifier: GPL-2.0-or-later
#include "GamingSession.h"
#include "GamingQos.h"
#include "OcSettings.h"
#include <QtConcurrent>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <algorithm>
namespace {
QString identity(const GamingProfile& profile) {return QString::fromLatin1(QCryptographicHash::hash((profile.target+"|"+profile.probe+"|"+QString::number(profile.port)).toUtf8(),QCryptographicHash::Sha256).toHex());}
}
GamingSession::GamingSession(QObject* parent):QObject(parent) {
    profile=GamingProfile::automaticPreset(); timer.setInterval(1000); connect(&timer,&QTimer::timeout,this,&GamingSession::tick);
    // Recover only journalled policies from this application; no blanket removal.
    connect(&qosWatcher,&QFutureWatcher<QString>::finished,this,[this] {qosError=qosWatcher.result(); qosState=qosError.isEmpty()?tr("Background upload policies ready"):qosError; publish();});
    qosWatcher.setFuture(QtConcurrent::run([] {QString error; GamingQos::restore(error); return error;}));
}
GamingSession::~GamingSession() {stop(); probeWatcher.waitForFinished(); mtuWatcher.waitForFinished(); qosWatcher.waitForFinished(); QString error; GamingQos::restore(error);}
void GamingSession::setEnabled(bool value) {
    if(armed==value) return;
    armed=value; emit armedChanged(value);
    if(!armed) {stop(); timer.stop();} else {timer.start(); tick();} publish();
}
void GamingSession::setConnection(const QString& adapter,const QString& name,bool connected) {
    if(!connected || adapter!=tunnel || name!=vpnProfile) stop();
    tunnel=adapter; vpnProfile=name; online=connected;
    if(armed) {timer.start(); tick();} publish();
}
void GamingSession::setTransport(const QString& cipher) {transportKnown=true; udp=!cipher.isEmpty(); if(running) {measuredTransportKnown=true; measuredUdp=udp;} publish();}
void GamingSession::reloadProfile() {stop(); profile=GamingProfile::automaticPreset(); if(armed) tick(); publish();}
bool GamingSession::protectsMatch() const {return armed;}
void GamingSession::begin() {
    profile=GamingProfile::automaticPreset();
    running=true; adjusted=false; rolledBack=false; qosRequested=false; before.clear(); after.clear(); successfulWindow=0; ++epoch;
    cancel=std::make_shared<std::atomic_bool>(false); lastNote.clear(); phase=tr("Measuring baseline before changes");
    sessionNetwork=GamingQuality::networkKey(tunnel);
    measuredTransportKnown=transportKnown; measuredUdp=udp;
    interval.start(); previousUpload=GamingQuality::uploadBytes(tunnel); qosState=tr("Waiting for baseline before upload limits");
}
void GamingSession::stop() {
    cancel->store(true); ++epoch;
    if(running) saveScore();
    running=false;
    QString error; if(!mtu.disable(&error)) lastNote=error;
    if(!qosWatcher.isRunning()) qosWatcher.setFuture(QtConcurrent::run([] {QString e; GamingQos::restore(e); return e;}));
    transportKnown=false; udp=false;
}
void GamingSession::tick() {
    if(!armed || !online || tunnel.isEmpty()) return;
    const bool detected=!profile.automatic || GamingProfile::processesRunning(profile.applications);
    if(!detected) {if(running) stop(); phase=tr("Waiting for the selected game to start"); publish(); return;}
    if(running && GamingQuality::networkKey(tunnel)!=sessionNetwork) {stop();}
    if(!running) begin();
    if((adjusted || profile.target.isEmpty()) && !qosRequested && !qosWatcher.isRunning()) {
        qosRequested=true; const auto selected=profile; const auto cancelled=cancel;
        qosWatcher.setFuture(QtConcurrent::run([selected,cancelled] {QString e; GamingQos::apply(selected,*cancelled,e); return e;}));
    }
    if(profile.target.isEmpty()) {phase=tr("Set a game destination IP to measure quality and discover MTU");
        if(!adjusted && !profile.autoMtu) {adjusted=true; QString error; if(!mtu.enable(tunnel,profile.mtu,error)) lastNote=error; else lastNote=tr("Manual tunnel MTU: %1").arg(profile.mtu);}
        publish(); return;
    }
    if(probeWatcher.isRunning() || mtuWatcher.isRunning()) return;
    if(!adjusted && qosWatcher.isRunning()) return;
    if(!adjusted && before.size()>=12) {
        if(profile.probe=="icmp" && GamingQuality::summarize(before).replies<3) {
            // Some providers filter echo requests. Measure a normal HTTPS
            // handshake instead; never treat echo silence as a broken game.
            profile.probe="tcp"; before.clear(); publish(); return;
        }
        adjusted=true; phase=tr("Applying gaming settings");
        if(profile.autoMtu && (profile.probe!="icmp" || GamingQuality::summarize(before).replies<8)) {
            lastNote=tr("Automatic MTU unverified; negotiated size retained");
            phase=tr("Measuring after changes"); publish(); return;
        }
        if(profile.autoMtu) {
            const QString adapter=tunnel; const auto selected=profile; const auto cancelled=cancel; const unsigned upper=GamingQuality::tunnelMtu(tunnel); const auto generation=epoch;
            disconnect(&mtuWatcher,nullptr,this,nullptr);
            connect(&mtuWatcher,&QFutureWatcher<unsigned>::finished,this,[this,generation] {
                if(epoch!=generation || !running) return;
                const auto size=mtuWatcher.result(); QString error;
                if(size && mtu.enable(tunnel,size,error)) lastNote=tr("ICMP-confirmed packet size: %1 · game UDP MTU is unverified").arg(size);
                else lastNote=error.isEmpty()?tr("Automatic MTU unverified; negotiated size retained"):error;
                phase=tr("Measuring after changes"); interval.restart(); previousUpload=GamingQuality::uploadBytes(tunnel); publish();
            });
            mtuWatcher.setFuture(QtConcurrent::run([adapter,selected,upper,cancelled] {return GamingQuality::discoverMtu(adapter,selected,upper,*cancelled);})); publish(); return;
        }
        QString error; if(!mtu.enable(tunnel,profile.mtu,error)) lastNote=error; else lastNote=tr("Manual tunnel MTU: %1").arg(profile.mtu);
        phase=tr("Measuring after changes");
    }
    if(adjusted && !profile.background.isEmpty() && (!qosRequested || qosWatcher.isRunning())) {publish(); return;}
    const QString adapter=tunnel; const auto selected=profile; const auto generation=epoch;
    const auto bytes=GamingQuality::uploadBytes(tunnel); const qint64 elapsed=interval.restart();
    const bool loaded=profile.uploadKbps && elapsed>0 && bytes>=previousUpload && (bytes-previousUpload)*8./elapsed>=profile.uploadKbps*.8;
    previousUpload=bytes;
    disconnect(&probeWatcher,nullptr,this,nullptr);
    connect(&probeWatcher,&QFutureWatcher<GamingSample>::finished,this,[this,generation,loaded] {
        if(epoch!=generation || !running) return;
        auto sample=probeWatcher.result(); sample.loaded=loaded;
        auto& samples=adjusted?after:before; samples.append(sample); if(samples.size()>120) samples.removeFirst();
        if(adjusted && !rolledBack && after.size()>=24 && GamingQuality::worse(GamingQuality::summarize(before),GamingQuality::summarize(after))) {
            rolledBack=true; QString error; const bool restored=mtu.disable(&error); lastNote=restored?tr("Quality worsened; tunnel MTU restored. Upload limits remain separately controlled."):error;
        }
        if(after.size()>=24 && ++successfulWindow%12==1) saveScore();
        publish();
    });
    const auto cancelled=cancel;
    probeWatcher.setFuture(QtConcurrent::run([adapter,selected,cancelled] {return cancelled->load()?GamingSample{}:GamingQuality::probe(adapter,selected);}));
}
void GamingSession::publish() {
    if(!armed) {emit updated(tr("Gaming mode is off.")+(qosError.isEmpty()?QString():"\n"+qosError)); return;}
    if(!online) {emit updated(tr("Automatic gaming is ready. Connect to start.")); return;}
    QString message=tr("Automatic gaming is active.");
    const auto summary=GamingQuality::summarize(after.isEmpty()?before:after);
    if(summary.replies>=3) message+="\n"+tr("Connection delay: %1 ms · general network probe").arg(summary.median,0,'f',0);
    else message+="\n"+(before.size()>=12?tr("Network measurement unavailable; connection settings retained."):tr("Checking connection automatically…"));
    if(rolledBack) message+="\n"+tr("Previous packet size restored automatically.");
    emit updated(message);
}
void GamingSession::saveScore() {
    const auto summary=GamingQuality::summarize(after.isEmpty()?before:after); if(!summary.comparable()) return;
    OcSettings settings; auto scores=QJsonDocument::fromJson(settings.value("Client/gamingScores").toByteArray()).array(); QJsonArray next;
    const auto key=identity(profile); const auto network=sessionNetwork;
    for(const auto& entry:scores) {const auto o=entry.toObject(); if(o.value("vpn").toString()!=vpnProfile || o.value("game").toString()!=key || o.value("network").toString()!=network) next.append(entry);}
    auto value=summary.toJson(); value.insert("vpn",vpnProfile); value.insert("game",key); value.insert("network",network);
    value.insert("udp",udp); value.insert("utc",QDateTime::currentDateTimeUtc().toSecsSinceEpoch()); next.append(value);
    while(next.size()>64) next.removeAt(0);
    settings.setValue("Client/gamingScores",QJsonDocument(next).toJson(QJsonDocument::Compact));
}
QJsonObject GamingSession::report() const {
    return {{"schema","gozarno-gaming-quality-v1"},{"application_version",QCoreApplication::applicationVersion()},
        {"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"method",profile.probe},
        {"udp_verified",measuredTransportKnown && measuredUdp},{"mtu_rolled_back",rolledBack},
        {"before",GamingQuality::summarize(before).toJson()},{"after",GamingQuality::summarize(after).toJson()},
        {"privacy","No credentials, addresses, server/profile names, executable paths or network identifiers."},
        {"measurement","Probe RTT/handshake observations, not game packet-loss or controlled bufferbloat proof."}};
}
QList<QJsonObject> GamingSession::scores(const GamingProfile& profile,const QString& tunnel) {
    OcSettings s; QList<QJsonObject> result; const auto game=identity(profile), network=GamingQuality::networkKey(tunnel);
    const qint64 now=QDateTime::currentDateTimeUtc().toSecsSinceEpoch();
    for(const auto& entry:QJsonDocument::fromJson(s.value("Client/gamingScores").toByteArray()).array()) {
        const auto o=entry.toObject(); const auto timestamp=o.value("utc").toVariant().toLongLong();
        if(o.value("game").toString()==game && o.value("network").toString()==network && timestamp<=now && now-timestamp<=3600 && o.value("replies").toInt()>=8) result.append(o);
    }
    const auto score=[](const QJsonObject& o) {return o.value("p95_ms").toDouble(10000)+2*o.value("failed_probe_percent").toDouble(100)+(o.value("udp").toBool()?0:20);};
    std::sort(result.begin(),result.end(),[&](const auto& a,const auto& b){return score(a)<score(b);}); return result;
}
