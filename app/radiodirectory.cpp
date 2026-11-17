#include "radiodirectory.h"
#include "deviceconfig.h"
#include <QDateTime>
#include <QDnsLookup>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>
#include <QHostAddress>
#include <QLocale>
#include <algorithm>
namespace {
constexpr qint64 freshFor=6*3600,keepFor=7*86400;
const QByteArray userAgent="ThinkSmart-Home/0.3 (personal internet radio)";
QString uuid(QString value){return QRegularExpression("^[a-f0-9]{8}(-[a-f0-9]{4}){3}-[a-f0-9]{12}$").match(value.toLower()).hasMatch()?value.toLower():QString();}
QString countryCode(const QString &name){
    static const QMap<QString,QString> countries=[](){
        QMap<QString,QString> names;
        for(int i=1;i<=int(QLocale::LastTerritory);i++){
            const auto territory=QLocale::Territory(i);const auto code=QLocale::territoryToCode(territory);
            if(code.size()==2)names.insert(QLocale::territoryToString(territory).toCaseFolded(),code);
        }
        const QMap<QString,QString> aliases{{"columbia","CO"},{"uk","GB"},{"great britain","GB"},{"britain","GB"},{"scotland","GB"},{"england","GB"},{"wales","GB"},{"usa","US"},{"united states of america","US"},{"south korea","KR"},{"russia","RU"}};
        for(auto i=aliases.begin();i!=aliases.end();++i)names.insert(i.key(),i.value());return names;
    }();
    if(QRegularExpression("^[A-Za-z]{2}$").match(name).hasMatch())return name.toUpper();
    return countries.value(name.toCaseFolded());
}
QString streamUrl(const QString &text){
    if(text.size()>4096)return {};
    const QUrl url(text.trimmed(),QUrl::StrictMode);const auto host=url.host().toLower();
    if(!url.isValid()||(url.scheme()!="http"&&url.scheme()!="https")||host.isEmpty()||!url.userInfo().isEmpty()||host=="localhost"||host.endsWith(".local"))return {};
    QHostAddress address;if(address.setAddress(host)){
        if(address.isNull()||address.isLoopback()||address.isLinkLocal()||address.isMulticast())return {};
        bool v4=false;const auto ip=address.toIPv4Address(&v4);
        if(v4&&((ip>>24)==0||(ip>>24)==10||(ip>>20)==0xac1||(ip>>16)==0xc0a8||(ip>>22)==0x191))return {};
        if(!v4&&(address.toIPv6Address()[0]&0xfe)==0xfc)return {};
    }
    return url.toString(QUrl::FullyEncoded);
}
QJsonObject reply(const QJsonObject &filters,const QJsonObject &entry,const QString &cache){
    return {{"ok",true},{"source","Radio Browser"},{"query",filters},{"cache",cache},{"asOf",entry.value("fetchedAt")},{"stations",entry.value("stations")},{"count",entry.value("stations").toArray().size()},{"hasMore",entry.value("hasMore").toBool()},
        {"message",entry.value("stations").toArray().isEmpty()?"No matching healthy stations. Try a related tag or relax filters.":"Choose a candidate matching the request."},
        {"guidance","Choose a suitable result and call radio.play with its id. URLs are public streams. Metadata describes a station, not a guarantee of its current programme."}};
}
}
RadioDirectory::RadioDirectory(QObject *parent):QObject(parent){
    if(!DeviceConfig::enabled("radio"))return;
    QFile file(cachePath());if(file.open(QIODevice::ReadOnly)&&file.size()<=2*1024*1024)m_cache=QJsonDocument::fromJson(file.readAll()).object();
    const auto now=QDateTime::currentSecsSinceEpoch();
    for(const auto &key:m_cache.keys()){const auto time=qint64(m_cache.value(key).toObject().value("fetchedAt").toDouble());if(time>now+60||time<now-keepFor)m_cache.remove(key);}
    rebuildStations();
}
RadioDirectory::~RadioDirectory(){for(auto *reply:m_network.findChildren<QNetworkReply *>()){reply->disconnect(this);reply->abort();}}
QString RadioDirectory::cachePath()const{return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/radio-browser/searches.json";}
QJsonObject RadioDirectory::station(const QString &id)const{
    return QDateTime::currentSecsSinceEpoch()-m_stationTimes.value(id)>keepFor?QJsonObject():m_stations.value(id);
}
QJsonObject RadioDirectory::filters(const QJsonObject &request,QString &error){
    QJsonObject result;
    for(const auto &key:{"country","language","tag","search"}){
        const auto value=request.value(key);if(value.isUndefined())continue;
        if(!value.isString()||value.toString().size()>100||value.toString().contains(QChar(0))){error="Country, language, tag and search must be strings of at most 100 characters.";return {};}
        auto text=value.toString().simplified().toLower();if(text.isEmpty())continue;
        if(QString(key)=="country"){
            text=countryCode(text);
            if(text.isEmpty()){error="Use an ISO two-letter country code or a recognized English country name.";return {};}
        }else if(QString(key)=="language"){
            const QMap<QString,QString> aliases{{"en","english"},{"es","spanish"},{"fr","french"},{"de","german"},{"pt","portuguese"},{"it","italian"},{"español","spanish"}};text=aliases.value(text,text);
        }
        result.insert(key,text);
    }
    if(result.isEmpty()&&request.value("browse").toBool())result.insert("browse",true);
    if(result.isEmpty())error="Supply at least one country, language, tag or station-name search. Filters are combined; broaden them if there are no results.";
    for(const auto &key:{"limit","offset"})if(request.contains(key)){
        const auto v=request.value(key);const int minimum=QString(key)=="limit"?1:0,maximum=QString(key)=="limit"?50:20000;
        if(!v.isDouble()||v.toDouble()!=v.toInt()||v.toInt()<minimum||v.toInt()>maximum){error="Invalid radio search limit or offset.";return {};}
        result.insert(key,v.toInt());
    }
    return result;
}
QJsonArray RadioDirectory::normalize(const QJsonArray &rows,int limit){
    QJsonArray stations;QSet<QString> ids,urls,names;
    for(const auto &value:rows){
        const auto row=value.toObject();const auto id=uuid(row.value("stationuuid").toString());
        auto url=streamUrl(row.value("url_resolved").toString());if(url.isEmpty())url=streamUrl(row.value("url").toString());
        const auto name=row.value("name").toString().simplified().left(180),country=row.value("country").toString().simplified().left(100),code=row.value("countrycode").toString().left(2).toUpper();
        const auto nameKey=name.toCaseFolded()+"/"+code;
        if(id.isEmpty()||url.isEmpty()||name.isEmpty()||row.value("lastcheckok").toInt()!=1||ids.contains(id)||urls.contains(url)||names.contains(nameKey))continue;
        ids.insert(id);urls.insert(url);names.insert(nameKey);
        QJsonArray tags;QStringList words;
        for(const auto &tag:row.value("tags").toString().split(',',Qt::SkipEmptyParts)){const auto text=tag.simplified().left(60);if(text.isEmpty())continue;tags.append(text);words.append(text);if(tags.size()==12)break;}
        const auto language=row.value("language").toString().simplified().left(120);
        stations.append(QJsonObject{{"id","rb:"+id},{"uuid",id},{"name",name},{"url",url},{"country",country},{"countryCode",code},{"language",language},{"tags",tags},
            {"description",QStringList{country,language,words.join(", ")}.join(" · ").left(600)},{"codec",row.value("codec").toString().left(20)},
            {"bitrate",std::clamp(row.value("bitrate").toInt(),0,1000000)},{"healthy",true},{"checkedAt",row.value("lastchecktime_iso8601").toString().left(40)}});
        if(stations.size()>=std::clamp(limit,1,50))break;
    }
    return stations;
}
void RadioDirectory::rebuildStations(){
    m_stations.clear();m_stationTimes.clear();
    for(const auto &entry:m_cache){const auto time=qint64(entry.toObject().value("fetchedAt").toDouble());for(const auto &value:entry.toObject().value("stations").toArray()){
        const auto row=value.toObject();const auto id=row.value("id").toString();
        if(id=="rb:"+uuid(row.value("uuid").toString())&&id.size()>3&&!streamUrl(row.value("url").toString()).isEmpty()&&time>=m_stationTimes.value(id)){
            m_stations.insert(id,row);m_stationTimes.insert(id,time);
        }
    }}
}
void RadioDirectory::remember(const QString &key,const QJsonObject &entry){
    m_cache.insert(key,entry);
    while(m_cache.size()>40){QString oldest;double time=1e30;for(auto i=m_cache.begin();i!=m_cache.end();++i){const auto t=i.value().toObject().value("fetchedAt").toDouble();if(t<time){time=t;oldest=i.key();}}m_cache.remove(oldest);}
    rebuildStations();const auto path=cachePath();QDir().mkpath(QFileInfo(path).path());
    QFile::setPermissions(QFileInfo(path).path(),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(path);if(file.open(QIODevice::WriteOnly)){file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(QJsonDocument(m_cache).toJson(QJsonDocument::Compact));file.commit();}
}
void RadioDirectory::servers(std::function<void()> ready){
    if(!m_servers.isEmpty()){ready();return;}
#if defined(THINKSMART_RADIO_TESTING) || defined(THINKSMART_VOICE_TESTING)
    for(const auto &text:qEnvironmentVariable("THINKSMART_RADIO_BROWSER_TEST_SERVERS").split(';',Qt::SkipEmptyParts)){
        const QUrl url(text);if(url.scheme()=="http"&&url.host()=="127.0.0.1")m_servers.append(url);
    }
    if(!m_servers.isEmpty()){ready();return;}
#endif
    m_waitingServers.append(std::move(ready));if(m_resolving)return;m_resolving=true;
    auto *dns=new QDnsLookup(QDnsLookup::SRV,"_api._tcp.radio-browser.info",this);
    connect(dns,&QDnsLookup::finished,this,[this,dns](){
        for(const auto &record:dns->serviceRecords()){
            auto host=record.target().toLower();if(host.endsWith('.'))host.chop(1);
            if(QRegularExpression("^[a-z0-9-]+\\.api\\.radio-browser\\.info$").match(host).hasMatch()){
                const QUrl url("https://"+host);if(!m_servers.contains(url))m_servers.append(url);
            }
        }
        if(m_servers.isEmpty())m_servers.append(QUrl("https://de1.api.radio-browser.info"));
        std::shuffle(m_servers.begin(),m_servers.end(),*QRandomGenerator::global());m_resolving=false;dns->deleteLater();
        auto callbacks=std::move(m_waitingServers);m_waitingServers.clear();for(auto &callback:callbacks)callback();
    });
    QTimer::singleShot(2000,dns,[dns](){dns->abort();});dns->lookup();
}
void RadioDirectory::search(const QJsonObject &request,Done done){
    if(!DeviceConfig::enabled("radio")){done({{"ok",false},{"error","Radio is disabled."}});return;}
    QString error;const auto query=filters(request,error);
    if(!error.isEmpty()){done({{"ok",false},{"error",error}});return;}
    const auto key=QString::fromLatin1(QCryptographicHash::hash(QJsonDocument(query).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex());
    auto cached=m_cache.value(key).toObject();const auto age=QDateTime::currentSecsSinceEpoch()-qint64(cached.value("fetchedAt").toDouble());
    if(age>keepFor||age<0){cached={};m_cache.remove(key);rebuildStations();}
    if(!cached.isEmpty()&&age>=0&&age<freshFor){done(reply(query,cached,"fresh"));return;}
    if(m_pending.contains(key)){m_pending[key]->callbacks.append(std::move(done));return;}
    if(m_pending.size()>=8){done({{"ok",false},{"error","Radio directory is busy. Try again shortly."}});return;}
    auto pending=std::make_shared<Search>();pending->key=key;pending->filters=query;pending->stale=cached;pending->callbacks.append(std::move(done));m_pending.insert(key,pending);
    servers([this,pending](){fetch(pending,0);});
}
void RadioDirectory::fetch(const std::shared_ptr<Search> &query,int attempt){
    if(attempt>=std::min(2,int(m_servers.size()))){
        if(!query->stale.isEmpty())finish(query,reply(query->filters,query->stale,"stale"));
        else finish(query,{{"ok",false},{"source","Radio Browser"},{"query",query->filters},{"error","Radio Browser is unavailable. Use a curated station without a directory lookup, or try again later."}});
        return;
    }
    QUrl url=m_servers[attempt];url.setPath("/json/stations/search");QUrlQuery params;
    for(auto i=query->filters.begin();i!=query->filters.end();++i){
        QString key=i.key();if(key=="limit"||key=="offset"||key=="browse")continue;if(key=="country"){if(i.value().toString().size()==2)key="countrycode";else params.addQueryItem("countryExact","true");}
        if(key=="search")key="name";params.addQueryItem(key,i.value().toString());
    }
    params.addQueryItem("hidebroken","true");params.addQueryItem("order","votes");params.addQueryItem("reverse","true");params.addQueryItem("limit",QString::number(qMax(25,query->filters.value("limit").toInt(5))));
    if(query->filters.value("offset").toInt()>0)params.addQueryItem("offset",QString::number(query->filters.value("offset").toInt()));url.setQuery(params);
    QNetworkRequest request(url);request.setRawHeader("User-Agent",userAgent);request.setTransferTimeout(6000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
    auto *networkReply=m_network.get(request);
    connect(networkReply,&QNetworkReply::downloadProgress,this,[networkReply](qint64 size,qint64 total){if(size>2*1024*1024||total>2*1024*1024)networkReply->abort();});
    connect(networkReply,&QNetworkReply::finished,this,[this,networkReply,query,attempt](){
        const auto bytes=networkReply->readAll();const int status=networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QJsonParseError error;const auto data=QJsonDocument::fromJson(bytes,&error);const bool ok=networkReply->error()==QNetworkReply::NoError&&status==200&&bytes.size()<=2*1024*1024&&error.error==QJsonParseError::NoError&&data.isArray();networkReply->deleteLater();
        if(!ok){fetch(query,status==429?int(m_servers.size()):attempt+1);return;}
        const QJsonObject entry{{"fetchedAt",QDateTime::currentSecsSinceEpoch()},{"stations",normalize(data.array(),query->filters.value("limit").toInt(5))},{"hasMore",data.array().size()>=qMax(25,query->filters.value("limit").toInt(5))}};remember(query->key,entry);
        finish(query,reply(query->filters,entry,"live"));
    });
}
void RadioDirectory::finish(const std::shared_ptr<Search> &query,QJsonObject result){m_pending.remove(query->key);for(auto &callback:query->callbacks)callback(result);}
void RadioDirectory::clicked(const QString &id){
    const auto stationId=id.startsWith("rb:")?uuid(id.mid(3)):QString();if(stationId.isEmpty())return;
    servers([this,stationId](){
        auto url=m_servers.first();url.setPath("/json/url/"+stationId);QNetworkRequest request(url);request.setRawHeader("User-Agent",userAgent);request.setTransferTimeout(2500);
        auto *r=m_network.get(request);connect(r,&QNetworkReply::downloadProgress,r,[r](qint64 n,qint64 total){if(n>65536||total>65536)r->abort();});connect(r,&QNetworkReply::finished,r,&QObject::deleteLater);
    });
}
