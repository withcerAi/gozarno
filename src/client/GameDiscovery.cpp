// SPDX-License-Identifier: GPL-2.0-or-later
#include "GameDiscovery.h"
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQueue>
#include <QSet>
#include <QDirIterator>
#ifdef _WIN32
#include <windows.h>
#endif
namespace {
bool local(const QString& path) {
    if(!QFileInfo(path).isAbsolute() || path.startsWith("//") || path.startsWith("\\\\")) return false;
#ifdef _WIN32
    const QString drive=QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath().left(3));
    const UINT type=GetDriveTypeW(reinterpret_cast<const wchar_t*>(drive.utf16()));
    if(type==DRIVE_REMOTE || type==DRIVE_UNKNOWN || type==DRIVE_NO_ROOT_DIR) return false;
#endif
    return true;
}
QByteArray read(const QString& path) {QFile f(path); if(!f.open(QIODevice::ReadOnly) || f.size()>1024*1024) return {}; return f.readAll();}
QString vdf(const QString& body,const QString& key) {
    const auto expression=QRegularExpression("\""+QRegularExpression::escape(key)+"\"\\s*\"([^\"]*)\""); const auto match=expression.match(body);
    return match.hasMatch()?match.captured(1).replace("\\\\","\\"):QString();
}
bool inside(const QString& file,const QString& root) {
    const QString directory=QFileInfo(root).canonicalFilePath(),path=QFileInfo(file).canonicalFilePath();
    return !directory.isEmpty() && !path.isEmpty() && path.startsWith(directory+"/",Qt::CaseInsensitive);
}
bool executable(const QFileInfo& f,const QString& root) {
    if(!f.isFile() || f.isSymLink() || f.suffix().compare("exe",Qt::CaseInsensitive)!=0 || !inside(f.absoluteFilePath(),root)) return false;
    const QString name=f.fileName().toLower();
    for(const auto& skip:{"unins","uninstall","crash","reporter","redist","vcredist","dxsetup","setup","installer","anti-cheat","anticheat","easyanticheat","battleye"}) if(name.contains(QLatin1String(skip))) return false;
    return true;
}
QList<QString> executables(const QString& root,const std::atomic_bool& cancelled) {
    QList<QString> result; if(!local(root) || QDir(root).isRoot() || !QFileInfo(root).isDir() || QFileInfo(root).isSymLink()) return result;
    QQueue<QPair<QString,int>> directories; directories.enqueue({root,0}); int visited=0,files=0;
    while(!directories.isEmpty() && visited++<512 && files<8192 && result.size()<16 && !cancelled.load()) {
        const auto current=directories.dequeue(); const QDir dir(current.first);
        QDirIterator iterator(dir.absolutePath(),QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot|QDir::NoSymLinks);
        while(iterator.hasNext()) {
            iterator.next(); const auto entry=iterator.fileInfo();
            if(cancelled.load() || ++files>8192) break;
            if(entry.isDir()) {const auto name=entry.fileName().toLower();
                if(current.second<4 && !QStringList{"node_modules",".git","redist","_commonredist","windowsapps"}.contains(name) && inside(entry.absoluteFilePath(),root)) directories.enqueue({entry.absoluteFilePath(),current.second+1});
            } else if(executable(entry,root)) result.append(entry.canonicalFilePath());
        }
    }
    return result;
}
}
namespace GameDiscovery {
QList<DiscoveredGame> folder(const QString& root,const std::atomic_bool& cancelled) {
    QList<DiscoveredGame> games; for(const auto& path:executables(root,cancelled)) games.append({QFileInfo(path).completeBaseName(),path,"Folder"}); return games;
}
QList<DiscoveredGame> steam(const QString& root,const std::atomic_bool& cancelled) {
    QList<DiscoveredGame> games; if(!local(root)) return games; QStringList libraries{QDir::cleanPath(root)};
    const QString body=QString::fromUtf8(read(root+"/steamapps/libraryfolders.vdf"));
    auto matches=QRegularExpression("\"path\"\\s*\"([^\"]*)\"").globalMatch(body);
    while(matches.hasNext() && libraries.size()<32) {const auto path=QDir::fromNativeSeparators(matches.next().captured(1).replace("\\\\","\\")); if(local(path) && !libraries.contains(path,Qt::CaseInsensitive)) libraries.append(path);}
    for(const auto& library:libraries) {
        const QDir dir(library+"/steamapps"); int count=0;
        for(const auto& manifest:dir.entryInfoList({"appmanifest_*.acf"},QDir::Files|QDir::NoSymLinks,QDir::Name)) {
            if(cancelled.load() || ++count>256 || games.size()>=512) break;
            const auto content=QString::fromUtf8(read(manifest.absoluteFilePath())); const auto name=vdf(content,"name"),install=vdf(content,"installdir");
            if(name.isEmpty() || name.size()>120 || install.isEmpty() || install.contains('/') || install.contains('\\') || install=="." || install=="..") continue;
            const QString gameRoot=library+"/steamapps/common/"+install;
            for(const auto& path:executables(gameRoot,cancelled)) games.append({name.left(120),path,"Steam"});
        }
        if(cancelled.load() || games.size()>=512) break;
    }
    return games;
}
QList<DiscoveredGame> epic(const QString& manifests,const std::atomic_bool& cancelled) {
    QList<DiscoveredGame> games; if(!local(manifests)) return games; int count=0;
    for(const auto& entry:QDir(manifests).entryInfoList({"*.item"},QDir::Files|QDir::NoSymLinks,QDir::Name)) {
        if(cancelled.load() || ++count>256) break;
        const auto o=QJsonDocument::fromJson(read(entry.absoluteFilePath())).object();
        const auto name=o.value("DisplayName").toString().left(120),root=QDir::fromNativeSeparators(o.value("InstallLocation").toString());
        const auto launch=o.value("LaunchExecutable").toString(); if(name.isEmpty() || !local(root)) continue;
        const QFileInfo file(QDir(root).filePath(QDir::fromNativeSeparators(launch)));
        if(executable(file,root)) games.append({name,file.canonicalFilePath(),"Epic"});
        else for(const auto& path:executables(root,cancelled)) games.append({name,path,"Epic"});
    }
    return games;
}
QList<DiscoveredGame> scan(const QStringList& extraRoots,const std::atomic_bool& cancelled) {
    QList<DiscoveredGame> results; QStringList steamRoots;
#ifdef _WIN32
    for(const auto& key:{"HKEY_CURRENT_USER\\Software\\Valve\\Steam","HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam","HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\Valve\\Steam"}) {
        QSettings s(QString::fromLatin1(key),QSettings::NativeFormat); const auto path=QDir::fromNativeSeparators(s.value("SteamPath",s.value("InstallPath")).toString()); if(!path.isEmpty()) steamRoots.append(path);
    }
    const QString programs=qEnvironmentVariable("ProgramFiles(x86)"); if(!programs.isEmpty()) steamRoots.append(programs+"/Steam");
    steamRoots.removeDuplicates();
    for(const auto& root:steamRoots) {if(cancelled.load()) break; results.append(steam(root,cancelled));}
    const QString data=qEnvironmentVariable("ProgramData"); if(!data.isEmpty() && !cancelled.load()) results.append(epic(data+"/Epic/EpicGamesLauncher/Data/Manifests",cancelled));
    for(const auto& key:{"HKEY_LOCAL_MACHINE\\SOFTWARE\\GOG.com\\Games","HKEY_LOCAL_MACHINE\\SOFTWARE\\WOW6432Node\\GOG.com\\Games"}) {
        QSettings s(QString::fromLatin1(key),QSettings::NativeFormat); int count=0;
        for(const auto& group:s.childGroups()) {if(cancelled.load() || ++count>256) break; s.beginGroup(group);
            const auto name=s.value("gameName").toString().left(120),root=QDir::fromNativeSeparators(s.value("path").toString()); s.endGroup();
            if(name.isEmpty()) continue;
            for(const auto& path:executables(root,cancelled)) results.append({name,path,"GOG"});
        }
    }
#endif
    for(const auto& root:extraRoots.mid(0,16)) {if(cancelled.load()) break; results.append(folder(root,cancelled));}
    QList<DiscoveredGame> unique; QSet<QString> seen;
    for(const auto& game:results) {const auto key=game.executable.toLower(); if(!seen.contains(key) && unique.size()<512) {seen.insert(key); unique.append(game);}}
    return unique;
}
}
