// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QHostAddress>
struct ProxySettings {
    bool enabled=false, lan=false;
    int port=1080;
    QString lanAddress, username="client", password;
    static ProxySettings load();
    bool validate(QString& error) const;
    bool save(QString& error) const;
    bool permits(const QHostAddress& peer) const;
};
