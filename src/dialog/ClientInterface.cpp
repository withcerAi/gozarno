// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "TrafficPolicyDialog.h"
#include "client/TrafficPolicy.h"
#include "client/AppRouter.h"
#include "client/ClientTheme.h"
#include "client/ClientLanguage.h"
#include "client/ConnectionButtonIcon.h"
#include "client/PublicIpLookup.h"
#include "client/GamingSession.h"
#include "client/ProxySettings.h"
#include <QNetworkInterface>
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QtConcurrent>
#include <QDateTime>
#include <QLocale>
#include "ServerLibrary.h"
#include "SessionActivity.h"
#include "ComponentsWidget.h"
#include "ProxyEndpointWidget.h"
#include <QStatusBar>
#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QTabBar>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QFormLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QMenuBar>
#include <QStyle>

namespace {
QLabel* text(const QString& value, const char* role, QWidget* parent)
{
    auto* label = new QLabel(value, parent);
    label->setProperty("role", role); label->setWordWrap(true);
    return label;
}
QFrame* card(QWidget* parent, QVBoxLayout*& layout, const QString& name = {})
{
    auto* frame = new QFrame(parent); frame->setObjectName(name);
    frame->setProperty("card", true);
    layout = new QVBoxLayout(frame); layout->setContentsMargins(16,16,16,16); layout->setSpacing(8);
    return frame;
}
QIcon navIcon(int kind)
{
    QPixmap image(24,24); image.fill(Qt::transparent);
    QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor("#9db5af"), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (kind == 0) {
        QPainterPath path; path.moveTo(12,3); path.lineTo(20,6); path.lineTo(19,14);
        path.quadTo(17,19,12,22); path.quadTo(7,19,5,14); path.lineTo(4,6); path.closeSubpath();
        p.drawPath(path); p.drawLine(9,12,11,14); p.drawLine(11,14,15,9);
    } else if (kind == 1) {
        for (int y : {4,13}) { p.drawRoundedRect(QRectF(3,y,18,7),2,2); p.drawPoint(6,y+3); p.drawLine(11,y+3,18,y+3); }
    } else if (kind == 2) {
        p.drawLine(4,18,4,12); p.drawLine(10,18,10,7); p.drawLine(16,18,16,3); p.drawLine(21,18,21,10);
    } else {
        p.drawEllipse(QPointF(12,12),6,6); p.drawEllipse(QPointF(12,12),2,2);
        for (int i=0;i<8;++i) { p.save(); p.translate(12,12); p.rotate(i*45); p.drawLine(0,-8,0,-10); p.restore(); }
    }
    return QIcon(image);
}
class ConnectionVisual final : public QWidget {
public:
    explicit ConnectionVisual(QWidget* parent) : QWidget(parent) { setMinimumHeight(96); setMaximumHeight(112); }
    int state = STATUS_DISCONNECTED;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(width()/2., height()/2.);
        const QColor accent(state == STATUS_CONNECTED ? "#45e0b6" : state == STATUS_CONNECTING ? "#e7be72" : "#53c9af");
        for(int radius : {47,39}) { p.setPen(QPen(QColor(accent.red(),accent.green(),accent.blue(),25),1)); p.setBrush(Qt::NoBrush); p.drawEllipse(c,radius,radius); }
        QRadialGradient halo(c,58); halo.setColorAt(0,QColor(69,224,182,35)); halo.setColorAt(1,QColor(69,224,182,0));
        p.setPen(Qt::NoPen); p.setBrush(halo); p.drawEllipse(c,58,58);
        p.setBrush(Qt::NoBrush); p.setPen(QPen(accent,5,Qt::SolidLine,Qt::RoundCap));
        p.drawArc(QRectF(c.x()-30,c.y()-30,60,60),35*16,290*16);
        p.drawLine(c+QPointF(27,3),c+QPointF(7,3));
        p.drawLine(c+QPointF(27,3),c+QPointF(27,20));
        p.setPen(QPen(QColor("#0d1d19"),4)); p.setBrush(accent); p.drawEllipse(c+QPointF(44,32),5,5);
    }
};
}

void MainWindow::buildClientShell()
{
    setMinimumSize(1000,720); resize(1040,740);
    ui->statusBar->hide();
    qApp->setFont(QFont("Segoe UI",10));
    // Keep the existing connection/controller widgets, rebuild their presentation.
    ui->iconLabel->hide(); ui->serverLabel->hide();
    delete ui->gridLayout_2;
    auto* home = new QVBoxLayout(ui->tabWidget_main); home->setContentsMargins(0,0,0,0); home->setSpacing(10);
    auto* columns = new QHBoxLayout; columns->setSpacing(10); home->addLayout(columns,1);
    QVBoxLayout *connectionLayout, *summaryLayout;
    auto* connectionCard = card(ui->tabWidget_main,connectionLayout,"connectionCard"); columns->addWidget(connectionCard,3);
    auto* badge = text(tr("●  DISCONNECTED"),"badge",connectionCard); badge->setAlignment(Qt::AlignCenter); connectionLayout->addWidget(badge);
    auto* visual = new ConnectionVisual(connectionCard); connectionLayout->addWidget(visual);
    auto* stateTitle = text(tr("Ready when you are"),"hero",connectionCard); stateTitle->setAlignment(Qt::AlignCenter); connectionLayout->addWidget(stateTitle);
    auto* stateHint = text(tr("Choose a server to start your secure connection."),"muted",connectionCard); stateHint->setAlignment(Qt::AlignCenter); connectionLayout->addWidget(stateHint);
    connectionLayout->addStretch();
    connectionLayout->addWidget(text(tr("CONNECT TO"),"eyebrow",connectionCard));
    auto* picker = new QHBoxLayout; picker->setSpacing(8); picker->addWidget(ui->serverList,1); picker->addWidget(ui->serverListControl); connectionLayout->addLayout(picker);
    ui->serverList->setEditable(false); ui->serverList->setPlaceholderText(tr("Add your first server"));
    ui->serverList->setMinimumHeight(46); ui->serverListControl->setMinimumSize(44,46);
    ui->serverListControl->setToolTip(tr("Manage selected server"));
    ui->serverListControl->setIcon(navIcon(1));
    ui->connectionButton->setProperty("translationSourceText","Connect");
    ui->connectionButton->setMinimumHeight(52);
    ui->connectionButton->setIconSize(QSize(20,20));
    ui->connectionButton->setIcon(connectionButtonIcon(ConnectionButtonAction::Connect));
    auto* mode=new QComboBox(connectionCard);mode->setObjectName("connectionModeSelector");
    mode->addItem(tr("Normal VPN"),false);mode->addItem(tr("SOCKS proxy · other connections stay direct"),true);
    mode->setCurrentIndex(ProxySettings::load().enabled?1:0);
    connectionLayout->addWidget(mode);
    auto* proxyEndpoint=new ProxyEndpointWidget(connectionCard);
    connectionLayout->addWidget(proxyEndpoint);proxyEndpoint->setVisible(mode->currentIndex()==1);
    connect(mode,&QComboBox::currentIndexChanged,this,[this,proxyEndpoint](int index) {
        OcSettings s;s.setValue("Client/proxyMode",index==1);proxyEndpoint->setVisible(index==1);
        gamingButton->setEnabled(index==0);
        if(auto* box=findChild<QCheckBox*>("killSwitchPreference"))box->setEnabled(index==0);
        if(index==1)gamingButton->setChecked(false);
    });
    connect(this,&MainWindow::proxy_configuration_locked_sig,mode,[mode](bool locked){mode->setEnabled(!locked);});
    gamingButton->setEnabled(mode->currentIndex()==0);
    if(auto* box=findChild<QCheckBox*>("killSwitchPreference"))box->setEnabled(mode->currentIndex()==0);
    connect(this,&MainWindow::proxy_notice_sig,proxyEndpoint,&ProxyEndpointWidget::showNotice,Qt::QueuedConnection);
    connect(this,&MainWindow::proxy_ready_sig,proxyEndpoint,[proxyEndpoint](const QString& endpoints){proxyEndpoint->setEndpoints(endpoints);},Qt::QueuedConnection);
    connect(this,&MainWindow::vpn_status_changed_sig,this,[this,mode,proxyEndpoint](int state) {
        mode->setEnabled(state==STATUS_DISCONNECTED && !connectionWanted);
        if(state==STATUS_DISCONNECTED){proxyEndpoint->clearEndpoints();gamingButton->setEnabled(mode->currentIndex()==0);}
    });
    connectionLayout->addWidget(ui->connectionButton);
    auto* summaryCard = card(ui->tabWidget_main,summaryLayout); columns->addWidget(summaryCard,2);
    summaryLayout->addWidget(text(tr("CONNECTION OVERVIEW"),"eyebrow",summaryCard));
    auto* profileTitle = text(tr("No server selected"),"section",summaryCard); profileTitle->setProperty("translationExcluded",true); summaryLayout->addWidget(profileTitle);
    auto* gateway = text(tr("Your saved servers will appear here."),"muted",summaryCard); gateway->setTextInteractionFlags(Qt::TextSelectableByMouse); summaryLayout->addWidget(gateway);
    auto* line = new QFrame(summaryCard); line->setObjectName("divider"); line->setFixedHeight(1); summaryLayout->addWidget(line);
    summaryLayout->addWidget(text(tr("PROTOCOL"),"eyebrow",summaryCard));
    auto* protocol = text("—","value",summaryCard); summaryLayout->addWidget(protocol);
    ClientLanguage::technical(protocol);
    summaryLayout->addWidget(text(tr("TRAFFIC POLICY"),"eyebrow",summaryCard));
    auto* routeSummary = text(tr("Server defaults"),"value",summaryCard); summaryLayout->addWidget(routeSummary);
    summaryLayout->addWidget(text(tr("IP before connection"),"eyebrow",summaryCard));
    auto* beforeIp=text(tr("Unknown"),"value",summaryCard); beforeIp->setTextFormat(Qt::PlainText); summaryLayout->addWidget(beforeIp);
    summaryLayout->addWidget(text(tr("VPN exit IP · approximate location"),"eyebrow",summaryCard));
    auto* exitIp=text(tr("Not connected"),"value",summaryCard); exitIp->setTextFormat(Qt::PlainText); summaryLayout->addWidget(exitIp);
    auto* countryFlag=new QLabel(summaryCard); countryFlag->hide(); summaryLayout->addWidget(countryFlag);
    auto* refreshIp=new QPushButton(tr("Refresh IP"),summaryCard); refreshIp->setProperty("quiet",true); summaryLayout->addWidget(refreshIp);
    refreshIp->setToolTip(tr("Uses Country.is over verified HTTPS. IP location is approximate. No account information is sent."));
    auto generation=std::make_shared<int>(0);
    auto busy=std::make_shared<bool>(false);
    auto lastLookup=std::make_shared<qint64>(0);
    auto cancel=std::make_shared<std::shared_ptr<std::atomic_bool>>(std::make_shared<std::atomic_bool>(false));
    connect(this,&QObject::destroyed,[cancel]() { (*cancel)->store(true); });
    const auto lookup=[this,beforeIp,exitIp,countryFlag,refreshIp,generation,busy,lastLookup,cancel]() {
        if(qApp->property("diagnosticMode").toBool() || QCoreApplication::arguments().contains("--preview-dir")) return;
        const bool online=currentConnectionState==STATUS_CONNECTED;
        if(online && property("proxyModeActive").toBool()) {exitIp->setText(tr("SOCKS tunnel active · system stays direct"));return;}

        if(!online && currentConnectionState!=STATUS_DISCONNECTED) return;
        if(*busy || QDateTime::currentMSecsSinceEpoch()-*lastLookup<10000) return;
        *lastLookup=QDateTime::currentMSecsSinceEpoch(); *busy=true; refreshIp->setEnabled(false);
        const int request=++*generation;
        *cancel=std::make_shared<std::atomic_bool>(false); const auto cancelled=*cancel;
        auto* label=online ? exitIp : beforeIp; label->setText(tr("Checking…"));
        const QString tunnel=online ? activeInterface : QString();
        auto* watcher=new QFutureWatcher<QJsonObject>(this);
        connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[=]() {
            *busy=false; refreshIp->setEnabled(true);
            const auto value=watcher->result(); watcher->deleteLater();
            if(request!=*generation) { if(currentConnectionState==STATUS_CONNECTED) QTimer::singleShot(100,refreshIp,&QPushButton::click); return; }
            if(value.isEmpty()) {label->setText(tr("Unknown · lookup unavailable")); return;}
            const QString code=value.value("country").toString();
            const QString city=value.value("city").toString();
            QString country=code;
            for(const auto& locale:QLocale::matchingLocales(QLocale::AnyLanguage,QLocale::AnyScript,QLocale::AnyTerritory))
                if(QLocale::territoryToCode(locale.territory())==code) {country=QLocale::territoryToString(locale.territory()); break;}
            label->setText(value.value("ip").toString()+"\n"+country+(city.isEmpty() ? QString() : " · "+city));
            if(online) { countryFlag->setPixmap(QPixmap(":/flags/"+code.toLower()+".png")); countryFlag->setVisible(!countryFlag->pixmap().isNull()); }
            label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        });
        watcher->setFuture(QtConcurrent::run([tunnel,cancelled] {return PublicIpLookup::fetch(tunnel,cancelled);}));
    };
    connect(refreshIp,&QPushButton::clicked,this,lookup);
    connect(this,&MainWindow::vpn_status_changed_sig,this,[=](int state) {
        ++*generation;
        (*cancel)->store(true); countryFlag->hide();
        if(state!=STATUS_CONNECTED) exitIp->setText(tr("Not connected"));
        if(state==STATUS_CONNECTED) { *lastLookup=0; QTimer::singleShot(1100,this,lookup); }
    });
    QTimer::singleShot(500,this,lookup);
    summaryLayout->addStretch();
    auto* stats = new QHBoxLayout;
    auto* downLayout = new QVBoxLayout; downLayout->addWidget(text(tr("↓  RECEIVED"),"eyebrow",summaryCard));
    auto* received = text("—","metric",summaryCard); downLayout->addWidget(received); stats->addLayout(downLayout);
    auto* upLayout = new QVBoxLayout; upLayout->addWidget(text(tr("↑  SENT"),"eyebrow",summaryCard));
    auto* sent = text("—","metric",summaryCard); upLayout->addWidget(sent); stats->addLayout(upLayout); summaryLayout->addLayout(stats);
    ui->viewLogButton->setText(tr("Open connection log  ↗")); ui->viewLogButton->setIcon(QIcon()); ui->viewLogButton->setProperty("quiet",true); summaryLayout->addWidget(ui->viewLogButton);

    auto* features = new QHBoxLayout; features->setSpacing(10); home->addLayout(features);
    QVBoxLayout *gamingLayout, *trafficLayout;
    auto* gamingCard = card(ui->tabWidget_main,gamingLayout); features->addWidget(gamingCard,1);
    auto* gameRow = new QHBoxLayout; gameRow->addWidget(text(tr("Gaming mode"),"section",gamingCard),1); gameRow->addWidget(gamingButton); gamingLayout->addLayout(gameRow);
    gamingButton->setText(tr("Enable")); gamingButton->setMinimumWidth(94);
    auto* gamingDetails=text(tr("Gaming mode is off."),"muted",gamingCard);
    gamingDetails->setTextFormat(Qt::PlainText); gamingDetails->setObjectName("gamingQualitySummary"); gamingLayout->addWidget(gamingDetails); gamingDetails->setMaximumHeight(52);
    connect(gamingSession,&GamingSession::updated,gamingDetails,[gamingDetails](const QString& value) {gamingDetails->setText(value); gamingDetails->setToolTip(value);});
    connect(gamingButton,&QPushButton::toggled,this,[this](bool active) { gamingButton->setText(active ? tr("Enabled") : tr("Enable")); });
    auto* trafficCard = card(ui->tabWidget_main,trafficLayout); features->addWidget(trafficCard,1);
    auto* trafficRow = new QHBoxLayout; trafficRow->addWidget(text(tr("Traffic control"),"section",trafficCard),1);
    auto* rules = new QPushButton(tr("Manage  ↗"),trafficCard); rules->setProperty("quiet",true); trafficRow->addWidget(rules); trafficLayout->addLayout(trafficRow);
    trafficLayout->addWidget(text(tr("Choose which apps, domains and IP addresses use the VPN."),"muted",trafficCard));
    const auto updateProfile = [this,profileTitle,gateway,protocol,routeSummary,rules,mode]() {
        const QString name = ui->serverList->currentText(); OcSettings settings;
        profileTitle->setProperty("translationExcluded",!name.isEmpty());
        profileTitle->setText(name.isEmpty() ? tr("No server selected") : name);
        gateway->setText(name.isEmpty() ? tr("Add a server using your provider's existing address.") : settings.value("server:"+name+"/server").toString());
        gateway->setProperty("technical",!name.isEmpty());
        gateway->setLayoutDirection(name.isEmpty() ? layoutDirection() : Qt::LeftToRight);
        gateway->setAlignment(name.isEmpty() && ClientLanguage::isPersian() ? Qt::AlignRight|Qt::AlignVCenter|Qt::AlignAbsolute : Qt::AlignLeft|Qt::AlignVCenter|Qt::AlignAbsolute);
        protocol->setText(name.isEmpty() ? "—" : settings.value("server:"+name+"/protocol-name","anyconnect").toString().toUpper());
        const auto policy = TrafficPolicy::load(name); int count=0; for(const auto& rule:policy.rules) if(rule.enabled) ++count;
        routeSummary->setText(name.isEmpty() ? "—" : (policy.mode == "full" ? tr("All traffic through VPN") : policy.mode == "selected" ? tr("Selected traffic only") : tr("Server defaults")) + (count ? tr(" · %1 rules").arg(count) : QString()));
        if(mode->currentIndex()==1)routeSummary->setText(tr("Only SOCKS clients use the VPN"));
        rules->setEnabled(mode->currentIndex()==0 && !name.isEmpty() && ui->serverList->isEnabled());
    };
    connect(ui->serverList,&QComboBox::currentTextChanged,this,[updateProfile](const QString&) { updateProfile(); });
    connect(rules,&QPushButton::clicked,this,[this,updateProfile]() { TrafficPolicyDialog dialog(ui->serverList->currentText(),this); dialog.exec(); updateProfile(); });
    connect(mode,&QComboBox::currentIndexChanged,this,[updateProfile](int){updateProfile();});
    updateProfile();
    connect(this,&MainWindow::stats_changed_sig,this,[received,sent](const QString& tx,const QString& rx,const QString&) { received->setText(rx); sent->setText(tx); });

    // A single navigation rail replaces the old tab strip and duplicated menus.
    ui->tabWidget->tabBar()->hide(); ui->tabWidget->setObjectName("pageStack"); ui->menuBar->hide();
    ui->horizontalLayout->removeWidget(ui->tabWidget); delete ui->horizontalLayout;
    auto* shell = new QHBoxLayout(ui->centralWidget); shell->setContentsMargins(0,0,0,0); shell->setSpacing(0);
    auto* sidebar = new QFrame(ui->centralWidget); sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(170); shell->addWidget(sidebar);
    auto* rail = new QVBoxLayout(sidebar); rail->setContentsMargins(18,28,18,22); rail->setSpacing(8);
    auto* wordmark = text("Gozarno","wordmark",sidebar); wordmark->setObjectName("brandWordmark"); rail->addWidget(wordmark);
    rail->addWidget(text(tr("VPN, on your terms."),"railMuted",sidebar)); rail->addSpacing(38);
    auto* nav = new QButtonGroup(sidebar); nav->setExclusive(true);
    const QStringList names{tr("Connection"),tr("Servers"),tr("Activity"),tr("Settings")};
    const QList<int> pages{0,2,1,3};
    const QStringList descriptions{tr("A clear view of your connection."),tr("Keep your servers organized and ready."),tr("Live details from your current VPN session."),tr("Make Gozarno work the way you do.")};
    for(int i=0;i<names.size();++i) {
        auto* item = new QPushButton(navIcon(i),names[i],sidebar); item->setObjectName("navButton"); item->setCheckable(true); item->setMinimumHeight(48); item->setIconSize(QSize(22,22)); nav->addButton(item,pages[i]); rail->addWidget(item);
    }
    nav->button(0)->setChecked(true); rail->addStretch();
    auto* more = new QPushButton(tr("Help && more"),sidebar); more->setObjectName("navButton");
    auto* moreMenu = new QMenu(more);
    moreMenu->addAction(ui->actionAbout); moreMenu->addAction(ui->actionLicense);
    moreMenu->addMenu(ui->menuLog_Level); moreMenu->addSeparator(); moreMenu->addAction(ui->actionQuit);
    more->setMenu(moreMenu); rail->addWidget(more);
    auto* trayState = text(tr("●  Not connected"),"railStatus",sidebar); rail->addWidget(trayState);
    rail->addWidget(text(tr("Gozarno VPN")+" · "+qApp->applicationVersion(),"railMuted",sidebar));
    auto* body = new QWidget(ui->centralWidget); body->setObjectName("body"); shell->addWidget(body,1);
    auto* bodyLayout = new QVBoxLayout(body); bodyLayout->setContentsMargins(18,16,18,12); bodyLayout->setSpacing(10);
    auto* headingRow = new QHBoxLayout;
    auto* headings = new QVBoxLayout; headings->setSpacing(7);
    auto* pageTitle = text(names[0],"pageTitle",body); auto* pageHint = text(descriptions[0],"muted",body); headings->addWidget(pageTitle); headings->addWidget(pageHint); headingRow->addLayout(headings,1);
    auto* addServer = new QPushButton(tr("＋  Add server"),body); addServer->setProperty("quiet",true); headingRow->addWidget(addServer,0,Qt::AlignTop); bodyLayout->addLayout(headingRow);
    connect(addServer,&QPushButton::clicked,this,&MainWindow::on_actionNewProfile_triggered);
    bodyLayout->addWidget(ui->tabWidget,1);
    auto* footer = text(tr("Existing provider servers · Local traffic controls"),"footer",body); bodyLayout->addWidget(footer);
    connect(nav,&QButtonGroup::idClicked,ui->tabWidget,&QTabWidget::setCurrentIndex);
    connect(ui->tabWidget,&QTabWidget::currentChanged,this,[nav,pageTitle,pageHint,names,pages,descriptions](int index) {
        if(nav->button(index)) nav->button(index)->setChecked(true);
        const int position=pages.indexOf(index); if(position>=0) { pageTitle->setText(names[position]); pageHint->setText(descriptions[position]); }
    });
    connect(this,&MainWindow::vpn_status_changed_sig,this,[=](int state) {
        visual->state=state; visual->update(); addServer->setEnabled(state==STATUS_DISCONNECTED); updateProfile();
        const bool online=state==STATUS_CONNECTED;
        badge->setText(online ? tr("●  CONNECTED") : state==STATUS_CONNECTING ? tr("●  CONNECTING") : state==STATUS_DISCONNECTING ? tr("●  DISCONNECTING") : tr("●  DISCONNECTED"));
        stateTitle->setText(online ? tr("You're connected") : state==STATUS_CONNECTING ? tr("Establishing connection") : state==STATUS_DISCONNECTING ? tr("Closing your session") : tr("Ready when you are"));
        stateHint->setText(online ? (property("proxyModeActive").toBool()?tr("SOCKS is ready. Other connections stay direct."):tr("Your VPN session is active.")) : state==STATUS_CONNECTING ? tr("Authenticating with your selected server…") : state==STATUS_DISCONNECTING ? tr("Restoring your local connection settings…") : tr("Choose a server to start your secure connection."));
        trayState->setText(online ? tr("●  Connected") : state==STATUS_CONNECTING ? tr("●  Connecting") : tr("●  Not connected"));
        badge->setProperty("online",online); badge->style()->unpolish(badge); badge->style()->polish(badge);
        if(state==STATUS_DISCONNECTED) { received->setText("—"); sent->setText("—"); }
    });
    // Card structure for settings; the existing controls retain their behavior.
    connect(this, &MainWindow::connectivity_health_sig, this, [=](bool healthy) {
        badge->setText(healthy ? tr("●  CONNECTED") : tr("●  CONNECTION INTERRUPTED"));
        stateTitle->setText(healthy ? tr("You're connected") : tr("Connection interrupted"));
        stateHint->setText(healthy ? tr("Tunnel connectivity verified.") : tr("Checking the tunnel and attempting recovery if enabled…"));
        trayState->setText(healthy ? tr("●  Connected") : tr("●  Connection interrupted"));
        badge->setProperty("online", healthy); badge->style()->unpolish(badge); badge->style()->polish(badge);
    });
    auto* preferences=ui->tabWidget->widget(3);
    connect(this,&MainWindow::diagnosis_message_sig,stateHint,&QLabel::setText);
    auto* oldLayout=qobject_cast<QVBoxLayout*>(preferences->layout());
    QLayout* form=nullptr;
    while(oldLayout && oldLayout->count()) {
        auto* item=oldLayout->takeAt(0);
        if(item->layout()) { form=item->layout(); form->setParent(nullptr); }
        else { if(item->widget()) item->widget()->hide(); delete item; }
    }
    delete oldLayout;
    auto* settingsLayout=new QVBoxLayout(preferences); settingsLayout->setContentsMargins(0,0,0,0); settingsLayout->setSpacing(10);
    auto* settingsSections=new QTabWidget(preferences); settingsSections->setObjectName("settingsSections"); settingsLayout->addWidget(settingsSections);
    auto* sourceForm=qobject_cast<QFormLayout*>(form);
    const QList<QPair<QString,QList<int>>> groups{
        {tr("Appearance & startup"),{8,9,12,13,14}},
        {tr("Connection & protection"),{0,1,2,3,4,5}},
        {tr("Gaming & routing"),{10,11}},
        {tr("Diagnostics"),{6,7}}
    };
    QList<QFormLayout*> destinations;
    for(const auto& group:groups) {
        auto* section=new QWidget(settingsSections); auto* box=new QVBoxLayout(section);
        box->setContentsMargins(18,18,18,18); box->setSpacing(12);
        box->addWidget(text(tr("Changes are saved automatically on this device."),"muted",section));
        auto* fields=new QFormLayout; fields->setSpacing(10); fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow); box->addLayout(fields); box->addStretch();
        destinations.append(fields); settingsSections->addTab(section,group.first);
    }
    if(sourceForm) {
        // Take rows backwards so the original row indexes stay stable.
        for(int row=sourceForm->rowCount()-1;row>=0;--row) {
            auto items=sourceForm->takeRow(row); int section=0;
            for(int i=0;i<groups.size();++i) if(groups[i].second.contains(row)) section=i;
            auto* fields=destinations[section];
            if(items.labelItem && items.labelItem->widget()) fields->insertRow(0,items.labelItem->widget(),items.fieldItem->widget());
            else if(items.fieldItem && items.fieldItem->widget()) fields->insertRow(0,items.fieldItem->widget());
            delete items.labelItem; delete items.fieldItem;
        }
        delete sourceForm;
    }
    auto* proxyPage=new QWidget(settingsSections);auto* proxyLayout=new QVBoxLayout(proxyPage);
    proxyLayout->setContentsMargins(18,18,18,18);auto* proxyForm=new QFormLayout;
    proxyLayout->addWidget(text(tr("Select SOCKS mode on the Connection page. Changes apply to the next connection."),"muted",proxyPage));
    const auto proxyConfig=ProxySettings::load();
    auto* proxyPort=new QSpinBox(proxyPage);proxyPort->setRange(1,65535);proxyPort->setValue(proxyConfig.port);ClientLanguage::technical(proxyPort);
    auto* shareLan=new QCheckBox(tr("Share SOCKS on the local network (LAN)"),proxyPage);shareLan->setChecked(proxyConfig.lan);
    auto* lanAddress=new QComboBox(proxyPage);ClientLanguage::technical(lanAddress);
    for(const auto& iface:QNetworkInterface::allInterfaces()) {
        if(!iface.flags().testFlag(QNetworkInterface::IsUp) || (iface.type()!=QNetworkInterface::Ethernet && iface.type()!=QNetworkInterface::Wifi))continue;
        for(const auto& entry:iface.addressEntries())if(entry.ip().protocol()==QAbstractSocket::IPv4Protocol && !entry.ip().isLoopback() && !entry.ip().isLinkLocal())
            lanAddress->addItem(iface.humanReadableName()+" · "+entry.ip().toString(),entry.ip().toString());
    }
    int savedAddress=lanAddress->findData(proxyConfig.lanAddress);if(savedAddress>=0)lanAddress->setCurrentIndex(savedAddress);
    auto* proxyUser=new QLineEdit(proxyConfig.username,proxyPage);ClientLanguage::technical(proxyUser);
    auto* proxyPassword=new QLineEdit(proxyConfig.password,proxyPage);proxyPassword->setEchoMode(QLineEdit::Password);ClientLanguage::technical(proxyPassword);
    proxyForm->addRow(tr("SOCKS port"),proxyPort);proxyForm->addRow(shareLan);
    proxyForm->addRow(tr("LAN address"),lanAddress);proxyForm->addRow(tr("Proxy username"),proxyUser);proxyForm->addRow(tr("Proxy password"),proxyPassword);
    proxyLayout->addLayout(proxyForm);
    proxyLayout->addWidget(text(tr("Local: 127.0.0.1. LAN requires a separate proxy password (8+ characters). Windows Firewall must allow the chosen port on your private LAN; this mode never changes firewall policy."),"muted",proxyPage));
    proxyLayout->addWidget(text(tr("Only IPv4 destinations are supported in this preview. Use remote DNS in your SOCKS client. Route, DNS, system proxy and physical MTU remain unchanged."),"muted",proxyPage));
    auto* saveProxy=new QPushButton(tr("Save proxy settings"),proxyPage);proxyLayout->addWidget(saveProxy);proxyLayout->addStretch();
    connect(saveProxy,&QPushButton::clicked,this,[=] {
        auto config=ProxySettings::load();config.port=proxyPort->value();config.lan=shareLan->isChecked();
        config.lanAddress=lanAddress->currentData().toString();config.username=proxyUser->text();config.password=proxyPassword->text();
        QString error;if(!config.save(error))QMessageBox::warning(this,tr("SOCKS proxy"),error);
        else QMessageBox::information(this,tr("SOCKS proxy"),tr("Proxy settings saved. They apply to the next connection."));
    });
    connect(this,&MainWindow::vpn_status_changed_sig,proxyPage,[this,proxyPage](int state){proxyPage->setEnabled(state==STATUS_DISCONNECTED && !connectionWanted);});
    connect(this,&MainWindow::proxy_configuration_locked_sig,proxyPage,[proxyPage](bool locked){proxyPage->setEnabled(!locked);});
    settingsSections->addTab(proxyPage,tr("SOCKS proxy"));
    auto* components=new ComponentsWidget(settingsSections); settingsSections->addTab(components,tr("Components"));
    if(auto* checkButton=preferences->findChild<QPushButton*>("engineCheckButton")) {
        disconnect(checkButton,nullptr,this,nullptr);
        connect(checkButton,&QPushButton::clicked,components,[settingsSections,components]{ settingsSections->setCurrentWidget(components); components->refresh(); });
    }
    // Keep the original protocol labels alive, while replacing the old form.
    const auto hideItems=[](auto&& self,QLayout* layout)->void {
        while(auto* item=layout->takeAt(0)) {
            if(item->widget()) item->widget()->hide();
            if(item->layout()) self(self,item->layout());
            delete item;
        }
    };
    hideItems(hideItems,ui->tabWidget_vpnInfo->layout());
    delete ui->tabWidget_vpnInfo->layout();
    auto* activityLayout=new QVBoxLayout(ui->tabWidget_vpnInfo); activityLayout->setContentsMargins(0,0,0,0);
    sessionActivity=new SessionActivity(ui->tabWidget_vpnInfo); activityLayout->addWidget(sessionActivity);
    connect(ui->serverList,&QComboBox::currentTextChanged,sessionActivity,&SessionActivity::setProfile);
    connect(this,&MainWindow::session_info_sig,this,[this](QString d,QString i,QString i6,QString tls,QString dtls) {
        dns=d; ip=i; ip6=i6; cstp_cipher=tls; dtls_cipher=dtls;
        sessionActivity->setSessionInfo(dns,ip,ip6,cstp_cipher,dtls_cipher);
    },Qt::QueuedConnection);
    connect(this,&MainWindow::vpn_status_changed_sig,sessionActivity,&SessionActivity::setConnectionState,Qt::QueuedConnection);
    connect(this,&MainWindow::traffic_totals_sig,sessionActivity,&SessionActivity::updateTraffic,Qt::QueuedConnection);
    OcSettings settings; applyClientTheme(settings.value("Client/darkTheme",true).toBool());
    ui->tabWidget->setCurrentIndex(0);
    reload_settings();
    if(ui->serverList->count()>0 && ui->serverList->currentIndex()<0) ui->serverList->setCurrentIndex(0);
}

void MainWindow::applyClientTheme(bool dark) { applyGozarnoTheme(dark); }
