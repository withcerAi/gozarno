// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "GamingProfile.h"
#include <QVector>
#include <atomic>
struct GamingSample {double milliseconds=-1; bool loaded=false;};
struct GamingSummary {
    int attempts=0, replies=0; double median=-1,p95=-1,jitter=-1,failedPercent=0,loadedDelta=-1;
    bool loadComparison=false;
    bool comparable() const {return attempts>=12 && replies>=8;}
    QJsonObject toJson() const;
};
namespace GamingQuality {
GamingSummary summarize(const QVector<GamingSample>&);
bool worse(const GamingSummary& before,const GamingSummary& after);
GamingSample probe(const QString& tunnel,const GamingProfile&,unsigned packetSize=0);
unsigned discoverMtu(const QString& tunnel,const GamingProfile&,unsigned upper,const std::atomic_bool& cancelled);
unsigned tunnelMtu(const QString& tunnel);
quint64 uploadBytes(const QString& tunnel);
QString networkKey(const QString& tunnel);
}
