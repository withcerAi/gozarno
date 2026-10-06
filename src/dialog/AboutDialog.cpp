// SPDX-License-Identifier: GPL-2.0-or-later
#include "AboutDialog.h"
#include <QCoreApplication>
#include "client/ClientLanguage.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QUrl>
#include <QPushButton>

AboutDialog::AboutDialog(QWidget* parent):QDialog(parent) {
    setObjectName("aboutDialog"); setWindowTitle(tr("About Gozarno")); setMinimumWidth(540); resize(580,440);
    auto* root=new QVBoxLayout(this); root->setContentsMargins(28,28,28,24); root->setSpacing(20);
    auto* heading=new QHBoxLayout;
    auto* icon=new QLabel(this); icon->setPixmap(QPixmap(":/Resources/mono_lock.png").scaled(72,72,Qt::KeepAspectRatio,Qt::SmoothTransformation)); heading->addWidget(icon);
    auto* titles=new QVBoxLayout;
    auto* title=new QLabel(tr("Gozarno VPN"),this); title->setProperty("role","pageTitle"); titles->addWidget(title);
    auto* tagline=new QLabel(tr("A new passage. Your connection, your control."),this); tagline->setProperty("role","muted"); tagline->setWordWrap(true); titles->addWidget(tagline); heading->addLayout(titles,1); root->addLayout(heading);
    auto* version=new QLabel(tr("Version %1 · 64-bit Windows").arg(QCoreApplication::applicationVersion()),this); version->setObjectName("aboutVersion"); root->addWidget(version);
    for(const auto& paragraph:{tr("An independent VPN client built on OpenConnect GUI and OpenConnect. Connect to your existing provider servers and control traffic locally."),
        tr("Original authors and open-source licenses are preserved. Gozarno is distributed under GPL-2.0-or-later; bundled components retain their own licenses.")}) {
        auto* text=new QLabel(paragraph,this); text->setWordWrap(true); text->setTextFormat(Qt::PlainText); root->addWidget(text);
    }
    auto* credits=new QLabel(QString::fromUtf8("OpenConnect GUI · OpenConnect · Qt · GnuTLS\nRed Hat · Nikos Mavrogiannopoulos · Ľubomír Carik\nDavid Woodhouse · Dimitri Papadopoulos Orfanos · Marios Paouris"),this);
    credits->setObjectName("aboutCredits"); credits->setProperty("role","muted"); credits->setWordWrap(true); ClientLanguage::technical(credits); root->addWidget(credits); root->addStretch();
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Close,this); connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);
    auto* upstream=buttons->addButton(tr("Upstream project"),QDialogButtonBox::ActionRole);
    connect(upstream,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://github.com/openconnect/openconnect-gui"));}); root->addWidget(buttons);
}
