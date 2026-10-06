// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QString>
#include <QVector>
struct InstalledComponent {
    enum State { Ready, Missing, Attention, Unknown };
    QString name,location,status,version;
    State state=Unknown;
};
namespace InstalledComponents {
QVector<InstalledComponent> inspect(const QString& applicationDirectory);
}
