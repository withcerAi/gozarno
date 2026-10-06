// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "TrafficPolicy.h"
#include <QHostAddress>
#include <QStringList>
#include <memory>

// Owns only this client's routes. Created before the upstream tunnel is configured.
class VpnRoutes {
public:
    explicit VpnRoutes(TrafficPolicy policy);
    ~VpnRoutes();
    bool prepare(const QString& gateway, QString& error);
    bool apply(const QString& interfaceName, const QStringList& dns, QString& error);
    void restore();
    quint32 interfaceIndex(bool tunnel, bool ipv6 = false) const;
    QHostAddress sourceAddress(bool tunnel, bool ipv6 = false) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
