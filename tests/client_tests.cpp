// SPDX-License-Identifier: GPL-2.0-or-later
#include <QtTest>
#include "client/PublicIpLookup.h"
#include <cstring>
#include <QTemporaryDir>
#include <QJsonArray>
#include "client/TrafficPolicy.h"
#include "client/ServerCatalog.h"
#include "client/SocksBridge.h"
#include "client/AppRouter.h"
#include "client/GamingMode.h"
#include "client/GamingProfile.h"
#include "client/GamingQuality.h"
#include "client/GamingQos.h"
#include "client/GamingSession.h"
#include "client/GameDiscovery.h"
#include "dialog/GamingDialog.h"
#include <QUuid>
#include <limits>
#include "client/RecoveryPolicy.h"
#include "client/ConnectionHealth.h"
#include "client/KillSwitch.h"
#include "client/FailoverPlan.h"
#include "client/DiagnosticReport.h"
#include "client/NetworkEvents.h"
#include "client/PinnedProbe.h"
#include "dialog/FallbackDialog.h"
#include <QListWidget>
#include "client/WindowsIpInterface.h"
#include "client/ConnectionButtonIcon.h"
#include "cryptdata.h"
#include "OcSettings.h"
#include "dialog/SessionActivity.h"
#include "dialog/ServerLibrary.h"
#include "dialog/StableItemDelegate.h"
#include "client/ClientTheme.h"
#include "client/ConnectionState.h"
#include "client/ClientLanguage.h"
#include "dialog/ProxyEndpointWidget.h"
#include <QClipboard>
#include <QVBoxLayout>
#include <QComboBox>
#include "dialog/AboutDialog.h"
#include "dialog/TrafficPolicyDialog.h"
#include <QJsonDocument>
#include <QFile>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLineEdit>
#include "client/InstalledComponents.h"
#include "dialog/ComponentsWidget.h"
#include "logger.h"
#include <QCheckBox>
#include <QTableWidget>

class ClientTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
private slots:
    void languageChangesExistingControlsWithoutResettingState() {
        ClientLanguage::apply("en");QWidget window;QVBoxLayout layout(&window);
        QLabel title("Connection");QPushButton button("Connect");button.setProperty("translationSourceText","Connect");QCheckBox checked("Reconnect automatically");checked.setChecked(true);
        QComboBox mode;mode.addItem("Normal VPN",false);mode.addItem("SOCKS proxy · other connections stay direct",true);mode.setCurrentIndex(1);
        QLineEdit input("unchanged-password");ClientLanguage::technical(&input);QLabel name("Connection");name.setProperty("translationExcluded",true);
        const auto source=QString("Connection interrupted · retry in %1 seconds. Click Disconnect to stop.");QLabel dynamic(source.arg(5));
        layout.addWidget(&title);layout.addWidget(&button);layout.addWidget(&checked);layout.addWidget(&mode);layout.addWidget(&input);layout.addWidget(&name);layout.addWidget(&dynamic);
        QSignalSpy toggle(&checked,&QCheckBox::toggled);QSignalSpy selection(&mode,&QComboBox::currentIndexChanged);
        window.show();ClientLanguage::apply("fa");QApplication::processEvents();
        QCOMPARE(title.text(),QString::fromUtf8("اتصال"));QCOMPARE(button.text(),QObject::tr("Connect"));QCOMPARE(mode.currentIndex(),1);QVERIFY(mode.currentData().toBool());
        QCOMPARE(input.text(),QString("unchanged-password"));QCOMPARE(input.layoutDirection(),Qt::LeftToRight);QCOMPARE(name.text(),QString("Connection"));
        QCOMPARE(dynamic.text(),QObject::tr("Connection interrupted · retry in %1 seconds. Click Disconnect to stop.").arg(5));QVERIFY(checked.isChecked());QCOMPARE(toggle.count(),0);QCOMPARE(selection.count(),0);
        ClientLanguage::apply("en");QApplication::processEvents();QCOMPARE(title.text(),QString("Connection"));QCOMPARE(button.text(),QString("Connect"));QCOMPARE(dynamic.text(),source.arg(5));
        const auto dark=qApp->styleSheet();applyGozarnoTheme(false);const auto light=qApp->styleSheet();QVERIFY(!light.isEmpty());applyGozarnoTheme(true);QVERIFY(qApp->styleSheet()!=light);
        QCOMPARE(input.text(),QString("unchanged-password"));QVERIFY(checked.isChecked());
    }
    void endpointCopiesOnlyTheClickedAddress() {
        ProxyEndpointWidget widget;widget.setEndpoints("127.0.0.1:1080 | 192.0.2.10:1080");widget.show();
        const auto buttons=widget.findChildren<QPushButton*>("copyProxyEndpoint");QCOMPARE(buttons.size(),2);
        QTest::mouseClick(buttons[0],Qt::LeftButton);QCOMPARE(QApplication::clipboard()->text(),QString("127.0.0.1:1080"));
        QTest::mouseClick(buttons[1],Qt::LeftButton);QCOMPARE(QApplication::clipboard()->text(),QString("192.0.2.10:1080"));
        ClientLanguage::apply("fa");QApplication::processEvents();QVERIFY(buttons[0]->text().contains("127.0.0.1:1080"));
        widget.clearEndpoints();QCOMPARE(widget.findChildren<QPushButton*>("copyProxyEndpoint").size(),0);
    }
    void automaticGamingIgnoresLegacyManualSettings() {
        OcSettings settings;
        const auto oldTargets=settings.value("Client/healthTargets");
        const auto oldProfiles=settings.value("Client/gamingProfiles");
        GamingProfile legacy; legacy.id=QUuid::createUuid().toString(); legacy.name="Legacy game";
        legacy.route="direct"; legacy.autoMtu=false; legacy.mtu=1400;
        legacy.applications={directory.path()+"/game.exe"}; legacy.background={directory.path()+"/cloud.exe"};
        settings.setValue("Client/gamingProfiles",QJsonDocument(QJsonArray{legacy.toJson()}).toJson());
        settings.setValue("Client/healthTargets",QStringList{"invalid","127.0.0.1","8.8.8.8"});
        const auto profile=GamingProfile::automaticPreset();
        QCOMPARE(profile.target,QString("8.8.8.8"));
        QCOMPARE(profile.route,QString("inherit"));
        QVERIFY(profile.applications.isEmpty()); QVERIFY(profile.background.isEmpty());
        QVERIFY(profile.autoMtu); QVERIFY(!profile.automatic); QCOMPARE(profile.uploadKbps,0u);
        const auto base=TrafficPolicy{}; QCOMPARE(GamingProfile::overlay(base,profile).rules.size(),base.rules.size());
        settings.setValue("Client/healthTargets",QStringList{"bad","0.0.0.0","::1"});
        QCOMPARE(GamingProfile::automaticPreset().target,QString("1.1.1.1"));
        if(oldTargets.isValid()) settings.setValue("Client/healthTargets",oldTargets); else settings.remove("Client/healthTargets");
        if(oldProfiles.isValid()) settings.setValue("Client/gamingProfiles",oldProfiles); else settings.remove("Client/gamingProfiles");
    }
    void gamingQualityMeasuresRealSamplesAndLossSeparately() {
        QVector<GamingSample> samples;
        for(int i=1;i<=10;++i) samples.append({double(i),false}); samples.append({-1,false}); samples.append({std::numeric_limits<double>::quiet_NaN(),false});
        const auto summary=GamingQuality::summarize(samples);
        QCOMPARE(summary.attempts,12); QCOMPARE(summary.replies,10); QCOMPARE(summary.median,5.5); QCOMPARE(summary.p95,10.); QCOMPARE(summary.jitter,1.);
        QVERIFY(qAbs(summary.failedPercent-100./6)<.001); QVERIFY(summary.comparable()); QVERIFY(!summary.loadComparison);
        QVERIFY(!GamingQuality::summarize({{-1,false}}).comparable());
    }
    void gamingLoadedLatencyAndRollbackNeedEnoughEvidence() {
        QVector<GamingSample> samples; for(int i=0;i<8;++i) {samples.append({20,false}); samples.append({60,true});}
        const auto summary=GamingQuality::summarize(samples); QVERIFY(summary.loadComparison); QCOMPARE(summary.loadedDelta,40.);
        GamingSummary before=summary,after=summary; after.median=before.median+40; QVERIFY(GamingQuality::worse(before,after)); after.replies=2; QVERIFY(!GamingQuality::worse(before,after));
        QCOMPARE(GamingQuality::summarize({{10,false},{80,true}}).loadComparison,false);
    }
    void gamingProfilesPreserveBaseRulesAndExcludeUnsafeThrottle() {
        GamingProfile profile; profile.id=QUuid::createUuid().toString(); profile.name="Game"; profile.applications={directory.path()+"/game.exe"}; profile.background={directory.path()+"/cloud.exe"}; profile.route="vpn";
        GamingProfile checked; QString error; QVERIFY(GamingProfile::fromJson(profile.toJson(),checked,error));
        TrafficPolicy base; base.rules.append({"application",profile.applications.first(),"direct",true}); base.rules.append({"network","192.0.2.0/24","vpn",true});
        const auto overlay=GamingProfile::overlay(base,checked); QCOMPARE(base.rules.first().action,QString("direct")); QCOMPARE(overlay.rules.size(),2); QCOMPARE(overlay.rules.last().action,QString("vpn"));
        profile.background=profile.applications; QVERIFY(!GamingProfile::fromJson(profile.toJson(),checked,error));
        profile.background={directory.path()+"/GozarnoVPN.exe"}; QVERIFY(!GamingProfile::fromJson(profile.toJson(),checked,error));
        profile.background.clear(); profile.target="not-an-ip"; QVERIFY(!GamingProfile::fromJson(profile.toJson(),checked,error));
    }
    void gamingQosPlansUseOwnedNamesAndNoScriptInterpolation() {
        GamingProfile profile; profile.id=QUuid::createUuid().toString(); profile.name="Game"; profile.background={directory.path()+"/cloud'$payload.exe"};
        const auto plan=GamingQos::plan(profile); QCOMPARE(plan.size(),1); QVERIFY(GamingQos::validOwnedName(plan.first().toObject().value("name").toString()));
        QVERIFY(!GamingQos::validOwnedName("OtherVendorPolicy")); QCOMPARE(plan.first().toObject().value("bits").toInteger(),qint64(1024000));
        QVERIFY(!GamingQos::script(false).contains(profile.background.first())); QVERIFY(GamingQos::script(false).contains("ActiveStore")); QVERIFY(GamingQos::script(true).contains("policy ownership mismatch"));
    }
    void gamingOffAndMissingAdapterNeverProbeDirectly() {
        GamingProfile profile; profile.target="1.1.1.1";
        QCOMPARE(GamingQuality::probe("missing-gozarno-test-adapter",profile).milliseconds,-1.);
        std::atomic_bool cancelled(true); QCOMPARE(GamingQuality::discoverMtu({},profile,1500,cancelled),0u);
        GamingSession session; QVERIFY(!session.enabled()); QVERIFY(!session.protectsMatch()); session.setEnabled(true); QVERIFY(session.protectsMatch()); session.setEnabled(false); QVERIFY(!session.protectsMatch());
        const auto report=QJsonDocument(session.report()).toJson(); QVERIFY(!report.contains("1.1.1.1")); QVERIFY(!report.contains("network"+QString("=").toUtf8()));
    }
    void installedGameScanRejectsEscapeAndHonorsCancellation() {
        const QString root=directory.path()+"/SteamTest"; QVERIFY(QDir().mkpath(root+"/steamapps/common/TestGame/bin"));
        const auto write=[](const QString& path,const QByteArray& bytes) {QFile f(path); if(!f.open(QIODevice::WriteOnly)) return false; return f.write(bytes)==bytes.size();};
        QVERIFY(write(root+"/steamapps/common/TestGame/bin/game.exe","fixture")); QVERIFY(write(root+"/steamapps/common/TestGame/uninstall.exe","fixture"));
        QVERIFY(write(root+"/steamapps/appmanifest_123.acf","\"name\" \"Test game\" \"installdir\" \"TestGame\""));
        QVERIFY(write(root+"/steamapps/appmanifest_456.acf","\"name\" \"Escape\" \"installdir\" \"../Elsewhere\""));
        std::atomic_bool cancelled(false); const auto games=GameDiscovery::steam(root,cancelled); QCOMPARE(games.size(),1); QCOMPARE(games.first().name,QString("Test game")); QVERIFY(games.first().executable.endsWith("game.exe"));
        cancelled.store(true); QVERIFY(GameDiscovery::steam(root,cancelled).isEmpty());
    }
    void gamingProcessDetectionMatchesFullExecutablePath() {
#ifdef _WIN32
        QVERIFY(GamingProfile::processesRunning({QCoreApplication::applicationFilePath()}));
        QVERIFY(!GamingProfile::processesRunning({directory.path()+"/"+QFileInfo(QCoreApplication::applicationFilePath()).fileName()}));
#endif
    }
    void publicIpResponseRejectsInvalidData() {
        QVERIFY(PublicIpLookup::parse("not json").isEmpty());
        QVERIFY(PublicIpLookup::parse("{\"ip\":\"127.0.0.1\",\"country\":\"US\"}").isEmpty());
        QVERIFY(PublicIpLookup::parse("{\"ip\":\"1.1.1.1\",\"country\":\"<b>\"}").isEmpty());
        const auto value=PublicIpLookup::parse("{\"ip\":\"1.1.1.1\",\"country\":\"AU\",\"city\":\"Sydney\"}");
        QCOMPARE(value.value("country").toString(),QString("AU"));
        QCOMPARE(value.value("city").toString(),QString("Sydney"));
    }
    void diagnosesDoNotBlameTheServerWithoutEvidence() {
        QCOMPARE(ConnectionHealth::classify(false,true,false,true,false),QString("network_unavailable"));
        QCOMPARE(ConnectionHealth::classify(true,false,false,true,false),QString("tunnel_unavailable"));
        QCOMPARE(ConnectionHealth::classify(true,true,false,true,true),QString("tunnel_unreachable"));
        QCOMPARE(ConnectionHealth::classify(true,true,true,true,false),QString("dns_failed"));
        QCOMPARE(ConnectionHealth::classify(true,true,true,false,false),QString("healthy"));
    }
    void dnsValidationRejectsWrongTransactionsErrorsAndTruncatedAnswers() {
        auto reply=PinnedProbe::dnsQuery(0x1234); reply[2]=char(0x81); reply[3]=char(0x80); reply[7]=char(1);
        QVERIFY(!PinnedProbe::dnsAnswer(reply,0x1234));
        reply+=QByteArray::fromHex("c00c000100010000003c000401010101");
        QVERIFY(PinnedProbe::dnsAnswer(reply,0x1234));
        QVERIFY(!PinnedProbe::dnsAnswer(reply,0x9999));
        QVERIFY(!PinnedProbe::dnsAnswer(reply.left(reply.size()-1),0x1234));
        reply[3]=char(0x83); QVERIFY(!PinnedProbe::dnsAnswer(reply,0x1234));
        reply[3]=char(0x80); reply[2]=char(0x83); QVERIFY(!PinnedProbe::dnsAnswer(reply,0x1234));
    }
    void fallbackUsesOnlyApprovedExistingProfilesOnce() {
        FailoverPlan plan; plan.reset("primary",{"unknown","backup-b","primary","backup-b","backup-a"},{"primary","backup-a","backup-b","unapproved"});
        QCOMPARE(plan.current(),QString("primary")); QCOMPARE(plan.next(),QString("backup-b"));
        QCOMPARE(plan.next(),QString("backup-a")); QVERIFY(!plan.hasNext()); QVERIFY(plan.next().isEmpty());
        plan.reset("primary",{}, {"primary","unapproved"}); QVERIFY(!plan.hasNext());
    }
    void fallbackDialogSavesOrderWithoutCopyingCredentials() {
        OcSettings settings;
        settings.setValue("server:primary/server","https://primary.example");
        settings.setValue("server:backup/server","https://backup.example");
        settings.setValue("server:primary/password","encrypted-primary"); settings.setValue("server:backup/password","encrypted-backup");
        settings.setValue("server:backup/protocol-name","fortinet"); settings.sync();
        FallbackDialog dialog("primary"); auto* list=dialog.findChild<QListWidget*>("fallbackProfiles"); QVERIFY(list); QCOMPARE(list->count(),1);
        list->item(0)->setCheckState(Qt::Checked);
        auto* buttons=dialog.findChild<QDialogButtonBox*>(); QVERIFY(buttons); buttons->button(QDialogButtonBox::Save)->click();
        OcSettings stored; QCOMPARE(stored.value("server:primary/recovery/fallbacks").toStringList(),QStringList{"backup"});
        QCOMPARE(stored.value("server:primary/password").toString(),QString("encrypted-primary"));
        QCOMPARE(stored.value("server:backup/password").toString(),QString("encrypted-backup"));
        QCOMPARE(stored.value("server:backup/protocol-name").toString(),QString("fortinet"));
    }
    void diagnosticExportRejectsSecretsAndArbitraryLogText() {
        DiagnosticReport report;
        report.record("failure",{{"reason","authentication"},{"error_code",-13},{"password","secret-password"},{"token","secret-token"},{"server","private.example"},{"path","C:/Users/private/key.pem"}});
        report.record("raw log containing secret-password");
        report.record("health",{{"reason","private.example"}});
        report.record("protection_test",{{"test","ipv4_direct_tcp"},{"result","skipped"}});
        const auto bytes=report.document().toJson();
        QVERIFY(!bytes.contains("secret-password")); QVERIFY(!bytes.contains("secret-token"));
        QVERIFY(!bytes.contains("private.example")); QVERIFY(!bytes.contains("key.pem"));
        QVERIFY(bytes.contains("authentication")); QVERIFY(bytes.contains("skipped"));
        for(int i=0;i<550;++i) report.record("recovery",{{"attempt",i}});
        QCOMPARE(report.document().object().value("events").toArray().size(),500);
    }
    void resumeNotificationsAreCoalesced() {
#ifdef _WIN32
        NetworkEvents environment; QSignalSpy events(&environment,&NetworkEvents::environmentChanged);
        MSG message{}; message.message=WM_POWERBROADCAST; message.wParam=PBT_APMRESUMEAUTOMATIC;
        environment.nativeEventFilter({},&message,nullptr); environment.nativeEventFilter({},&message,nullptr);
        QTest::qWait(1700); QCOMPARE(events.count(),1); QCOMPARE(events.at(0).at(0).toString(),QString("resume"));
#endif
    }
    void recoveryNeedsRepeatedFailuresAndBacksOff() {
        RecoveryPolicy policy;
        QVERIFY(!policy.result(false, 0, true));
        QVERIFY(!policy.result(false, 15000, true));
        QVERIFY(policy.result(false, 30000, true));
        QVERIFY(!policy.result(false, 45000, true));
        QVERIFY(policy.result(false, 60000, true));
        QVERIFY(!policy.result(false, 90000, true));
        QVERIFY(policy.result(false, 120000, true));
        QVERIFY(!policy.result(true, 121000, true));
        QCOMPARE(policy.failures, 0); QCOMPARE(policy.attempts, 0);
        QVERIFY(!policy.result(false, 130000, true));
    }
    void automaticRecoveryCanBeDisabledAndDelayIsBounded() {
        RecoveryPolicy policy;
        for (int i=0;i<50;++i) QVERIFY(!policy.result(false, i*15000, false));
        QCOMPARE(policy.attempts, 0);
        qint64 now=1000000;
        for (int i=0;i<20;++i) {
            QVERIFY(policy.result(false, now, true));
            QVERIFY(policy.nextAttempt-now<=300000);
            now=policy.nextAttempt;
        }
    }
    void stoppingHealthMonitorCancelsPendingResults() {
        ConnectionHealth health;
        QSignalSpy recovery(&health, &ConnectionHealth::recoveryRequested);
        QSignalSpy result(&health, &ConnectionHealth::healthChanged);
        health.start("nonexistent-gozarno-interface");
        QVERIFY(QMetaObject::invokeMethod(&health, "probe", Qt::DirectConnection));
        health.stop();
        health.recheck(); // A stopped session must never restart after resume.
        QTest::qWait(50); QCOMPARE(recovery.count(),0); QCOMPARE(result.count(),0);
    }
    void killSwitchRejectsEmptyGatewayWithoutChangingSystem() {
        KillSwitch protection; QString error;
        QVERIFY(!protection.arm({},error)); QVERIFY(!error.isEmpty());
        QVERIFY(!protection.active()); QVERIFY(protection.release(error));
    }
    void initTestCase() {
        QVERIFY(directory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    }
    void cleanup() { ClientLanguage::apply("en"); OcSettings settings; settings.clear(); settings.sync(); }
    void persianLanguageMirrorsFormsAndPreservesTechnicalFields() {
        ClientLanguage::apply("en"); QCOMPARE(QApplication::layoutDirection(),Qt::LeftToRight);
        QCOMPARE(QObject::tr("Connection"),QString("Connection"));
        ClientLanguage::apply("fa"); QCOMPARE(QApplication::layoutDirection(),Qt::RightToLeft);
        QCOMPARE(QObject::tr("Connection"),QString::fromUtf8("اتصال"));
        QLabel mixed(QString::fromUtf8("Server · تغییرات در اتصال بعدی اعمال می‌شوند")); mixed.show(); QTest::qWait(10);
        QVERIFY(mixed.alignment().testFlag(Qt::AlignRight));
        OcSettings settings; settings.setValue("server:نمونه/server","https://vpn.example.com"); settings.sync();
        ServerLibrary library; library.resize(850,500); library.show(); QTest::qWait(10);
        auto* servers=library.findChild<QTableWidget*>(); QVERIFY(servers);
        QCOMPARE(servers->horizontalHeaderItem(0)->text(),QString::fromUtf8("علاقه‌مندی"));
        QVERIFY(servers->visualItemRect(servers->item(0,0)).center().x()>servers->visualItemRect(servers->item(0,4)).center().x());
        const QRect favorite=servers->visualItemRect(servers->item(0,0));
        QTest::mouseClick(servers->viewport(),Qt::LeftButton,Qt::NoModifier,favorite.center());
        QCOMPARE(servers->item(0,0)->checkState(),Qt::Checked);
        TrafficPolicy policy; policy.rules={{"network","198.51.100.0/24","direct",true}}; QString error; QVERIFY(policy.save("نمونه",error));
        TrafficPolicyDialog rules(QString::fromUtf8("نمونه"));
        QCOMPARE(rules.layoutDirection(),Qt::RightToLeft);
        auto* ruleTable=rules.findChild<QTableWidget*>(); QVERIFY(ruleTable);
        auto* target=qobject_cast<QLineEdit*>(ruleTable->cellWidget(0,2)); QVERIFY(target);
        QCOMPARE(target->layoutDirection(),Qt::LeftToRight); QCOMPARE(target->text(),QString("198.51.100.0/24"));
        SessionActivity activity; activity.setSessionInfo("1.1.1.1","10.0.0.2","","TLS-AES","");
        QCOMPARE(activity.findChild<QLabel*>("session_ip")->layoutDirection(),Qt::LeftToRight);
        QCOMPARE(activity.findChild<QLabel*>("session_ip6")->layoutDirection(),Qt::RightToLeft);
    }
    void aboutUsesValidUnicodeInBothLanguages() {
        for(const auto& language:{QString("en"),QString("fa")}) {
            ClientLanguage::apply(language); AboutDialog about;
            auto* credits=about.findChild<QLabel*>("aboutCredits"); QVERIFY(credits);
            QVERIFY(credits->text().contains(QString::fromUtf8("Ľubomír Carik")));
            QVERIFY(!credits->text().contains(QChar(0xfffd)));
            QCOMPARE(credits->layoutDirection(),Qt::LeftToRight);
            if(language=="fa") {
                QCOMPARE(about.windowTitle(),QString::fromUtf8("دربارهٔ گذرنو"));
                QCOMPARE(about.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Close)->text(),QString::fromUtf8("بستن"));
            }
        }
    }
    void persianCatalogPreservesSubstitutionFields() {
        QFile file(":/translations/fa.json"); QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonParseError error; const auto document=QJsonDocument::fromJson(file.readAll(),&error);
        QCOMPARE(error.error,QJsonParseError::NoError); QVERIFY(document.object().size()>450);
        const QRegularExpression placeholders("%[1-9][0-9]*|%n");
        const auto catalog=document.object();
        for(auto it=catalog.constBegin();it!=catalog.constEnd();++it) {
            QStringList before,after; auto source=placeholders.globalMatch(it.key()); auto target=placeholders.globalMatch(it.value().toString());
            while(source.hasNext()) before<<source.next().captured(); while(target.hasNext()) after<<target.next().captured();
            before.sort(); after.sort(); QCOMPARE(before,after);
        }
    }
    void componentInventorySeparatesBundledFilesFromSystemPrerequisites() {
        QTemporaryDir package; QVERIFY(package.isValid());
        const auto missing=InstalledComponents::inspect(package.path()); QCOMPARE(missing.size(),8);
        for(int i=0;i<5;++i) { QCOMPARE(missing[i].state,InstalledComponent::Missing); QVERIFY(!missing[i].status.isEmpty()); }
        QVERIFY(QDir(package.path()).mkpath("backend"));
        for(const auto& name:{"libopenconnect-5.dll","wintun.dll","backend/ProxiFyre.exe","Qt6Core.dll","libgnutls-30.dll"}) {
            QFile file(package.filePath(QString::fromLatin1(name))); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("test file");
        }
        const auto present=InstalledComponents::inspect(package.path());
        for(int i=0;i<5;++i) { QCOMPARE(present[i].state,InstalledComponent::Ready); QCOMPARE(present[i].location,QString("Inside Gozarno")); }
        for(const auto& language:{QString("en"),QString("fa")}) {
            ClientLanguage::apply(language); ComponentsWidget widget; widget.resize(850,600); widget.show(); QTest::qWait(10);
            auto* table=widget.findChild<QTableWidget*>("componentsTable"); QVERIFY(table); QCOMPARE(table->rowCount(),8);
            if(language=="fa") QCOMPARE(table->horizontalHeaderItem(2)->text(),QString::fromUtf8("وضعیت"));
            widget.refresh(); QCOMPARE(table->rowCount(),8);
        }
    }
    void legacySettingsMigrateWithoutOverwritingCredentials() {
        QSettings old(QSettings::IniFormat,QSettings::UserScope,"Arovan","ArovanVPN"); old.clear();
        QString entropy="https://old.example.com";
        const QByteArray encrypted=CryptData::encode(entropy,QString::fromUtf8("migration-test-رمز"));
        QVERIFY(!encrypted.isEmpty());
        old.setValue("server:imported/server","https://old.example.com"); old.setValue("server:imported/password",encrypted);
        old.setValue("server:existing/server","https://other.example.com"); old.setValue("server:existing/password",encrypted); old.sync();
        OcSettings current; current.setValue("server:existing/server","https://new.example.com"); current.sync();
        QVERIFY(OcSettings::migrateLegacy());
        OcSettings imported; QCOMPARE(imported.value("server:imported/password").toByteArray(),encrypted);
        QString password; QVERIFY(CryptData::decode(entropy,imported.value("server:imported/password").toByteArray(),password));
        QCOMPARE(password,QString::fromUtf8("migration-test-رمز"));
        QCOMPARE(imported.value("server:existing/server").toString(),QString("https://new.example.com"));
        QVERIFY(!imported.contains("server:existing/password"));
        QCOMPARE(old.value("server:imported/password").toByteArray(),encrypted);
        old.setValue("server:later/server","https://later.example.com"); old.sync();
        QVERIFY(OcSettings::migrateLegacy()); QVERIFY(!OcSettings().contains("server:later/server")); old.clear(); old.sync();
    }
    void checkboxesHaveVisibleStableCheckedStates() {
        for(bool dark:{false,true}) {
            applyGozarnoTheme(dark); QCheckBox checkbox("Save settings"); checkbox.resize(240,52); checkbox.show(); QTest::qWait(20);
            const auto size=checkbox.size(); const QImage off=checkbox.grab().toImage();
            QTest::mouseClick(&checkbox,Qt::LeftButton,Qt::NoModifier,QPoint(10,checkbox.height()/2));
            QVERIFY(checkbox.isChecked()); const QImage on=checkbox.grab().toImage(); QVERIFY(off!=on); QCOMPARE(checkbox.size(),size);
            QTest::keyClick(&checkbox,Qt::Key_Space); QVERIFY(!checkbox.isChecked());
            checkbox.setEnabled(false); QTest::mouseClick(&checkbox,Qt::LeftButton); QVERIFY(!checkbox.isChecked());
        }
    }
    void favoriteCellTogglesAndPersistsAfterSelection() {
        OcSettings settings; settings.setValue("server:test/server","https://vpn.example.com"); settings.sync();
        ServerLibrary library; library.resize(850,500); library.show(); QTest::qWait(20);
        auto* table=library.findChild<QTableWidget*>(); QVERIFY(table); QCOMPARE(table->rowCount(),1);
        for(int column=1;column<5;++column) {
            QVERIFY(!table->item(0,column)->flags().testFlag(Qt::ItemIsUserCheckable));
            QVERIFY(!table->item(0,column)->data(Qt::CheckStateRole).isValid());
        }
        const auto rect=table->visualItemRect(table->item(0,0));
        QTest::mouseClick(table->viewport(),Qt::LeftButton,Qt::NoModifier,rect.center());
        QCOMPARE(table->item(0,0)->checkState(),Qt::Checked);
        QVERIFY(OcSettings().value("server:test/catalog/favorite").toBool());
        const auto selectionImage=table->viewport()->grab().toImage();
        const QColor selection=selectionImage.pixelColor(table->viewport()->width()-30,rect.center().y());
        QVERIFY2(selection.green()>selection.blue() && selection.green()>selection.red(),"Selected rows must use the green theme, not native blue");
        library.refresh(); QCOMPARE(table->item(0,0)->checkState(),Qt::Checked);
        table->setCurrentCell(0,0); QTest::keyClick(table,Qt::Key_Space); QCOMPARE(table->item(0,0)->checkState(),Qt::Unchecked);
    }
    void sessionShowsRealValuesAndRetainsLastSession() {
        SessionActivity activity; activity.resize(850,650); activity.show();
        activity.setProfile("test"); activity.setSessionInfo("1.1.1.1","10.0.0.2","","TLS-AES","DTLS-AES");
        activity.setConnectionState(STATUS_CONNECTED); QTest::qWait(20); activity.updateTraffic(1024,2048);
        QCOMPARE(activity.findChild<QLabel*>("session_sent")->text(),QString("1.0 KiB"));
        QCOMPARE(activity.findChild<QLabel*>("session_received")->text(),QString("2.0 KiB"));
        QCOMPARE(activity.findChild<QLabel*>("session_ip")->text(),QString("10.0.0.2"));
        QVERIFY(!activity.findChild<QLabel*>("session_ip6")->text().isEmpty());
        activity.setConnectionState(STATUS_DISCONNECTED); activity.updateTraffic(0,0);
        QCOMPARE(activity.findChild<QLabel*>("session_received")->text(),QString("2.0 KiB"));
        QVERIFY(activity.findChild<QLabel*>("sessionState")->text().contains("Last session"));
        activity.setConnectionState(STATUS_CONNECTED);
        QCOMPARE(activity.findChild<QLabel*>("session_received")->text(),QString("0 B"));
    }
    void loggerIsBoundedAndSafeForReentrantReaders() {
        auto& logger=Logger::instance(); logger.clear(); bool queried=false;
        const auto connection=connect(&logger,&Logger::newLogMessage,this,[&](const Logger::Message& message){ queried=!logger.getMessages(message.id-1).isEmpty(); },Qt::DirectConnection);
        logger.addMessage("first"); QVERIFY(queried); disconnect(connection);
        for(int i=0;i<10000;++i) logger.addMessage(QString::number(i));
        const auto messages=logger.getMessages(); QCOMPARE(messages.size(),4096); QCOMPARE(messages.last().text,QString("9999"));
        QVERIFY(logger.getMessages(messages.last().id).isEmpty()); logger.clear();
    }
    void rejectUnsafeRules() {
        QString error;
        for (const auto& text : {"0.0.0.0/0", "127.0.0.1", "224.0.0.1", "192.0.2.1/33", "garbage"}) {
            TrafficRule rule{"network", text, "vpn", true}; QVERIFY(!TrafficPolicy::validateRule(rule, error));
        }
        TrafficRule host{"network", "203.0.113.42", "direct", true};
        QVERIFY(TrafficPolicy::validateRule(host, error)); QCOMPARE(host.target, QString("203.0.113.42/32"));
        TrafficRule v6{"network", "2001:db8::/32", "vpn", true}; QVERIFY(TrafficPolicy::validateRule(v6, error));
        for (const auto& text : {"https://example.com/path", "*.example.com", "bad host.com", "example.com;command"}) {
            TrafficRule rule{"domain", text, "vpn", true}; QVERIFY(!TrafficPolicy::validateRule(rule, error));
        }
    }
    void policyRoundTripAndDuplicates() {
        TrafficPolicy policy; policy.mode = "selected";
        policy.rules = {{"domain", "Example.COM", "vpn", true}, {"network", "192.0.2.0/24", "direct", false}};
        QString error; QVERIFY(policy.save("sample", error));
        const auto loaded = TrafficPolicy::load("sample", &error); QVERIFY(error.isEmpty());
        QCOMPARE(loaded.mode, QString("selected")); QCOMPARE(loaded.rules.size(), 2);
        QCOMPARE(loaded.rules[0].target, QString("example.com")); QVERIFY(!loaded.rules[1].enabled);
        policy.rules.append({"domain", "example.com", "direct", true}); QVERIFY(!policy.save("sample", error));
        QCOMPARE(TrafficPolicy::load("sample").rules.size(), 2);
    }
    void catalogProtectsSecretsAndExistingProfiles() {
        OcSettings settings;
        settings.setValue("server:existing/server", "https://vpn.example.com");
        settings.setValue("server:existing/password", "secret-never-export");
        settings.setValue("server:existing/token-str", "otp-secret");
        settings.setValue("server:existing/client-key", "private-key"); settings.sync();
        const auto catalog = ServerCatalog::exportProfiles();
        QVERIFY(!catalog.toJson().contains("secret-never-export"));
        QVERIFY(!catalog.toJson().contains("otp-secret")); QVERIFY(!catalog.toJson().contains("private-key"));
        QStringList names; QString error; QVERIFY(ServerCatalog::importProfiles(catalog, names, error));
        QCOMPARE(names.size(), 1); QVERIFY(names[0] != "existing");
        QCOMPARE(settings.value("server:existing/password").toString(), QString("secret-never-export"));
        OcSettings reloaded; QVERIFY(!reloaded.contains("server:" + names[0] + "/password"));
    }
    void invalidCatalogIsAtomic() {
        QJsonArray entries{QJsonObject{{"name", "good"}, {"gateway", "https://vpn.example.com"}},
            QJsonObject{{"name", "bad/child"}, {"gateway", "https://vpn.example.com"}}};
        QJsonDocument document(QJsonObject{{"format", "openconnect-client-catalog"}, {"version", 1}, {"servers", entries}});
        QStringList names; QString error; QVERIFY(!ServerCatalog::importProfiles(document, names, error));
        OcSettings settings; QVERIFY(settings.allKeys().isEmpty());
        QVERIFY(!ServerCatalog::validGateway("https://user:password@vpn.example.com"));
        QVERIFY(!ServerCatalog::validGateway("http://vpn.example.com"));
    }
    void windowsSecretRoundTrip() {
#ifdef _WIN32
        QString entropy = "https://vpn.example.com";
        const QString password = QString::fromUtf8("test-password-رمز");
        const auto encoded = CryptData::encode(entropy, password);
        QVERIFY(encoded.startsWith("xxxx")); QVERIFY(!encoded.contains(password.toUtf8()));
        QString decoded; QVERIFY(CryptData::decode(entropy, encoded, decoded)); QCOMPARE(decoded, password);
        QString wrong = "https://different.example.com";
        QVERIFY(!CryptData::decode(wrong, encoded, decoded)); QVERIFY(decoded.isEmpty());
#endif
    }
    void fragmentedSocksTargets() {
        const auto packet = QByteArray::fromHex("010102030401bb");
        QString host; quint16 port = 0;
        for (int length = 0; length < packet.size(); ++length)
            QCOMPARE(readSocksTarget(packet.left(length), 0, host, port), 0);
        QCOMPARE(readSocksTarget(packet, 0, host, port), 7);
        QCOMPARE(host, QString("1.2.3.4")); QCOMPARE(port, quint16(443));
        QCOMPARE(readSocksTarget(QByteArray::fromHex("090000"), 0, host, port), -1);
        QCOMPARE(readSocksTarget(QByteArray::fromHex("0300"), 0, host, port), -1);
        QCOMPARE(readSocksTarget(QByteArray::fromHex("01010203040000"), 0, host, port), -1);
    }
    void missingAppBackendFailsExplicitly() {
        OcSettings settings; settings.setValue("Client/appRouterPath", directory.filePath("absent.exe"));
        TrafficPolicy policy; policy.rules.append({"application", "C:\\Program Files\\Browser\\browser.exe", "vpn", true});
        QString error; QVERIFY(!AppRouter::preflight(policy, error)); QVERIFY(!error.isEmpty());
        policy.rules.clear(); QVERIFY(AppRouter::preflight(policy, error));
    }
    void connectionActionIconsAreVisibleAtDisplayScales() {
        for (const auto action : {ConnectionButtonAction::Connect, ConnectionButtonAction::Disconnect, ConnectionButtonAction::Cancel}) {
            const auto icon = connectionButtonIcon(action);
            QVERIFY(!icon.isNull());
            for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
                for (const qreal scale : {1.0, 1.5, 2.0, 3.0}) {
                    const auto pixmap = icon.pixmap(QSize(20,20),scale,mode);
                    QVERIFY(!pixmap.isNull());
                    const auto pixels = pixmap.toImage(); int ink = 0;
                    for (int y=0; y<pixels.height(); ++y) for (int x=0; x<pixels.width(); ++x)
                        if (qAlpha(pixels.pixel(x,y)) > 128) ++ink;
                    QVERIFY(ink > pixels.width()*pixels.height()/10);
                }
            }
        }
    }
    void gamingMtuWritePreservesInterfaceSettings() {
#ifdef _WIN32
        MIB_IPINTERFACE_ROW row{}; InitializeIpInterfaceEntry(&row);
        row.Family = AF_INET; row.InterfaceLuid.Value = 123;
        row.NlMtu = 1400; row.SitePrefixLength = 32;
        row.Metric = 23; row.UseAutomaticMetric = FALSE;
        row.DisableDefaultRoutes = TRUE;
        auto expected = row; expected.NlMtu = 1280; expected.SitePrefixLength = 0;
        const auto enabled = WindowsIpInterface::withMtu(row, 1280);
        QVERIFY(std::memcmp(&enabled, &expected, sizeof(row)) == 0);
        QCOMPARE(row.NlMtu, ULONG(1400)); // Input snapshot stays intact.
        const auto restored = WindowsIpInterface::withMtu(enabled, 1400);
        QCOMPARE(restored.NlMtu, ULONG(1400));
        QCOMPARE(restored.SitePrefixLength, ULONG(0));
        row.Family = AF_INET6; row.SitePrefixLength = 64;
        expected = row; expected.NlMtu = 1280;
        const auto ipv6 = WindowsIpInterface::withMtu(row, 1280);
        QVERIFY(std::memcmp(&ipv6, &expected, sizeof(row)) == 0);
#endif
    }
    void gamingRequiresAnActiveTunnel() {
        GamingMode mode; QString error;
        QVERIFY(!mode.enable({}, 1280, error)); QVERIFY(!mode.active());
        QVERIFY(!mode.enable("nonexistent-arovan-adapter", 1200, error));
        QVERIFY(mode.disable()); QVERIFY(!mode.active());
    }
    void socksAuthenticationAndFragmentation() {
        SocksBridge bridge({}, "test-session-secret"); QString error; QVERIFY(bridge.listen(error));
        QTcpSocket client; client.connectToHost(QHostAddress::LocalHost, bridge.port());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write(QByteArray::fromHex("05")); QTest::qWait(10); QCOMPARE(client.bytesAvailable(), qint64(0));
        client.write(QByteArray::fromHex("0102")); QTRY_VERIFY(client.bytesAvailable() >= 2);
        QCOMPARE(client.read(2), QByteArray::fromHex("0502"));
        const QByteArray password("test-session-secret");
        client.write(QByteArray::fromHex("0106") + "client" + QByteArray(1, char(password.size())) + password);
        QTRY_VERIFY(client.bytesAvailable() >= 2); QCOMPARE(client.read(2), QByteArray::fromHex("0100"));
        client.write(QByteArray::fromHex("050100010102030401bb"));
        QTRY_VERIFY(client.bytesAvailable() >= 10); const auto response = client.readAll();
        QCOMPARE(quint8(response[1]), quint8(3)); // No outbound interface: explicit failure, no fallback.
    }
};
QTEST_MAIN(ClientTests)
#include "client_tests.moc"
