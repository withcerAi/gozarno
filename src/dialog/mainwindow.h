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

#include "common.h"
#include "OcSettings.h"
#include "client/ConnectionState.h"
#include "client/DiagnosticReport.h"
#include "client/FailoverPlan.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QMenu>
#include <QMutex>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QNetworkReply>
#include <QHostAddress>
#include <QProgressDialog>
#include <memory>
#include <atomic>

#ifndef _WIN32
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#else
#include <winsock2.h>
#endif

extern "C" {
#include <openconnect.h>
}

class LogDialog;
class QStateMachine;
class ServerLibrary;
class GamingMode;
class GamingSession;
class QPushButton;
class SessionActivity;
class ConnectionHealth;
class KillSwitch;
class NetworkEvents;
class QLabel;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = 0, bool useTray = false, const QString profileName = {});
    ~MainWindow();

    void updateStats(const struct oc_stats* stats, QString dtls);
    void reload_settings();

    void vpn_status_changed(int connected);
    void vpn_status_changed(int connected,
        QString& dns,
        QString& ip,
        QString& ip6,
        QString& cstp_cipher,
        QString& dtls_cipher);

    int get_log_level();
    bool protectTunnel(const QString& name, const QList<QHostAddress>& gateways, QString& error);
    void exportDiagnosticReport();
    bool writeDiagnosticReport(const QString& path);
    void runProtectionChecks();
    void requestShutdown();


public slots:
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void statsChanged(QString, QString, QString);
    void changeStatus(int);

    void blink_ui(void);

    void request_update_stats();

    void on_connectClicked();
    void on_disconnectClicked();

    void closeEvent(QCloseEvent* event) override;

    void on_actionAbout_triggered();
    void on_actionCheckForUpdates_triggered();

    void on_actionNewProfile_triggered();
    void on_actionNewProfileAdvanced_triggered();
    void on_actionEditSelectedProfile_triggered();
    void on_actionRemoveSelectedProfile_triggered();

    void on_actionLicense_triggered();
    void on_actionReport_an_issue_triggered();
    void on_actionWebSite_triggered();

signals:
    void connectivity_health_sig(bool healthy);
    void automatic_recovery_blocked_sig();
    void connection_failure_sig(QString reason,int code);
    void diagnosis_message_sig(QString message);
    void tunnel_context_sig(QString interfaceName,QStringList resolvers,QList<QHostAddress> gateways);
    void protection_validation_finished_sig();
    void session_info_sig(QString,QString,QString,QString,QString);
    void traffic_totals_sig(quint64,quint64);
    void stats_changed_sig(QString, QString, QString);
    void vpn_status_changed_sig(int);
    void timeout(void);
    void readyToShutdown();
    void version_download_completed_sig();
    void client_error_sig(QString message);
    void tunnel_ready_sig(QString interfaceName);
    void proxy_ready_sig(QString endpoints);
    void proxy_diagnostics_sig(QJsonObject counters);
    void proxy_notice_sig(QString message);
    void proxy_configuration_locked_sig(bool locked);

private slots:
    void createLogDialog();
    void tryCheckLatestVersion();
    void checkForUpdatesDialog();

private:
    // Client interface changes, 2026-10-06.
    void setupClientInterface();
    void buildClientShell();
    void applyClientTheme(bool dark);
    ServerLibrary* serverLibrary = nullptr;
    SessionActivity* sessionActivity = nullptr;
    GamingSession* gamingSession = nullptr;

    QPushButton* gamingButton = nullptr;
    QString activeInterface;
    ConnectionHealth* connectionHealth = nullptr;
    NetworkEvents* networkEvents = nullptr;
    DiagnosticReport diagnosticReport;
    FailoverPlan failoverPlan;
    QStringList recoveryGatewayHosts;
    QStringList tunnelDns;
    QList<QHostAddress> tunnelGateways;
    QString pendingFallback;
    QString lastDiagnosis;
    bool waitingForNetwork = false;
    QFutureWatcher<QJsonObject> protectionWatcher;
    bool validationStarted = false;
    bool shuttingDown = false;
    int currentConnectionState = STATUS_DISCONNECTED;
    QPushButton* validationButton = nullptr;
    std::shared_ptr<std::atomic_bool> validationStop;
    void setConnectionMessage(const QString& message);
    void scheduleRecovery();
    void tryFallback();
    std::unique_ptr<KillSwitch> killSwitch;
    QLabel* healthLabel = nullptr;
    bool connectionWanted = false;
    bool sessionEstablished = false;
    bool recoveryLaunch = false;
    bool recoveryBlocked = false;
    int reconnectAttempts = 0;
    QString reconnectProfile;
    QTimer* reconnectTimer = nullptr;
    void gotLatestVersion(QNetworkReply *reply);
    void checkLatestVersion() const;

    static QString normalize_byte_size(uint64_t bytes);
    void createTrayIcon();

    void readSettings();
    void writeSettings();

    /* we keep the fd instead of a pointer to vpninfo to avoid
     * any multithread issues */
    SOCKET cmd_fd;
    bool minimize_on_connect;
    Ui::MainWindow* ui;
    QTimer* timer;
    QTimer* blink_timer;
    QFutureWatcher<void> futureWatcher; // watches the vpninfo

    QString dns;
    QString ip;
    QString ip6;
    QString cstp_cipher;
    QString dtls_cipher;

    QString latest_version;
    time_t last_check_time;
    QProgressDialog *downloadProgress;

    QNetworkAccessManager *manager;

    QStateMachine* m_appWindowStateMachine;
    QSystemTrayIcon* m_trayIcon;
    QMenu* m_trayIconMenu = nullptr;
    QMenu* m_trayIconMenuConnections;
    QAction* m_disconnectAction;
    QAction* m_minimizeAction;
    QAction* m_restoreAction;
    QAction* m_quitAction;
};
