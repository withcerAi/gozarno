// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "GamingProfile.h"
#include <QJsonArray>
#include <atomic>
namespace GamingQos {
QJsonArray plan(const GamingProfile&);
bool validOwnedName(const QString&);
QString script(bool remove);
bool restore(QString& error);
bool apply(const GamingProfile&,const std::atomic_bool& cancelled,QString& error);
}
