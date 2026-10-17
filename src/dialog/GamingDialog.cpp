// SPDX-License-Identifier: GPL-2.0-or-later
#include "GamingDialog.h"
#include "client/GamingSession.h"
#include "client/ClientLanguage.h"
#include "OcSettings.h"
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QTabWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QUuid>
#include <QDateTime>
#include <QtConcurrent>
GamingDialog::GamingDialog(const QStringList& vpnProfiles,const QString& adapter,QWidget* parent):QDialog(parent),servers(vpnProfiles),tunnel(adapter) {
    setWindowTitle(tr("Gaming profiles and quality")); resize(820,650); setMinimumSize(800,640);
    profiles=GamingProfile::load(); auto* outer=new QVBoxLayout(this);
    auto* body=new QWidget(this); auto* layout=new QVBoxLayout(body); outer->addWidget(body); auto* row=new QHBoxLayout;
    picker=new QComboBox(body); picker->setObjectName("gamingProfilePicker"); for(const auto& p:profiles) picker->addItem(p.name); row->addWidget(picker,1);
    auto* add=new QPushButton(tr("Add"),body); auto* remove=new QPushButton(tr("Remove"),body); row->addWidget(add); row->addWidget(remove); layout->addLayout(row);
    auto* explanation=new QLabel(tr("Gaming changes run only while the mode is enabled. Probes measure the selected destination, not actual game packet loss. Game routing activates on VPN connection setup; disabling gaming restores its application rules. No game injection or anti-cheat hooks are used."),body); explanation->setWordWrap(true); layout->addWidget(explanation);
    auto* tabs=new QTabWidget(body); tabs->setObjectName("gamingSections"); layout->addWidget(tabs,1);
    const auto section=[&](const QString& title) {auto* page=new QWidget(tabs); auto* box=new QVBoxLayout(page); box->setContentsMargins(14,14,14,14); box->setSpacing(8); tabs->addTab(page,title); return box;};
    auto* gameLayout=section(tr("Game & detection"));
    auto* qualityLayout=section(tr("Measurements & MTU"));
    auto* backgroundLayout=section(tr("Background traffic"));
    auto* scoresLayout=section(tr("Server comparison"));
    auto* gameForm=new QFormLayout; gameLayout->addLayout(gameForm);
    auto* qualityForm=new QFormLayout; qualityLayout->addLayout(qualityForm);
    auto* backgroundForm=new QFormLayout; backgroundLayout->addLayout(backgroundForm);
    auto* form=gameForm;
    name=new QLineEdit(body); name->setObjectName("gamingProfileName"); form->addRow(tr("Game profile"),name);
    form=qualityForm;
    target=new QLineEdit(body); target->setObjectName("gamingTarget"); target->setPlaceholderText(tr("Numeric destination IP · optional")); ClientLanguage::technical(target); form->addRow(tr("Measurement destination"),target);
    method=new QComboBox(body); method->addItem(tr("ICMP echo · probe RTT"),"icmp"); method->addItem(tr("TCP handshake · connection latency"),"tcp"); form->addRow(tr("Measurement method"),method);
    port=new QSpinBox(body); port->setRange(1,65535); ClientLanguage::technical(port); form->addRow(tr("TCP destination port"),port);
    const auto applicationList=[&](const QString& title,QListWidget*& list) {
        auto* group=new QWidget(body); auto* box=new QVBoxLayout(group); box->setContentsMargins(0,0,0,0); list=new QListWidget(group); list->setFixedHeight(80); ClientLanguage::technical(list); box->addWidget(list);
        auto* actions=new QHBoxLayout; auto* choose=new QPushButton(tr("Choose executable…"),group); auto* erase=new QPushButton(tr("Remove selected"),group); actions->addWidget(choose); actions->addWidget(erase); actions->addStretch(); box->addLayout(actions); form->addRow(title,group);
        auto* destination=list;
        connect(choose,&QPushButton::clicked,this,[this,destination] {const auto path=QFileDialog::getOpenFileName(this,tr("Choose executable"),{},tr("Applications (*.exe)")); if(path.isEmpty()) return; if(destination->findItems(path,Qt::MatchFixedString).isEmpty()) destination->addItem(path);});
        connect(erase,&QPushButton::clicked,this,[destination] {delete destination->takeItem(destination->currentRow());});
    };
    form=gameForm;
    applicationList(tr("Game, launcher and voice applications"),applications); applications->setObjectName("gamingApplications");
    auto* discoveryActions=new QHBoxLayout;
    auto* scan=new QPushButton(tr("Find installed games"),body); scan->setObjectName("scanInstalledGamesButton");
    auto* scanFolder=new QPushButton(tr("Scan a game folder…"),body); auto* addDetected=new QPushButton(tr("Add selected detected game"),body);
    discoveryActions->addWidget(scan); discoveryActions->addWidget(scanFolder); gameLayout->addLayout(discoveryActions);
    auto* found=new QListWidget(body); found->setObjectName("discoveredGames"); found->setFixedHeight(80); gameLayout->addWidget(found);
    auto* scanStatus=new QLabel(tr("Scans local Steam, Epic and GOG libraries. Other games can be added from a chosen folder. Results stay on this device."),body); scanStatus->setWordWrap(true); gameLayout->addWidget(scanStatus); gameLayout->addWidget(addDetected);
    const auto beginScan=[this,scan,scanFolder,found,scanStatus](const QStringList& folders) {
        if(discoveryWatcher.isRunning()) return;
        scanCancelled=std::make_shared<std::atomic_bool>(false); const auto cancelled=scanCancelled; scan->setEnabled(false); scanFolder->setEnabled(false); found->clear(); scanStatus->setText(tr("Scanning game libraries…"));
        discoveryWatcher.setFuture(QtConcurrent::run([folders,cancelled] {return GameDiscovery::scan(folders,*cancelled);}));
    };
    connect(scan,&QPushButton::clicked,this,[beginScan] {beginScan({});});
    connect(scanFolder,&QPushButton::clicked,this,[this,beginScan] {const QString folder=QFileDialog::getExistingDirectory(this,tr("Choose a game folder")); if(!folder.isEmpty()) beginScan({folder});});
    connect(&discoveryWatcher,&QFutureWatcher<QList<DiscoveredGame>>::finished,this,[this,scan,scanFolder,found,scanStatus] {
        scan->setEnabled(true); scanFolder->setEnabled(true); const auto games=discoveryWatcher.result();
        for(const auto& game:games) {auto* item=new QListWidgetItem(game.name+" · "+game.source+"\n"+game.executable,found); item->setData(Qt::UserRole,game.executable); item->setData(Qt::UserRole+1,game.name);}
        scanStatus->setText(tr("Found %1 executable candidates. Select the actual game executable; protected Store games may need manual selection.").arg(games.size()));
    });
    connect(addDetected,&QPushButton::clicked,this,[this,found] {const auto* selected=found->currentItem(); if(!selected) return;
        const QString path=selected->data(Qt::UserRole).toString(); if(applications->findItems(path,Qt::MatchFixedString).isEmpty()) applications->addItem(path);
        name->setText(selected->data(Qt::UserRole+1).toString().left(80)); automatic->setChecked(true);
    });
    automatic=new QCheckBox(tr("Activate session settings only while a selected game application is running"),body); form->addRow(automatic);
    route=new QComboBox(body); route->addItem(tr("Keep existing traffic rules"),"inherit"); route->addItem(tr("Use VPN for selected game applications"),"vpn"); route->addItem(tr("Direct route for selected game applications"),"direct"); form->addRow(tr("Game application route"),route);
    auto* routeNote=new QLabel(tr("Set the game profile and enable gaming before connecting to apply its route. Turning it on during an existing connection does not move game traffic. Direct game routing is unavailable with kill switch."),body); routeNote->setWordWrap(true); gameLayout->addWidget(routeNote);
    form=qualityForm;
    autoMtu=new QCheckBox(tr("Discover packet size with verified IPv4 ICMP replies"),body); form->addRow(autoMtu);
    mtu=new QSpinBox(body); mtu->setRange(1280,1500); ClientLanguage::technical(mtu); form->addRow(tr("Manual tunnel MTU"),mtu);
    capacity=new QSpinBox(body); capacity->setRange(0,1000000); capacity->setSpecialValueText(tr("Unknown · no load comparison")); capacity->setSuffix(tr(" kbps")); form->addRow(tr("VPN upload capacity"),capacity);
    form=backgroundForm;
    applicationList(tr("Background applications to limit during gaming"),background); background->setObjectName("gamingBackgroundApplications");
    limit=new QSpinBox(body); limit->setRange(64,1000000); limit->setSuffix(tr(" kbps per application")); form->addRow(tr("Background upload limit"),limit);
    auto* limits=new QLabel(tr("Limits affect only selected applications' uploads on this computer. They do not cap downloads or other devices. Temporary Windows QoS policies are removed on disable/exit; after a crash they are recovered on next launch or disappear on reboot."),body); limits->setWordWrap(true); backgroundLayout->addWidget(limits);
    table=new QTableWidget(body); table->setColumnCount(4); table->setHorizontalHeaderLabels({tr("VPN profile"),tr("Observed p95"),tr("Failed probes"),tr("Transport")}); table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch); table->setSelectionBehavior(QAbstractItemView::SelectRows); table->setSelectionMode(QAbstractItemView::SingleSelection); table->setEditTriggers(QAbstractItemView::NoEditTriggers); table->setMaximumHeight(210); scoresLayout->addWidget(table);
    auto* scoreNote=new QLabel(tr("Ranking uses measurements from the last hour on this network for the same destination and method. Unmeasured servers have no score. Connect and measure before starting a match; no automatic server switch is performed."),body); scoreNote->setWordWrap(true); scoresLayout->addWidget(scoreNote);
    auto* measure=new QPushButton(tr("Save, connect to selected server and measure"),body); scoresLayout->addWidget(measure);
    auto* exportReport=new QPushButton(tr("Export current or last gaming quality report…"),body); scoresLayout->addWidget(exportReport);
    connect(exportReport,&QPushButton::clicked,this,&GamingDialog::exportQuality);
    gameLayout->addStretch(); qualityLayout->addStretch(); backgroundLayout->addStretch(); scoresLayout->addStretch();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this); outer->addWidget(buttons);
    const auto save=[this]() {if(!storeDraft()) return false; QString error; if(!GamingProfile::save(profiles,profiles[current].id,error)) {QMessageBox::warning(this,tr("Gaming profile"),error); return false;} return true;};
    connect(buttons,&QDialogButtonBox::accepted,this,[this,save] {if(save()) accept();}); connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(measure,&QPushButton::clicked,this,[this,save] {const int row=table->currentRow(); if(row<0) return; if(!save()) return;
        if(GamingProfile::processesRunning(profiles[current].applications)) {QMessageBox::warning(this,tr("Game is running"),tr("Close the selected game applications before changing VPN servers.")); return;}
        const QString server=table->item(row,0)->text(); accept(); emit measureServer(server);
    });
    connect(picker,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int index) {if(index==current) return; if(current>=0 && !storeDraft()) {QSignalBlocker block(picker); picker->setCurrentIndex(current); return;} display(index);});
    connect(add,&QPushButton::clicked,this,[this] {if(profiles.size()>=16 || !storeDraft()) return; GamingProfile p; p.id=QUuid::createUuid().toString(); p.name=tr("New game profile"); profiles.append(p); {QSignalBlocker block(picker); picker->addItem(p.name); picker->setCurrentIndex(profiles.size()-1);} display(profiles.size()-1);});
    connect(remove,&QPushButton::clicked,this,[this] {if(profiles.size()<=1) return; {QSignalBlocker block(picker); profiles.removeAt(current); picker->removeItem(current); picker->setCurrentIndex(0);} display(0);});
    connect(autoMtu,&QCheckBox::toggled,mtu,&QWidget::setDisabled);
    OcSettings settings; const QString selected=settings.value("Client/gamingProfile").toString(); int index=0; for(int i=0;i<profiles.size();++i) if(profiles[i].id==selected) index=i;
    {QSignalBlocker block(picker); picker->setCurrentIndex(index);} display(index);
}
GamingDialog::~GamingDialog() {scanCancelled->store(true);}
bool GamingDialog::storeDraft() {
    if(current<0) return true;
    GamingProfile p=profiles[current]; p.name=name->text(); p.target=target->text(); p.probe=method->currentData().toString(); p.port=quint16(port->value());
    p.route=route->currentData().toString(); p.automatic=automatic->isChecked(); p.autoMtu=autoMtu->isChecked(); p.mtu=mtu->value(); p.uploadKbps=capacity->value(); p.backgroundKbps=limit->value(); p.applications.clear(); p.background.clear();
    for(int i=0;i<applications->count();++i) p.applications.append(applications->item(i)->text());
    for(int i=0;i<background->count();++i) p.background.append(background->item(i)->text());
    GamingProfile checked; QString error; if(!GamingProfile::fromJson(p.toJson(),checked,error)) {QMessageBox::warning(this,tr("Gaming profile"),error); return false;}
    OcSettings settings; if(checked.route=="direct" && settings.value("Client/killSwitch",false).toBool()) {QMessageBox::warning(this,tr("Gaming profile"),tr("Direct game routing is unavailable with kill switch.")); return false;}
    profiles[current]=checked; {QSignalBlocker block(picker); picker->setItemText(current,checked.name);} return true;
}
void GamingDialog::display(int index) {
    if(index<0 || index>=profiles.size()) return;
    current=index; const auto& p=profiles[index]; name->setText(p.name); target->setText(p.target); method->setCurrentIndex(method->findData(p.probe)); port->setValue(p.port); route->setCurrentIndex(route->findData(p.route));
    automatic->setChecked(p.automatic); autoMtu->setChecked(p.autoMtu); mtu->setValue(p.mtu); mtu->setDisabled(p.autoMtu); capacity->setValue(p.uploadKbps); limit->setValue(p.backgroundKbps);
    applications->clear(); applications->addItems(p.applications); background->clear(); background->addItems(p.background); refreshScores();
}
void GamingDialog::refreshScores() {
    table->setRowCount(0); QStringList remaining=servers;
    const auto append=[this](const QString& server,const QString& p95,const QString& failed,const QString& transport) {const int row=table->rowCount(); table->insertRow(row); const QStringList values{server,p95,failed,transport}; for(int i=0;i<values.size();++i) {auto* item=new QTableWidgetItem(values[i]); table->setItem(row,i,item);}};
    for(const auto& score:GamingSession::scores(profiles[current],tunnel)) {const auto server=score.value("vpn").toString(); if(!remaining.removeOne(server)) continue;
        append(server,QString::number(score.value("p95_ms").toDouble(),'f',1)+tr(" ms"),QString::number(score.value("failed_probe_percent").toDouble(),'f',1)+"%",score.value("udp").toBool()?"UDP":"TCP / ?");
    }
    for(const auto& server:remaining) append(server,tr("Not measured"),"—","—");
    if(table->rowCount()) table->selectRow(0);
}
