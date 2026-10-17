// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QJsonObject>
#include <atomic>
#include <memory>
namespace PublicIpLookup {
QJsonObject fetch(const QString& tunnel, const std::shared_ptr<std::atomic_bool>& cancelled);
QJsonObject parse(const QByteArray& body);
}
