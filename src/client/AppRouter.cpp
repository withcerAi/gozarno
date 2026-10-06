// SPDX-License-Identifier: GPL-2.0-or-later
#include "AppRouter.h"
#include "SocksBridge.h"
#include "VpnRoutes.h"
#include "OcSettings.h"
#include "logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QProcess>
#include <QThread>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QElapsedTimer>
#include <QUuid>
#include <QStandardPaths>
#ifdef _WIN32
#include <windows.h>
#include <winsvc.h>
#include <sddl.h>
#include <aclapi.h>
#include <shlobj.h>
#endif

struct AppRouter::Impl {
    QThread thread;
    QObject* worker = nullptr;
    QProcess* engine = nullptr;
    std::unique_ptr<QTemporaryDir> directory;
    QString firewallName;
};
static bool firewallRule(const QStringList& arguments)
{
#ifdef _WIN32
    QProcess process;
    process.start(qEnvironmentVariable("SystemRoot") + "/System32/netsh.exe", arguments);
    return process.waitForFinished(10000) && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
#else
    Q_UNUSED(arguments);
    return false;
#endif
}
static QString enginePath()
{
    OcSettings settings;
    return settings.value("Client/appRouterPath",
        QCoreApplication::applicationDirPath() + "/backend/ProxiFyre.exe").toString();
}
static bool hasApplicationRules(const TrafficPolicy& policy)
{
    for (const auto& rule : policy.rules) if (rule.enabled && rule.kind == "application") return true;
    return false;
}
AppRouter::AppRouter() : impl(new Impl) {}
AppRouter::~AppRouter() { stop(); }
bool AppRouter::preflight(const TrafficPolicy& policy, QString& error)
{
    if (!hasApplicationRules(policy)) return true;
#ifdef _WIN32
    if (!QFile::exists(enginePath())) {
        error = QObject::tr("Application routing requires the local ProxiFyre component. Choose its executable in Preferences. No server change is required.");
        return false;
    }
    SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    SC_HANDLE service = manager ? OpenServiceW(manager, L"ndisrd", SERVICE_QUERY_STATUS) : nullptr;
    SERVICE_STATUS status{};
    const bool ready = service && QueryServiceStatus(service, &status) && status.dwCurrentState == SERVICE_RUNNING;
    if (service) CloseServiceHandle(service);
    if (manager) CloseServiceHandle(manager);
    if (!ready) {
        error = QObject::tr("The local Windows Packet Filter driver is not running. Install the ProxiFyre prerequisites on this device before using application rules.");
        return false;
    }
    return true;
#else
    error = QObject::tr("Application rules are currently supported on Windows only."); return false;
#endif
}
bool AppRouter::start(const TrafficPolicy& policy, const VpnRoutes& routes, QString& error, std::function<void()> onFailure)
{
    if (!hasApplicationRules(policy)) return true;
    if (!preflight(policy, error)) return false;
    OcSettings recovery;
    const auto priorRules = recovery.value("Client/firewallJournal").toStringList();
    for (const auto& name : priorRules) {
        if (!name.startsWith("GozarnoVPN-session-") && !name.startsWith("ArovanVPN-session-") && !name.startsWith("QivarynVPN-session-")) continue;
        if (!firewallRule({"advfirewall", "firewall", "delete", "rule", "name=" + name})) {
            error = QObject::tr("Cannot recover the previous application routing firewall rule."); return false;
        }
    }
    recovery.remove("Client/firewallJournal");
    recovery.sync();
    QString runtimeRoot;
#ifdef _WIN32
    PWSTR programData = nullptr;
    if (SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &programData) != S_OK) {
        error = QObject::tr("Cannot locate the protected application workspace."); return false;
    }
    runtimeRoot = QString::fromWCharArray(programData) + "/GozarnoVPN/runtime";
    CoTaskMemFree(programData);
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)", SDDL_REVISION_1, &descriptor, nullptr)) {
        error = QObject::tr("Cannot protect the local application routing workspace."); return false;
    }
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    const QString productRoot = QFileInfo(runtimeRoot).absolutePath();
    BOOL present = FALSE, defaulted = FALSE; PACL acl = nullptr;
    if (!GetSecurityDescriptorDacl(descriptor, &present, &acl, &defaulted) || !present || !acl) {
        LocalFree(descriptor); error = QObject::tr("Cannot protect the routing workspace."); return false;
    }
    for (const auto& path : {productRoot, runtimeRoot}) {
        QString native = QDir::toNativeSeparators(path);
        if (!CreateDirectoryW(reinterpret_cast<const wchar_t*>(native.utf16()), &security) && GetLastError() != ERROR_ALREADY_EXISTS) {
            LocalFree(descriptor); error = QObject::tr("Cannot create protected routing workspace."); return false;
        }
        if (SetNamedSecurityInfoW(reinterpret_cast<wchar_t*>(native.data()), SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, acl, nullptr) != ERROR_SUCCESS) {
            LocalFree(descriptor); error = QObject::tr("Cannot secure the routing workspace."); return false;
        }
    }
    LocalFree(descriptor);
#else
    error = QObject::tr("Application routing requires Windows."); return false;
#endif
    impl->directory.reset(new QTemporaryDir(runtimeRoot + "/session-XXXXXX"));
    if (!impl->directory->isValid()) { error = QObject::tr("Cannot create application routing workspace."); return false; }
    // The engine loads configuration beside its executable. Use an isolated copy;
    // never overwrite another installation's app-config.json or run its service.
    const QDir source(QFileInfo(enginePath()).absolutePath());
    for (const auto& entry : source.entryInfoList(QDir::Files)) {
        if (entry.fileName().endsWith(".exe", Qt::CaseInsensitive)
            || entry.fileName().endsWith(".dll", Qt::CaseInsensitive)
            || entry.fileName().endsWith(".config", Qt::CaseInsensitive)) {
            if (!QFile::copy(entry.absoluteFilePath(), impl->directory->filePath(entry.fileName()))) {
                error = QObject::tr("Cannot stage the application routing component."); return false;
            }
        }
    }
    QByteArray secret;
    for (int i = 0; i < 8; ++i) secret.append(QByteArray::number(QRandomGenerator::system()->generate(), 16).rightJustified(8, '0'));
    impl->worker = new QObject;
    impl->worker->moveToThread(&impl->thread);
    QObject::connect(&impl->thread, &QThread::finished, impl->worker, &QObject::deleteLater);
    impl->thread.start();
    QJsonArray proxies;
    bool ready = true;
    QMetaObject::invokeMethod(impl->worker, [&]() {
        for (const auto& rule : policy.rules) {
            if (!rule.enabled || rule.kind != "application") continue;
            const bool vpn = rule.action == "vpn";
            PinnedInterface iface{routes.interfaceIndex(vpn), routes.interfaceIndex(vpn, true),
                routes.sourceAddress(vpn), routes.sourceAddress(vpn, true)};
            if (iface.source4.isNull() && iface.source6.isNull()) {
                error = QObject::tr("The selected interface has no usable addresses for %1.").arg(rule.target); ready = false; return;
            }
            auto* bridge = new SocksBridge(iface, secret, impl->worker);
            if (!bridge->listen(error)) { ready = false; return; }
            QJsonArray families;
            if (!iface.source4.isNull()) families.append("IPv4");
            if (!iface.source6.isNull()) families.append("IPv6");
            proxies.append(QJsonObject{{"appNames", QJsonArray{rule.target}},
                {"socks5ProxyEndpoint", "127.0.0.1:" + QString::number(bridge->port())},
                {"username", "client"}, {"password", QString::fromLatin1(secret)},
                {"supportedProtocols", QJsonArray{"TCP", "UDP"}}, {"supportedAddressFamilies", families}});
        }
    }, Qt::BlockingQueuedConnection);
    if (!ready) { stop(); return false; }
    const QJsonObject config{{"logLevel", "Info"}, {"bypassLan", false}, {"proxies", proxies},
        {"excludes", QJsonArray{QCoreApplication::applicationFilePath(), enginePath(),
            impl->directory->filePath(QFileInfo(enginePath()).fileName())}}};
    QSaveFile file(impl->directory->filePath("app-config.json"));
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(config).toJson()) < 0 || !file.commit()) {
        error = QObject::tr("Cannot write application routing configuration."); stop(); return false;
    }
    QSaveFile logConfig(impl->directory->filePath("NLog.config"));
    const QByteArray logging = "<nlog xmlns=\"http://www.nlog-project.org/schemas/NLog.xsd\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"><targets><target name=\"console\" xsi:type=\"Console\" layout=\"${message}\" /></targets><rules><logger name=\"*\" minlevel=\"Info\" writeTo=\"console\" /></rules></nlog>";
    if (!logConfig.open(QIODevice::WriteOnly) || logConfig.write(logging) != logging.size() || !logConfig.commit()) {
        error = QObject::tr("Cannot configure routing component diagnostics."); stop(); return false;
    }
    impl->firewallName = "GozarnoVPN-session-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    recovery.setValue("Client/firewallJournal", QStringList{impl->firewallName});
    recovery.sync();
    if (recovery.status() != QSettings::NoError || !firewallRule({"advfirewall", "firewall", "add", "rule",
        "name=" + impl->firewallName, "dir=in", "action=allow", "enable=yes", "profile=any",
        "program=" + QDir::toNativeSeparators(impl->directory->filePath(QFileInfo(enginePath()).fileName()))})) {
        error = QObject::tr("Cannot authorize the local application routing engine in Windows Firewall."); stop(); return false;
    }
    QMetaObject::invokeMethod(impl->worker, [&]() {
        impl->engine = new QProcess(impl->worker);
        impl->engine->setWorkingDirectory(impl->directory->path());
        impl->engine->setProcessChannelMode(QProcess::MergedChannels);
        impl->engine->setProgram(impl->directory->filePath(QFileInfo(enginePath()).fileName()));
        impl->engine->start();
        ready = impl->engine->waitForStarted(5000);
        QByteArray startup;
        QElapsedTimer timer; timer.start();
        while (ready && timer.elapsed() < 10000 && !startup.contains("ProxiFyre Service is running")) {
            impl->engine->waitForReadyRead(200);
            startup += impl->engine->readAllStandardOutput();
            if (startup.size() > 128 * 1024) startup = startup.right(64 * 1024);
            ready = impl->engine->state() != QProcess::NotRunning;
        }
        ready = ready && startup.contains("ProxiFyre Service is running");
        if (!ready) error = QObject::tr("Application routing engine failed to start: %1").arg(impl->engine->errorString());
        else {
            QObject::connect(impl->engine, &QProcess::readyReadStandardOutput, impl->worker, [this]() {
                // Drain the bounded QProcess pipe. Do not log generated proxy credentials.
                impl->engine->readAllStandardOutput();
            });
            QObject::connect(impl->engine, &QProcess::finished, impl->worker, [onFailure]() {
                Logger::instance().addMessage(QObject::tr("Application routing engine stopped. Disconnect the VPN and inspect the local component."));
                if (onFailure) onFailure();
            });
        }
    }, Qt::BlockingQueuedConnection);
    if (!ready) { stop(); return false; }
    return true;
}
void AppRouter::stop()
{
    if (impl->thread.isRunning()) {
        QMetaObject::invokeMethod(impl->worker, [this]() {
            if (impl->engine) {
                impl->engine->disconnect();
                impl->engine->terminate();
                if (!impl->engine->waitForFinished(1500)) { impl->engine->kill(); impl->engine->waitForFinished(1500); }
            }
        }, Qt::BlockingQueuedConnection);
        impl->thread.quit(); impl->thread.wait(); impl->worker = nullptr; impl->engine = nullptr;
    }
    impl->directory.reset();
    if (!impl->firewallName.isEmpty()) {
        if (firewallRule({"advfirewall", "firewall", "delete", "rule", "name=" + impl->firewallName})) {
            OcSettings settings; settings.remove("Client/firewallJournal"); settings.sync();
        }
        impl->firewallName.clear();
    }
}
