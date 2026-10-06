// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QWidget>
class QTableWidget;
class QLabel;
class QLineEdit;
class ServerLibrary : public QWidget {
    Q_OBJECT
public:
    explicit ServerLibrary(QWidget* parent = nullptr);
    void refresh();
signals:
    void connectProfile(QString name);
    void editProfile(QString name);
    void newProfile();
    void profilesChanged();
private:
    QLabel* emptyState = nullptr;
    QTableWidget* table;
    QLineEdit* search;
    QString selected() const;
    void filter();
};
