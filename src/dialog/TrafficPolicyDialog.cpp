// SPDX-License-Identifier: GPL-2.0-or-later
#include "TrafficPolicyDialog.h"
#include "StableItemDelegate.h"
#include "client/ClientLanguage.h"
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QStyle>
#include <algorithm>

TrafficPolicyDialog::TrafficPolicyDialog(QString profile, QWidget* parent)
    : QDialog(parent), profile(std::move(profile))
{
    setWindowTitle(tr("Traffic rules — %1").arg(this->profile)); resize(920, 650); setMinimumSize(780,560);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28,28,28,28); layout->setSpacing(18);
    auto* title=new QLabel(tr("Traffic control"),this); title->setProperty("role","pageTitle"); layout->addWidget(title);
    auto* subtitle=new QLabel(tr("%1 · Changes take effect on the next connection").arg(this->profile),this); subtitle->setProperty("role","muted"); layout->addWidget(subtitle);
    auto* modeLabel=new QLabel(tr("DEFAULT ROUTE"),this); modeLabel->setProperty("role","eyebrow"); layout->addWidget(modeLabel);
    mode = new QComboBox(this);
    mode->addItem(tr("Use server routing (existing behavior)"), "server");
    mode->addItem(tr("All internet traffic through VPN"), "full");
    mode->addItem(tr("Only selected destinations and programs through VPN"), "selected");
    layout->addWidget(mode);
    auto* help = new QLabel(tr("Rules apply on the next connection. More specific IP routes take priority. Application rules override destination routing for matched programs. Domain rules match exact hostnames and refresh every 60 seconds; shared IP addresses affect other sites using the same address."), this);
    help->setWordWrap(true); help->setProperty("role","muted"); help->hide();
    table = new QTableWidget(0, 4, this);
    table->setItemDelegate(new StableItemDelegate(table));
    table->setHorizontalHeaderLabels({tr("Enabled"), tr("Type"), tr("Destination / executable"), tr("Route")});
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows); layout->addWidget(table);
    table->setShowGrid(false); table->verticalHeader()->hide(); table->verticalHeader()->setDefaultSectionSize(58);
    table->setColumnWidth(0,85); table->setColumnWidth(1,160); table->setColumnWidth(3,130);
    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(tr("＋  IP / network"), this);
    auto* domain = new QPushButton(tr("＋  Domain"), this);
    auto* choose = new QPushButton(tr("Choose program…"), this);
    auto* remove = new QPushButton(tr("Remove selected"), this);
    buttons->addWidget(add); buttons->addWidget(domain); buttons->addWidget(choose); buttons->addStretch(); buttons->addWidget(remove); layout->addLayout(buttons);
    connect(add, &QPushButton::clicked, this, [this]() { addRow({"network", "", "vpn", true}); });
    connect(domain, &QPushButton::clicked, this, [this]() { addRow({"domain", "", "vpn", true}); });
    connect(choose, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("Select application"), {}, tr("Windows applications (*.exe)"));
        if (!path.isEmpty()) addRow({"application", path, "vpn", true});
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        auto rows = table->selectionModel()->selectedRows();
        std::sort(rows.begin(), rows.end(), [](const QModelIndex& a, const QModelIndex& b) { return a.row() > b.row(); });
        for (const auto& row : rows) table->removeRow(row.row());
    });
    QString error;
    const auto policy = TrafficPolicy::load(this->profile, &error);
    mode->setCurrentIndex(mode->findData(policy.mode));
    for (const auto& rule : policy.rules) addRow(rule);
    if (!error.isEmpty()) QMessageBox::warning(this, tr("Routing policy"), error);
    auto* explain=new QPushButton(tr("How traffic rules work  ⌄"),this); explain->setProperty("quiet",true);
    connect(explain,&QPushButton::clicked,help,[help]() { help->setVisible(!help->isVisible()); }); layout->addWidget(explain,0,Qt::AlignLeft); layout->addWidget(help);
    auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(box, &QDialogButtonBox::accepted, this, &TrafficPolicyDialog::save);
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject); layout->addWidget(box);
    box->button(QDialogButtonBox::Save)->setText(tr("Save rules")); box->button(QDialogButtonBox::Save)->setProperty("primary",true);
    box->button(QDialogButtonBox::Save)->style()->unpolish(box->button(QDialogButtonBox::Save));
    box->button(QDialogButtonBox::Save)->style()->polish(box->button(QDialogButtonBox::Save));
}
void TrafficPolicyDialog::addRow(const TrafficRule& rule)
{
    const int row = table->rowCount(); table->insertRow(row);
    auto* enabled = new QTableWidgetItem;
    enabled->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
    enabled->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked); table->setItem(row, 0, enabled);
    auto* kind = new QComboBox(table);
    kind->addItem(tr("IP / network"), "network"); kind->addItem(tr("Domain"), "domain"); kind->addItem(tr("Program"), "application");
    kind->setCurrentIndex(kind->findData(rule.kind)); table->setCellWidget(row, 1, kind);
    auto* target=new QLineEdit(rule.target,table); table->setCellWidget(row,2,target);
    ClientLanguage::technical(target);
    const auto placeholder=[target,kind]() { target->setPlaceholderText(kind->currentData()=="network" ? QObject::tr("203.0.113.10 or 10.20.0.0/16") : kind->currentData()=="domain" ? "example.com" : "C:/Games/Game.exe"); };
    placeholder(); connect(kind,QOverload<int>::of(&QComboBox::currentIndexChanged),target,[placeholder](int) { placeholder(); });
    auto* action = new QComboBox(table); action->addItem(tr("VPN"), "vpn"); action->addItem(tr("Direct"), "direct");
    action->setCurrentIndex(action->findData(rule.action)); table->setCellWidget(row, 3, action);
}
void TrafficPolicyDialog::save()
{
    TrafficPolicy policy; policy.mode = mode->currentData().toString();
    for (int row = 0; row < table->rowCount(); ++row) {
        policy.rules.append({qobject_cast<QComboBox*>(table->cellWidget(row, 1))->currentData().toString(),
            qobject_cast<QLineEdit*>(table->cellWidget(row, 2))->text(), qobject_cast<QComboBox*>(table->cellWidget(row, 3))->currentData().toString(),
            table->item(row, 0)->checkState() == Qt::Checked});
    }
    QString error;
    if (!policy.save(profile, error)) { QMessageBox::warning(this, tr("Invalid traffic rules"), error); return; }
    accept();
}
