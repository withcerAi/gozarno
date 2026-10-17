// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QStringList>
#include <QList>
#include <atomic>
struct DiscoveredGame {QString name, executable, source;};
namespace GameDiscovery {
QList<DiscoveredGame> scan(const QStringList& extraRoots,const std::atomic_bool& cancelled);
QList<DiscoveredGame> steam(const QString& root,const std::atomic_bool& cancelled);
QList<DiscoveredGame> epic(const QString& manifests,const std::atomic_bool& cancelled);
QList<DiscoveredGame> folder(const QString& root,const std::atomic_bool& cancelled);
}
