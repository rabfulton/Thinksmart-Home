#include "layouteditor.h"
#include "deviceconfig.h"
#include <QUuid>
namespace {
QJsonObject removePlacements(QJsonObject draft,const QStringList &ids,const QString &keepPage={}){
    auto flags=draft.value("modules").toObject();QStringList affected;
    for(const auto &id:ids){flags.insert(id,false);for(const auto &e:ModuleCatalog::entries())if(e.id==id&&!e.widget.isEmpty())affected<<e.widget;}
    QJsonArray pages;
    for(const auto &value:draft.value("pages").toArray()){
        auto page=value.toObject();
        if(page.value("kind")=="module"&&ids.contains(page.value("module").toString()))continue;
        if(page.value("kind")=="widgets"){
            const auto old=page.value("widgets").toArray();QJsonArray widgets;
            for(const auto &w:old)if(!affected.contains(w.toString())||ModuleCatalog::widgetEnabled(w.toString(),flags.toVariantMap()))widgets.append(w);
            if(!old.isEmpty()&&widgets.isEmpty()&&page.value("id")!="home"&&page.value("id")!=keepPage)continue;
            page.insert("widgets",widgets);
        }
        pages.append(page);
    }
    draft.insert("modules",flags);draft.insert("pages",pages);return draft;
}
}
LayoutEditor::LayoutEditor(QObject *parent):QObject(parent){begin();}
QVariantList LayoutEditor::pages()const{return m_draft.value("pages").toArray().toVariantList();}
QVariantMap LayoutEditor::modules()const{return m_draft.value("modules").toObject().toVariantMap();}
QVariantList LayoutEditor::catalogue()const{return ModuleCatalog::options();}
QVariantList LayoutEditor::widgets()const{return ModuleCatalog::views();}
QVariantList LayoutEditor::fullPages()const{QVariantList r;for(const auto &id:ModuleCatalog::fullPages())r<<QVariantMap{{"id",id},{"title",ModuleCatalog::title(id)}};return r;}
QVariantList LayoutEditor::applications(bool active)const{
    QVariantList result;const auto flags=modules();
    for(const auto &value:catalogue()){
        auto row=value.toMap();const auto id=row.value("id").toString();if(flags.value(id).toBool()!=active)continue;
        QStringList placements;
        for(const auto &p:m_draft.value("pages").toArray()){
            const auto page=p.toObject();
            if(page.value("kind")=="module"&&page.value("module")==id){placements<<"Full page · "+page.value("title").toString();row.insert("pageTitle",page.value("title").toString());}
            else if(page.value("kind")=="widgets"&&page.value("widgets").toArray().contains(row.value("widget").toString()))placements<<"Widget · "+page.value("title").toString();
        }
        row.insert("placement",placements.isEmpty()?"Open on request":placements.join("; "));result<<row;
    }
    return result;
}
QVariantList LayoutEditor::activeApplications()const{return applications(true);}
QVariantList LayoutEditor::availableApplications()const{return applications(false);}
QVariantList LayoutEditor::locations(const QString &application)const{
    QString widget;for(const auto &e:ModuleCatalog::entries())if(e.id==application)widget=e.widget;
    QVariantList result;if(widget.isEmpty())return result;
    for(const auto &value:m_draft.value("pages").toArray()){
        const auto page=value.toObject();
        if(page.value("kind")!="widgets")continue;
        const auto widgets=page.value("widgets").toArray();
        if(widgets.size()<6||widgets.contains(widget))result<<QVariantMap{{"id",page.value("id").toString()},{"title",page.value("title").toString()},{"description",widgets.contains(widget)?"Use the existing card":QString::number(6-widgets.size())+" free slots"}};
    }
    return result;
}
bool LayoutEditor::reject(const QString &problem){m_problem=problem;emit changed();return false;}
bool LayoutEditor::canCreatePage(const QString &id)const{return removePlacements(m_draft,{id}).value("pages").toArray().size()<24;}
bool LayoutEditor::placeApplication(const QString &id,const QString &mode,const QString &pageId,const QString &title,bool replace){
    if(!ModuleCatalog::names().contains(id))return reject("Choose an application from the list.");
    if(modules().value(id).toBool()!=replace)return reject(replace?"This application is not active.":"This application is already active. Edit its placement instead.");
    QString widget,pageModule;for(const auto &e:ModuleCatalog::entries())if(e.id==id){widget=e.widget;pageModule=e.page;}
    if((mode=="widget"&&widget.isEmpty())||(mode=="page"&&pageModule.isEmpty())||!QStringList{"widget","page","ondemand"}.contains(mode))return reject("Choose a supported display type.");
    if(mode!="widget"&&!pageId.isEmpty())return reject("A full-page application needs its own page.");
    QJsonObject previousPage;int previousIndex=-1;
    if(replace&&mode=="page"){
        const auto previous=m_draft.value("pages").toArray();
        for(int i=0;i<previous.size();++i)if(previous[i].toObject().value("kind")=="module"&&previous[i].toObject().value("module")==id){previousPage=previous[i].toObject();previousIndex=i;break;}
    }
    auto draft=removePlacements(m_draft,{id},pageId);auto flags=draft.value("modules").toObject();flags.insert(id,true);draft.insert("modules",flags);
    auto pages=draft.value("pages").toArray();
    if(mode=="widget"&&!pageId.isEmpty()){
        bool found=false;
        for(int i=0;i<pages.size();++i){auto p=pages[i].toObject();if(p.value("id")!=pageId)continue;
            if(p.value("kind")!="widgets")return reject("Choose a widget page.");
            auto widgets=p.value("widgets").toArray();if(!widgets.contains(widget)){if(widgets.size()>=6)return reject("This page is full. Choose another page or create one.");widgets.append(widget);}
            p.insert("widgets",widgets);pages[i]=p;found=true;break;
        }
        if(!found)return reject("That page is no longer available.");
    }else if(mode!="ondemand"){
        if(pages.size()>=24)return reject("All 24 pages are in use. Choose an existing page or open on request.");
        QJsonObject page{{"id",previousPage.isEmpty()?"page-"+QUuid::createUuid().toString(QUuid::WithoutBraces):previousPage.value("id").toString()},{"title",title.isEmpty()?ModuleCatalog::title(id):title.trimmed()},{"kind",mode=="page"?"module":"widgets"}};
        if(mode=="page")page.insert("module",pageModule);else page.insert("widgets",QJsonArray{widget});
        if(previousIndex>=0)pages.insert(qMin(previousIndex,int(pages.size())),page);else pages.append(page);
    }
    draft.insert("pages",pages);return update(draft);
}
bool LayoutEditor::removeApplications(const QStringList &ids){
    if(ids.isEmpty())return reject("Select applications to remove.");
    for(const auto &id:ids)if(!ModuleCatalog::names().contains(id)||!modules().value(id).toBool())return reject("Select active applications from the list.");
    return update(removePlacements(m_draft,ids));
}
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
