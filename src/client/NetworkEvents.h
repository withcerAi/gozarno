// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QTimer>
#include <QAbstractNativeEventFilter>
class NetworkEvents : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit NetworkEvents(QObject* parent=nullptr);
    ~NetworkEvents() override;
    void setTunnel(const QString& name);
    static bool physicalNetworkAvailable(const QString& tunnel);
    bool nativeEventFilter(const QByteArray&, void*, qintptr*) override;
signals:
    void environmentChanged(QString reason);
private:
    QString fingerprint() const;
    void check();
    void schedule(const QString& reason);
    QTimer poll, settle;
    QString tunnel, previous, pending;
    qint64 lastTick=0;
};
