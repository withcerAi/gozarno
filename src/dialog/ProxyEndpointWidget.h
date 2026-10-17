// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
class QLabel;
class QVBoxLayout;
class ProxyEndpointWidget final:public QWidget {
public:
    explicit ProxyEndpointWidget(QWidget* parent=nullptr);
    void setEndpoints(const QString&);
    void clearEndpoints();
    void showNotice(const QString&);
private:
    QVBoxLayout* rows;
    QVBoxLayout* addressRows;
    QWidget* controls;
    QLabel* placeholder;
    QLabel* status;
};
