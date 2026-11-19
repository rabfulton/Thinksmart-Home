#pragma once
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTimeZone>
#include <QVariantMap>
#include <QStandardPaths>
#include <cmath>
#include "dashboardconfig.h"

// Public, non-secret settings. Credentials remain in the existing private files.
// Module defaults reload on restart; the private weather override can change at runtime.
class DeviceConfig {
public:
    static QString path(){return qEnvironmentVariable("THINKSMART_DEVICE_CONFIG","/etc/thinksmart/device.json");}
    static QStringList names(){return ModuleCatalog::names();}
    static void reload(){state()=read();}
    static QString problem(){return state().problem;}
    static bool enabled(const QString &name){return names().contains(name)&&state().data.value("modules").toObject().value(name).toBool(ModuleCatalog::defaultEnabled(name));}
    static QVariantMap modules(){QVariantMap result;for(const auto &name:names())result.insert(name,enabled(name));return result;}
    static QJsonObject dashboard(){return state().dashboard.isEmpty()?DashboardConfig::defaults(modules()):state().dashboard;}
    static QJsonArray pageDefinitions(){QJsonArray r;for(const auto &p:dashboard().value("pages").toArray())if(p.toObject().value("kind")=="widgets"||enabled(p.toObject().value("module").toString()))r.append(p);return r;}
    static QStringList pages(){QStringList r;for(const auto &p:pageDefinitions())r<<p.toObject().value("id").toString();return r;}
    static bool appEnabled(const QString &app){
        if(app=="home")return true;
        if(app=="solar")return enabled("vrm");
        if(app=="music")return enabled("navidrome")||enabled("bluetooth");
        if(app=="video")return enabled("jellyfin");
        return enabled(app);
    }
    static QString weatherPath(){return qEnvironmentVariable("THINKSMART_WEATHER_CONFIG",QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/weather.json");}
    static bool validWeather(const QJsonObject &o){
        if(o.size()!=4||!o.value("name").isString()||o.value("name").toString().trimmed().isEmpty()||o.value("name").toString().size()>80||!o.value("timezone").isString()||!QTimeZone(o.value("timezone").toString().toUtf8()).isValid())return false;
        for(const auto &key:{"latitude","longitude"}){const auto v=o.value(key);if(!v.isDouble()||!std::isfinite(v.toDouble())||std::abs(v.toDouble())>(QString(key)=="latitude"?90.:180.))return false;}
        return true;
    }
    static QJsonObject weather(){
        QJsonObject result{{"name","London"},{"latitude",51.5074},{"longitude",-0.1278},{"timezone","Europe/London"}};
        const auto configured=state().data.value("weather").toObject();
        for(auto i=configured.begin();i!=configured.end();++i)result.insert(i.key(),i.value());
        QFile saved(weatherPath());
        if(saved.open(QIODevice::ReadOnly)&&saved.size()<=4096){const auto override=QJsonDocument::fromJson(saved.readAll()).object();if(validWeather(override))return override;}
        return result;
    }
    static QString weatherKey(){return QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(weather()).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex().left(16));}
    static bool actionEnabled(const QString &action){
        for(const auto &entry:ModuleCatalog::entries())if(!entry.actionPrefix.isEmpty()&&action.startsWith(entry.actionPrefix))return enabled(entry.id);
        return true;
    }
    static QString context(){
        QStringList available;for(const auto &name:names())if(enabled(name))available<<name;
        return "\n\n# Device configuration\nLocation: "+weather().value("name").toString()+". Timezone: "+weather().value("timezone").toString()+
            ". Enabled modules: "+available.join(", ")+". Only the provided tools are available; disabled modules cannot be used. This capability list takes precedence over examples mentioning unavailable services.";
    }
private:
    struct State {QJsonObject data;QString problem;QJsonObject dashboard;};
    static State &state(){static State value=read();return value;}
    static State read(){
        QFile file(path());
        auto invalid=[](){QJsonObject modules;for(const auto &name:names())modules.insert(name,false);return State{{{"modules",modules}},"Invalid device.json. Check module flags and weather coordinates/timezone, then restart."};};
        if(file.exists()&&(!file.open(QIODevice::ReadOnly)||file.size()>16384))return invalid();
        QJsonParseError error;const auto document=QJsonDocument::fromJson(file.isOpen()?file.readAll():QByteArray("{}"),&error);
        if(error.error!=QJsonParseError::NoError||!document.isObject())return invalid();
        const auto data=document.object();
        for(auto i=data.begin();i!=data.end();++i)if(!QStringList{"version","modules","weather"}.contains(i.key()))return invalid();
        if(data.contains("version")&&(!data.value("version").isDouble()||data.value("version").toDouble()!=1))return invalid();
        if(data.contains("modules")&&!data.value("modules").isObject())return invalid();
        const auto modules=data.value("modules").toObject();
        for(auto i=modules.begin();i!=modules.end();++i)if(!names().contains(i.key())||!i.value().isBool())return invalid();
        if(data.contains("weather")&&!data.value("weather").isObject())return invalid();
        const auto weather=data.value("weather").toObject();
        for(auto i=weather.begin();i!=weather.end();++i)if(!QStringList{"name","latitude","longitude","timezone"}.contains(i.key()))return invalid();
        for(const auto &key:{"latitude","longitude"})if(weather.contains(key)){
            const auto value=weather.value(key);const auto limit=QString(key)=="latitude"?90.:180.;
            if(!value.isDouble()||!std::isfinite(value.toDouble())||std::abs(value.toDouble())>limit)return invalid();
        }
        if(weather.contains("name")&&(!weather.value("name").isString()||weather.value("name").toString().trimmed().isEmpty()||weather.value("name").toString().size()>80))return invalid();
        if(weather.contains("timezone")&&(!weather.value("timezone").isString()||!QTimeZone(weather.value("timezone").toString().toUtf8()).isValid()))return invalid();
        auto effective=data;const auto dashboard=DashboardConfig::load();auto flags=data.value("modules").toObject();
        const auto overrides=dashboard.data.value("modules").toObject();for(auto i=overrides.begin();i!=overrides.end();++i)flags.insert(i.key(),i.value());
        effective.insert("modules",flags);return {effective,dashboard.problem,dashboard.data};
    }
};
