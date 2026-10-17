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
#include <QAbstractButton>
#include <QComboBox>
#include <QTabBar>
#include <QGroupBox>
#include <QAction>
#include <QTableWidget>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <memory>
namespace {
QJsonObject catalog;
QHash<QString,QString> reverseCatalog;
bool changing=false;
struct Pattern {QRegularExpression expression;QString source;QStringList markers;};
QVector<Pattern> patterns;
void loadCatalog() {
    if(!catalog.isEmpty())return;
    QFile file(":/translations/fa.json");if(file.open(QIODevice::ReadOnly))catalog=QJsonDocument::fromJson(file.readAll()).object();
    QRegularExpression marker("%[1-9][0-9]*");
    for(auto it=catalog.begin();it!=catalog.end();++it) {
        const QString english=it.key(),persian=it.value().toString();reverseCatalog.insert(persian,english);
        for(const auto& rendered:{english,persian}) {
            auto matches=marker.globalMatch(rendered);if(!matches.hasNext())continue;
            QString expression="^";QStringList markers;int last=0;
            while(matches.hasNext()) {const auto m=matches.next();expression+=QRegularExpression::escape(rendered.mid(last,m.capturedStart()-last))+"([\\s\\S]*?)";markers.append(m.captured());last=m.capturedEnd();}
            expression+=QRegularExpression::escape(rendered.mid(last))+"$";
            patterns.append({QRegularExpression(expression),english,markers});
        }
    }
}
QString sourceText(const QString& value) {
    if(catalog.contains(value))return value;
    const auto found=reverseCatalog.constFind(value);if(found!=reverseCatalog.cend())return found.value();
    return {};
}
QString translated(const QString& value) {
    if(value.isEmpty())return value;
    const auto source=sourceText(value);
    if(!source.isEmpty())return ClientLanguage::isPersian()?catalog.value(source).toString(source):source;
    for(const auto& pattern:patterns) {
        const auto match=pattern.expression.match(value);if(!match.hasMatch())continue;
        QString target=ClientLanguage::isPersian()?catalog.value(pattern.source).toString(pattern.source):pattern.source;
        QHash<QString,QString> replacements;
        for(int i=0;i<pattern.markers.size();++i)replacements.insert(pattern.markers[i],match.captured(i+1));
        // Replace markers in a single pass so user text containing %1 is preserved.
        const QRegularExpression marker("%[1-9][0-9]*");auto hits=marker.globalMatch(target);QString result;int at=0;
        while(hits.hasNext()){auto m=hits.next();result+=target.mid(at,m.capturedStart()-at)+replacements.value(m.captured(),m.captured());at=m.capturedEnd();}
        result+=target.mid(at);return result;
    }
    return value;
}
void refresh(QObject* object) {
    if(!object || object->property("translationExcluded").toBool())return;
    const bool technical=object->property("technical").toBool();
    if(!technical) {
        if(auto* label=qobject_cast<QLabel*>(object)) {
            const auto text=translated(label->text());if(text!=label->text())label->setText(text);
            if(!(label->alignment()&Qt::AlignHCenter) && (label->alignment()&(Qt::AlignLeft|Qt::AlignRight)))
                label->setAlignment((label->alignment()&~(Qt::AlignLeft|Qt::AlignRight))|(ClientLanguage::isPersian()?Qt::AlignRight:Qt::AlignLeft)|Qt::AlignAbsolute);
        }
        if(auto* button=qobject_cast<QAbstractButton*>(object)){
            const auto key=button->property("translationSourceText").toString();
            const auto text=key.isEmpty()?translated(button->text()):(ClientLanguage::isPersian()?catalog.value(key).toString(key):key);
            if(text!=button->text())button->setText(text);
        }
        if(auto* group=qobject_cast<QGroupBox*>(object)){const auto text=translated(group->title());if(text!=group->title())group->setTitle(text);}
        if(auto* combo=qobject_cast<QComboBox*>(object)) {
            if(combo->objectName()!="serverList" && combo->objectName()!="languageSelector") {
                QSignalBlocker blocked(combo);for(int i=0;i<combo->count();++i){const auto text=translated(combo->itemText(i));if(text!=combo->itemText(i))combo->setItemText(i,text);}
            }
        }
    }
    if(auto* tabs=qobject_cast<QTabBar*>(object)){QSignalBlocker blocked(tabs);for(int i=0;i<tabs->count();++i){const auto text=translated(tabs->tabText(i));if(text!=tabs->tabText(i))tabs->setTabText(i,text);}}
    if(auto* action=qobject_cast<QAction*>(object)) {
        if(!technical){const auto text=translated(action->text());if(text!=action->text())action->setText(text);}
        const auto tip=translated(action->toolTip());if(tip!=action->toolTip())action->setToolTip(tip);
    }
    if(auto* widget=qobject_cast<QWidget*>(object)) {
        const auto tip=translated(widget->toolTip());if(tip!=widget->toolTip())widget->setToolTip(tip);
        const auto name=translated(widget->accessibleName());if(name!=widget->accessibleName())widget->setAccessibleName(name);
        const auto title=translated(widget->windowTitle());if(title!=widget->windowTitle())widget->setWindowTitle(title);
    }
    if(auto* field=qobject_cast<QLineEdit*>(object)){const auto value=translated(field->placeholderText());if(value!=field->placeholderText())field->setPlaceholderText(value);}
    if(auto* table=qobject_cast<QTableWidget*>(object))for(int i=0;i<table->columnCount();++i)if(auto* item=table->horizontalHeaderItem(i)){const auto value=translated(item->text());if(value!=item->text())item->setText(value);}
}
class LocaleLayoutFilter final:public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* object,QEvent* event) override {
        if(!changing && (event->type()==QEvent::Show || event->type()==QEvent::Polish || event->type()==QEvent::Paint || event->type()==QEvent::LanguageChange))refresh(object);
        return false;
    }
};
class PersianTranslator final:public QTranslator {
public:
    bool isEmpty() const override {return catalog.isEmpty();}
    QString translate(const char*,const char* source,const char*,int) const override {
        const auto value=catalog.value(QString::fromUtf8(source));return value.isString()?value.toString():QString();
    }
};
std::unique_ptr<PersianTranslator> translator;
QPointer<LocaleLayoutFilter> layoutFilter;
}
void ClientLanguage::apply(const QString& code) {
    loadCatalog();changing=true;
    if(!translator)translator=std::make_unique<PersianTranslator>();
    if(!layoutFilter){layoutFilter=new LocaleLayoutFilter(qApp);qApp->installEventFilter(layoutFilter);}
    const bool persian=code=="fa";qApp->setProperty("interfaceLanguage",persian?"fa":"en");
    qApp->removeTranslator(translator.get());if(persian)qApp->installTranslator(translator.get());
    QLocale::setDefault(QLocale(persian?"fa_IR":"en_US"));QApplication::setLayoutDirection(persian?Qt::RightToLeft:Qt::LeftToRight);
    for(auto* widget:QApplication::allWidgets()) {
        refresh(widget);for(auto* action:widget->actions())refresh(action);
        for(auto* action:widget->findChildren<QAction*>())refresh(action);
        widget->update();
    }
    changing=false;
}
bool ClientLanguage::isPersian(){return qApp->property("interfaceLanguage").toString()=="fa";}
void ClientLanguage::technical(QWidget* widget) {
    widget->setProperty("technical",true);widget->setLayoutDirection(Qt::LeftToRight);
    if(auto* number=qobject_cast<QAbstractSpinBox*>(widget))number->setLocale(QLocale::c());
    if(auto* field=qobject_cast<QLineEdit*>(widget))field->setAlignment(Qt::AlignLeft);
    if(auto* value=qobject_cast<QLabel*>(widget))value->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
}
