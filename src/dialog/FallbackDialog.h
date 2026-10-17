// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
class FallbackDialog : public QDialog {
    Q_OBJECT
public:
    explicit FallbackDialog(const QString& primary,QWidget* parent=nullptr);
};
