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

#include "editdialog.h"
#include "VpnProtocolModel.h"
#include "common.h"
#include "server_storage.h"
#include "ui_editdialog.h"
#include "client/ClientLanguage.h"
#include <QFileDialog>
#include <QItemSelectionModel>
#include <QListWidget>
#include <QMessageBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QAction>
#include <OcSettings.h>
#include <QUrl>
#include <QScrollArea>
#include <QPushButton>
#include <QStyle>

#ifdef USE_SYSTEM_KEYS
extern "C" {
#include <gnutls/system-keys.h>
}
#endif

static int token_tab(int mode)
{
    // keep in sync with the indices of the QComboBox items in src/dialog/editdialog.ui
    switch (mode) {
    case OC_TOKEN_MODE_HOTP:
        return 0;
    case OC_TOKEN_MODE_TOTP:
        return 1;
    case OC_TOKEN_MODE_STOKEN:
        return 2;
    default:
        return -1;
    }
}

static int token_rtab[] = {
    // keep in sync with the indices of the QComboBox items in src/dialog/editdialog.ui
    OC_TOKEN_MODE_HOTP,  // [0]
    OC_TOKEN_MODE_TOTP,  // [1]
    OC_TOKEN_MODE_STOKEN // [2]
};

static int loglevel_tab(int mode)
{
    // keep in sync with the indices of the QComboBox items in src/dialog/editdialog.ui
    switch (mode) {
    case -1: //application default
        return 0;
    case PRG_ERR:
        return 1;
    case PRG_INFO:
        return 2;
    case PRG_DEBUG:
        return 3;
    case PRG_TRACE:
        return 4;
    default:
        return -1;
    }
}

static int loglevel_rtab[] = {
    // keep in sync with the indices of the QComboBox items in src/dialog/editdialog.ui
    -1,        // [0]
    PRG_ERR,   // [1]
    PRG_INFO,  // [2]
    PRG_DEBUG, // [3]
    PRG_TRACE  // [4]
};

void EditDialog::load_win_certs()
{
#ifdef USE_SYSTEM_KEYS
    QString prekey = ss->get_key_url();

    this->winCerts.clear();
    ui->loadWinCertList->clear();

    int ret = -1;
    gnutls_system_key_iter_t iter = nullptr;
    char* cert_url;
    char* key_url;
    char* label;
    int row = 0;
    int idx = -1;
    do {
        ret = gnutls_system_key_iter_get_info(&iter, GNUTLS_CRT_X509, &cert_url, &key_url, &label,
            nullptr, 0);
        if (ret >= 0) {
            win_cert_st st;
            QString l;
            if (label != nullptr)
                l = QString::fromUtf8(label);
            else
                l = QString::fromUtf8(cert_url);
            ui->loadWinCertList->addItem(l);
            if (prekey.isEmpty() == false) {
                if (QString::compare(prekey, QString::fromUtf8(key_url), Qt::CaseSensitive) == 0) {
                    ui->userCertEdit->setText(cert_url);
                    ui->userKeyEdit->setText(prekey);

                    idx = row;
                }
            }
            row++;

            st.label = l;
            st.key_url = QString::fromUtf8(key_url);
            st.cert_url = QString::fromUtf8(cert_url);
            this->winCerts.push_back(st);
        }
    } while (ret >= 0);

    if (idx != -1) {
        ui->loadWinCertList->setCurrentRow(idx);
        ui->loadWinCertList->item(idx)->setSelected(true);
    }
    gnutls_system_key_iter_deinit(iter);
#endif
}

EditDialog::EditDialog(QString server, QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::EditDialog)
    , ss(new StoredServer())
{
    ui->setupUi(this);
    for(const auto& name:{"gatewayEdit","usernameEdit","groupnameEdit","caCertEdit","serverCertHash","tokenEdit","caCertHash","interfaceNameEdit","vpncScriptEdit","userCertEdit","userCertHash","userKeyEdit"})
        if(auto* field=findChild<QLineEdit*>(QString::fromLatin1(name))) ClientLanguage::technical(field);
    ClientLanguage::technical(ui->protocolComboBox);
    // Keep advanced profiles usable on smaller displays and at high DPI.
    ui->verticalLayout->removeWidget(ui->buttonBox);
    auto* contents = new QWidget(this);
    contents->setLayout(ui->verticalLayout);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true); scroll->setWidget(contents);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(24,24,24,24); outer->setSpacing(18);
    auto* heading=new QLabel(tr("Server settings"),this); heading->setProperty("role","pageTitle"); outer->addWidget(heading);
    auto* hint=new QLabel(tr("Account, authentication and advanced connection options."),this); hint->setProperty("role","muted"); outer->addWidget(hint);
    scroll->setFrameShape(QFrame::NoFrame);
    ui->settingsProfileLayout->setVerticalSpacing(16);
    ui->verticalLayout->setSpacing(20);
    outer->addWidget(scroll); outer->addWidget(ui->buttonBox);
    resize(760, 740);

#ifdef _WIN32
    ui->interfaceNameEdit->setMaxLength(OC_IFNAME_MAX_LENGTH);
#endif

    VpnProtocolModel* model = new VpnProtocolModel(this);
    ui->protocolComboBox->setModel(model);

    if (ss->load(server) < 0) {
        QMessageBox::information(this,
            qApp->applicationName(),
            ss->m_last_err.isEmpty() ? tr("Some server information failed to load") : ss->m_last_err);
    }

    ss->set_window(this);

    // Explicit saved credentials, independent of unattended/batch behavior.
    passwordEdit = new QLineEdit(this);
    ClientLanguage::technical(passwordEdit);
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setText(ss->get_password());
    passwordEdit->setPlaceholderText(tr("Enter a password, or leave empty to ask on connection"));
    auto* showPassword = passwordEdit->addAction(tr("Show"), QLineEdit::TrailingPosition);
    showPassword->setCheckable(true);
    connect(showPassword, &QAction::toggled, this, [this, showPassword](bool visible) {
        passwordEdit->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
        showPassword->setText(visible ? tr("Hide") : tr("Show"));
    });
    rememberPassword = new QCheckBox(tr("Save password on this device"), this);
    rememberPassword->setChecked(ss->get_remember_password());
#ifdef _WIN32
    rememberPassword->setToolTip(tr("Protected by Windows for the current user account. One-time codes are not saved."));
#else
    rememberPassword->setChecked(false);
    rememberPassword->setEnabled(false);
    rememberPassword->setToolTip(tr("Secure password storage is currently available on Windows."));
#endif
    ui->settingsProfileLayout->insertRow(3, tr("Password"), passwordEdit);
    ui->settingsProfileLayout->insertRow(4, rememberPassword);

    QString txt = ss->get_label();
    ui->nameEdit->setText(txt);
    if (txt.isEmpty() == true) {
        ui->nameEdit->setText(server);
    }
    ui->groupnameEdit->setText(ss->get_groupname());
    ui->usernameEdit->setText(ss->get_username());
    ui->gatewayEdit->setText(ss->get_server_gateway());
    ui->userCertHash->setText(ss->get_client_cert_pin());
    ui->caCertHash->setText(ss->get_ca_cert_pin());
    ui->batchModeBox->setChecked(ss->get_batch_mode());
    ui->batchModeBox->setToolTip(tr("Use unattended authentication where supported. Saving a password is a separate preference."));
    ui->minimizeBox->setChecked(ss->get_minimize());
    ui->useProxyBox->setChecked(ss->get_proxy());
    ui->disableUdpBox->setChecked(ss->get_disable_udp());
    ui->reconnectTimeoutSpinBox->setValue(ss->get_reconnect_timeout());
    ui->dtlsAttemptPeriodSpinBox->setValue(ss->get_dtls_reconnect_timeout());

    // Load the windows certificates
    load_win_certs();

    int type = ss->get_token_type();
    if (type >= 0) {
        ui->tokenBox->setCurrentIndex(token_tab(ss->get_token_type()));
        ui->tokenEdit->setText(ss->get_token_str());
    }

    ui->protocolComboBox->setCurrentIndex(model->findIndex(ss->get_protocol_name()));
    ui->interfaceNameEdit->setText(ss->get_interface_name());
    ui->vpncScriptEdit->setText(ss->get_vpnc_script_filename());

    type = loglevel_tab(ss->get_log_level());
    if (type != -1) {
        ui->loglevelBox->setCurrentIndex(type);
    }

    QString hash;
    ss->get_server_pin(hash);
    ui->serverCertHash->setText(hash);

    // Everyday account fields are separate from certificate and advanced settings.
    auto* editorTabs=new QTabWidget(contents);
    const auto makePage=[editorTabs](const QString& title) {
        auto* page=new QWidget(editorTabs); auto* layout=new QVBoxLayout(page);
        layout->setContentsMargins(12,22,12,12); layout->setSpacing(20);
        editorTabs->addTab(page,title); return page;
    };
    auto* account=makePage(tr("Account")); auto* security=makePage(tr("Authentication")); auto* advanced=makePage(tr("Advanced"));
    auto* accountForm=new QFormLayout; auto* securityForm=new QFormLayout; auto* advancedForm=new QFormLayout;
    for(auto* form : {accountForm,securityForm,advancedForm}) { form->setSpacing(18); form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow); }
    qobject_cast<QVBoxLayout*>(account->layout())->addLayout(accountForm);
    qobject_cast<QVBoxLayout*>(security->layout())->addLayout(securityForm);
    qobject_cast<QVBoxLayout*>(advanced->layout())->addLayout(advancedForm);
    while(ui->settingsProfileLayout->rowCount()>0) {
        auto row=ui->settingsProfileLayout->takeRow(0);
        auto* label=row.labelItem ? row.labelItem->widget() : nullptr;
        auto* field=row.fieldItem ? row.fieldItem->widget() : nullptr;
        auto* destination=(label==ui->nameLabel || label==ui->gatewayLabel || label==ui->usernameLabel || label==ui->groupnameLabel || label==ui->protocolLabel || field==passwordEdit || field==rememberPassword) ? accountForm
            : (label==ui->interfaceNameLabel || label==ui->vpncScriptLabel || label==ui->logLevelLabel) ? advancedForm : securityForm;
        if(row.fieldItem && row.fieldItem->layout()) {
            auto* fields=row.fieldItem->layout(); fields->setParent(nullptr);
            if(label) destination->addRow(label,fields); else destination->addRow(fields);
        } else if(field) {
            if(label) destination->addRow(label,field); else destination->addRow(field);
            delete row.fieldItem;
        }
        delete row.labelItem;
    }
    ui->verticalLayout->removeWidget(ui->settingsTabWidget); ui->verticalLayout->removeWidget(ui->settingsGroupBox);
    security->layout()->addWidget(ui->settingsTabWidget); advanced->layout()->addWidget(ui->settingsGroupBox);
    qobject_cast<QVBoxLayout*>(account->layout())->addStretch();
    qobject_cast<QVBoxLayout*>(advanced->layout())->addStretch();
    delete ui->verticalLayout;
    auto* contentLayout=new QVBoxLayout(contents); contentLayout->setContentsMargins(0,0,0,0); contentLayout->addWidget(editorTabs);
    ui->buttonBox->button(QDialogButtonBox::Save)->setProperty("primary",true);
    ui->buttonBox->button(QDialogButtonBox::Save)->style()->unpolish(ui->buttonBox->button(QDialogButtonBox::Save));
    ui->buttonBox->button(QDialogButtonBox::Save)->style()->polish(ui->buttonBox->button(QDialogButtonBox::Save));
}

EditDialog::~EditDialog()
{
    delete ui;
    delete ss;
}

QString EditDialog::getEditedProfileName() const
{
    return ss->get_label();
}

void EditDialog::on_buttonBox_accepted()
{
    const QString newName = ui->nameEdit->text().trimmed();
    const QString oldName = ss->get_label();
    OcSettings profileSettings;
    if (newName.contains('/') || newName.contains('\\') || newName.size() > 128
        || (newName != oldName && profileSettings.contains("server:" + newName + "/server"))) {
        QMessageBox::warning(this, tr("Invalid profile name"),
            tr("Use a unique name of at most 128 characters without slashes."));
        return;
    }
    QString gatewayText = ui->gatewayEdit->text().trimmed();
    QUrl gateway(gatewayText.contains("://") ? gatewayText : "https://" + gatewayText);
    if (!gateway.isValid() || gateway.host().isEmpty() || gateway.scheme() != "https"
        || !gateway.userInfo().isEmpty() || gateway.hasFragment()) {
        QMessageBox::warning(this, tr("Invalid server"), tr("Enter an HTTPS gateway without embedded credentials."));
        return;
    }
    if (ui->gatewayEdit->text().isEmpty() == true) {
        QMessageBox::information(this,
            qApp->applicationName(),
            tr("You need to specify a gateway. E.g. vpn.example.com:443"));
        return;
    }

    if (ui->nameEdit->text().isEmpty() == true) {
        QMessageBox::information(this,
            qApp->applicationName(),
            tr("You need to specify a name for this connection. E.g. 'My company'"));
        return;
    }

    if (ui->caCertEdit->text().isEmpty() == false) {
        if (ss->set_ca_cert(ui->caCertEdit->text()) != 0) {
            QMessageBox mbox;
            mbox.setText(tr("Cannot import CA certificate."));
            if (ss->m_last_err.isEmpty() == false)
                mbox.setInformativeText(ss->m_last_err);
            mbox.exec();
            return;
        } else {
            ui->caCertHash->setText(ss->get_ca_cert_pin());
        }
    }

    if (ui->userKeyEdit->text().isEmpty() == false) {
        if (ss->set_client_key(ui->userKeyEdit->text()) != 0) {
            QMessageBox mbox;
            mbox.setText(tr("Cannot import user key."));
            if (ss->m_last_err.isEmpty() == false)
                mbox.setInformativeText(ss->m_last_err);
            mbox.exec();
            return;
        }
    }

    if (ui->userCertEdit->text().isEmpty() == false) {
        if (ss->set_client_cert(ui->userCertEdit->text()) != 0) {
            QMessageBox mbox;
            mbox.setText(tr("Cannot import user certificate."));
            if (ss->m_last_err.isEmpty() == false)
                mbox.setInformativeText(ss->m_last_err);
            mbox.exec();
            return;
        } else {
            ui->userCertHash->setText(ss->get_client_cert_pin());
        }
    }

    if (ss->client_is_complete() != true) {
        QMessageBox::information(this,
            qApp->applicationName(),
            tr("There is a client certificate specified but no key!"));
        return;
    }
    ss->set_label(newName);
    ss->set_username(ui->usernameEdit->text());
    ss->set_password(passwordEdit->text());
    ss->set_remember_password(rememberPassword->isChecked());
    ss->set_server_gateway(gateway.toString());
    ss->set_batch_mode(ui->batchModeBox->isChecked());
    ss->set_minimize(ui->minimizeBox->isChecked());
    ss->set_proxy(ui->useProxyBox->isChecked());
    ss->set_disable_udp(ui->disableUdpBox->isChecked());
    ss->set_reconnect_timeout(ui->reconnectTimeoutSpinBox->value());
    ss->set_dtls_reconnect_timeout(ui->dtlsAttemptPeriodSpinBox->value());

    int type = ui->tokenBox->currentIndex();
    if (type != -1 && ui->tokenEdit->text().isEmpty() == false) {
        ss->set_token_str(ui->tokenEdit->text());
        ss->set_token_type(token_rtab[type]);
    } else {
        ss->set_token_str("");
        ss->set_token_type(-1);
    }

    ss->set_protocol_name(ui->protocolComboBox->currentData(ROLE_PROTOCOL_NAME).toString());
    ss->set_interface_name(ui->interfaceNameEdit->text());
    ss->set_vpnc_script_filename(ui->vpncScriptEdit->text());

    type = ui->loglevelBox->currentIndex();
    if (type == -1) {
        type = 0; //first entry is "application default"
    }
    ss->set_log_level(loglevel_rtab[type]);

    if (ss->save() < 0) {
        QMessageBox::warning(this, tr("Unable to save"), ss->m_last_err);
        return;
    }
    if (oldName != newName && !oldName.isEmpty()) {
        // Preserve client routing rules and metadata across renames.
        profileSettings.beginGroup("server:" + oldName);
        const auto keys = profileSettings.allKeys();
        QMap<QString, QVariant> extras;
        for (const auto& key : keys)
            if (key.startsWith("routing/") || key.startsWith("catalog/"))
                extras.insert(key, profileSettings.value(key));
        profileSettings.endGroup();
        profileSettings.beginGroup("server:" + newName);
        for (auto it = extras.cbegin(); it != extras.cend(); ++it)
            profileSettings.setValue(it.key(), it.value());
        profileSettings.endGroup();
        profileSettings.remove("server:" + oldName);
        profileSettings.sync();
    }
    this->accept();
}

void EditDialog::on_buttonBox_rejected()
{
    this->reject();
}

void EditDialog::on_userCertButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Open certificate"), "",
        tr("Certificate Files (*.crt *.pem *.der *.p12)"));

    // FIXME: check empty result
    ui->userCertEdit->setText(filename);
}

void EditDialog::on_userKeyButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Open private key"), "",
        tr("Private key Files (*.key *.pem *.der *.p8 *.p12)"));

    // FIXME: check empty result
    ui->userKeyEdit->setText(filename);
}

void EditDialog::on_caCertButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Open certificate"), "",
        tr("Certificate Files (*.crt *.pem *.der)"));

    // FIXME: check empty result
    ui->caCertEdit->setText(filename);
}

void EditDialog::on_userCertClear_clicked()
{
    ss->clear_cert();
    ui->userCertEdit->clear();
    ui->userCertHash->clear();
}

void EditDialog::on_userKeyClear_clicked()
{
    ss->clear_key();
    ui->userKeyEdit->clear();
}

void EditDialog::on_caCertClear_clicked()
{
    ss->clear_ca();
    ui->caCertEdit->clear();
    ui->caCertHash->clear();
}

void EditDialog::on_serverCertClear_clicked()
{
    ss->clear_server_pin();
    ui->serverCertHash->clear();
}

void EditDialog::on_tokenClear_clicked()
{
    ui->tokenBox->setCurrentIndex(-1);
    ui->tokenEdit->clear();
}

void EditDialog::on_groupnameClear_clicked()
{
    ss->clear_groupname();
    ui->groupnameEdit->clear();
}

void EditDialog::on_loadWinCert_clicked()
{
    int idx = ui->loadWinCertList->currentRow();
    win_cert_st st;
    if (idx < 0 || this->winCerts.size() <= (unsigned)idx)
        return;

    st = this->winCerts.at(idx);
    ui->userCertEdit->setText(st.cert_url);
    ui->userKeyEdit->setText(st.key_url);
}

void EditDialog::on_groupnameEdit_textChanged(const QString& arg1)
{
    ui->groupnameClear->setEnabled(!arg1.isEmpty());
}

void EditDialog::on_caCertEdit_textChanged(const QString& arg1)
{
    ui->caCertClear->setEnabled(!arg1.isEmpty());
}

void EditDialog::on_serverCertHash_textChanged(const QString& arg1)
{
    ui->serverCertClear->setEnabled(!arg1.isEmpty());
}

void EditDialog::on_tokenEdit_textChanged(const QString& arg1)
{
    ui->tokenClear->setEnabled(!arg1.isEmpty());
}

void EditDialog::on_userCertEdit_textChanged(const QString& arg1)
{
    ui->userCertClear->setEnabled(!arg1.isEmpty());
}

void EditDialog::on_userKeyEdit_textChanged(const QString& arg1)
{
    ui->userKeyClear->setEnabled(!arg1.isEmpty());
}

void EditDialog::on_loadWinCertList_itemSelectionChanged()
{
    ui->loadWinCert->setEnabled(!ui->loadWinCertList->selectedItems().empty());
}

void EditDialog::on_resetWinCertSelection_clicked()
{
    ui->loadWinCertList->setCurrentRow(-1);

    on_userCertClear_clicked();
    on_userKeyClear_clicked();
}

void EditDialog::on_vpncScriptButton_clicked()
{
#ifdef Q_OS_WIN32
    QString filter = tr("Javascript Files (*.js)");
#else
    QString filter = nullptr;
#endif

    QString filename = QFileDialog::getOpenFileName(this,
        tr("Select vpnc-script"),
        ui->vpncScriptEdit->text(),
        filter
    );

    if (! filename.isEmpty()) {
        filename = QDir::toNativeSeparators(filename);
        ui->vpncScriptEdit->setText(filename);
    }
}
