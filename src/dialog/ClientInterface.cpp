// SPDX-License-Identifier: GPL-2.0-or-later
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "TrafficPolicyDialog.h"
#include "client/TrafficPolicy.h"
#include "client/AppRouter.h"
#include "client/ClientTheme.h"
#include "client/ClientLanguage.h"
#include "ServerLibrary.h"
#include "SessionActivity.h"
#include "ComponentsWidget.h"
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
    layout = new QVBoxLayout(frame); layout->setContentsMargins(24,24,24,24); layout->setSpacing(14);
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
    explicit ConnectionVisual(QWidget* parent) : QWidget(parent) { setMinimumHeight(148); setMaximumHeight(170); }
    int state = STATUS_DISCONNECTED;
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        const QPointF c(width()/2., height()/2.);
        const QColor accent(state == STATUS_CONNECTED ? "#45e0b6" : state == STATUS_CONNECTING ? "#e7be72" : "#53c9af");
        for(int radius : {67,54}) { p.setPen(QPen(QColor(accent.red(),accent.green(),accent.blue(),25),1)); p.setBrush(Qt::NoBrush); p.drawEllipse(c,radius,radius); }
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
    setMinimumSize(940,640); resize(1120,830);
    ui->statusBar->hide();
    qApp->setFont(QFont("Segoe UI",10));
    // Keep the existing connection/controller widgets, rebuild their presentation.
    ui->iconLabel->hide(); ui->serverLabel->hide();
    delete ui->gridLayout_2;
    auto* home = new QVBoxLayout(ui->tabWidget_main); home->setContentsMargins(0,0,0,0); home->setSpacing(18);
    auto* columns = new QHBoxLayout; columns->setSpacing(18); home->addLayout(columns,1);
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
    ui->connectionButton->setMinimumHeight(52); ui->connectionButton->setIcon(QIcon()); connectionLayout->addWidget(ui->connectionButton);
    auto* summaryCard = card(ui->tabWidget_main,summaryLayout); columns->addWidget(summaryCard,2);
    summaryLayout->addWidget(text(tr("CONNECTION OVERVIEW"),"eyebrow",summaryCard));
    auto* profileTitle = text(tr("No server selected"),"section",summaryCard); summaryLayout->addWidget(profileTitle);
    auto* gateway = text(tr("Your saved servers will appear here."),"muted",summaryCard); gateway->setTextInteractionFlags(Qt::TextSelectableByMouse); summaryLayout->addWidget(gateway);
    auto* line = new QFrame(summaryCard); line->setObjectName("divider"); line->setFixedHeight(1); summaryLayout->addWidget(line);
    summaryLayout->addWidget(text(tr("PROTOCOL"),"eyebrow",summaryCard));
    auto* protocol = text("—","value",summaryCard); summaryLayout->addWidget(protocol);
    ClientLanguage::technical(protocol);
    summaryLayout->addWidget(text(tr("TRAFFIC POLICY"),"eyebrow",summaryCard));
    auto* routeSummary = text(tr("Server defaults"),"value",summaryCard); summaryLayout->addWidget(routeSummary);
    summaryLayout->addStretch();
    auto* stats = new QHBoxLayout;
    auto* downLayout = new QVBoxLayout; downLayout->addWidget(text(tr("↓  RECEIVED"),"eyebrow",summaryCard));
    auto* received = text("—","metric",summaryCard); downLayout->addWidget(received); stats->addLayout(downLayout);
    auto* upLayout = new QVBoxLayout; upLayout->addWidget(text(tr("↑  SENT"),"eyebrow",summaryCard));
    auto* sent = text("—","metric",summaryCard); upLayout->addWidget(sent); stats->addLayout(upLayout); summaryLayout->addLayout(stats);
    ui->viewLogButton->setText(tr("Open connection log  ↗")); ui->viewLogButton->setIcon(QIcon()); ui->viewLogButton->setProperty("quiet",true); summaryLayout->addWidget(ui->viewLogButton);

    auto* features = new QHBoxLayout; features->setSpacing(18); home->addLayout(features);
    QVBoxLayout *gamingLayout, *trafficLayout;
    auto* gamingCard = card(ui->tabWidget_main,gamingLayout); features->addWidget(gamingCard,1);
    auto* gameRow = new QHBoxLayout; gameRow->addWidget(text(tr("Gaming mode"),"section",gamingCard),1); gameRow->addWidget(gamingButton); gamingLayout->addLayout(gameRow);
    gamingButton->setText(tr("Enable")); gamingButton->setMinimumWidth(94);
    gamingLayout->addWidget(text(tr("Adjust tunnel packet size. Available while connected; restored on disconnect."),"muted",gamingCard));
    connect(gamingButton,&QPushButton::toggled,this,[this](bool active) { gamingButton->setText(active ? tr("Enabled") : tr("Enable")); });
    auto* trafficCard = card(ui->tabWidget_main,trafficLayout); features->addWidget(trafficCard,1);
    auto* trafficRow = new QHBoxLayout; trafficRow->addWidget(text(tr("Traffic control"),"section",trafficCard),1);
    auto* rules = new QPushButton(tr("Manage  ↗"),trafficCard); rules->setProperty("quiet",true); trafficRow->addWidget(rules); trafficLayout->addLayout(trafficRow);
    trafficLayout->addWidget(text(tr("Choose which apps, domains and IP addresses use the VPN."),"muted",trafficCard));
    const auto updateProfile = [this,profileTitle,gateway,protocol,routeSummary,rules]() {
        const QString name = ui->serverList->currentText(); OcSettings settings;
        profileTitle->setText(name.isEmpty() ? tr("No server selected") : name);
        gateway->setText(name.isEmpty() ? tr("Add a server using your provider's existing address.") : settings.value("server:"+name+"/server").toString());
        gateway->setProperty("technical",!name.isEmpty());
        gateway->setLayoutDirection(name.isEmpty() ? layoutDirection() : Qt::LeftToRight);
        gateway->setAlignment(name.isEmpty() && ClientLanguage::isPersian() ? Qt::AlignRight|Qt::AlignVCenter|Qt::AlignAbsolute : Qt::AlignLeft|Qt::AlignVCenter|Qt::AlignAbsolute);
        protocol->setText(name.isEmpty() ? "—" : settings.value("server:"+name+"/protocol-name","anyconnect").toString().toUpper());
        const auto policy = TrafficPolicy::load(name); int count=0; for(const auto& rule:policy.rules) if(rule.enabled) ++count;
        routeSummary->setText(name.isEmpty() ? "—" : (policy.mode == "full" ? tr("All traffic through VPN") : policy.mode == "selected" ? tr("Selected traffic only") : tr("Server defaults")) + (count ? tr(" · %1 rules").arg(count) : QString()));
        rules->setEnabled(!name.isEmpty() && ui->serverList->isEnabled());
    };
    connect(ui->serverList,&QComboBox::currentTextChanged,this,[updateProfile](const QString&) { updateProfile(); });
    connect(rules,&QPushButton::clicked,this,[this,updateProfile]() { TrafficPolicyDialog dialog(ui->serverList->currentText(),this); dialog.exec(); updateProfile(); });
    updateProfile();
    connect(this,&MainWindow::stats_changed_sig,this,[received,sent](const QString& tx,const QString& rx,const QString&) { received->setText(rx); sent->setText(tx); });

    // A single navigation rail replaces the old tab strip and duplicated menus.
    ui->tabWidget->tabBar()->hide(); ui->tabWidget->setObjectName("pageStack"); ui->menuBar->hide();
    ui->horizontalLayout->removeWidget(ui->tabWidget); delete ui->horizontalLayout;
    auto* shell = new QHBoxLayout(ui->centralWidget); shell->setContentsMargins(0,0,0,0); shell->setSpacing(0);
    auto* sidebar = new QFrame(ui->centralWidget); sidebar->setObjectName("sidebar"); sidebar->setFixedWidth(204); shell->addWidget(sidebar);
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
    auto* bodyLayout = new QVBoxLayout(body); bodyLayout->setContentsMargins(30,28,30,24); bodyLayout->setSpacing(24);
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
        stateHint->setText(online ? tr("Your VPN session is active.") : state==STATUS_CONNECTING ? tr("Authenticating with your selected server…") : state==STATUS_DISCONNECTING ? tr("Restoring your local connection settings…") : tr("Choose a server to start your secure connection."));
        trayState->setText(online ? tr("●  Connected") : state==STATUS_CONNECTING ? tr("●  Connecting") : tr("●  Not connected"));
        badge->setProperty("online",online); badge->style()->unpolish(badge); badge->style()->polish(badge);
        if(state==STATUS_DISCONNECTED) { received->setText("—"); sent->setText("—"); }
    });
    // Card structure for settings; the existing controls retain their behavior.
    auto* preferences=ui->tabWidget->widget(3);
    auto* oldLayout=qobject_cast<QVBoxLayout*>(preferences->layout());
    QLayout* form=nullptr;
    while(oldLayout && oldLayout->count()) {
        auto* item=oldLayout->takeAt(0);
        if(item->layout()) { form=item->layout(); form->setParent(nullptr); }
        else { if(item->widget()) item->widget()->hide(); delete item; }
    }
    delete oldLayout;
    auto* settingsLayout=new QVBoxLayout(preferences); settingsLayout->setContentsMargins(0,0,0,0); settingsLayout->setSpacing(18);
    auto* settingsSections=new QTabWidget(preferences); settingsSections->setObjectName("settingsSections"); settingsLayout->addWidget(settingsSections);
    auto* general=new QWidget(settingsSections); auto* generalLayout=new QVBoxLayout(general); generalLayout->setContentsMargins(14,18,14,14); generalLayout->setSpacing(18);
    settingsSections->addTab(general,tr("General"));
    auto* components=new ComponentsWidget(settingsSections); settingsSections->addTab(components,tr("Components & prerequisites").replace("&","&&"));
    QVBoxLayout* settingsCardLayout; auto* settingsCard=card(general,settingsCardLayout); generalLayout->addWidget(settingsCard);
    settingsCardLayout->addWidget(text(tr("Client preferences"),"section",settingsCard));
    settingsCardLayout->addWidget(text(tr("Changes are saved automatically on this device."),"muted",settingsCard));
    if(form) { form->setParent(nullptr); settingsCardLayout->addLayout(form); form->setSpacing(12); }
    QVBoxLayout* noteLayout; auto* noteCard=card(general,noteLayout); generalLayout->addWidget(noteCard);
    noteLayout->addWidget(text(tr("Credentials stay on your device"),"section",noteCard));
    noteLayout->addWidget(text(tr("Saving a password is optional. Saved passwords use Windows encryption and are excluded from exported server catalogs."),"muted",noteCard));
    generalLayout->addStretch();
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
    for(int index=0;index<ui->tabWidget->count();++index) {
        auto* page=ui->tabWidget->widget(index); const QString title=ui->tabWidget->tabText(index);
        page->layout()->setSizeConstraint(QLayout::SetMinimumSize);
        if(index==0) page->setMinimumHeight(610);
        if(index==3) page->setMinimumHeight(590);
        ui->tabWidget->removeTab(index);
        auto* scroll=new QScrollArea(ui->tabWidget); scroll->setObjectName("pageScroll"); scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setWidget(page);
        ui->tabWidget->insertTab(index,scroll,title);
    }
    ui->tabWidget->setCurrentIndex(0);
    reload_settings();
    if(ui->serverList->count()>0 && ui->serverList->currentIndex()<0) ui->serverList->setCurrentIndex(0);
}

void MainWindow::applyClientTheme(bool dark) { applyGozarnoTheme(dark); }
