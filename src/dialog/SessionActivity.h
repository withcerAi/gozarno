// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
#include <QElapsedTimer>
#include <QLabel>
#include <QTimer>
#include <QMap>
#include <deque>

class SessionActivity : public QWidget {
public:
    explicit SessionActivity(QWidget* parent=nullptr);
    void setProfile(const QString& name);
    void setSessionInfo(const QString& dns,const QString& ip,const QString& ip6,const QString& tls,const QString& dtls);
    void setConnectionState(int state);
    void updateTraffic(quint64 sent,quint64 received);
protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
private:
    void refreshDuration();
    QMap<QString,QLabel*> values;
    QLabel* stateLabel;
    QLabel* rateLabel;
    QWidget* chart;
    QTimer durationTimer;
    QElapsedTimer elapsed;
    qint64 previousSample=0, finalDuration=0;
    quint64 previousSent=0, previousReceived=0;
    bool connected=false, hasSession=false;
    QString selectedProfile;
    std::deque<QPointF> samples;
};
