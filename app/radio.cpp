#include "radio.h"
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QUuid>
#include <QDateTime>
#include <QTimeZone>
namespace {
const QString parliamentApi="https://exposure.api.redbee.live/v2/customer/UKParliament/businessunit/ParliamentLive";
QString plain(QString text) { text.remove(QRegularExpression("<[^>]*>")); return text.replace("&amp;","&").replace("&nbsp;"," ").simplified(); }
}
RadioCatalog::RadioCatalog(QObject *parent):QObject(parent) {
    QFile file(qEnvironmentVariable("THINKSMART_RADIO_CATALOG","/etc/thinksmart/radio-catalog.json"));
    if(!file.open(QIODevice::ReadOnly)){file.setFileName(":/radio-catalog.json");file.open(QIODevice::ReadOnly);}
    if(file.size()<512*1024)m_stations=QJsonDocument::fromJson(file.readAll()).object().value("stations").toArray();
    // Keep the previously configured Radio 4 stream when migrating to the catalog.
    QFile legacy(qEnvironmentVariable("THINKSMART_STATIONS","/etc/thinksmart/stations.json"));
    if(legacy.open(QIODevice::ReadOnly)) {
        const auto url=QJsonDocument::fromJson(legacy.readAll()).object().value("radio4").toString();
        if(!url.isEmpty())for(int i=0;i<m_stations.size();i++){auto row=m_stations.at(i).toObject();if(row.value("id")=="radio4"){row.insert("url",url);m_stations.replace(i,row);}}
    }
}
RadioCatalog::~RadioCatalog(){ for(auto r:m_network.findChildren<QNetworkReply *>()){r->disconnect(this);r->abort();} }
QVariantList RadioCatalog::stations() const { return list().toVariantList(); }
QJsonArray RadioCatalog::list(const QString &genre,const QString &search) const {
    QJsonArray result;
    for(const auto &value:m_stations){
        auto row=value.toObject();const auto tags=row.value("genres").toArray();bool match=genre.isEmpty();
        QString haystack=row.value("name").toString()+" "+row.value("description").toString();
        for(const auto &tag:tags){haystack+=' '+tag.toString();if(tag.toString().compare(genre,Qt::CaseInsensitive)==0)match=true;}
        if(!match||(!search.isEmpty()&&!haystack.contains(search,Qt::CaseInsensitive)))continue;
        row.remove("url");row.remove("resolver");result.append(row);
    }
    return result;
}
QJsonObject RadioCatalog::snapshot() const { return {{"selectedId",m_selected},{"busy",m_busy},{"status",m_status}}; }
void RadioCatalog::cancel(){++m_serial;if(m_busy)m_status="Stopped · choose a station";m_busy=false;emit changed();}
void RadioCatalog::fail(const QString &message){m_busy=false;m_status=message;emit changed();}
bool RadioCatalog::knows(const QString &id)const{
    for(const auto &row:m_stations)if(row.toObject().value("id")==id)return true;
    return !m_directory.station(id).isEmpty();
}
bool RadioCatalog::random(const QString &genre){
    QStringList choices;for(const auto &value:list(genre)){auto row=value.toObject();if(row.value("id").toString()!=m_selected)choices<<row.value("id").toString();}
    if(choices.isEmpty())for(const auto &value:list(genre))choices<<value.toObject().value("id").toString();
    if(choices.isEmpty()){fail("No stations match that genre.");return false;}
    return play(choices.at(QRandomGenerator::global()->bounded(choices.size())));
}
bool RadioCatalog::play(const QString &id){
    QJsonObject station;for(const auto &value:m_stations)if(value.toObject().value("id")==id){station=value.toObject();break;}
    if(station.isEmpty())station=m_directory.station(id);
    if(station.isEmpty()){fail("Unknown station ID. Use radio.list for favourites or radio.search for global stations, then play a returned id.");return false;}
    emit interruptPlayback();++m_serial;m_selected=id;
    const auto resolver=station.value("resolver").toString();
    if(resolver=="commons"||resolver=="lords") {
        m_busy=true;m_status="Finding the latest "+station.value("name").toString()+" sitting…";emit changed();
        const QString house=resolver=="commons"?"Commons":"Lords";
        request(QUrl("https://parliamentlive.tv/"+house),[this,house](const QByteArray &body){probe(parliamentEvents(QString::fromUtf8(body),house),0,house);});
    }else {
        const QUrl url(station.value("url").toString());
        if(!url.isValid()||(url.scheme()!="http"&&url.scheme()!="https")){fail("Station address is unavailable.");return false;}
        m_busy=false;m_status="Selected · "+station.value("name").toString();emit changed();
        emit playRequested(id,url,station.value("name").toString(),station.value("description").toString());
        if(id.startsWith("rb:"))m_directory.clicked(id);
    }
    return true;
}
void RadioCatalog::request(const QUrl &url,std::function<void(const QByteArray &)> done,const QJsonObject &post,const QByteArray &bearer){
    const int serial=m_serial;
    QNetworkRequest request(url);request.setTransferTimeout(15000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent","ThinkSmart Home personal Parliament audio");
    if(!bearer.isEmpty())request.setRawHeader("Authorization","Bearer "+bearer);
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto reply=post.isEmpty()?m_network.get(request):m_network.post(request,QJsonDocument(post).toJson(QJsonDocument::Compact));
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 size,qint64 total){if(size>2*1024*1024||total>2*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,done,serial](){
        const auto body=reply->readAll();const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();if(serial!=m_serial)return;
        if(reply->error()!=QNetworkReply::NoError||status!=200||body.size()>2*1024*1024){fail("Parliament audio is unavailable. Try again later.");return;}
        done(body);
    });
}
QJsonArray RadioCatalog::parliamentEvents(const QString &html,const QString &house){
    const QRegularExpression anchors("<a[^>]+href=\"(?:https://parliamentlive.tv)?/Event/Index/([a-fA-F0-9-]{36})\"[^>]*>(.*?)</a>",QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression paragraphs("<p[^>]*>(.*?)</p>",QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression heading("<h[35][^>]*>(.*?)</h[35]>",QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpression datePattern("([0-9]{1,2}) ([A-Za-z]+) ([0-9]{4})");
    const QDate today=QDateTime::currentDateTime(QTimeZone("Europe/London")).date();
    QJsonArray result;QSet<QString> seen;auto matches=anchors.globalMatch(html);
    while(matches.hasNext()){
        const auto match=matches.next();const QString body=match.captured(2);const auto title=plain(paragraphs.match(body).captured(1));
        if(title!="House of "+house||seen.contains(match.captured(1)))continue;
        const QString label=plain(heading.match(body).captured(1));const auto date=datePattern.match(label);
        if(date.hasMatch()&&QDate::fromString(date.captured(0),"d MMMM yyyy")>today)continue;
        seen.insert(match.captured(1));result.append(QJsonObject{{"id",match.captured(1)},{"date",label},{"title",title}});
        if(result.size()>=8)break;
    }
    return result;
}
void RadioCatalog::probe(const QJsonArray &events,int index,const QString &house){
    if(index>=events.size()){fail("No playable "+house+" sitting was found. Live audio is available while sitting.");return;}
    const auto event=events.at(index).toObject();
    request(QUrl("https://videoplayback.parliamentlive.tv/Player/Index/"+event.value("id").toString()+"?audioOnly=True"),[this,events,index,house,event](const QByteArray &body){
        const QRegularExpression expression("stateChanged[(]'[^']*', '[^']*', '[^']*', '([^']*)'");
        const QString state=expression.match(QString::fromUtf8(body)).captured(1);
        if(state=="LIVE"||state=="ARCHIVE")resolve(event,state,house);else probe(events,index+1,house);
    });
}
void RadioCatalog::resolve(const QJsonObject &event,const QString &state,const QString &house){
    const QString part=state=="LIVE"?"Live":"Recorded";
    request(QUrl("https://videoplayback.parliamentlive.tv/Player/"+part+"/"+event.value("id").toString()+"?audioOnly=True&autoStart=True"),[this,event,state,house](const QByteArray &body){
        const QString asset=QRegularExpression("source:\\s*\"([^\"]+)\"").match(QString::fromUtf8(body)).captured(1);
        if(!QRegularExpression("^[A-Za-z0-9_-]{1,150}$").match(asset).hasMatch()){fail("Parliament player format changed. Audio source could not be found.");return;}
        request(QUrl(parliamentApi+"/auth/anonymous"),[this,asset,event,state,house](const QByteArray &bytes){
            const auto auth=QJsonDocument::fromJson(bytes).object().value("sessionToken").toString().toUtf8();
            if(auth.isEmpty()){fail("Parliament playback session is unavailable.");return;}
            request(QUrl(parliamentApi+"/entitlement/"+asset+"/play?audioOnly=true"),[this,event,state,house](const QByteArray &bytes){
                const auto data=QJsonDocument::fromJson(bytes).object();QUrl url;
                for(const auto &format:data.value("formats").toArray())if(format.toObject().value("format")=="HLS"){url=QUrl(format.toObject().value("mediaLocator").toString());break;}
                if(!url.isValid()||url.scheme()!="https"){fail("No compatible Parliament audio stream is available.");return;}
                m_busy=false;m_status=(state=="LIVE"?"Live sitting · ":"Recorded sitting · ")+event.value("date").toString();emit changed();
                emit playRequested(m_selected,url,"House of "+house,m_status);
            },{},auth);
        },{{"deviceId",QUuid::createUuid().toString(QUuid::Id128)},{"device",QJsonObject{{"type","WEB"},{"name","ThinkSmart Home"}}}});
    });
}
