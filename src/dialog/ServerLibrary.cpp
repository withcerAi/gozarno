// SPDX-License-Identifier: GPL-2.0-or-later
#include "ServerLibrary.h"
#include "TrafficPolicyDialog.h"
#include "StableItemDelegate.h"
#include "client/ServerCatalog.h"
#include "OcSettings.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QSaveFile>
#include <QJsonDocument>
#include <QSignalBlocker>
#include <QLabel>
#include <QMenu>
#include <functional>
#include <algorithm>

ServerLibrary::ServerLibrary(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0); layout->setSpacing(16);
    search = new QLineEdit(this); search->setPlaceholderText(tr("Search servers, protocols and notes…")); layout->addWidget(search);
    table = new QTableWidget(0, 5, this);
    table->setItemDelegate(new StableItemDelegate(table));
    table->setHorizontalHeaderLabels({tr("Favorite"), tr("Profile"), tr("Gateway"), tr("Protocol"), tr("Notes")});
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection); layout->addWidget(table);
    table->setShowGrid(false); table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(58);
    table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(3,QHeaderView::ResizeToContents);
    emptyState = new QLabel(this); emptyState->setWordWrap(true); emptyState->setAlignment(Qt::AlignCenter);
    emptyState->setProperty("role","muted"); emptyState->setMinimumHeight(220); layout->addWidget(emptyState,1);
    auto* actions = new QHBoxLayout;
    auto* moreMenu = new QMenu(this);
    const auto button = [this, actions, moreMenu](const QString& label, std::function<void()> handler) {
        if(label==tr("Copy") || label==tr("Remove")) { auto* action=moreMenu->addAction(label); connect(action,&QAction::triggered,this,std::move(handler)); return; }
        auto* item = new QPushButton(label, this); actions->addWidget(item);
        if(label==tr("Connect")) item->setProperty("primary",true);
        if(label!=tr("Add")) {
            item->setEnabled(false);
            connect(table,&QTableWidget::itemSelectionChanged,item,[this,item]() { item->setEnabled(!selected().isEmpty()); });
        }
        connect(item, &QPushButton::clicked, this, std::move(handler));
    };
    button(tr("Connect"), [this]() { if (!selected().isEmpty()) emit connectProfile(selected()); });
    button(tr("Edit"), [this]() { if (!selected().isEmpty()) emit editProfile(selected()); });
    button(tr("Traffic rules"), [this]() {
        if (selected().isEmpty()) return;
        TrafficPolicyDialog dialog(selected(), this); dialog.exec();
    });
    button(tr("Copy"), [this]() {
        const auto source = selected(); if (source.isEmpty()) return;
        bool ok;
        const auto target = QInputDialog::getText(this, tr("Copy profile"), tr("New profile name"), QLineEdit::Normal, source + tr(" copy"), &ok);
        if (!ok) return;
        QString error;
        if (!ServerCatalog::duplicate(source, target.trimmed(), error)) QMessageBox::warning(this, tr("Unable to copy"), error);
        else { refresh(); emit profilesChanged(); }
    });
    button(tr("Remove"), [this]() {
        const auto name = selected(); if (name.isEmpty()) return;
        if (QMessageBox::question(this, tr("Remove profile"), tr("Remove '%1' and its saved credentials?").arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        OcSettings settings; settings.remove("server:" + name); settings.sync();
        if (settings.status() != QSettings::NoError) QMessageBox::warning(this, tr("Unable to remove"), tr("Could not update the server library."));
        refresh(); emit profilesChanged();
    });
    auto* more = new QPushButton(tr("More"),this); more->setMenu(moreMenu); actions->addWidget(more); actions->addStretch();
    layout->addLayout(actions);
    auto* transfer = new QHBoxLayout;
    auto* importButton = new QPushButton(tr("Import catalog…"), this);
    auto* exportButton = new QPushButton(tr("Export catalog…"), this);
    transfer->addWidget(importButton); transfer->addWidget(exportButton); transfer->addStretch(); layout->addLayout(transfer);
    exportButton->setToolTip(tr("Exports server addresses, notes and rules. Passwords, certificates and private keys are excluded."));
    connect(exportButton, &QPushButton::clicked, this, [this]() {
        const auto path = QFileDialog::getSaveFileName(this, tr("Export catalog without credentials"), "servers.json", tr("JSON files (*.json)"));
        if (path.isEmpty()) return;
        QSaveFile file(path);
        const auto bytes = ServerCatalog::exportProfiles().toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            QMessageBox::warning(this, tr("Export failed"), file.errorString());
    });
    connect(importButton, &QPushButton::clicked, this, [this]() {
        const auto path = QFileDialog::getOpenFileName(this, tr("Import catalog"), {}, tr("JSON files (*.json)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
            QMessageBox::warning(this, tr("Import failed"), tr("Choose a readable catalog no larger than 1 MB.")); return;
        }
        QJsonParseError parse; const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
        QStringList names; QString error;
        if (parse.error != QJsonParseError::NoError || !ServerCatalog::importProfiles(document, names, error)) {
            QMessageBox::warning(this, tr("Import failed"), error.isEmpty() ? parse.errorString() : error); return;
        }
        refresh(); emit profilesChanged();
        QMessageBox::information(this, tr("Catalog imported"), tr("Imported %1 profiles. Existing profiles were preserved; enter credentials before connecting.").arg(names.size()));
    });
    connect(search, &QLineEdit::textChanged, this, &ServerLibrary::filter);
    connect(table, &QTableWidget::cellDoubleClicked, this, [this](int, int column) {
        if (column != 0 && column != 4 && !selected().isEmpty()) emit connectProfile(selected());
    });
    connect(table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        const auto name = table->item(item->row(), 1)->text();
        OcSettings settings;
        if (item->column() == 0) settings.setValue("server:" + name + "/catalog/favorite", item->checkState() == Qt::Checked);
        if (item->column() == 4) settings.setValue("server:" + name + "/catalog/notes", item->text().left(4096));
    });
    refresh();
}
QString ServerLibrary::selected() const
{
    const auto row=table->currentRow();
    return row>=0 && !table->isRowHidden(row) && table->item(row,1) ? table->item(row,1)->text() : QString();
}
void ServerLibrary::refresh()
{
    const QString previous = selected();
    QSignalBlocker blocker(table);
    table->setRowCount(0); OcSettings settings;
    auto groups = settings.childGroups();
    std::stable_sort(groups.begin(), groups.end(), [&settings](const QString& a, const QString& b) {
        const bool af = settings.value(a + "/catalog/favorite", false).toBool();
        const bool bf = settings.value(b + "/catalog/favorite", false).toBool();
        return af != bf ? af : a.localeAwareCompare(b) < 0;
    });
    for (const auto& group : groups) {
        if (!group.startsWith("server:") || !settings.contains(group + "/server")) continue;
        const int row = table->rowCount(); table->insertRow(row);
        auto* favorite = new QTableWidgetItem;
        favorite->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        favorite->setCheckState(settings.value(group + "/catalog/favorite", false).toBool() ? Qt::Checked : Qt::Unchecked);
        table->setItem(row, 0, favorite);
        const QStringList values{group.mid(7), settings.value(group + "/server").toString(),
            settings.value(group + "/protocol-name", "anyconnect").toString(), settings.value(group + "/catalog/notes").toString()};
        for (int column = 1; column <= 4; ++column) {
            auto* item = new QTableWidgetItem(values.at(column - 1));
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | (column==4 ? Qt::ItemIsEditable : Qt::NoItemFlags));
            if(column==2 || column==3) item->setTextAlignment(Qt::AlignLeft|Qt::AlignVCenter|Qt::AlignAbsolute);
            table->setItem(row, column, item);
        }
        if (group.mid(7) == previous) table->selectRow(row);
    }
    filter();
    blocker.unblock();
    emit table->itemSelectionChanged();
}
void ServerLibrary::filter()
{
    int visible=0;
    for (int row = 0; row < table->rowCount(); ++row) {
        QString text;
        for (int column = 1; column <= 4; ++column) text += table->item(row, column)->text() + " ";
        table->setRowHidden(row, !text.contains(search->text(), Qt::CaseInsensitive));
        if(!table->isRowHidden(row)) ++visible;
    }
    table->setVisible(visible>0); emptyState->setVisible(visible==0);
    emptyState->setText(table->rowCount()==0 ? tr("Your server library starts here\n\nAdd a server using the address from your provider,\nor import a saved server catalog.") : tr("No servers match your search\n\nTry another server name, protocol or note."));
}
