#include "layouteditor.h"
#include "deviceconfig.h"
#include <QUuid>
LayoutEditor::LayoutEditor(QObject *parent):QObject(parent){begin();}
QVariantList LayoutEditor::pages()const{return m_draft.value("pages").toArray().toVariantList();}
QVariantMap LayoutEditor::modules()const{return m_draft.value("modules").toObject().toVariantMap();}
QVariantList LayoutEditor::catalogue()const{return ModuleCatalog::options();}
QVariantList LayoutEditor::widgets()const{return ModuleCatalog::views();}
QVariantList LayoutEditor::fullPages()const{QVariantList r;for(const auto &id:ModuleCatalog::fullPages())r<<QVariantMap{{"id",id},{"title",ModuleCatalog::title(id)}};return r;}
void LayoutEditor::begin(){m_initial=DeviceConfig::dashboard();auto flags=DeviceConfig::modules();const auto saved=m_initial.value("modules").toObject();for(auto i=saved.begin();i!=saved.end();++i)flags.insert(i.key(),i.value().toBool());m_initial.insert("modules",QJsonObject::fromVariantMap(flags));m_draft=m_initial;m_problem.clear();emit changed();}
bool LayoutEditor::update(const QJsonObject &draft){m_problem=DashboardConfig::validate(draft);if(m_problem.isEmpty())m_draft=draft;emit changed();return m_problem.isEmpty();}
void LayoutEditor::setModule(const QString &id,bool enabled){if(!ModuleCatalog::names().contains(id))return;auto d=m_draft;auto flags=d.value("modules").toObject();flags.insert(id,enabled);d.insert("modules",flags);update(d);}
void LayoutEditor::addPage(const QString &module){
    if(!module.isEmpty()&&!ModuleCatalog::fullPages().contains(module))return;
    auto d=m_draft;auto pages=d.value("pages").toArray();QJsonObject page{{"id","page-"+QUuid::createUuid().toString(QUuid::WithoutBraces)},{"title",module.isEmpty()?"New page":ModuleCatalog::title(module)},{"kind",module.isEmpty()?"widgets":"module"}};
    if(module.isEmpty())page.insert("widgets",QJsonArray{});else page.insert("module",module);pages.append(page);d.insert("pages",pages);update(d);
}
void LayoutEditor::renamePage(int index,const QString &title){auto d=m_draft;auto pages=d.value("pages").toArray();if(index<0||index>=pages.size())return;auto p=pages[index].toObject();p.insert("title",title.trimmed());pages[index]=p;d.insert("pages",pages);update(d);}
void LayoutEditor::removePage(int index){auto d=m_draft;auto pages=d.value("pages").toArray();if(index<0||index>=pages.size())return;pages.removeAt(index);d.insert("pages",pages);update(d);}
void LayoutEditor::movePage(int index,int direction){auto d=m_draft;auto pages=d.value("pages").toArray();const int to=index+(direction<0?-1:1);if(index<0||index>=pages.size()||to<0||to>=pages.size())return;auto p=pages.takeAt(index);pages.insert(to,p);d.insert("pages",pages);update(d);}
void LayoutEditor::addWidget(int page,const QString &widget){auto d=m_draft;auto pages=d.value("pages").toArray();if(page<0||page>=pages.size()||pages[page].toObject().value("kind")!="widgets")return;auto p=pages[page].toObject();auto widgets=p.value("widgets").toArray();widgets.append(widget);p.insert("widgets",widgets);pages[page]=p;d.insert("pages",pages);update(d);}
void LayoutEditor::removeWidget(int page,int index){auto d=m_draft;auto pages=d.value("pages").toArray();if(page<0||page>=pages.size())return;auto p=pages[page].toObject();auto widgets=p.value("widgets").toArray();if(index<0||index>=widgets.size())return;widgets.removeAt(index);p.insert("widgets",widgets);pages[page]=p;d.insert("pages",pages);update(d);}
void LayoutEditor::moveWidget(int page,int index,int direction){auto d=m_draft;auto pages=d.value("pages").toArray();if(page<0||page>=pages.size())return;auto p=pages[page].toObject();auto widgets=p.value("widgets").toArray();const int to=index+(direction<0?-1:1);if(index<0||index>=widgets.size()||to<0||to>=widgets.size())return;auto w=widgets.takeAt(index);widgets.insert(to,w);p.insert("widgets",widgets);pages[page]=p;d.insert("pages",pages);update(d);}
bool LayoutEditor::apply(){if(!dirty())return false;if(!DashboardConfig::write(m_draft,m_problem)){emit changed();return false;}m_initial=m_draft;emit changed();emit restartRequested();return true;}
