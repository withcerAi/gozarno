#pragma once

#include <QObject>
#include <QSettings>

// This class is a thin wrapper over QSettings that ensures we retain
// Gozarno uses its own settings namespace so installing alongside
// OpenConnect GUI never changes that application's profiles or preferences.
//
// Modify it when settings should become intentionally incompatible.
class OcSettings : public QSettings {
public:
    OcSettings() : QSettings(QSettings::defaultFormat(), QSettings::UserScope, "Gozarno", "GozarnoVPN") { };
    static bool migrateLegacy() {
        OcSettings destination;
        if(destination.value("Client/migratedLegacyNames",false).toBool()) return true;
        for(const auto& brand:{QString("Qivaryn"),QString("Arovan")}) {
            QSettings source(QSettings::defaultFormat(),QSettings::UserScope,brand,brand+"VPN");
            const auto existing=destination.childGroups();
            for(const auto& key:source.allKeys()) {
                // Never mix credentials from different profiles with the same name.
                if(key.startsWith("server:") && existing.contains(key.section('/',0,0))) continue;
                if(!destination.contains(key)) destination.setValue(key,source.value(key));
            }
        }
        destination.setValue("Client/migratedLegacyNames",true); destination.sync();
        return destination.status()==QSettings::NoError;
    }
};
