// SPDX-License-Identifier: GPL-2.0-or-later
#include "ProxyEndpointWidget.h"
#include "client/ClientLanguage.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QHostAddress>
ProxyEndpointWidget::ProxyEndpointWidget(QWidget* parent):QWidget(parent) {
    setObjectName("proxyEndpoint");rows=new QVBoxLayout(this);rows->setContentsMargins(0,0,0,0);rows->setSpacing(5);
    placeholder=new QLabel(tr("SOCKS is not connected."),this);placeholder->setProperty("role","muted");
    rows->addWidget(placeholder);
    controls=new QWidget(this);addressRows=new QVBoxLayout(controls);addressRows->setContentsMargins(0,0,0,0);addressRows->setSpacing(5);
    status=new QLabel(this);status->setProperty("role","muted");status->setTextFormat(Qt::PlainText);status->setWordWrap(true);status->hide();
    controls->hide();
    rows->addWidget(controls);rows->addWidget(status);setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Maximum);
}
void ProxyEndpointWidget::clearEndpoints() {
    for(auto* button:findChildren<QPushButton*>("copyProxyEndpoint")){addressRows->removeWidget(button);delete button;}
    placeholder->show();controls->hide();status->clear();status->hide();
}
void ProxyEndpointWidget::setEndpoints(const QString& endpoints) {
    clearEndpoints();
    for(const auto& raw:endpoints.split('|')) {
        const auto address=raw.trimmed();const int colon=address.lastIndexOf(':');bool valid=false;
        const auto port=address.mid(colon+1).toUShort(&valid);
        if(colon<1 || !valid || !port || QHostAddress(address.left(colon)).isNull())continue;
        auto* button=new QPushButton(address+QString::fromUtf8("   ⧉"),this);button->setObjectName("copyProxyEndpoint");
        button->setProperty("proxyAddress",address);button->setProperty("technical",true);button->setProperty("proxyEndpointButton",true);
        button->setCursor(Qt::PointingHandCursor);button->setMinimumHeight(38);button->setFont(QFont("Consolas",12));
        ClientLanguage::technical(button);button->setToolTip(tr("Click to copy the SOCKS address"));button->setAccessibleName(tr("Copy SOCKS address"));
        connect(button,&QPushButton::clicked,this,[button,address] {
            QApplication::clipboard()->setText(address);QToolTip::showText(button->mapToGlobal(button->rect().center()),tr("Copied"),button);
        });addressRows->addWidget(button);
    }
    const bool available=!findChildren<QPushButton*>("copyProxyEndpoint").isEmpty();
    placeholder->setVisible(!available);controls->setVisible(available);
}

void ProxyEndpointWidget::showNotice(const QString& message){status->setText(message);status->show();}
