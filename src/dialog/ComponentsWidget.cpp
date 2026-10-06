// SPDX-License-Identifier: GPL-2.0-or-later
#include "ComponentsWidget.h"
#include "client/InstalledComponents.h"
#include "client/ClientLanguage.h"
#include "StableItemDelegate.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QCoreApplication>
#include <QShowEvent>

ComponentsWidget::ComponentsWidget(QWidget* parent):QWidget(parent) {
    setObjectName("componentsWidget"); auto* root=new QVBoxLayout(this); root->setContentsMargins(18,20,18,20); root->setSpacing(16);
    auto* header=new QHBoxLayout; auto* title=new QLabel(tr("Components & prerequisites"),this); title->setProperty("role","section"); header->addWidget(title,1);
    auto* refreshButton=new QPushButton(tr("Refresh status"),this); header->addWidget(refreshButton); root->addLayout(header);
    auto* help=new QLabel(tr("Core libraries and the routing engine stay inside Gozarno. Network drivers and shared Windows runtimes must be registered in Windows; Setup manages the supported prerequisites."),this);
    help->setProperty("role","muted"); help->setWordWrap(true); root->addWidget(help);
    table=new QTableWidget(0,4,this); table->setObjectName("componentsTable"); table->setHorizontalHeaderLabels({tr("Component"),tr("Location"),tr("Status"),tr("Version")});
    table->setItemDelegate(new StableItemDelegate(table)); table->setSelectionMode(QAbstractItemView::NoSelection); table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->hide(); table->setShowGrid(false); table->verticalHeader()->setDefaultSectionSize(46);
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(2,QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    table->setMinimumHeight(240); root->addWidget(table,1);
    auto* note=new QLabel(tr("This is a read-only check. Missing components affect only the features that need them. Use the Gozarno installer to install or repair prerequisites; shared system components are preserved on uninstall."),this);
    note->setProperty("role","muted"); note->setWordWrap(true); root->addWidget(note);
    connect(refreshButton,&QPushButton::clicked,this,&ComponentsWidget::refresh);
}
void ComponentsWidget::showEvent(QShowEvent* event) { QWidget::showEvent(event); if(!loaded) refresh(); }
void ComponentsWidget::refresh() {
    const auto components=InstalledComponents::inspect(QCoreApplication::applicationDirPath());
    table->setUpdatesEnabled(false); table->setRowCount(components.size());
    for(int row=0;row<components.size();++row) {
        const auto& component=components[row];
        const QStringList fields{component.name,component.location,component.status,component.version.isEmpty() ? tr("Unavailable") : component.version};
        for(int column=0;column<fields.size();++column) {
            auto* item=new QTableWidgetItem(fields[column]); item->setFlags(Qt::ItemIsEnabled);
            if(column==0) item->setTextAlignment((ClientLanguage::isPersian() ? Qt::AlignRight : Qt::AlignLeft)|Qt::AlignVCenter|Qt::AlignAbsolute);
            if(column==3 && !component.version.isEmpty()) item->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter|Qt::AlignAbsolute);
            if(column==2) item->setForeground(QColor(component.state==InstalledComponent::Ready ? (palette().base().color().lightness()>150 ? "#147353" : "#64deb4") : component.state==InstalledComponent::Missing ? "#d66b6b" : component.state==InstalledComponent::Attention ? "#be9347" : "#83988f"));
            table->setItem(row,column,item);
        }
    }
    loaded=true; table->setUpdatesEnabled(true);
}
