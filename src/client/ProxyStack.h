// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QStringList>
#include <QJsonObject>
#include <memory>
#include <functional>
#include "ProxySettings.h"
// One stack per application, confined to the VPN worker thread. No OS adapter.
class ProxyStack {
public:
    ProxyStack();
    ~ProxyStack();
    bool start(const QString& address,int mtu,const QStringList& dns,const ProxySettings&,QString& error);
    int readPacket(unsigned char*,int);
    int writePacket(const unsigned char*,int);
    QString endpoints() const;
    QJsonObject diagnostics() const;
    void setNoticeHandler(std::function<void(QString)> handler);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
