// SPDX-License-Identifier: GPL-2.0-or-later
#include "ClientLanguage.h"
#include <QApplication>
#include <QTranslator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QLocale>
#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QEvent>
#include <QPointer>
#include <QAbstractSpinBox>
#include <memory>

namespace {
class LocaleLayoutFilter final : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* object,QEvent* event) override {
        if((event->type()==QEvent::Polish || event->type()==QEvent::Show) && ClientLanguage::isPersian()) {
            if(auto* label=qobject_cast<QLabel*>(object)) {
                if(!label->property("technical").toBool() && !(label->alignment() & Qt::AlignHCenter))
                    label->setAlignment((label->alignment() & Qt::AlignVertical_Mask)|Qt::AlignRight|Qt::AlignAbsolute);
            }
        }
        return false;
    }
};
class PersianTranslator final : public QTranslator {
public:
    PersianTranslator() {
        QFile file(":/translations/fa.json");
        if(file.open(QIODevice::ReadOnly)) messages=QJsonDocument::fromJson(file.readAll()).object();
    }
    bool isEmpty() const override { return messages.isEmpty(); }
    QString translate(const char*,const char* source,const char*,int) const override {
        const auto value=messages.value(QString::fromUtf8(source));
        return value.isString() ? value.toString() : QString();
    }
private:
    QJsonObject messages;
};
std::unique_ptr<PersianTranslator> translator;
QPointer<LocaleLayoutFilter> layoutFilter;
}
void ClientLanguage::apply(const QString& code) {
    if(!translator) translator=std::make_unique<PersianTranslator>();
    if(!layoutFilter) { layoutFilter=new LocaleLayoutFilter(qApp); qApp->installEventFilter(layoutFilter); }
    qApp->removeTranslator(translator.get());
    const bool persian=code=="fa";
    if(persian) qApp->installTranslator(translator.get());
    qApp->setProperty("interfaceLanguage",persian ? "fa" : "en");
    QLocale::setDefault(QLocale(persian ? "fa_IR" : "en_US"));
    QApplication::setLayoutDirection(persian ? Qt::RightToLeft : Qt::LeftToRight);
}
bool ClientLanguage::isPersian() { return qApp->property("interfaceLanguage").toString()=="fa"; }
void ClientLanguage::technical(QWidget* widget) {
    widget->setProperty("technical",true);
    widget->setLayoutDirection(Qt::LeftToRight);
    if(auto* number=qobject_cast<QAbstractSpinBox*>(widget)) number->setLocale(QLocale::c());
    if(auto* field=qobject_cast<QLineEdit*>(widget)) field->setAlignment(Qt::AlignLeft);
    if(auto* value=qobject_cast<QLabel*>(widget)) value->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
}
