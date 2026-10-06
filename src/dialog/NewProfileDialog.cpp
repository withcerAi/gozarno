#include "NewProfileDialog.h"
#include "ui_NewProfileDialog.h"

#include "VpnProtocolModel.h"

#include "server_storage.h"

#include <QPushButton>
#include <OcSettings.h>
#include <QUrl>
#include <QMessageBox>
#include <QLabel>
#include "client/ServerCatalog.h"
#include "client/ClientLanguage.h"

#include <memory>

NewProfileDialog::NewProfileDialog(QWidget* parent)
    : QDialog(parent)
    , ui(new Ui::NewProfileDialog)
{
    ui->setupUi(this);
    ClientLanguage::technical(ui->lineEditGateway);
    ClientLanguage::technical(ui->protocolComboBox);
    setWindowTitle(tr("Add server")); resize(580,390); setMinimumWidth(520);
    ui->verticalLayout->setContentsMargins(28,28,28,28); ui->verticalLayout->setSpacing(22);
    ui->formLayout->setSpacing(16);
    auto* title=new QLabel(tr("Add a server"),this); title->setProperty("role","pageTitle"); ui->verticalLayout->insertWidget(0,title);
    auto* hint=new QLabel(tr("Use the existing VPN address and protocol from your provider."),this); hint->setWordWrap(true); hint->setProperty("role","muted"); ui->verticalLayout->insertWidget(1,hint);
    ui->lineEditGateway->setPlaceholderText("https://vpn.example.com");
    ui->buttonBox->button(QDialogButtonBox::SaveAll)->setProperty("primary",true);
    VpnProtocolModel* model = new VpnProtocolModel(this);
    ui->protocolComboBox->setModel(model);

    ui->buttonBox->button(QDialogButtonBox::SaveAll)->setText(tr("Add && connect"));
    ui->buttonBox->button(QDialogButtonBox::SaveAll)->setDefault(true);

    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::SaveAll)->setEnabled(false);

    quick_connect = false;
}

NewProfileDialog::~NewProfileDialog()
{
    delete ui;
}

void NewProfileDialog::setQuickConnect()
{
    ui->buttonBox->button(QDialogButtonBox::SaveAll)->setEnabled(true);
    ui->buttonBox->button(QDialogButtonBox::Save)->setVisible(false);
    ui->checkBoxCustomize->setVisible(false);
    ui->protocolComboBox->setFocus();
    this->quick_connect = true;
}

QString NewProfileDialog::urlToName(QUrl & url)
{
    if (url.port(443) == 443)
        return url.host();
    else
        return (url.host() + tr(":%1").arg(url.port(443)));
}

void NewProfileDialog::updateName(QUrl & url)
{
    ui->lineEditName->setText(urlToName(url));
}

void NewProfileDialog::setUrl(QUrl & url)
{
    updateName(url);
    ui->lineEditGateway->setText(url.toString());
}

QString NewProfileDialog::getNewProfileName() const
{
    return ui->lineEditName->text();
}

void NewProfileDialog::changeEvent(QEvent* e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void NewProfileDialog::on_checkBoxCustomize_toggled(bool checked)
{
    if (checked == false) {
        QUrl url = QUrl::fromUserInput(ui->lineEditGateway->text());
        if (url.isValid()) {
            updateName(url);
        }

        ui->lineEditGateway->setFocus();
    } else {
        ui->lineEditName->setFocus();
    }
}

void NewProfileDialog::on_lineEditName_textChanged(const QString&)
{
    if (quick_connect == false)
        updateButtons();
}

void NewProfileDialog::on_lineEditGateway_textChanged(const QString& text)
{
    QUrl url(text, QUrl::StrictMode);
    if (ui->checkBoxCustomize->isChecked() == false && (url.isValid() || text.isEmpty())) {
        updateName(url);
    }

    updateButtons();
}

#define PREFIX "server:"
void NewProfileDialog::updateButtons()
{
    bool enableButtons{ false };
    if (ServerCatalog::validName(ui->lineEditName->text()) && ServerCatalog::validGateway(ui->lineEditGateway->text())) {

        enableButtons = true;

        // TODO: refactor this too :/
        OcSettings settings;
        for (const auto& key : settings.allKeys()) {
            if (key.startsWith(PREFIX) && key.endsWith("/server")) {
                QString str{ key };
                str.remove(0, sizeof(PREFIX) - 1); /* remove prefix */
                str.remove(str.size() - 7, 7); /* remove /server suffix */
                if (str == ui->lineEditName->text()) {
                    enableButtons = false;
                    break;
                }
            }
        }
    }

    ui->buttonBox->button(QDialogButtonBox::Save)->setEnabled(enableButtons);
    ui->buttonBox->button(QDialogButtonBox::SaveAll)->setEnabled(enableButtons);
}

void NewProfileDialog::on_buttonBox_clicked(QAbstractButton* button)
{
    connect_after_save = !quick_connect && ui->buttonBox->standardButton(button) == QDialogButtonBox::SaveAll;
}

void NewProfileDialog::on_buttonBox_accepted()
{
    QString gateway;
    if (!ServerCatalog::validName(ui->lineEditName->text()) || !ServerCatalog::validGateway(ui->lineEditGateway->text(), &gateway)) {
        QMessageBox::warning(this, tr("Invalid server"), tr("Enter a valid profile name and HTTPS gateway without embedded credentials."));
        return;
    }
    auto ss{ std::make_unique<StoredServer>() };
    ss->set_label(ui->lineEditName->text());
    ss->set_server_gateway(gateway);
    ss->set_protocol_name(ui->protocolComboBox->currentData(ROLE_PROTOCOL_NAME).toString());
    if (ss->save() != 0) {
        QMessageBox::warning(this, tr("Save failed"), tr("The server profile could not be saved."));
        return;
    }

    accept();
    if (connect_after_save) emit connect();
}
