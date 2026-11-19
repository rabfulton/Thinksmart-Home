#include "weather.h"
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSaveFile>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QTimeZone>
#include <cmath>
namespace {
QString timeLabel(qint64 epoch,const QString &format) { return QDateTime::fromSecsSinceEpoch(epoch,QTimeZone(DeviceConfig::weather().value("timezone").toString().toUtf8())).toString(format); }
QJsonValue numeric(const QJsonValue &value,double low,double high) {
    if(!value.isDouble() || !std::isfinite(value.toDouble()) || value.toDouble()<low || value.toDouble()>high) return QJsonValue();
    return value;
}
void number(QJsonObject &out,const QJsonObject &in,const char *key,double low,double high) { out.insert(key,numeric(in.value(key),low,high)); }
}
QUrl WeatherClient::forecastUrl() {
    QUrl url(qEnvironmentVariable("THINKSMART_WEATHER_FORECAST_URL","https://api.open-meteo.com/v1/forecast"));
    QUrlQuery query;
    const auto location=DeviceConfig::weather();
    query.addQueryItem("latitude",QString::number(location.value("latitude").toDouble(),'f',6)); query.addQueryItem("longitude",QString::number(location.value("longitude").toDouble(),'f',6));
    query.addQueryItem("current","temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,weather_code,wind_speed_10m,wind_gusts_10m,is_day");
    query.addQueryItem("hourly","temperature_2m,precipitation_probability,precipitation,weather_code,wind_speed_10m,is_day");
    query.addQueryItem("daily","weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max,wind_speed_10m_max,sunrise,sunset");
    query.addQueryItem("wind_speed_unit","mph"); query.addQueryItem("timezone",location.value("timezone").toString());
    query.addQueryItem("timeformat","unixtime"); query.addQueryItem("forecast_days","5");
    url.setQuery(query); return url;
}
QString WeatherClient::condition(int code) {
    if(code==0)return "Clear sky"; if(code==1)return "Mainly clear"; if(code==2)return "Partly cloudy"; if(code==3)return "Overcast";
    if(code==45||code==48)return "Fog"; if(code>=51&&code<=57)return "Drizzle";
    if(code>=61&&code<=67)return "Rain"; if(code>=71&&code<=77)return "Snow";
    if(code>=80&&code<=82)return "Rain showers"; if(code==85||code==86)return "Snow showers";
    if(code==95||code==96||code==99)return "Thunderstorms"; return "Conditions unavailable";
}
QJsonObject WeatherClient::normalize(const QJsonObject &response,qint64 now) {
    const auto input=response.value("current").toObject();
    const qint64 stamp=qint64(input.value("time").toDouble());
    if(stamp<now-7*86400 || stamp>now+900 || !input.value("temperature_2m").isDouble())return {};
    QJsonObject current{{"time",stamp},{"timeLabel",timeLabel(stamp,"d MMM, HH:mm")}};
    number(current,input,"temperature_2m",-100,65); number(current,input,"apparent_temperature",-120,90);
    number(current,input,"relative_humidity_2m",0,100); number(current,input,"precipitation",0,1000);
    number(current,input,"wind_speed_10m",0,300); number(current,input,"wind_gusts_10m",0,400);
    number(current,input,"weather_code",0,99); number(current,input,"is_day",0,1);
    current.insert("condition",condition(current.value("weather_code").isDouble()?current.value("weather_code").toInt():-1));
    QJsonArray hours,days;
    const auto hourly=response.value("hourly").toObject(); const auto times=hourly.value("time").toArray();
    for(int i=0;i<times.size()&&hours.size()<12;i++) {
        const qint64 time=qint64(times.at(i).toDouble()); if(time<=now)continue;
        QJsonObject row{{"time",time},{"label",timeLabel(time,"HH:mm")}};
        for(const char *key:{"temperature_2m","precipitation_probability","precipitation","weather_code","wind_speed_10m","is_day"}) {
            const auto values=hourly.value(key).toArray(); row.insert(key,i<values.size()?values.at(i):QJsonValue());
        }
        row.insert("condition",condition(row.value("weather_code").isDouble()?row.value("weather_code").toInt():-1)); hours.append(row);
    }
    const auto daily=response.value("daily").toObject(); const auto dates=daily.value("time").toArray();
    for(int i=0;i<dates.size()&&i<5;i++) {
        const qint64 time=qint64(dates.at(i).toDouble());
        QJsonObject row{{"time",time},{"label",timeLabel(time,"ddd d MMM")}};
        for(const char *key:{"temperature_2m_max","temperature_2m_min","precipitation_sum","precipitation_probability_max","wind_speed_10m_max","weather_code","sunrise","sunset"}) {
            const auto values=daily.value(key).toArray(); row.insert(key,i<values.size()?values.at(i):QJsonValue());
        }
        row.insert("condition",condition(row.value("weather_code").isDouble()?row.value("weather_code").toInt():-1));
        for(const char *key:{"sunrise","sunset"})row.insert(QString(key)+"Label",row.value(key).isDouble()?timeLabel(qint64(row.value(key).toDouble()),"HH:mm"):"—");
        days.append(row);
    }
    return {{"location",DeviceConfig::weather().value("name")},{"latitude",DeviceConfig::weather().value("latitude")},{"longitude",DeviceConfig::weather().value("longitude")},{"timezone",DeviceConfig::weather().value("timezone")},
        {"provider","Open-Meteo"},{"attribution","Weather data by Open-Meteo.com (CC BY 4.0)"},
        {"units",QJsonObject{{"temperature","°C"},{"wind","mph"},{"precipitation","mm"},{"probability","%"},{"time","Unix seconds"}}},
        {"current",current},{"hourly",hours},{"daily",days},{"fetchedAt",now}};
}
WeatherClient::WeatherClient(QObject *parent):QObject(parent) {
    if(!DeviceConfig::enabled("weather"))return;
    loadCache();
    m_poll.setInterval(15*60*1000); connect(&m_poll,&QTimer::timeout,this,&WeatherClient::refresh);
    if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_WEATHER")){m_poll.start(); QTimer::singleShot(0,this,&WeatherClient::refresh);}
}
void WeatherClient::loadCache(){
    m_data={};QFile file(cachePath());
    if(file.open(QIODevice::ReadOnly)&&file.size()<512*1024) {
        const auto cached=QJsonDocument::fromJson(file.readAll()).object();
        if(cached.value("location")==DeviceConfig::weather().value("name")&&cached.value("latitude")==DeviceConfig::weather().value("latitude")&&cached.value("longitude")==DeviceConfig::weather().value("longitude")&&cached.value("timezone")==DeviceConfig::weather().value("timezone")&&cached.value("current").isObject())m_data=cached;
    }
}
WeatherClient::~WeatherClient(){ for(auto r:m_network.findChildren<QNetworkReply *>()){r->disconnect(this);r->abort();} }
QString WeatherClient::cachePath() const { return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/weather-"+DeviceConfig::weatherKey()+".json"; }
QVariantMap WeatherClient::current() const { return m_data.value("current").toObject().toVariantMap(); }
QVariantList WeatherClient::hourly() const { QVariantList rows;const auto now=QDateTime::currentSecsSinceEpoch();for(const auto &v:m_data.value("hourly").toArray())if(v.toObject().value("time").toDouble()>now)rows.append(v.toObject().toVariantMap());return rows; }
QVariantList WeatherClient::daily() const { return m_data.value("daily").toArray().toVariantList(); }
bool WeatherClient::stale() const { const auto stamp=qint64(m_data.value("current").toObject().value("time").toDouble()); return stamp==0||QDateTime::currentSecsSinceEpoch()-stamp>90*60; }
QString WeatherClient::status() const {
    if(m_busy)return "Updating forecast…";
    if(m_data.isEmpty())return "Weather unavailable";
    return QString(stale()?"Stale · ":!m_problem.isEmpty()?"Cached · ":"Updated · ")+m_data.value("current").toObject().value("timeLabel").toString();
}
QJsonObject WeatherClient::snapshot() const { auto data=m_data;data.insert("hourly",QJsonArray::fromVariantList(hourly()));const auto place=DeviceConfig::weather();for(auto i=place.begin();i!=place.end();++i)data.insert(i.key()=="name"?"location":i.key(),i.value());data.insert("available",!m_data.isEmpty());data.insert("stale",stale());data.insert("busy",m_busy);data.insert("status",status());data.insert("error",m_problem);return data; }
void WeatherClient::refresh() {
    const qint64 now=QDateTime::currentSecsSinceEpoch();
    if(!DeviceConfig::enabled("weather")||m_busy||now-m_attempt<60||now<m_retryAfter)return;
    m_attempt=now;m_busy=true;m_problem.clear();emit changed();
    QNetworkRequest request(forecastUrl());request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto reply=m_network.get(request);m_forecastReply=reply;
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 bytes,qint64 total){if(bytes>512*1024||total>512*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply](){
        m_forecastReply=nullptr;m_busy=false; const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto bytes=reply->readAll();
        if(reply->error()!=QNetworkReply::NoError||status!=200||bytes.size()>512*1024){m_problem="Cannot update weather. The last forecast is kept.";if(status==429)m_retryAfter=QDateTime::currentSecsSinceEpoch()+3600;}
        else {
            auto data=normalize(QJsonDocument::fromJson(bytes).object(),QDateTime::currentSecsSinceEpoch());
            if(data.isEmpty())m_problem="Weather provider returned an incomplete forecast.";
            else {
                m_data=data; const QString path=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);QDir().mkpath(path);
                QSaveFile file(cachePath());if(file.open(QIODevice::WriteOnly)){file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(QJsonDocument(m_data).toJson(QJsonDocument::Compact));file.commit();}
            }
        }
        reply->deleteLater();emit changed();
    });
}

QVariantList WeatherClient::normalizeLocations(const QJsonObject &response){
    QVariantList result;
    for(const auto &v:response.value("results").toArray()){
        const auto row=v.toObject();QJsonObject location;
        for(const auto &key:{"name","latitude","longitude","timezone"})location.insert(key,row.value(key));
        if(!DeviceConfig::validWeather(location))continue;
        QStringList label{location.value("name").toString()};
        for(const auto &key:{"admin1","country"})if(row.value(key).isString()&&!row.value(key).toString().isEmpty()&&!label.contains(row.value(key).toString()))label<<row.value(key).toString();
        auto item=location.toVariantMap();item.insert("label",label.join(", "));result.append(item);if(result.size()==8)break;
    }
    return result;
}
void WeatherClient::cancelSearch(){
    if(m_searchReply){auto reply=m_searchReply.data();m_searchReply=nullptr;reply->disconnect(this);reply->abort();reply->deleteLater();}
    m_locations.clear();m_searchProblem.clear();emit searchChanged();
}
void WeatherClient::searchLocations(const QString &text){
    cancelSearch();if(!DeviceConfig::enabled("weather"))return;
    const auto term=text.trimmed();if(term.size()<2||term.size()>100){m_searchProblem="Enter a town, city or postal code (at least two characters).";emit searchChanged();return;}
    QUrl url(qEnvironmentVariable("THINKSMART_WEATHER_GEOCODING_URL","https://geocoding-api.open-meteo.com/v1/search"));QUrlQuery query;
    query.addQueryItem("name",term);query.addQueryItem("count","8");query.addQueryItem("language","en");query.addQueryItem("format","json");url.setQuery(query);
    QNetworkRequest request(url);request.setTransferTimeout(12000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto reply=m_network.get(request);m_searchReply=reply;emit searchChanged();
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 bytes,qint64 total){if(bytes>128*1024||total>128*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply](){
        m_searchReply=nullptr;const auto bytes=reply->readAll();QJsonParseError parse;const auto doc=QJsonDocument::fromJson(bytes,&parse);
        if(reply->error()!=QNetworkReply::NoError||reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200||bytes.size()>128*1024||parse.error!=QJsonParseError::NoError||!doc.isObject()||doc.object().value("error").toBool())m_searchProblem="Location search unavailable. Your saved location is unchanged.";
        else{m_locations=normalizeLocations(doc.object());if(m_locations.isEmpty())m_searchProblem="No places found. Try a nearby town or add the country.";}
        reply->deleteLater();emit searchChanged();
    });
}
bool WeatherClient::selectLocation(int index){
    if(!DeviceConfig::enabled("weather")||index<0||index>=m_locations.size())return false;
    const auto item=QJsonObject::fromVariantMap(m_locations.at(index).toMap());QJsonObject location;
    for(const auto &key:{"name","latitude","longitude","timezone"})location.insert(key,item.value(key));
    if(!DeviceConfig::validWeather(location))return false;
    const auto path=DeviceConfig::weatherPath();QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){m_searchProblem="Cannot save location. Check settings permissions.";emit searchChanged();return false;}
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto bytes=QJsonDocument(location).toJson();
    if(file.write(bytes)!=bytes.size()||!file.commit()){m_searchProblem="Cannot save location. Your previous location is kept.";emit searchChanged();return false;}
    locationChanged();return true;
}
bool WeatherClient::resetLocation(){
    if(!DeviceConfig::enabled("weather"))return false;
    const auto path=DeviceConfig::weatherPath();if(QFile::exists(path)&&!QFile::remove(path)){m_searchProblem="Cannot restore the installed location.";emit searchChanged();return false;}
    locationChanged();return true;
}
void WeatherClient::locationChanged(){
    if(m_forecastReply){auto reply=m_forecastReply.data();m_forecastReply=nullptr;reply->disconnect(this);reply->abort();reply->deleteLater();}
    m_busy=false;m_attempt=0;m_problem.clear();loadCache();cancelSearch();emit changed();
    if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_WEATHER"))refresh();
}
