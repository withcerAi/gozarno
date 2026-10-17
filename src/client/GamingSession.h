// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "GamingMode.h"
#include "GamingQuality.h"
#include <QObject>
#include <QTimer>
#include <QFutureWatcher>
#include <QElapsedTimer>
#include <memory>
class GamingSession : public QObject {
    Q_OBJECT
public:
    explicit GamingSession(QObject* parent=nullptr);
    ~GamingSession() override;
    void setEnabled(bool);
    bool enabled() const {return armed;}
    void setConnection(const QString& tunnel,const QString& vpnProfile,bool online);
    void setTransport(const QString& cipher);
    void reloadProfile();
    QJsonObject report() const;
    bool protectsMatch() const;
    static QList<QJsonObject> scores(const GamingProfile&,const QString& tunnel);
signals:
    void updated(QString message);
    void armedChanged(bool);
private:
    void tick();
    void begin();
    void stop();
    void publish();
    void saveScore();
    bool armed=false, online=false, running=false, adjusted=false, rolledBack=false, qosRequested=false;
    bool udp=false, transportKnown=false;
    bool measuredUdp=false, measuredTransportKnown=false;
    QString qosError;
    QString tunnel, vpnProfile, lastNote, phase, qosState, sessionNetwork;
    GamingProfile profile;
    GamingMode mtu;
    QVector<GamingSample> before,after;
    QTimer timer;
    QElapsedTimer interval;
    quint64 previousUpload=0;
    quint64 epoch=0;
    int successfulWindow=0;
    QFutureWatcher<GamingSample> probeWatcher;
    QFutureWatcher<unsigned> mtuWatcher;
    QFutureWatcher<QString> qosWatcher;
    std::shared_ptr<std::atomic_bool> cancel=std::make_shared<std::atomic_bool>(false);
};
