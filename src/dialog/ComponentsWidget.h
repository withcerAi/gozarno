// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
class QTableWidget;
class ComponentsWidget : public QWidget {
public:
    explicit ComponentsWidget(QWidget* parent=nullptr);
    void refresh();
protected:
    void showEvent(QShowEvent* event) override;
private:
    QTableWidget* table;
    bool loaded=false;
};
