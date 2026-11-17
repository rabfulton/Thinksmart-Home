#include "iptv.h"
#include "deviceconfig.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <memory>
#include <QFileInfo>
#include <QLocale>
#include <QUrlQuery>

namespace {
bool writeJson(const QString &path,const QJsonObject &data){
    if(!QDir().mkpath(QFileInfo(path).absolutePath()))return false;
    QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))return false;f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    const auto bytes=QJsonDocument(data).toJson(QJsonDocument::Compact);return f.write(bytes)==bytes.size()&&f.commit();
}
QString groupId(const QString &channel,const QString &feed){return "org-"+QString::fromLatin1(QCryptographicHash::hash((channel+"@"+feed).toUtf8(),QCryptographicHash::Sha256).toHex().left(24));}
bool matches(const QJsonObject &row,const QJsonObject &f){
    const auto search=f.value("search").toString().trimmed().left(120),country=f.value("country").toString().trimmed().toUpper(),category=f.value("category").toString().trimmed().toLower();
    return (search.isEmpty()||(row.value("name").toString()+" "+row.value("feed").toString()).contains(search,Qt::CaseInsensitive))&&(country.isEmpty()||row.value("country").toString()==country)&&(category.isEmpty()||row.value("categories").toArray().contains(category));
}
}
bool IptvClient::softwareDecode(const QUrl &url){
    // This feed freezes displayed Venus playback even when its direct rendition
    // decodes headlessly. Keep other IPTV hardware accelerated; retain HD here.
    return url.host().endsWith(".cdn-redge.media",Qt::CaseInsensitive)&&url.path().contains("/bbc_news/",Qt::CaseInsensitive);
}
void IptvClient::refreshModels(){
    m_channelModel.setRows(matching(m_filters,0,m_limit));
    const auto search=m_filters.value("search");
    m_savedModel.setRows(matching({{"saved",true},{"search",search}},0,100));
    m_suggestedModel.setRows(matching({{"suggested",true},{"search",search}},0,30));
}
QString IptvClient::storagePath(){return qEnvironmentVariable("THINKSMART_IPTV_FILE",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/iptv/favourites.json");}
QString IptvClient::cachePath()const{return qEnvironmentVariable("THINKSMART_IPTV_CACHE",QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/iptv/catalogue.json");}
bool IptvClient::validUrl(const QString &value){const QUrl u(value);return value.size()<=4096&&!value.contains(QRegularExpression("[\\x00-\\x1f\\x7f]"))&&u.isValid()&&!u.host().isEmpty()&&(u.scheme()=="https"||u.scheme()=="http");}
IptvClient::IptvClient(QObject *parent):QObject(parent){if(DeviceConfig::enabled("iptv")){load();refreshModels();}}
IptvClient::~IptvClient(){cancelPlayback();cancel();}
void IptvClient::load(){
    QFile seed(":/iptv-stations.json");if(seed.open(QIODevice::ReadOnly))m_seed=QJsonDocument::fromJson(seed.readAll()).object().value("channels").toArray();m_rows=m_seed;
    QFile cache(cachePath());if(cache.open(QIODevice::ReadOnly)&&cache.size()<16*1024*1024){const auto o=QJsonDocument::fromJson(cache.readAll()).object();const auto rows=o.value("channels").toArray();bool valid=o.value("version")==1&&!rows.isEmpty()&&rows.size()<=20000;for(const auto &v:rows)if(!validUrl(v.toObject().value("url").toString()))valid=false;if(valid){m_rows=rows;m_updated=qint64(o.value("updated").toDouble());}}
    QFile saved(storagePath());if(!saved.exists())return;
    if(!saved.open(QIODevice::ReadOnly)||saved.size()>1024*1024){m_storeHealthy=false;m_problem="Cannot read saved TV channels. Repair the favourites file before changing it.";return;}
    QJsonParseError e;const auto doc=QJsonDocument::fromJson(saved.readAll(),&e);const auto o=doc.object();const auto rows=o.value("favourites").toArray();bool valid=e.error==QJsonParseError::NoError&&o.value("version")==1&&o.value("favourites").isArray()&&rows.size()<=100;
    QStringList ids;for(const auto &v:rows){const auto row=v.toObject();if(!validUrl(row.value("url").toString())||row.value("id").toString().isEmpty()||ids.contains(row.value("id").toString()))valid=false;ids<<row.value("id").toString();}
    if(!valid){m_storeHealthy=false;m_problem="Saved TV channels are invalid. The file has been left untouched.";return;}
    m_saved=rows;const auto last=o.value("last").toObject();if(validUrl(last.value("url").toString()))m_last=last;
}
void IptvClient::cancel(){++m_generation;if(m_reply){m_reply->abort();m_reply->deleteLater();m_reply=nullptr;}m_busy=false;}
void IptvClient::setActive(bool value){value=value&&DeviceConfig::enabled("iptv");if(value==m_active)return;m_active=value;if(value&&QDateTime::currentSecsSinceEpoch()-m_updated>86400)refresh();else if(!value&&m_pending.isEmpty())cancel();emit changed();}
QString IptvClient::status()const{if(!DeviceConfig::enabled("iptv"))return "IPTV disabled";if(m_busy)return "Updating channel directory…";return m_updated>0?"IPTV-org · updated "+QDateTime::fromSecsSinceEpoch(m_updated).toLocalTime().toString("d MMM, HH:mm"):"Starter channels · IPTV-org directory";}
QJsonObject IptvClient::summary(const QJsonObject &r,bool saved){QJsonObject result;for(const auto &key:{"id","name","country","categories","quality","labels","origin"})if(r.contains(key))result.insert(key,r.value(key));result.insert("saved",saved);result.insert("decoderPreference",softwareDecode(QUrl(r.value("url").toString()))?"software":"hardware");return result;}
QVariantList IptvClient::favourites()const{QVariantList r;for(const auto &v:m_saved)r<<summary(v.toObject(),true).toVariantMap();return r;}
QVariantMap IptvClient::lastChannel()const{return m_last.isEmpty()?QVariantMap{}:summary(m_last).toVariantMap();}
QJsonObject IptvClient::resolve(const QString &id)const{
    if(id.isEmpty())return {};
    // A refreshed directory can update a saved channel's URL without losing its identity.
    for(const auto &v:m_rows)if(v.toObject().value("id")==id)return v.toObject();
    for(const auto &v:m_saved)if(v.toObject().value("id")==id)return v.toObject();
    return m_last.value("id")==id?m_last:QJsonObject{};
}
QJsonArray IptvClient::matching(const QJsonObject &filters,int offset,int limit)const{
    const auto source=filters.value("saved").toBool()?m_saved:filters.value("suggested").toBool()?m_seed:m_rows;QJsonArray r;int skipped=0;
    for(const auto &v:source){auto row=v.toObject();if(!matches(row,filters))continue;if(skipped++<offset)continue;bool saved=false;for(const auto &s:m_saved)if(s.toObject().value("id")==row.value("id")){saved=true;break;}r.append(summary(row,saved));if(r.size()>=limit)break;}return r;
}
QVariantList IptvClient::channels()const{return matching(m_filters,0,m_limit).toVariantList();}
bool IptvClient::hasMore()const{return matching(m_filters,m_limit,1).size()>0;}
void IptvClient::filter(const QString &search,const QString &country,const QString &category){m_filters={{"search",search.left(120)},{"country",country},{"category",category}};m_limit=30;refreshModels();emit changed();}
void IptvClient::loadMore(){m_limit=qMin(20000,m_limit+30);refreshModels();emit changed();}
QVariantList IptvClient::countries()const{QStringList values;for(const auto &v:m_rows){const auto c=v.toObject().value("country").toString();if(!c.isEmpty()&&!values.contains(c))values<<c;}values.sort();QVariantList r{QVariantMap{{"id",""},{"title","Any country"}}};for(const auto &v:values){const auto country=QLocale::codeToTerritory(v);r<<QVariantMap{{"id",v},{"title",country==QLocale::AnyTerritory?v:QLocale::territoryToString(country)}};}return r;}
QVariantList IptvClient::categories()const{QStringList values;for(const auto &v:m_rows)for(const auto &x:v.toObject().value("categories").toArray())if(!values.contains(x.toString()))values<<x.toString();values.sort();QVariantList r{QVariantMap{{"id",""},{"title","Any category"}}};for(const auto &v:values)r<<QVariantMap{{"id",v},{"title",v.left(1).toUpper()+v.mid(1)}};return r;}
QJsonArray IptvClient::normalize(const QJsonArray &channels,const QJsonArray &streams,const QJsonArray &preferred){
    QHash<QString,QJsonObject> metadata,best;QHash<QString,int> scores;QHash<QString,QString> preferredUrls;
    for(const auto &v:preferred){const auto r=v.toObject();preferredUrls.insert(r.value("id").toString(),r.value("url").toString());}
    for(const auto &v:channels){const auto c=v.toObject();if(c.value("is_nsfw").isBool()&&!c.value("is_nsfw").toBool()&&(!c.contains("closed")||c.value("closed").isNull()))metadata.insert(c.value("id").toString(),c);}
    for(const auto &v:streams){
        const auto s=v.toObject(),c=metadata.value(s.value("channel").toString());const auto url=s.value("url").toString(),quality=s.value("quality").toString();
        if(c.isEmpty()||!validUrl(url)||!QUrl(url).userInfo().isEmpty()||!s.value("referrer").toString().isEmpty()||!s.value("user_agent").toString().isEmpty())continue;
        const int height=quality.section(QRegularExpression("[^0-9]"),0,0).toInt();if(height>1080||quality.contains("4k",Qt::CaseInsensitive)||quality.contains("8k",Qt::CaseInsensitive))continue;
        const auto id=groupId(s.value("channel").toString(),s.value("feed").toString());int score=(QUrl(url).scheme()=="https"?10:0)+(height==720?50:height==1080?40:height>=480?25:5);
        QString official=QUrl(c.value("website").toString()).host().toLower();if(official.startsWith("www."))official=official.mid(4);const auto host=QUrl(url).host().toLower();if(!official.isEmpty()&&(host==official||host.endsWith("."+official)))score+=1000;if(preferredUrls.value(id)==url)score+=2000;
        if(best.contains(id)&&scores.value(id)>=score)continue;
        QJsonArray categories,labels;for(const auto &x:c.value("categories").toArray())if(categories.size()<12)categories.append(x.toString().left(40));for(const auto &x:s.value("labels").toArray())if(labels.size()<5)labels.append(x.toString().left(80));
        best.insert(id,{{"id",id},{"name",s.value("title").toString(c.value("name").toString()).simplified().left(150)},{"country",c.value("country").toString().left(2)},{"categories",categories},{"quality",quality.left(20)},{"labels",labels},{"url",url},{"origin","iptv-org"}});scores.insert(id,score);
    }
    for(const auto &v:preferred){const auto r=v.toObject();if(!best.contains(r.value("id").toString()))best.insert(r.value("id").toString(),r);}
    auto rows=best.values();std::sort(rows.begin(),rows.end(),[](const auto &a,const auto &b){return a.value("name").toString().compare(b.value("name").toString(),Qt::CaseInsensitive)<0;});QJsonArray result;for(const auto &r:rows.mid(0,20000))result.append(r);return result;
}
void IptvClient::fetch(const QString &name,std::function<void(QJsonArray)> done){
    QString base="https://iptv-org.github.io/api";
#ifdef THINKSMART_IPTV_TESTING
    const QUrl test(qEnvironmentVariable("THINKSMART_IPTV_TEST_API"));if(test.scheme()=="http"&&test.host()=="127.0.0.1")base=test.toString();
#endif
    QNetworkRequest request(QUrl(base+"/"+name+".json"));request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);request.setRawHeader("User-Agent","ThinkSmart-Home/1.0");
    auto reply=m_network.get(request);m_reply=reply;const int generation=m_generation;auto bytes=std::make_shared<QByteArray>();
    connect(reply,&QIODevice::readyRead,this,[reply,bytes](){*bytes+=reply->readAll();if(bytes->size()>16*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,generation,done=std::move(done)](){
        *bytes+=reply->readAll();reply->deleteLater();if(generation!=m_generation)return;m_reply=nullptr;
        QJsonParseError e;auto d=QJsonDocument::fromJson(*bytes,&e);
        if(reply->error()!=QNetworkReply::NoError||bytes->size()>16*1024*1024||e.error!=QJsonParseError::NoError||!d.isArray()||d.array().isEmpty()||d.array().size()>50000){finish(false);return;}done(d.array());
    });
}
void IptvClient::refresh(){
    if(!DeviceConfig::enabled("iptv")||m_busy)return;m_busy=true;if(m_storeHealthy)m_problem.clear();emit changed();
    fetch("channels",[this](QJsonArray channels){fetch("streams",[this,channels](QJsonArray streams){m_rows=normalize(channels,streams,m_seed);m_updated=QDateTime::currentSecsSinceEpoch();writeJson(cachePath(),{{"version",1},{"updated",double(m_updated)},{"channels",m_rows}});refreshModels();emit directoryChanged();finish(true);});});
}
void IptvClient::finish(bool success){m_busy=false;if(!success&&m_storeHealthy)m_problem="Directory unavailable · starter, cached and saved channels remain available.";emit changed();const auto pending=std::move(m_pending);m_pending.clear();for(const auto &q:pending)q.done(request(q.request));}
bool IptvClient::save(const QJsonArray &favourites,const QJsonObject &last){
    if(!m_storeHealthy)return false;
    if(!writeJson(storagePath(),{{"version",1},{"favourites",favourites},{"last",last}})){m_problem="Cannot save TV channels.";emit changed();return false;}
    m_saved=favourites;m_last=last;m_problem.clear();refreshModels();emit changed();return true;
}
QUrl IptvClient::selectHlsVariant(const QByteArray &playlist,const QUrl &base){
    if(playlist.size()>262144)return {};
    const auto text=QString::fromUtf8(playlist).trimmed();if(!text.startsWith("#EXTM3U"))return {};
    const auto lines=text.split('\n');if(lines.size()>4096)return {};
    QJsonObject attributes;bool waiting=false,master=false;QUrl best;qint64 bestArea=0,bestRate=0;
    const QRegularExpression attribute("([A-Z0-9-]+)=(\"[^\"]*\"|[^,]*)");
    for(auto line:lines){
        line=line.trimmed();
        if(line.startsWith("#EXT-X-STREAM-INF:")){
            master=waiting=true;attributes={};auto it=attribute.globalMatch(line.mid(18));
            while(it.hasNext()){const auto m=it.next();auto value=m.captured(2);if(value.startsWith('\"'))value=value.mid(1,value.size()-2);attributes.insert(m.captured(1),value);}
            continue;
        }
        if(!waiting||line.isEmpty()||line.startsWith('#'))continue;
        waiting=false;
        // This compatibility path selects one muxed rendition before libavformat
        // opens the master. Separate-audio groups need an explicit combined path.
        if(!attributes.value("AUDIO").toString().isEmpty())continue;
        const auto size=attributes.value("RESOLUTION").toString().split('x');if(size.size()!=2)continue;
        const int w=size[0].toInt(),h=size[1].toInt();if(w<=0||h<=0||w>1280||h>720)continue;
        const auto codecs=attributes.value("CODECS").toString();if(!codecs.contains("avc1")&&!codecs.contains("avc3"))continue;
        const auto url=base.resolved(QUrl(line));if(!validUrl(url.toString())||(base.scheme()=="https"&&url.scheme()!="https"))continue;
        const qint64 area=qint64(w)*h,rate=attributes.value("BANDWIDTH").toString().toLongLong();
        if(area>bestArea||(area==bestArea&&rate>bestRate)){best=url;bestArea=area;bestRate=rate;}
    }
    return master?best:(text.contains("#EXTINF:")?base:QUrl{});
}
void IptvClient::cancelPlayback(){
    ++m_playGeneration;const auto reply=m_playReply;m_playReply=nullptr;
    if(reply){reply->abort();reply->deleteLater();emit changed();}
}
void IptvClient::preparePlaylist(const QJsonObject &row){
    QUrl url(row.value("url").toString());
#ifdef THINKSMART_IPTV_TESTING
    const QUrl test(qEnvironmentVariable("THINKSMART_IPTV_TEST_PLAYLIST"));if(test.scheme()=="http"&&test.host()=="127.0.0.1")url=test;
#endif
    QNetworkRequest request(url);request.setTransferTimeout(10000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);request.setRawHeader("User-Agent","ThinkSmart-Home/1.0");
    auto reply=m_network.get(request);m_playReply=reply;const auto generation=m_playGeneration;auto bytes=std::make_shared<QByteArray>();emit changed();
    connect(reply,&QIODevice::readyRead,this,[reply,bytes](){*bytes+=reply->readAll();if(bytes->size()>262144)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,bytes,generation,row](){
        reply->deleteLater();if(generation!=m_playGeneration)return;m_playReply=nullptr;*bytes+=reply->readAll();
        const auto selected=reply->error()==QNetworkReply::NoError?selectHlsVariant(*bytes,reply->url()):QUrl{};
        if(selected.isEmpty()){m_problem="Cannot prepare this TV playlist. Please try again.";emit changed();return;}
        emit changed();emit playRequested(selected,row.value("name").toString(),softwareDecode(QUrl(row.value("url").toString())));
    });
}
bool IptvClient::play(const QString &id){
    if(!DeviceConfig::enabled("iptv"))return false;
    const auto row=resolve(id);if(row.isEmpty()){m_problem="Unknown TV channel. Refresh or choose a saved channel.";emit changed();return false;}
    cancelPlayback();if(!m_storeHealthy||!save(m_saved,row)){m_last=row;emit changed();}
    const QUrl url(row.value("url").toString());
    // The BBC UK Redge master hard-locked this device; a direct HD rendition
    // completed hardware decoding. Keep the stable master in saved channels and
    // resolve its current rendition at playback, preserving HD and B-frames.
    if(url.host().endsWith(".cdn-redge.media",Qt::CaseInsensitive)&&url.path().contains("/bbc_news/",Qt::CaseInsensitive)&&!QUrlQuery(url).hasQueryItem("videoId"))preparePlaylist(row);
    else emit playRequested(url,row.value("name").toString(),softwareDecode(url));
    return true;
}
bool IptvClient::favourite(const QString &id,bool saved){
    if(!DeviceConfig::enabled("iptv"))return false;auto rows=m_saved;for(int i=rows.size()-1;i>=0;--i)if(rows[i].toObject().value("id")==id)rows.removeAt(i);
    if(saved){const auto row=resolve(id);if(row.isEmpty()||rows.size()>=100){m_problem="Choose a known channel; at most 100 channels can be saved.";emit changed();return false;}rows.append(row);}return save(rows,m_last);
}
bool IptvClient::addCustom(const QString &name,const QString &url){
    if(!DeviceConfig::enabled("iptv")||name.trimmed().isEmpty()||name.size()>120||!validUrl(url)){m_problem="Enter a channel name and a direct HTTP(S) stream URL.";emit changed();return false;}
    if(m_saved.size()>=100){m_problem="At most 100 TV channels can be saved.";emit changed();return false;}
    auto rows=m_saved;rows.append(QJsonObject{{"id","custom-"+QUuid::createUuid().toString(QUuid::WithoutBraces)},{"name",name.trimmed()},{"url",url},{"origin","custom"},{"country",""},{"categories",QJsonArray{}},{"quality",""},{"labels",QJsonArray{}}});return save(rows,m_last);
}
QJsonObject IptvClient::request(const QJsonObject &r){
    if(!DeviceConfig::enabled("iptv"))return {{"ok",false},{"error","IPTV is disabled."}};
    const auto action=r.value("action").toString();QJsonObject result{{"ok",true},{"source","IPTV-org"},{"updated",double(m_updated)},{"stale",QDateTime::currentSecsSinceEpoch()-m_updated>86400},{"busy",m_busy},{"last",m_last.isEmpty()?QJsonObject{}:summary(m_last)},{"favouriteCount",m_saved.size()}};
    if(!m_problem.isEmpty())result.insert("warning",m_problem);
    if(action=="iptv.play"){if(!play(r.value("id").toString()))return {{"ok",false},{"error",m_problem}};result.insert("accepted",true);}
    else if(action=="iptv.save"||action=="iptv.remove"){if(!favourite(r.value("id").toString(),action=="iptv.save"))return {{"ok",false},{"error",m_problem}};}
    else if(action=="iptv.search"||action=="iptv.list"){auto filters=r;if(action=="iptv.list")filters.insert("saved",true);result.insert("channels",matching(filters,qBound(0,r.value("offset").toInt(),20000),qBound(1,r.value("limit").toInt(5),30)));}
    else if(action!="iptv.status")return {{"ok",false},{"error","Unknown IPTV action."}};
    if(action=="iptv.status")result.insert("favourites",QJsonArray::fromVariantList(favourites()));result.insert("preparing",preparing());return result;
}
void IptvClient::search(const QJsonObject &r,Done done){if(!DeviceConfig::enabled("iptv")||QDateTime::currentSecsSinceEpoch()-m_updated<86400){done(request(r));return;}m_pending.append({r,std::move(done)});refresh();}
