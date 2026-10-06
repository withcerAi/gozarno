#include "client/ClientLanguage.h"
#include "dialog/AboutDialog.h"
/*
 * Copyright (C) 2014 Red Hat
 * Copyright (C) 2016 by Lubomír Carik <Lubomir.Carik@gmail.com>
 *
 * This file is part of openconnect-gui.
 *
 * openconnect-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"
#include "config.h"
#include "dialog/MyInputDialog.h"
#include "dialog/mainwindow.h"
#include "openconnect-gui.h"

#include "FileLogger.h"
#include "logger.h"

extern "C" {
#include <gnutls/pkcs11.h>
}

#include <QApplication>
#if !defined(_WIN32) && !defined(PROJ_GNUTLS_DEBUG)
#include <QMessageBox>
#endif
#include <QCommandLineParser>
#include <OcSettings.h>
#include <QtSingleApplication>
#include <QTimer>
#include <QDir>
#include <QTabWidget>
#include <QComboBox>
#include "dialog/NewProfileDialog.h"
#include "dialog/editdialog.h"
#include "dialog/TrafficPolicyDialog.h"
#include "client/TrafficPolicy.h"

#ifdef __MACH__
#include <Security/Security.h>
#include <mach-o/dyld.h>
#endif

#include <csignal>
#include <cstdio>

static void log_callback(int level, const char* str)
{
    Logger::instance().addMessage(QString(str).trimmed(),
        Logger::MessageType::DEBUG,
        Logger::ComponentType::GNUTLS);
}

#if defined(Q_OS_MACOS) && defined(PROJ_ADMIN_PRIV_ELEVATION)
bool relaunch_as_root()
{
    QMessageBox msgBox;
    char appPath[2048];
    uint32_t size = sizeof(appPath);
    AuthorizationRef authRef;
    OSStatus status;

    /* Get the path of the current program */
    if (_NSGetExecutablePath(appPath, &size) != 0) {
        msgBox.setText(QObject::tr("Could not get program path to elevate privileges."));
        return false;
    }

    status = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
        kAuthorizationFlagDefaults, &authRef);

    if (status != errAuthorizationSuccess) {
        msgBox.setText(QObject::tr("Failed to create authorization reference."));
        return false;
    }
    status = AuthorizationExecuteWithPrivileges(authRef, appPath,
        kAuthorizationFlagDefaults, NULL, NULL);
    AuthorizationFree(authRef, kAuthorizationFlagDestroyRights);

    if (status == errAuthorizationSuccess) {
        /* We've successfully re-launched with root privs. */
        return true;
    }

    return false;
}
#endif

int pin_callback(void* userdata, int attempt, const char* token_url,
    const char* token_label, unsigned flags, char* pin,
    size_t pin_max)
{
    QString type = QObject::tr("user");
    if (flags & GNUTLS_PIN_SO) {
        type = QObject::tr("security officer");
    }

    QString outtext = QObject::tr("Please enter the %1 PIN for %2.").arg(type).arg(token_label);
    if (flags & GNUTLS_PKCS11_PIN_FINAL_TRY) {
        outtext += QObject::tr(" This is the FINAL try!");
    }
    if (flags & GNUTLS_PKCS11_PIN_COUNT_LOW) {
        outtext += QObject::tr(" Only few tries before token lock!");
    }

    MainWindow* w = (MainWindow*)userdata;
    MyInputDialog dialog(w, QLatin1String(token_url), outtext, QLineEdit::Password);
    dialog.show();

    QString text;
    bool ok = dialog.result(text);
    if (ok == false) {
        return -1;
    }

    snprintf(pin, pin_max, "%s", text.toUtf8().data());
    return 0;
}

int main(int argc, char* argv[])
{
    bool haveTray = false;

    qputenv("LOG2FILE", "1");

    qRegisterMetaType<Logger::Message>();

#if defined(Q_OS_MACOS) && defined(PROJ_ADMIN_PRIV_ELEVATION)
    /* Re-launching with root privs on OS X needs Qt to allow setsuid */
    QApplication::setSetuidAllowed(true);
#endif
    QCoreApplication::setApplicationName(APP_NAME);
    QCoreApplication::setApplicationVersion(PROJECT_VERSION);
    QCoreApplication::setOrganizationName(PRODUCT_NAME_COMPANY);
    QCoreApplication::setOrganizationDomain(PRODUCT_NAME_COMPANY_DOMAIN);

    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
    QtSingleApplication app(argc, argv);
    app.setApplicationDisplayName(APP_NAME);
    app.setQuitOnLastWindowClosed(false);

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        haveTray=true;
    }

#if defined(Q_OS_MACOS) && defined(PROJ_ADMIN_PRIV_ELEVATION)
    if (geteuid() != 0) {
        if (relaunch_as_root()) {
            /* We have re-launched with root privs. Exit this process. */
            return 0;
        }

        QMessageBox msgBox;
        msgBox.setText(QObject::tr("This program requires root privileges to fully function."));
        msgBox.setInformativeText(QObject::tr("VPN connection establishment would fail."));
        msgBox.exec();
        return -1;
    }
#endif

    gnutls_global_init();
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    openconnect_init_ssl();
    app.setProperty("openConnectVersion",QString::fromUtf8(openconnect_get_version()));
    app.setProperty("gnutlsVersion",QString::fromUtf8(gnutls_check_version(nullptr)));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QObject::tr("OpenConnect is a VPN client, that utilizes TLS and DTLS "
                    "for secure session establishment, and is compatible "
                    "with many VPN protocols."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"preview-dir", QObject::tr("Save isolated interface previews and exit"), "directory"});
    parser.addOption({"preview-sample", QObject::tr("Use fictional profiles for interface previews")});
    parser.addOption({"preview-persian", QObject::tr("Preview the Persian interface")});
    parser.addOption({"preview-light", QObject::tr("Preview the light appearance")});
    parser.addOption({"preview-delay-ms", QObject::tr("Delay isolated preview capture"), "milliseconds", "500"});
    parser.addOption({"preview-compact", QObject::tr("Preview at the minimum window size")});
    parser.addOption({ { "s", "server" },
        QObject::tr("auto-connect to existing profile <name>"),
        QObject::tr("name")

    });

    parser.process(app);

    const QString previewDir = parser.value("preview-dir");
    if (!previewDir.isEmpty()) {
        QDir().mkpath(previewDir);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, previewDir + "/isolated-settings");
        haveTray = false;
        OcSettings previewSettings; previewSettings.setValue("Client/language",parser.isSet("preview-persian") ? "fa" : "en");
        ClientLanguage::apply(previewSettings.value("Client/language").toString());
        if(parser.isSet("preview-sample")) {
            OcSettings settings;
            for(const auto& name : {QString("Europe · Primary"),QString("Gaming · Backup"),QString("Work · Gateway")}) {
                settings.setValue("server:"+name+"/server","https://vpn.example.com");
                settings.setValue("server:"+name+"/protocol-name","anyconnect");
                settings.setValue("server:"+name+"/catalog/notes",QObject::tr("Preview profile · fictional address"));
            }
            settings.setValue("server:Europe · Primary/catalog/favorite",true);
            TrafficPolicy policy; policy.mode="selected"; policy.rules={{"network","198.51.100.0/24","vpn",true},{"domain","updates.example.com","direct",true},{"application","C:/Games/Game.exe","vpn",true}};
            QString error; policy.save("Europe · Primary",error);
        }
    }

    if(previewDir.isEmpty()) {
        if(!OcSettings::migrateLegacy()) QMessageBox::warning(nullptr,APP_NAME,QObject::tr("Could not copy previous settings. Your old profiles remain in Arovan; check settings storage permissions."));
    if (app.isRunning()) {
        OcSettings settings;
        if (settings.value("Settings/singleInstanceMode", true).toBool()) {
            app.sendMessage("Wake up!");
            return 0;
        }
    }
    }
    ClientLanguage::apply(OcSettings().value("Client/language","en").toString());
    app.setApplicationDisplayName(QObject::tr("Gozarno VPN"));
    auto fileLog=std::make_unique<FileLogger>(nullptr,previewDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/logs" : previewDir+"/logs");
    Logger::instance().addMessage(QString("%1 (%2) logging started...").arg(app.applicationDisplayName(),app.applicationVersion()));
    const QString profileName{ parser.value(QLatin1String("server")) };
    MainWindow mainWindow(nullptr, haveTray, profileName);
    app.setActivationWindow(&mainWindow);
#ifdef PROJ_PKCS11
    gnutls_pkcs11_set_pin_function(pin_callback, &mainWindow);
#endif
    gnutls_global_set_log_function(log_callback);
#ifdef PROJ_GNUTLS_DEBUG
    gnutls_global_set_log_level(3);
#endif

    mainWindow.show();
    mainWindow.setWindowTitle(app.applicationDisplayName());
    if (!previewDir.isEmpty()) {
        if(parser.isSet("preview-light")) mainWindow.findChild<QComboBox*>("appearanceSelector")->setCurrentIndex(0);
        if(parser.isSet("preview-compact")) mainWindow.resize(mainWindow.minimumSize());
        QTimer::singleShot(qBound(500,parser.value("preview-delay-ms").toInt(),60000), &mainWindow, [&]() {
            if(parser.isSet("preview-sample")) {
                auto* servers=mainWindow.findChild<QComboBox*>("serverList");
                if(servers && servers->count()>0) servers->setCurrentIndex(0);
                QFile report(previewDir+"/layout-report.txt");
                if(report.open(QIODevice::WriteOnly)) report.write(QString("Server count: %1\nSelected index: %2\nWindow: %3 x %4\n").arg(servers ? servers->count() : -1).arg(servers ? servers->currentIndex() : -1).arg(mainWindow.width()).arg(mainWindow.height()).toUtf8());
            }
            auto* tabs = mainWindow.findChild<QTabWidget*>("pageStack");
            for (int i = 0; tabs && i < tabs->count(); ++i) {
                tabs->setCurrentIndex(i);
                app.processEvents();
                mainWindow.grab().save(previewDir + QString("/screen-%1.png").arg(i));
            }
            if(parser.isSet("preview-sample")) {
                tabs->setCurrentIndex(3); auto* sections=mainWindow.findChild<QTabWidget*>("settingsSections");
                if(sections) { sections->setCurrentIndex(1); app.processEvents(); mainWindow.grab().save(previewDir+"/components.png"); sections->setCurrentIndex(0); }
                AboutDialog about(&mainWindow); about.show(); app.processEvents(); about.grab().save(previewDir+"/about.png"); about.hide();
                NewProfileDialog add(&mainWindow); add.show(); app.processEvents(); add.grab().save(previewDir+"/add-server.png"); add.hide();
                TrafficPolicyDialog rules("Europe · Primary",&mainWindow); rules.show(); app.processEvents(); rules.grab().save(previewDir+"/traffic-rules.png"); rules.hide();
                EditDialog edit("Europe · Primary",&mainWindow); edit.show(); app.processEvents(); edit.grab().save(previewDir+"/edit-server.png"); edit.hide();
            }
            app.quit();
        });
    }

    QObject::connect(&app, &QtSingleApplication::messageReceived,
        [&mainWindow](const QString& message) {
            Logger::instance().addMessage(message);
        });
    return app.exec();
}
