// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "client/GamingProfile.h"
#include "client/GameDiscovery.h"
#include <QFutureWatcher>
#include <memory>
#include <QDialog>
class QComboBox; class QLineEdit; class QSpinBox; class QCheckBox; class QListWidget; class QTableWidget;
class GamingDialog : public QDialog {
    Q_OBJECT
public:
    GamingDialog(const QStringList& vpnProfiles,const QString& tunnel,QWidget* parent=nullptr);
    ~GamingDialog() override;
signals:
    void measureServer(QString name);
    void exportQuality();
private:
    bool storeDraft();
    void display(int index);
    void refreshScores();
    QList<GamingProfile> profiles;
    QStringList servers;
    QString tunnel;
    int current=-1;
    QComboBox *picker,*method,*route;
    QLineEdit *name,*target;
    QSpinBox *port,*mtu,*capacity,*limit;
    QCheckBox *automatic,*autoMtu;
    QListWidget *applications,*background;
    QTableWidget* table;
    QFutureWatcher<QList<DiscoveredGame>> discoveryWatcher;
    std::shared_ptr<std::atomic_bool> scanCancelled=std::make_shared<std::atomic_bool>(false);
};
