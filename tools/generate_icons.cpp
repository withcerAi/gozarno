// SPDX-License-Identifier: GPL-2.0-or-later
// Reproducible, code-drawn Gozarno branding; no third-party icon assets.
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QDataStream>

static QImage icon(int size, const QString& kind, const QColor& accent)
{
    QImage image(size, size, QImage::Format_ARGB32); image.fill(Qt::transparent);
    QPainter painter(&image); painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(size / 64.0, size / 64.0);
    if (kind == "app" || kind == "status") {
        QLinearGradient gradient(0, 0, 64, 64); gradient.setColorAt(0, QColor("#14233c")); gradient.setColorAt(1, QColor("#0a1120"));
        painter.setPen(Qt::NoPen); painter.setBrush(gradient); painter.drawRoundedRect(QRectF(2, 2, 60, 60), 15, 15);
        painter.setBrush(Qt::NoBrush); painter.setPen(QPen(accent,5,Qt::SolidLine,Qt::RoundCap));
        painter.drawArc(QRectF(13,12,38,38),35*16,290*16);
        painter.setPen(QPen(QColor("#eefcf7"),5,Qt::SolidLine,Qt::RoundCap));
        painter.drawLine(49,33,36,33); painter.drawLine(49,33,49,43);
        if (kind == "status") { painter.setPen(QPen(QColor("#0a1120"), 3)); painter.setBrush(accent); painter.drawEllipse(QPointF(50, 50), 9, 9); }
    } else {
        painter.setPen(QPen(accent, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); painter.setBrush(Qt::NoBrush);
        if (kind == "servers") for (int y : {15, 29, 43}) { painter.drawRoundedRect(QRectF(10, y, 44, 10), 3, 3); painter.drawPoint(17, y + 5); }
        else if (kind == "plus") { painter.drawLine(32, 14, 32, 50); painter.drawLine(14, 32, 50, 32); }
        else if (kind == "stop") { painter.drawRoundedRect(QRectF(17, 17, 30, 30), 5, 5); }
        else if (kind == "log") { painter.drawRoundedRect(QRectF(13, 9, 38, 46), 5, 5); for (int y : {22, 32, 42}) painter.drawLine(22, y, 42, y); }
        else if (kind == "edit") { painter.drawLine(18, 46, 46, 18); painter.drawLine(14, 50, 24, 47); painter.drawLine(46, 18, 51, 23); }
        else if (kind == "trash") { painter.drawLine(14, 18, 50, 18); painter.drawLine(24, 10, 40, 10); painter.drawRoundedRect(QRectF(19, 21, 26, 32), 3, 3); painter.drawLine(28, 29, 28, 44); painter.drawLine(36, 29, 36, 44); }
        else if (kind == "game") { painter.drawRoundedRect(QRectF(9, 21, 46, 29), 10, 10); painter.drawLine(19, 35, 31, 35); painter.drawLine(25, 29, 25, 41); painter.drawPoint(42, 31); painter.drawPoint(46, 38); }
    }
    return image;
}
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc != 2) return 1;
    const QDir root(QString::fromLocal8Bit(argv[1]));
    const QString path = root.filePath("src/images/gozarno"); QDir().mkpath(path);
    for(bool checked:{false,true}) for(bool disabled:{false,true}) {
        QImage image(20,20,QImage::Format_ARGB32); image.fill(Qt::transparent);
        QPainter p(&image); p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(disabled ? QColor("#64716c") : QColor("#6c9d8d"),1.5));
        p.setBrush(checked ? QColor(disabled ? "#7b9188" : "#64deb4") : QColor("#152721"));
        p.drawRoundedRect(QRectF(1,1,18,18),4,4);
        if(checked) { p.setPen(QPen(QColor("#08271c"),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin)); p.drawPolyline(QPolygonF{{5,10},{9,14},{15,6}}); }
        p.end(); if(!image.save(path+"/check-"+(disabled ? "disabled-" : "")+(checked ? "on" : "off")+".png")) return 2;
    }
    const QMap<QString, QColor> states{{"connected", QColor("#34d399")}, {"disconnected", QColor("#94a3b8")},
        {"connecting", QColor("#fbbf24")}, {"idle", QColor("#64748b")}};
    for (auto it = states.cbegin(); it != states.cend(); ++it)
        if (!icon(96, "status", it.value()).save(path + "/" + it.key() + ".png")) return 2;
    if (!icon(256, "app", QColor("#34d399")).save(path + "/app.png")) return 2;
    for (const auto& kind : {"servers", "plus", "stop", "log", "edit", "trash", "game"})
        if (!icon(64, kind, QColor("#3b82f6")).save(path + "/" + kind + ".png")) return 2;
    QList<QByteArray> frames;
    const QList<int> sizes{16, 24, 32, 48, 64, 128, 256};
    for (int size : sizes) { QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); icon(size, "app", QColor("#34d399")).save(&buffer, "PNG"); frames.append(bytes); }
    QFile file(root.filePath("src/openconnect-gui.ico")); if (!file.open(QIODevice::WriteOnly)) return 3;
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
    stream << quint16(0) << quint16(1) << quint16(frames.size());
    quint32 offset = 6 + 16 * frames.size();
    for (int i = 0; i < frames.size(); ++i) { stream << quint8(sizes[i] == 256 ? 0 : sizes[i]) << quint8(sizes[i] == 256 ? 0 : sizes[i])
        << quint8(0) << quint8(0) << quint16(1) << quint16(32) << quint32(frames[i].size()) << offset; offset += frames[i].size(); }
    for (const auto& frame : frames) file.write(frame);
    return 0;
}
