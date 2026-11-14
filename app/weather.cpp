#include "weather.h"
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QSaveFile>
#include <QDir>
#include <QFile>
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
    QUrl url("https://api.open-meteo.com/v1/forecast");
    QUrlQuery query;
    const auto location=DeviceConfig::weather();
    query.addQueryItem("latitude",QString::number(location.value("latitude").toDouble(),'f',6)); query.addQueryItem("longitude",QString::number(location.value("longitude").toDouble(),'f',6));
    query.addQueryItem("current","temperature_2m,apparent_temperature,relative_humidity_2m,precipitation,weather_code,wind_speed_10m,wind_gusts_10m,is_day");
    query.addQueryItem("hourly","temperature_2m,precipitation_probability,precipitation,weather_code,wind_speed_10m");
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
        const qint64 time=qint64(times.at(i).toDouble()); if(time<now-900)continue;
        QJsonObject row{{"time",time},{"label",timeLabel(time,"HH:mm")}};
        for(const char *key:{"temperature_2m","precipitation_probability","precipitation","weather_code","wind_speed_10m"}) {
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
    QFile file(cachePath());
    if(file.open(QIODevice::ReadOnly)&&file.size()<512*1024) {
        const auto cached=QJsonDocument::fromJson(file.readAll()).object();
        if(cached.value("location")==DeviceConfig::weather().value("name")&&cached.value("current").isObject())m_data=cached;
    }
    m_poll.setInterval(15*60*1000); connect(&m_poll,&QTimer::timeout,this,&WeatherClient::refresh);
    if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_WEATHER")){m_poll.start(); QTimer::singleShot(0,this,&WeatherClient::refresh);}
}
WeatherClient::~WeatherClient(){ for(auto r:m_network.findChildren<QNetworkReply *>()){r->disconnect(this);r->abort();} }
QString WeatherClient::cachePath() const { return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/weather-"+DeviceConfig::weatherKey()+".json"; }
QVariantMap WeatherClient::current() const { return m_data.value("current").toObject().toVariantMap(); }
QVariantList WeatherClient::hourly() const { return m_data.value("hourly").toArray().toVariantList(); }
QVariantList WeatherClient::daily() const { return m_data.value("daily").toArray().toVariantList(); }
bool WeatherClient::stale() const { const auto stamp=qint64(m_data.value("current").toObject().value("time").toDouble()); return stamp==0||QDateTime::currentSecsSinceEpoch()-stamp>90*60; }
QString WeatherClient::status() const {
    if(m_busy)return "Updating forecast…";
    if(m_data.isEmpty())return "Weather unavailable";
    return QString(stale()?"Stale · ":!m_problem.isEmpty()?"Cached · ":"Updated · ")+m_data.value("current").toObject().value("timeLabel").toString();
}
QJsonObject WeatherClient::snapshot() const { auto data=m_data;data.insert("available",!m_data.isEmpty());data.insert("stale",stale());data.insert("busy",m_busy);data.insert("status",status());data.insert("error",m_problem);return data; }
void WeatherClient::refresh() {
    const qint64 now=QDateTime::currentSecsSinceEpoch();
    if(!DeviceConfig::enabled("weather")||m_busy||now-m_attempt<60||now<m_retryAfter)return;
    m_attempt=now;m_busy=true;m_problem.clear();emit changed();
    QNetworkRequest request(forecastUrl());request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto reply=m_network.get(request);
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 bytes,qint64 total){if(bytes>512*1024||total>512*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply](){
        m_busy=false; const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
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
