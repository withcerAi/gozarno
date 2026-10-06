// SPDX-License-Identifier: GPL-2.0-or-later
#include "SessionActivity.h"
#include "client/ConnectionState.h"
#include "client/ClientLanguage.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QPainter>
#include <QShowEvent>
#include <QHideEvent>
#include <functional>
#include <algorithm>

namespace {
QString bytes(quint64 value) {
    double amount=value; const QStringList units{"B","KiB","MiB","GiB","TiB"}; int unit=0;
    while(amount>=1024 && unit<units.size()-1) { amount/=1024; ++unit; }
    return QString::number(amount,'f',unit==0 ? 0 : 1)+" "+units[unit];
}
QLabel* label(const QString& value,const char* role,QWidget* parent) {
    auto* item=new QLabel(value,parent); item->setProperty("role",role); item->setWordWrap(true);
    item->setTextInteractionFlags(Qt::TextSelectableByMouse); return item;
}
class TrafficChart : public QWidget {
public:
    TrafficChart(const std::deque<QPointF>& data,QWidget* parent) : QWidget(parent),data(data) { setMinimumHeight(118); setMaximumHeight(148); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const QRectF plot=rect().adjusted(4,8,-4,-8); p.setPen(QPen(QColor("#456459"),1));
        for(int i=0;i<4;++i) { const auto y=plot.top()+plot.height()*i/3; p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y)); }
        if(data.empty()) { p.setPen(palette().color(QPalette::PlaceholderText)); p.drawText(rect(),Qt::AlignCenter,tr("Traffic history appears during a connection")); return; }
        double maximum=1024; for(const auto& point:data) maximum=std::max(maximum,std::max(point.x(),point.y()));
        for(int channel=0;channel<2;++channel) {
            QPolygonF line; int i=0;
            for(const auto& point:data) { const double value=channel==0 ? point.x() : point.y(); line<<QPointF(plot.right()-((data.size()-1-i++)/59.0)*plot.width(),plot.bottom()-value/maximum*plot.height()); }
            p.setPen(QPen(QColor(channel==0 ? "#64deb4" : "#71b8ee"),2)); if(line.size()==1) p.drawEllipse(line.first(),2,2); else p.drawPolyline(line);
        }
    }
private: const std::deque<QPointF>& data;
};
}
SessionActivity::SessionActivity(QWidget* parent) : QWidget(parent) {
    setObjectName("sessionActivity"); auto* root=new QVBoxLayout(this); root->setContentsMargins(0,0,0,0); root->setSpacing(18);
    auto* frame=new QFrame(this); frame->setProperty("card",true); auto* layout=new QVBoxLayout(frame); layout->setContentsMargins(24,24,24,24); layout->setSpacing(16);
    auto* heading=new QHBoxLayout; heading->addWidget(label(tr("Session overview"),"section",frame)); heading->addStretch();
    stateLabel=label(tr("No active connection"),"muted",frame); stateLabel->setWordWrap(false); stateLabel->setObjectName("sessionState"); heading->addWidget(stateLabel); layout->addLayout(heading);
    auto* metrics=new QGridLayout; metrics->setHorizontalSpacing(24);
    int column=0; for(const auto& entry:QList<QPair<QString,QString>>{{"received",tr("DOWNLOADED")},{"sent",tr("UPLOADED")},{"duration",tr("CONNECTED FOR")}}) {
        metrics->addWidget(label(entry.second,"eyebrow",frame),0,column); auto* value=label(entry.first=="duration" ? "00:00:00" : "0 B","metric",frame);
        value->setObjectName("session_"+entry.first); values[entry.first]=value; metrics->addWidget(value,1,column++);
        ClientLanguage::technical(value);
    } layout->addLayout(metrics);
    rateLabel=label(tr("↓ Download 0 B/s     ↑ Upload 0 B/s"),"muted",frame); rateLabel->setObjectName("sessionRate"); layout->addWidget(rateLabel);
    chart=new TrafficChart(samples,frame); layout->addWidget(chart); layout->addWidget(label(tr("Green: download · Blue: upload · Last 60 samples"),"muted",frame)); root->addWidget(frame);
    auto* details=new QFrame(this); details->setProperty("card",true); auto* grid=new QGridLayout(details); grid->setContentsMargins(24,24,24,24); grid->setHorizontalSpacing(24); grid->setVerticalSpacing(14);
    grid->addWidget(label(tr("Network & encryption"),"section",details),0,0,1,2); int row=1;
    for(const auto& entry:QList<QPair<QString,QString>>{{"profile",tr("Server profile")},{"ip",tr("IPv4 address")},{"ip6",tr("IPv6 address")},{"dns",tr("DNS servers")},{"tls",tr("TLS encryption")},{"dtls",tr("DTLS encryption")}}) {
        grid->addWidget(label(entry.second,"muted",details),row,0); auto* value=label(tr("Available after connecting"),"value",details); value->setObjectName("session_"+entry.first); values[entry.first]=value; grid->addWidget(value,row++,1);
    } grid->setColumnStretch(1,1); root->addWidget(details); root->addStretch();
    durationTimer.setInterval(1000); connect(&durationTimer,&QTimer::timeout,this,[this]{refreshDuration();});
}
void SessionActivity::setProfile(const QString& name) {
    selectedProfile=name;
    if(!hasSession) values["profile"]->setText(name.isEmpty() ? tr("Select a server") : name);
}
void SessionActivity::setSessionInfo(const QString& dns,const QString& ip,const QString& ip6,const QString& tls,const QString& dtls) {
    const QList<QPair<QString,QString>> fields{{"dns",dns},{"ip",ip},{"ip6",ip6},{"tls",tls},{"dtls",dtls}};
    for(const auto& field:fields) {
        auto* value=values[field.first]; value->setText(field.second.isEmpty() ? tr("Not provided by this connection") : field.second);
        value->setProperty("technical",!field.second.isEmpty());
        value->setLayoutDirection(field.second.isEmpty() ? layoutDirection() : Qt::LeftToRight);
        value->setAlignment(field.second.isEmpty() && ClientLanguage::isPersian() ? Qt::AlignRight|Qt::AlignVCenter|Qt::AlignAbsolute : Qt::AlignLeft|Qt::AlignVCenter|Qt::AlignAbsolute);
    }
}
void SessionActivity::setConnectionState(int state) {
    if(state==STATUS_CONNECTED && !connected) {
        connected=true; hasSession=true; elapsed.start(); previousSample=0; previousSent=previousReceived=0; finalDuration=0; samples.clear();
        values["profile"]->setText(selectedProfile);
        values["sent"]->setText("0 B"); values["received"]->setText("0 B"); refreshDuration(); if(isVisible()) durationTimer.start();
    } else if(state!=STATUS_CONNECTED && connected) { finalDuration=elapsed.elapsed(); connected=false; durationTimer.stop(); refreshDuration(); }
    stateLabel->setText(state==STATUS_CONNECTED ? tr("Live connection") : state==STATUS_CONNECTING ? tr("Connecting…") : state==STATUS_DISCONNECTING ? tr("Disconnecting…") : hasSession ? tr("Last session · disconnected") : tr("No active connection"));
    if(!connected) rateLabel->setText(tr("↓ Download 0 B/s     ↑ Upload 0 B/s"));
    chart->update();
}
void SessionActivity::updateTraffic(quint64 sent,quint64 received) {
    if(!connected) return;
    const qint64 now=elapsed.elapsed(); const qint64 interval=now-previousSample;
    if(interval<=0) return;
    const double upload=(sent>=previousSent ? sent-previousSent : sent)*1000.0/interval;
    const double download=(received>=previousReceived ? received-previousReceived : received)*1000.0/interval;
    previousSent=sent; previousReceived=received; previousSample=now;
    values["sent"]->setText(bytes(sent)); values["received"]->setText(bytes(received));
    rateLabel->setText(tr("↓ Download %1/s     ↑ Upload %2/s").arg(bytes(quint64(download)),bytes(quint64(upload))));
    samples.emplace_back(download,upload); if(samples.size()>60) samples.pop_front(); if(isVisible()) chart->update();
}
void SessionActivity::refreshDuration() {
    const qint64 seconds=(connected ? elapsed.elapsed() : finalDuration)/1000;
    values["duration"]->setText(QString("%1:%2:%3").arg(seconds/3600,2,10,QChar('0')).arg(seconds/60%60,2,10,QChar('0')).arg(seconds%60,2,10,QChar('0')));
}
void SessionActivity::showEvent(QShowEvent* event) { QWidget::showEvent(event); refreshDuration(); if(connected) durationTimer.start(); }
void SessionActivity::hideEvent(QHideEvent* event) { durationTimer.stop(); QWidget::hideEvent(event); }
