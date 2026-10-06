// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QIcon>
#include <QPainter>
#include <QPixmap>

enum class ConnectionButtonAction { Connect, Disconnect, Cancel };
inline QIcon connectionButtonIcon(ConnectionButtonAction action)
{
    QIcon icon;
    for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
        for (const int size : {20, 40, 60}) {
            QPixmap pixmap(size, size); pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap); painter.setRenderHint(QPainter::Antialiasing);
            painter.scale(size / 20.0, size / 20.0);
            const QColor ink(mode == QIcon::Disabled ? "#8aab9c" : "#08271c");
            painter.setPen(QPen(ink, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            if (action == ConnectionButtonAction::Disconnect) {
                painter.setBrush(ink); painter.drawRoundedRect(QRectF(4,4,12,12),2,2);
            } else if (action == ConnectionButtonAction::Cancel) {
                painter.drawLine(QPointF(5,5),QPointF(15,15));
                painter.drawLine(QPointF(15,5),QPointF(5,15));
            } else {
                painter.drawArc(QRectF(3,3,14,14),135*16,270*16);
                painter.drawLine(QPointF(10,2),QPointF(10,10));
            }
            painter.end();
            pixmap.setDevicePixelRatio(size / 20.0);
            icon.addPixmap(pixmap, mode);
        }
    }
    return icon;
}
