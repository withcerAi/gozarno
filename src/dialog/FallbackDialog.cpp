// SPDX-License-Identifier: GPL-2.0-or-later
#include "FallbackDialog.h"
#include "OcSettings.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMessageBox>
FallbackDialog::FallbackDialog(const QString& primary,QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Fallback servers")); resize(520,440);
    auto* layout=new QVBoxLayout(this);
    auto* note=new QLabel(tr("Check the profiles you allow as backups, then order them with Up and Down. Each uses its own saved account, protocol and traffic rules. Authentication errors stop automatic switching."),this);
    note->setWordWrap(true); layout->addWidget(note);
    auto* list=new QListWidget(this); list->setObjectName("fallbackProfiles"); layout->addWidget(list);
    OcSettings settings;
    const auto approved=settings.value("server:"+primary+"/recovery/fallbacks").toStringList();
    QStringList names;
    for(const auto& group:settings.childGroups())
        if(group.startsWith("server:") && group.mid(7)!=primary && settings.contains(group+"/server")) names.append(group.mid(7));
    names.sort(); QStringList ordered;
    for(const auto& name:approved) if(names.contains(name) && !ordered.contains(name)) ordered.append(name);
    for(const auto& name:names) if(!ordered.contains(name)) ordered.append(name);
    for(const auto& name:ordered) {
        auto* item=new QListWidgetItem(name,list);
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsUserCheckable);
        item->setCheckState(approved.contains(name) ? Qt::Checked : Qt::Unchecked);
    }
    auto* moves=new QHBoxLayout;
    for(const int direction:{-1,1}) {
        auto* button=new QPushButton(direction<0 ? tr("Up") : tr("Down"),this); moves->addWidget(button);
        connect(button,&QPushButton::clicked,this,[list,direction] {
            const int row=list->currentRow(), next=row+direction;
            if(row<0 || next<0 || next>=list->count()) return;
            list->insertItem(next,list->takeItem(row)); list->setCurrentRow(next);
        });
    }
    layout->addLayout(moves);
    if(names.isEmpty()) { auto* empty=new QLabel(tr("Add another profile to enable fallback servers."),this); empty->setWordWrap(true); layout->addWidget(empty); }
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel,this); layout->addWidget(buttons);
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    connect(buttons,&QDialogButtonBox::accepted,this,[this,list,primary] {
        QStringList selected;
        for(int i=0;i<list->count();++i) if(list->item(i)->checkState()==Qt::Checked) selected.append(list->item(i)->text());
        OcSettings stored; stored.setValue("server:"+primary+"/recovery/fallbacks",selected); stored.sync();
        if(stored.status()!=QSettings::NoError) { QMessageBox::warning(this,tr("Save failed"),tr("Could not save fallback servers.")); return; }
        accept();
    });
}
