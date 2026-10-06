/*
 * Copyright (C) 2014 Red Hat
 *
 * This file is part of openconnect-gui.
 *
 * openconnect-gui is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "logdialog.h"
#include "ui_logdialog.h"

#include <QClipboard>
#include <QDateTime>
#include <QMessageBox>
#include <OcSettings.h>
#include <QTimer>

LogDialog::LogDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::LogDialog)
    , m_timer(std::make_unique<QTimer>())
{
    ui->setupUi(this);

    loadSettings();
    ui->listWidget->setSelectionMode(QAbstractItemView::ContiguousSelection);

    const auto history=Logger::instance().getMessages();
    for (const auto& msg : history.mid(qMax(qsizetype(0),history.size()-2000))) {
        append(msg);
        lastKnownId=msg.id;
    }

    if (ui->checkBox_autoScroll->checkState() == Qt::Checked) {
        ui->listWidget->scrollToBottom();
    }

    m_timer->setInterval(250);
    connect(m_timer.get(),&QTimer::timeout,this,[this] {
        if(!isVisible()) return;
        const auto messages=Logger::instance().getMessages(lastKnownId);
        if(messages.isEmpty()) return;
        ui->listWidget->setUpdatesEnabled(false);
        for(const auto& message:messages.mid(qMax(qsizetype(0),messages.size()-2000))) { append(message); lastKnownId=message.id; }
        if(ui->checkBox_autoScroll->isChecked()) ui->listWidget->scrollToBottom();
        ui->listWidget->setUpdatesEnabled(true);
    });
    m_timer->start();
}

LogDialog::~LogDialog()
{
    disconnect(&Logger::instance(), &Logger::newLogMessage,
        this, &LogDialog::append);

    delete ui;
}

void LogDialog::on_pushButtonSelectAll_clicked()
{
    ui->listWidget->selectAll();
}

void LogDialog::append(const Logger::Message& message)
{
    QDateTime dt;
    dt.setMSecsSinceEpoch(message.timeStamp);
    ui->listWidget->addItem(QString("%1 | %2 | %3")
                                .arg(dt.toString("yyyy-MM-dd hh:mm:ss"))
                                .arg(QString::number((long long)message.threadId, 16), 4)
                                .arg(message.text));
    while(ui->listWidget->count()>2000) delete ui->listWidget->takeItem(0);
}

void LogDialog::on_pushButtonClear_clicked()
{
    if (ui->listWidget->count()) {
        if (QMessageBox::question(this,
                "",
                tr("Are you sure you want to clear the log?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No)
            == QMessageBox::Yes) {

            ui->listWidget->clear();
            Logger::instance().clear();
        }
    }
}

void LogDialog::on_pushButtonCopy_clicked()
{
    QList<QListWidgetItem*> items = ui->listWidget->selectedItems();
    QString text;
    foreach (QListWidgetItem* item, items) {
        text.append(QString("%1%2").arg(item->text()).arg('\n'));
    }

    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(text);
}

void LogDialog::onItemSelectionChanged()
{
    ui->pushButtonCopy->setEnabled(!ui->listWidget->selectedItems().empty());
}

void LogDialog::loadSettings()
{
    OcSettings settings;
    settings.beginGroup("LogWindow");
    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }

    //remove old settings if they exist
    if (settings.contains("size")) {
        settings.remove("size");
    }
    if (settings.contains("pos")) {
        settings.remove("pos");
    }
    settings.endGroup();
}

void LogDialog::saveSettings()
{

    OcSettings settings;
    settings.beginGroup("LogWindow");
    settings.setValue("geometry", saveGeometry());
    settings.endGroup();
}

void LogDialog::on_checkBox_autoScroll_toggled(bool checked)
{
    if (checked) {
        ui->listWidget->scrollToBottom();
    }
}

void LogDialog::on_LogDialog_rejected()
{
    saveSettings();
}
