// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QJsonObject>
#include <QHostAddress>
#include <QList>
#include <functional>
namespace ProtectionValidation {
// Runs in a worker. All WFP rules are temporary and owned by this invocation.
// "skipped" is never reported as a passed leak test.
QJsonObject run(const QString& tunnel,const QList<QHostAddress>& gateways,const QStringList& dns,
    const std::function<void()>& reconnect={},const std::function<bool()>& cancelled={});
}
