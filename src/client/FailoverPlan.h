// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QStringList>
class FailoverPlan {
public:
    void reset(const QString& primary,const QStringList& approved,const QStringList& available) {
        profiles={primary}; index=0;
        for(const auto& item:approved)
            if(available.contains(item) && !profiles.contains(item)) profiles.append(item);
    }
    QString current() const { return profiles.value(index); }
    QString next() { return index+1<profiles.size() ? profiles.at(++index) : QString(); }
    bool hasNext() const { return index+1<profiles.size(); }
    QStringList approved() const { return profiles; }
private:
    QStringList profiles;
    int index=0;
};
