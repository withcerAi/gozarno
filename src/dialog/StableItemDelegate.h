// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QStyledItemDelegate>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>

// Draw checkable cells independently of the native selection/focus decoration.
class StableItemDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QStyleOptionViewItem styled(option); initStyleOption(&styled,index);
        styled.state &= ~QStyle::State_HasFocus;
        const bool checkable=index.flags().testFlag(Qt::ItemIsUserCheckable) && index.data(Qt::CheckStateRole).isValid();
        styled.features &= ~QStyleOptionViewItem::HasCheckIndicator;
        const auto* style=option.widget ? option.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem,&styled,painter,option.widget);
        if(!checkable) return;
        painter->save(); painter->setRenderHint(QPainter::Antialiasing);
        const QRectF box(option.rect.center().x()-9,option.rect.center().y()-9,18,18);
        const bool checked=index.data(Qt::CheckStateRole).toInt()==Qt::Checked;
        const bool enabled=index.flags().testFlag(Qt::ItemIsEnabled);
        painter->setPen(QPen(enabled ? QColor("#6c9d8d") : QColor("#64716c"),1.5));
        painter->setBrush(checked ? QColor(enabled ? "#64deb4" : "#7b9188") : option.palette.base());
        painter->drawRoundedRect(box,4,4);
        if(checked) {
            painter->setPen(QPen(QColor("#08271c"),2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            painter->drawPolyline(QPolygonF{box.topLeft()+QPointF(4,9),box.topLeft()+QPointF(8,13),box.topLeft()+QPointF(14,5)});
        }
        painter->restore();
    }
    bool editorEvent(QEvent* event,QAbstractItemModel* model,const QStyleOptionViewItem&,const QModelIndex& index) override {
        if(!(index.flags() & Qt::ItemIsUserCheckable) || !(index.flags() & Qt::ItemIsEnabled) || !index.data(Qt::CheckStateRole).isValid()) return false;
        bool toggle=false;
        if(event->type()==QEvent::MouseButtonRelease) toggle=static_cast<QMouseEvent*>(event)->button()==Qt::LeftButton;
        if(event->type()==QEvent::KeyPress) { const auto key=static_cast<QKeyEvent*>(event)->key(); toggle=key==Qt::Key_Space || key==Qt::Key_Select; }
        if(toggle) return model->setData(index,index.data(Qt::CheckStateRole).toInt()==Qt::Checked ? Qt::Unchecked : Qt::Checked,Qt::CheckStateRole);
        // Consume double clicks: a single pointer release performs the toggle.
        return event->type()==QEvent::MouseButtonDblClick;
    }
};
