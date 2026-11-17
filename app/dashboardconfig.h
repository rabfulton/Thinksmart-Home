#pragma once
#include "modulecatalog.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

class DashboardConfig {
public:
    struct Result {QJsonObject data;QString problem;};
    static QString path(){return qEnvironmentVariable("THINKSMART_DASHBOARD_CONFIG",QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/dashboard.json");}
    static QString validate(const QJsonObject &o){
        for(auto i=o.begin();i!=o.end();++i)if(!QStringList{"version","modules","pages"}.contains(i.key()))return "Unknown dashboard setting.";
        if(o.value("version").toDouble()!=1||!o.value("modules").isObject()||!o.value("pages").isArray())return "Invalid dashboard format.";
        const auto modules=o.value("modules").toObject();
        for(auto i=modules.begin();i!=modules.end();++i)if(!ModuleCatalog::names().contains(i.key())||!i.value().isBool())return "Unknown module or invalid switch.";
        const auto pages=o.value("pages").toArray();if(pages.isEmpty()||pages.size()>24)return "Keep between one and 24 pages.";
        QStringList ids;bool home=false;
        for(const auto &v:pages){
            if(!v.isObject())return "Invalid page.";const auto p=v.toObject();
            for(auto i=p.begin();i!=p.end();++i)if(!QStringList{"id","title","kind","widgets","module"}.contains(i.key()))return "Unknown page setting.";
            const auto id=p.value("id").toString(),title=p.value("title").toString(),kind=p.value("kind").toString();
            if(!QRegularExpression("^[a-zA-Z0-9_-]{1,64}$").match(id).hasMatch()||ids.contains(id))return "Page IDs must be unique.";
            ids<<id;if(title.trimmed().isEmpty()||title.size()>32||title.contains(QChar(0))||title.contains('\n'))return "Page names need 1–32 characters.";
            if(kind=="widgets"){
                if(p.contains("module")||!p.value("widgets").isArray())return "Invalid widget page.";
                const auto widgets=p.value("widgets").toArray();if(widgets.size()>6)return "A page can contain up to six widgets.";
                QStringList seen;for(const auto &w:widgets){const auto widget=w.toString();if(!ModuleCatalog::widgets().contains(widget)||seen.contains(widget))return "Unknown or duplicate widget.";seen<<widget;}
            }else if(kind=="module"){
                if(p.contains("widgets")||!ModuleCatalog::fullPages().contains(p.value("module").toString()))return "Unknown full-page view.";
            }else return "Unknown page type.";
            if(id=="home"){if(kind!="widgets")return "Home must remain a widget page.";home=true;}
        }
        return home?QString():QString("Keep the Home page so Settings stays accessible.");
    }
    static Result load(){
        if(!QFile::exists(path()))return {};
        auto read=[](const QString &path)->QJsonObject{QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>65536)return {};QJsonParseError e;auto d=QJsonDocument::fromJson(f.readAll(),&e);return e.error==QJsonParseError::NoError&&d.isObject()&&validate(d.object()).isEmpty()?d.object():QJsonObject{};};
        auto data=read(path());if(!data.isEmpty())return {data,{}};
        data=read(path()+".bak");if(!data.isEmpty()){
            QJsonObject stopped;for(const auto &id:ModuleCatalog::names())stopped.insert(id,false);data.insert("modules",stopped);
            return {data,"Recovered the previous layout with modules stopped. Review the module switches and apply to repair settings."};
        }
        QJsonObject modules;for(const auto &id:ModuleCatalog::names())modules.insert(id,false);
        return {{{"version",1},{"modules",modules},{"pages",QJsonArray{QJsonObject{{"id","home"},{"title","Home"},{"kind","widgets"},{"widgets",QJsonArray{}}}}}},"Invalid dashboard settings. Optional modules are stopped; use Pages & modules to recover."};
    }
    static bool write(const QJsonObject &data,QString &problem){
        problem=validate(data);if(!problem.isEmpty())return false;
        if(!QDir().mkpath(QFileInfo(path()).absolutePath())){problem="Cannot create the dashboard settings folder.";return false;}
        auto save=[&](const QString &name,const QJsonObject &value){QSaveFile f(name);if(!f.open(QIODevice::WriteOnly))return false;f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto bytes=QJsonDocument(value).toJson();return f.write(bytes)==bytes.size()&&f.commit();};
        if(QFile::exists(path())){const auto previous=load().data;if(!previous.isEmpty()&&!save(path()+".bak",previous)){problem="Cannot back up dashboard settings.";return false;}}
        if(!save(path(),data)){problem="Cannot save dashboard settings.";return false;}return true;
    }
    static QJsonObject defaults(const QVariantMap &modules){
        QJsonArray widgets,pages;for(const auto &id:QStringList{"assistant","weather","energy","radio","music","video"})if(ModuleCatalog::widgetEnabled(id,modules))widgets.append(id);
        for(const auto &id:QStringList{"chess","notes","news"})if(modules.value(id).toBool())pages.append(QJsonObject{{"id",id},{"title",ModuleCatalog::title(id)},{"kind","module"},{"module",id}});
        pages.append(QJsonObject{{"id","home"},{"title","Home"},{"kind","widgets"},{"widgets",widgets}});
        return {{"version",1},{"modules",QJsonObject::fromVariantMap(modules)},{"pages",pages}};
    }
};
