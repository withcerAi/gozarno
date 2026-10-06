// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

struct PinnedInterface {
    quint32 ipv4 = 0, ipv6 = 0;
    QHostAddress source4, source6;
};

// Local, authenticated SOCKS5 endpoint used by the per-application redirector.
// Its outbound TCP and UDP sockets are pinned to the selected Windows adapter.
class SocksBridge : public QObject {
    Q_OBJECT
public:
    explicit SocksBridge(PinnedInterface route, QByteArray secret, QObject* parent = nullptr);
    bool listen(QString& error);
    quint16 port() const { return server.serverPort(); }
private:
    QTcpServer server;
    PinnedInterface route;
    QByteArray secret;
};

// Shared parser exposed for fragmented-handshake tests. Returns 0 when incomplete,
// -1 for invalid packets, otherwise the number of bytes consumed.
int readSocksTarget(const QByteArray& bytes, int offset, QString& host, quint16& port, bool allowZeroPort = false);
