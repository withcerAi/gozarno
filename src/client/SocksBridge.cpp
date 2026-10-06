// SPDX-License-Identifier: GPL-2.0-or-later
#include "SocksBridge.h"
#include <QHostInfo>
#include <QTimer>
#include <QHash>
#include <QDateTime>
#include <QNetworkDatagram>
#include <functional>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

int readSocksTarget(const QByteArray& bytes, int offset, QString& host, quint16& port, bool allowZeroPort)
{
    if (bytes.size() <= offset) return 0;
    const int type = quint8(bytes.at(offset));
    int length = 0, start = offset + 1;
    if (type == 1) length = 4;
    else if (type == 4) length = 16;
    else if (type == 3) {
        if (bytes.size() <= start) return 0;
        length = quint8(bytes.at(start++));
        if (!length) return -1;
    } else return -1;
    if (bytes.size() < start + length + 2) return 0;
    if (type == 1) {
        quint32 value = 0;
        for (int i = 0; i < 4; ++i) value = (value << 8) | quint8(bytes.at(start + i));
        host = QHostAddress(value).toString();
    } else if (type == 4) {
        host = QHostAddress(reinterpret_cast<const quint8*>(bytes.constData() + start)).toString();
    } else {
        host = QString::fromLatin1(bytes.mid(start, length));
        if (host.contains(QChar('\0')) || host.contains('/') || host.contains(':')) return -1;
    }
    port = (quint16(quint8(bytes.at(start + length))) << 8) | quint8(bytes.at(start + length + 1));
    return port || allowZeroPort ? start + length + 2 - offset : -1;
}

static QByteArray targetHeader(const QHostAddress& address, quint16 port)
{
    QByteArray bytes;
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        bytes.append(char(1));
        const quint32 value = address.toIPv4Address();
        for (int shift = 24; shift >= 0; shift -= 8) bytes.append(char(value >> shift));
    } else {
        bytes.append(char(4));
        const auto value = address.toIPv6Address();
        bytes.append(reinterpret_cast<const char*>(value.c), 16);
    }
    bytes.append(char(port >> 8)); bytes.append(char(port));
    return bytes;
}

static bool pin(QAbstractSocket* socket, const PinnedInterface& route, const QHostAddress& destination)
{
    const bool ipv6 = destination.protocol() == QAbstractSocket::IPv6Protocol;
    const QHostAddress source = ipv6 ? route.source6 : route.source4;
    if (source.isNull() || destination.isLoopback() || destination.isMulticast() || destination.isLinkLocal()) return false;
    if (!socket->bind(source, 0)) return false;
#ifdef _WIN32
    const quint32 index = ipv6 ? route.ipv6 : route.ipv4;
    if (!index) return false;
    const DWORD iface = ipv6 ? index : htonl(index);
    return setsockopt(SOCKET(socket->socketDescriptor()), ipv6 ? IPPROTO_IPV6 : IPPROTO_IP,
        ipv6 ? IPV6_UNICAST_IF : IP_UNICAST_IF, reinterpret_cast<const char*>(&iface), sizeof(iface)) == 0;
#else
    return false;
#endif
}

class SocksSession : public QObject {
public:
    SocksSession(QTcpSocket* client, PinnedInterface route, QByteArray secret, QObject* parent)
        : QObject(parent), client(client), route(std::move(route)), secret(std::move(secret))
    {
        client->setParent(this);
        client->setReadBufferSize(256 * 1024);
        timeout.setParent(this); timeout.setSingleShot(true); timeout.start(15000);
        connect(&timeout, &QTimer::timeout, this, [this]() { this->client->abort(); deleteLater(); });
        connect(client, &QTcpSocket::disconnected, this, &QObject::deleteLater);
        connect(client, &QTcpSocket::readyRead, this, [this]() { receive(); });
    }
private:
    QTcpSocket* client;
    QTcpSocket* remote = nullptr;
    QUdpSocket* relay = nullptr;
    PinnedInterface route;
    QByteArray secret, buffer;
    QTimer timeout;
    int stage = 0;
    quint16 udpClientPort = 0;
    QHash<QString, QUdpSocket*> udpFlows;
    QHash<QString, qint64> udpTimes;

    void fail(char code) {
        client->write(QByteArray::fromHex("05000001000000000000").replace(1, 1, QByteArray(1, code)));
        client->disconnectFromHost();
    }
    void success(const QHostAddress& bindAddress, quint16 port) {
        client->write(QByteArray::fromHex("050000") + targetHeader(bindAddress, port));
        timeout.stop();
    }
    void receive() {
        if (stage == 4) { pump(client, remote); return; }
        if (stage == 5) { client->readAll(); return; }
        buffer += client->readAll();
        if (buffer.size() > 64 * 1024) { client->abort(); return; }
        if (stage == 0) {
            if (buffer.size() < 2) return;
            const int count = quint8(buffer[1]);
            if (quint8(buffer[0]) != 5 || !count) { client->abort(); return; }
            if (buffer.size() < 2 + count) return;
            if (!buffer.mid(2, count).contains(char(2))) {
                client->write(QByteArray::fromHex("05ff")); client->disconnectFromHost(); return;
            }
            buffer.remove(0, 2 + count);
            client->write(QByteArray::fromHex("0502")); stage = 1;
        }
        if (stage == 1) {
            if (buffer.size() < 2) return;
            const int userLength = quint8(buffer[1]);
            if (quint8(buffer[0]) != 1 || !userLength) { client->abort(); return; }
            if (buffer.size() < 3 + userLength) return;
            const int passLength = quint8(buffer[2 + userLength]);
            if (buffer.size() < 3 + userLength + passLength) return;
            if (buffer.mid(2, userLength) != "client" || buffer.mid(3 + userLength, passLength) != secret) {
                client->write(QByteArray::fromHex("0101")); client->disconnectFromHost(); return;
            }
            buffer.remove(0, 3 + userLength + passLength);
            client->write(QByteArray::fromHex("0100")); stage = 2;
        }
        if (stage == 2) {
            if (buffer.size() < 4) return;
            if (quint8(buffer[0]) != 5 || buffer[2] != 0) { fail(1); return; }
            const int command = quint8(buffer[1]);
            QString host; quint16 port = 0;
            const int used = readSocksTarget(buffer, 3, host, port, command == 3);
            if (!used) return;
            if (used < 0) { fail(8); return; }
            buffer.remove(0, 3 + used); stage = 3;
            if (command == 1) connectRemote(host, port);
            else if (command == 3) startUdp();
            else fail(7);
        }
    }
    static void pump(QTcpSocket* from, QTcpSocket* to) {
        if (!to) return;
        const qint64 capacity = 256 * 1024 - to->bytesToWrite();
        if (capacity > 0) to->write(from->read(qMin(capacity, from->bytesAvailable())));
    }
    void resolve(const QString& host, std::function<void(QHostAddress)> callback) {
        const QHostAddress literal(host);
        if (!literal.isNull()) { callback(literal); return; }
        QHostInfo::lookupHost(host, this, [this, callback](const QHostInfo& result) {
            for (const auto& address : result.addresses()) {
                if (address.protocol() == QAbstractSocket::IPv4Protocol && !route.source4.isNull()) { callback(address); return; }
                if (address.protocol() == QAbstractSocket::IPv6Protocol && !route.source6.isNull()) { callback(address); return; }
            }
            callback({});
        });
    }
    void connectRemote(const QString& host, quint16 port) {
        resolve(host, [this, port](const QHostAddress& destination) {
            remote = new QTcpSocket(this);
            remote->setReadBufferSize(256 * 1024);
            if (destination.isNull() || !pin(remote, route, destination)) { fail(3); return; }
            connect(remote, &QTcpSocket::connected, this, [this]() {
                success(remote->localAddress(), remote->localPort()); stage = 4;
                remote->write(buffer); buffer.clear();
                pump(client, remote);
            });
            connect(remote, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
                if (stage != 4) fail(5); else client->abort();
            });
            connect(remote, &QTcpSocket::readyRead, this, [this]() { pump(remote, client); });
            connect(remote, &QTcpSocket::bytesWritten, this, [this](qint64) { pump(client, remote); });
            connect(client, &QTcpSocket::bytesWritten, this, [this](qint64) { pump(remote, client); });
            connect(remote, &QTcpSocket::disconnected, this, [this]() { client->disconnectFromHost(); });
            remote->connectToHost(destination, port);
        });
    }
    void startUdp() {
        relay = new QUdpSocket(this);
        if (!relay->bind(QHostAddress::LocalHost, 0)) { fail(1); return; }
        success(QHostAddress::LocalHost, relay->localPort()); stage = 5;
        auto* reaper = new QTimer(this);
        reaper->setInterval(30000);
        connect(reaper, &QTimer::timeout, this, [this]() {
            const auto now = QDateTime::currentMSecsSinceEpoch();
            for (const auto& key : udpTimes.keys()) if (now - udpTimes.value(key) > 120000) {
                auto* socket = udpFlows.take(key); udpTimes.remove(key); if (socket) socket->deleteLater();
            }
        });
        reaper->start();
        connect(relay, &QUdpSocket::readyRead, this, [this]() {
            while (relay->hasPendingDatagrams()) {
                const auto packet = relay->receiveDatagram(65536);
                if (!packet.senderAddress().isLoopback()) continue;
                if (udpClientPort && udpClientPort != packet.senderPort()) continue;
                const auto data = packet.data();
                if (data.size() < 4 || data[0] || data[1] || data[2]) continue;
                QString host; quint16 port;
                const int used = readSocksTarget(data, 3, host, port);
                if (used <= 0) continue;
                udpClientPort = packet.senderPort();
                const QByteArray payload = data.mid(3 + used);
                resolve(host, [this, payload, port](const QHostAddress& destination) {
                    if (destination.isNull()) return;
                    const QString key = destination.toString() + ":" + QString::number(port);
                    auto* socket = udpFlows.value(key);
                    if (!socket) {
                        if (udpFlows.size() >= 256) return;
                        socket = new QUdpSocket(this);
                        if (!pin(socket, route, destination)) { socket->deleteLater(); return; }
                        socket->connectToHost(destination, port);
                        udpFlows.insert(key, socket);
                        connect(socket, &QUdpSocket::readyRead, this, [this, socket, destination, port, key]() {
                            while (socket->hasPendingDatagrams()) {
                                const auto datagram = socket->receiveDatagram(65536);
                                const QByteArray response = QByteArray(3, char(0)) + targetHeader(destination, port) + datagram.data();
                                relay->writeDatagram(response, QHostAddress::LocalHost, udpClientPort);
                                udpTimes[key] = QDateTime::currentMSecsSinceEpoch();
                            }
                        });
                    }
                    udpTimes[key] = QDateTime::currentMSecsSinceEpoch(); socket->write(payload);
                });
            }
        });
    }
};

SocksBridge::SocksBridge(PinnedInterface route, QByteArray secret, QObject* parent)
    : QObject(parent), route(std::move(route)), secret(std::move(secret))
{
    server.setParent(this);
    connect(&server, &QTcpServer::newConnection, this, [this]() {
        while (server.hasPendingConnections()) {
            auto* socket = server.nextPendingConnection();
            if (!socket->peerAddress().isLoopback() || findChildren<QTcpSocket*>().size() > 1024) {
                socket->abort(); socket->deleteLater(); continue;
            }
            new SocksSession(socket, this->route, this->secret, this);
        }
    });
}
bool SocksBridge::listen(QString& error)
{
    if (!server.listen(QHostAddress::LocalHost, 0)) { error = server.errorString(); return false; }
    return true;
}
