// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QList>
#include <QHostAddress>
#include <memory>
// Temporary WFP protection. Closing the engine/process removes every rule.
class KillSwitch {
public:
    KillSwitch();
    ~KillSwitch();
    bool arm(const QList<QHostAddress>& gateways, QString& error);
    bool allowTunnel(const QString& name, QString& error);
    bool blockTunnel(QString& error);
    bool release(QString& error);
    bool active() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
