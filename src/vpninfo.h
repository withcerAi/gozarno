/*
 * Copyright (C) 2014 Red Hat
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

#pragma once

#include <QString>
#include <QUrl>
#include <QStringList>
#include <QHostAddress>
#include "common.h"
#include "client/TrafficPolicy.h"
#include <memory>

class MainWindow;
class StoredServer;
class VpnRoutes;
class AppRouter;
class ProxyStack;
#include "client/ProxySettings.h"

class VpnInfo {
public:
    VpnInfo(QString name, StoredServer* ss, MainWindow* m,const QStringList& recoveryHosts={});
    ~VpnInfo();

    void setUrl(const QUrl& url);
    int connect();
    void mainloop();
    void get_info(QString& dns, QString& ip, QString& ip6);
    void get_cipher_info(QString& cstp, QString& dtls);
    SOCKET get_cmd_fd() const;
    void reset_vpn();
    bool get_minimize() const;
    bool is_username_form_option(struct oc_auth_form* form, struct oc_form_opt* opt);
    bool is_password_form_option(struct oc_auth_form* form, struct oc_form_opt* opt);
    bool applyTraffic(const QString& interfaceName);
    bool setupProxy();
    bool isProxyMode() const { return proxySettings.enabled; }
    void restoreGamingRoutingIfDisabled();
    bool clientFailure = false;
    bool diagnosticMode = false;

    QString last_err;
    QUrl mUrl;
    MainWindow* m;
    StoredServer* ss;
    struct openconnect_info* vpninfo;
    unsigned int authgroup_set;
    unsigned int password_set;
    unsigned int form_attempt;
    unsigned int form_pass_attempt;

    void logVpncScriptOutput();
    QByteArray generateUniqueInterfaceName();

private:
    SOCKET cmd_fd;
    ProxySettings proxySettings;
    std::unique_ptr<ProxyStack> proxyStack;
    TrafficPolicy trafficPolicy;
    TrafficPolicy baseTrafficPolicy;
    bool gamingRoutesActive = false;
    std::unique_ptr<VpnRoutes> routes;
    std::unique_ptr<AppRouter> appRouter;
    bool routesPrepared = false;
    bool protectionEnabled = false;
    QStringList recoveryHosts;
    QList<QHostAddress> recoveryPeers;
};
