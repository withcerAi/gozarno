// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include "client/TrafficPolicy.h"
class QComboBox;
class QTableWidget;
class TrafficPolicyDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrafficPolicyDialog(QString profile, QWidget* parent = nullptr);
private:
    QString profile;
    QComboBox* mode;
    QTableWidget* table;
    void addRow(const TrafficRule& rule);
    void save();
};
