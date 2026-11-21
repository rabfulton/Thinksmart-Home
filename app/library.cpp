#include "deviceconfig.h"
#include "library.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QSet>
#include <algorithm>
namespace {
QString clipped(const QJsonValue &value){return value.toString().left(256);}
QString encoded(const QString &value){return QString::fromLatin1(QUrl::toPercentEncoding(value));}
QVariantMap normalizeItem(const QJsonObject &o,bool music,bool songs){
    QString id=music?o.value("id").toString():o.value("Id").toString();if(id.isEmpty()||id.size()>256)return {};
    QString type=music?(songs?"song":"album"):o.value("Type").toString().toLower();
    QString name=music?o.value("title").toString(o.value("name").toString()):o.value("Name").toString();
    QString subtitle=music?o.value("artist").toString():o.value("SeriesName").toString(o.value("ProductionYear").isDouble()?QString::number(o.value("ProductionYear").toInt()):type);
    double seconds=music?o.value("duration").toDouble():o.value("RunTimeTicks").toDouble()/10000000.0;
    double resume=music?0:o.value("UserData").toObject().value("PlaybackPositionTicks").toDouble()/10000000.0;
    QVariantMap item{{"id",id},{"title",name.left(256)},{"subtitle",subtitle.left(256)},{"type",type},{"duration",seconds},{"resume",resume},
        {"overview",o.value("Overview").toString().left(6000)},{"genres",o.value("Genres").toArray().toVariantList()},
        {"year",o.value("ProductionYear").toInt()},{"watched",o.value("UserData").toObject().value("Played").toBool()}};
    const QString tag=o.value("ImageTags").toObject().value("Primary").toString();
    item.insert("poster",tag.isEmpty()?QString():"image://jellyfin/"+id+"/Primary?tag="+QString::fromLatin1(QUrl::toPercentEncoding(tag)));
    return item;
}
}
void LibraryItemsModel::replace(const QVariantList &items){
    bool same=items.size()==m_items.size();if(same)for(int i=0;i<items.size();i++)if(items[i].toMap().value("id")!=m_items[i].toMap().value("id")){same=false;break;}
    if(!same){beginResetModel();m_items=items;endResetModel();return;}
    for(int i=0;i<items.size();i++)if(items[i]!=m_items[i]){m_items[i]=items[i];emit dataChanged(index(i),index(i),{Qt::UserRole+1});}
}
void LibraryItemsModel::append(const QVariantList &items){if(items.isEmpty())return;beginInsertRows({},m_items.size(),m_items.size()+items.size()-1);m_items+=items;endInsertRows();}
bool MediaLibrary::sameOrigin(const QUrl &a,const QUrl &b){return a.scheme()==b.scheme()&&a.host().compare(b.host(),Qt::CaseInsensitive)==0&&a.port(a.scheme()=="https"?443:80)==b.port(b.scheme()=="https"?443:80);}
QUrl MediaLibrary::authenticatedSubsonicUrl(const QUrl &base,const QJsonObject &credentials,const QString &method,QUrlQuery query){
    query.addQueryItem("v","1.16.1");query.addQueryItem("c","ThinkSmart");query.addQueryItem("f","json");
    const QString key=credentials.value("apiKey").toString();
    if(!key.isEmpty())query.addQueryItem("apiKey",key);
    else{QString salt=QUuid::createUuid().toString(QUuid::Id128);query.addQueryItem("u",credentials.value("username").toString());query.addQueryItem("s",salt);query.addQueryItem("t",QString::fromLatin1(QCryptographicHash::hash((credentials.value("password").toString()+salt).toUtf8(),QCryptographicHash::Md5).toHex()));}
    QUrl url=base.resolved(QUrl("rest/"+method+".view"));url.setQuery(query.query(QUrl::FullyEncoded).replace("+","%2B"));return url;
}
MediaLibrary::MediaLibrary(const QString &provider,QObject *parent):QObject(parent),m_provider(provider){
    if(!DeviceConfig::enabled(provider))return;
    m_title=provider=="navidrome"?"Music":"Films & series";
    QFile file(qEnvironmentVariable("THINKSMART_MEDIA_CONFIG",QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/media.json"));
    if(!file.exists()){m_problem="Add a server connection when it is available.";return;}
    if(file.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)){m_problem="Media credentials need private file permissions (600).";return;}
    if(!file.open(QIODevice::ReadOnly)||file.size()>65536){m_problem="Cannot read media connection settings.";return;}
    m_config=QJsonDocument::fromJson(file.readAll()).object().value(provider).toObject();m_base=QUrl(m_config.value("url").toString().trimmed());
    if(!m_base.isValid()||m_base.host().isEmpty()||!m_base.userInfo().isEmpty()||m_base.hasQuery()||m_base.hasFragment()||(m_base.scheme()!="https"&&m_base.scheme()!="http")){m_problem="A valid server address is needed.";return;}
    if(m_base.scheme()=="http"&&!m_config.value("allowHttp").toBool()){m_problem="Use HTTPS, or explicitly enable HTTP for your private network.";return;}
    QString path=m_base.path();if(!path.endsWith('/'))path+='/';m_base.setPath(path);
    const QString token=m_config.value("accessToken").toString();
    if(token.contains('\n')||token.contains('\r')||token.contains('"')||token.contains(',')){m_problem="Invalid media access token.";return;}
    m_configured=provider=="navidrome"?(!m_config.value("apiKey").toString().isEmpty()||(!m_config.value("username").toString().isEmpty()&&!m_config.value("password").toString().isEmpty())):!token.isEmpty();
    if(!m_configured){m_problem="Server credentials are not configured.";return;}
    m_deviceId=m_settings.value("media/deviceId").toString();if(m_deviceId.isEmpty()){m_deviceId=QUuid::createUuid().toString(QUuid::Id128);m_settings.setValue("media/deviceId",m_deviceId);}
    loadCache();
    if(m_provider=="navidrome")loadQueue();
}
MediaLibrary::~MediaLibrary(){for(auto r:m_network.findChildren<QNetworkReply *>()){r->disconnect(this);r->abort();}}
QString MediaLibrary::status()const{if(!configured())return "Server not configured";if(m_busy)return "Loading…";if(!m_problem.isEmpty())return m_cached?"Server unavailable · cached library":"Server unavailable";if(m_cached)return "Cached library · "+QDateTime::fromSecsSinceEpoch(m_savedAt).toLocalTime().toString("d MMM, HH:mm");return QString::number(m_items.size())+" items";}
QUrl MediaLibrary::endpoint(const QString &path,QUrlQuery query)const{QUrl u=m_base.resolved(QUrl(path));u.setQuery(query.query(QUrl::FullyEncoded).replace("+","%2B"));return u;}
QByteArray MediaLibrary::authorization(bool player)const{
    QString header="MediaBrowser Token=\""+m_config.value("accessToken").toString()+"\"";
    if(!player)header+=", Client=\"ThinkSmart\", Device=\"ThinkSmart View\", DeviceId=\""+m_deviceId+"\", Version=\"0.2\"";
    return header.toUtf8();
}
void MediaLibrary::request(const QUrl &url,const QJsonObject &body,bool post,std::function<void(const QJsonObject &)> done,bool quiet){
    if(!sameOrigin(url,m_base)){m_problem="The server returned an unexpected media address.";emit changed();return;}
    if(!quiet){m_busy=true;m_problem.clear();emit changed();}
    QNetworkRequest request(url);request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept","application/json");if(m_provider=="jellyfin")request.setRawHeader("Authorization",authorization());
    request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    auto reply=post?m_network.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact)):m_network.get(request);
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 received,qint64 total){if(received>4*1024*1024||total>4*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,done,quiet](){
        if(!quiet)m_busy=false;
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes=reply->readAll();
        if(reply->error()!=QNetworkReply::NoError||http<200||http>=300||bytes.size()>4*1024*1024){
            if(!quiet){m_loadingMore=false;}else if(m_provider=="navidrome")m_filtersRequested=false;
            if(!quiet)m_problem=http==401||http==403?"Server access denied. Check the connection credentials.":"Cannot reach the server. Any cached library is kept.";
        }else if(quiet&&bytes.trimmed().isEmpty()){done({});}
        else{
            QJsonParseError error;QJsonObject data=QJsonDocument::fromJson(bytes,&error).object();
            if(error.error!=QJsonParseError::NoError){if(!quiet)m_problem="Invalid server response.";}
            else{
                if(m_provider=="navidrome")data=data.value("subsonic-response").toObject();
                if(m_provider=="navidrome"&&data.value("status")!="ok"){if(quiet)m_filtersRequested=false;else m_loadingMore=false;if(!quiet)m_problem=QString("Music server rejected the request (code %1).").arg(data.value("error").toObject().value("code").toInt());}
                else done(data);
            }
        }
        reply->deleteLater();if(!quiet)emit changed();
    });
}
QVariantMap MediaLibrary::route()const{return {{"parent",m_parent},{"search",m_search},{"title",m_title},{"offset",m_offset},{"genre",m_genre},{"section",m_section},{"library",m_library},{"decade",m_decade}};}
void MediaLibrary::restoreRoute(const QVariantMap &r){m_parent=r.value("parent").toString();m_search=r.value("search").toString();m_title=r.value("title").toString();m_offset=r.value("offset").toInt();m_genre=r.value("genre").toString();m_section=r.value("section","all").toString();m_library=r.value("library").toString();m_decade=r.value("decade").toInt();}
void MediaLibrary::refresh(){if(!m_configured||m_busy)return;m_loadingMore=false;m_offset=0;if(m_provider=="jellyfin"&&m_config.value("userId").toString().isEmpty()){request(endpoint("Users/Me"),{},false,[this](const QJsonObject &user){QString id=user.value("Id").toString();if(id.isEmpty()){m_problem="A Jellyfin user ID is needed for this token.";return;}m_config.insert("userId",id);browse();});}else browse();}
void MediaLibrary::browse(){
    if(!m_loadingMore){const int offset=m_offset;loadCache();m_offset=offset;}
    loadFilters();
    if(m_provider=="navidrome"){
        QUrlQuery query;QString method;
        if(!m_parent.isEmpty()){method="getAlbum";query.addQueryItem("id",m_parent);}
        else if(!m_search.isEmpty()){method="search3";query.addQueryItem("query",m_search);query.addQueryItem("songCount","60");query.addQueryItem("songOffset",QString::number(m_offset));query.addQueryItem("albumCount","0");query.addQueryItem("artistCount","0");}
        else{
            method="getAlbumList2";query.addQueryItem("type",m_section=="all"?"newest":m_section);query.addQueryItem("size","60");query.addQueryItem("offset",QString::number(m_offset));
            if(m_section=="byGenre")query.addQueryItem("genre",m_genre);
            if(m_section=="byYear"){query.addQueryItem("fromYear",QString::number(m_decade+9));query.addQueryItem("toYear",QString::number(m_decade));}
        }
        const int requestedOffset=m_offset;
        request(authenticatedSubsonicUrl(m_base,m_config,method,query),{},false,[this,requestedOffset](const QJsonObject &response){
            const bool songs=!m_parent.isEmpty()||!m_search.isEmpty();QJsonArray list;
            if(!m_parent.isEmpty())list=response.value("album").toObject().value("song").toArray();
            else if(!m_search.isEmpty())list=response.value("searchResult3").toObject().value("song").toArray();
            else list=response.value("albumList2").toObject().value("album").toArray();
            QVariantList items;QSet<QString> seen;
            if(m_loadingMore)for(const auto &item:m_items)seen.insert(item.toMap().value("id").toString());
            for(const auto &r:list){auto item=normalizeItem(r.toObject(),true,songs);const auto id=item.value("id").toString();if(!item.isEmpty()&&!seen.contains(id)){items.append(item);seen.insert(id);}}
            const bool append=m_loadingMore;m_loadingMore=false;
            m_offset=requestedOffset+list.size();
            m_total=inAlbum()||list.size()<60||items.isEmpty()?(append?m_items.size():0)+items.size():-1;
            if(append){m_items+=items;m_itemModel.append(items);storeItems(m_items);}else storeItems(items);
        });
    }else{
        QUrlQuery query;query.addQueryItem("Limit","60");query.addQueryItem("StartIndex",QString::number(m_offset));query.addQueryItem("SortBy","SortName");query.addQueryItem("SortOrder",m_section=="recent"?"Descending":"Ascending");
        query.addQueryItem("Fields","Overview,Genres,MediaStreams,UserData");
        if(m_section=="recent"){query.removeAllQueryItems("SortBy");query.addQueryItem("SortBy","DateCreated");}
        if(m_section=="resume")query.addQueryItem("IsResumable","true");
        if(m_parent.isEmpty()&&m_decade>0){QStringList years;for(int y=m_decade;y<m_decade+10;y++)years.append(QString::number(y));query.addQueryItem("Years",years.join(","));}
        if(m_parent.isEmpty()&&!m_genre.isEmpty())query.addQueryItem("Genres",m_genre);
        if(m_parent.isEmpty()){query.addQueryItem("IncludeItemTypes",m_section=="movies"?"Movie":(m_section=="series"?"Series":(m_section=="collections"?"BoxSet":"Movie,Series")));query.addQueryItem("Recursive","true");if(!m_library.isEmpty())query.addQueryItem("ParentId",m_library);}else query.addQueryItem("ParentId",m_parent);
        if(!m_search.isEmpty())query.addQueryItem("SearchTerm",m_search);
        request(endpoint("Users/"+encoded(m_config.value("userId").toString())+"/Items",query),{},false,[this](const QJsonObject &response){m_total=response.value("TotalRecordCount").toInt(-1);QVariantList items;for(const auto &r:response.value("Items").toArray()){auto item=normalizeItem(r.toObject(),false,false);if(!item.isEmpty())items.append(item);}if(m_loadingMore){m_items+=items;m_itemModel.append(items);m_loadingMore=false;storeItems(m_items);}else storeItems(items);});
    }
}
void MediaLibrary::open(int index){
    if(m_busy||index<0||index>=m_items.size())return;const auto item=m_items.at(index).toMap();const auto type=item.value("type").toString();
    if(type=="album"||type=="series"||type=="season"||type=="folder"||type=="collectionfolder"||type=="boxset"){
        m_history.append(route());m_parent=item.value("id").toString();m_search.clear();m_offset=0;m_title=item.value("title").toString();refresh();
    }else if(type=="song"){enqueueTracks({item});}
    else if(type=="movie"||type=="episode")play(item);
}
void MediaLibrary::back(){if(m_busy||m_history.isEmpty())return;restoreRoute(m_history.takeLast().toMap());refresh();}
void MediaLibrary::page(int delta){if(m_busy||(!canNext()&&delta>0)||(!canPrevious()&&delta<0))return;m_offset=qMax(0,m_offset+delta*60);refresh();}
void MediaLibrary::search(const QString &text){if(m_busy)return;m_history.append(route());m_parent.clear();m_search=text.trimmed().left(200);if(m_provider=="navidrome"&&!m_search.isEmpty()){m_section="all";m_genre.clear();m_decade=0;}m_title=m_search.isEmpty()?(m_provider=="navidrome"?"Music":"Films & series"):"Search: "+m_search;m_offset=0;refresh();}
void MediaLibrary::advance(int delta){
    if(m_provider!="navidrome"||!configured()||m_queue.isEmpty())return;
    const int index=m_queueIndex<0?0:m_queueIndex+delta;
    if(index>=0&&index<m_queue.size())playQueue(index);
}
void MediaLibrary::replay(){if(m_provider=="navidrome")playQueue();else if(!m_lastItem.isEmpty()&&!m_busy)play(m_lastItem);}
QString MediaLibrary::cachePath()const{
    const QString identity=m_provider+m_base.toString()+m_config.value("username").toString()+m_config.value("userId").toString()+m_config.value("apiKey").toString()+m_parent+m_search+m_genre+m_section+m_library+QString::number(m_decade)+(m_provider=="jellyfin"?QString():QString("0"));
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/library-"+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex())+".json";
}
void MediaLibrary::loadCache(){
    m_items.clear();m_itemModel.replace(m_items);m_cached=false;m_savedAt=0;m_total=-1;
    QFile file(cachePath());if(!file.open(QIODevice::ReadOnly)||file.size()>8*1024*1024)return;
    const auto o=QJsonDocument::fromJson(file.readAll()).object();if(o.value("version").toInt()!=1)return;
    for(const auto &r:o.value("items").toArray()){auto item=r.toObject();if(item.value("id").isString()&&item.value("title").isString())m_items.append(item.toVariantMap());}
    m_itemModel.replace(m_items);m_total=o.value("total").toInt(-1);
    if(m_provider=="navidrome"){
        if(m_genres.isEmpty())m_genres=o.value("genres").toArray().toVariantList();
        // The next page offset is independent of how many duplicate IDs were skipped.
        m_offset=o.value("nextOffset").toInt(m_items.size());
    }
    m_savedAt=o.value("savedAt").toVariant().toLongLong();m_cached=!m_items.isEmpty();emit changed();
}
void MediaLibrary::storeItems(const QVariantList &items){
    m_items=items;m_itemModel.replace(m_items);m_cached=false;m_savedAt=QDateTime::currentSecsSinceEpoch();
    const QString directory=QFileInfo(cachePath()).absolutePath();QDir().mkpath(directory);QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"total",m_total},{"savedAt",m_savedAt},{"items",QJsonArray::fromVariantList(items)},{"nextOffset",m_offset},{"genres",QJsonArray::fromVariantList(m_genres)}}).toJson(QJsonDocument::Compact);
    QSaveFile file(cachePath());if(bytes.size()<=8*1024*1024&&file.open(QIODevice::WriteOnly)){file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(bytes);file.commit();}emit changed();
}
QJsonObject MediaLibrary::videoProfile(){
    QJsonArray conditions;
    for(const auto &condition:QList<QPair<QString,QString>>{{"Width","1920"},{"Height","1080"},{"VideoBitDepth","8"},{"VideoFramerate","30"}})
        conditions.append(QJsonObject{{"Condition","LessThanEqual"},{"Property",condition.first},{"Value",condition.second},{"IsRequired",true}});
    return {{"Name","ThinkSmart View"},{"MaxStreamingBitrate",40000000},{"MaxStaticBitrate",100000000},
        {"DirectPlayProfiles",QJsonArray{QJsonObject{{"Container","mp4,m4v,mkv,mov,ts,mpegts"},{"VideoCodec","h264,hevc"},{"AudioCodec","aac,mp3,flac,opus,vorbis,ac3,eac3,dts,truehd"},{"Type","Video"}}}},
        {"TranscodingProfiles",QJsonArray{QJsonObject{{"Container","ts"},{"Protocol","hls"},{"VideoCodec","h264"},{"AudioCodec","aac"},{"Type","Video"},{"Context","Streaming"},{"MaxAudioChannels","2"},{"MinSegments",1},{"BreakOnNonKeyFrames",true}}}},
        {"CodecProfiles",QJsonArray{QJsonObject{{"Type","Video"},{"Codec","h264,hevc"},{"Conditions",conditions}}}},
        {"SubtitleProfiles",QJsonArray{QJsonObject{{"Format","srt"},{"Method","External"}},QJsonObject{{"Format","ass"},{"Method","Embed"}},QJsonObject{{"Format","subrip"},{"Method","Embed"}}}}};
}
void MediaLibrary::play(const QVariantMap &item){
    if(!configured()||(m_provider!="navidrome"&&m_busy))return;m_lastItem=item;
    if(m_provider=="navidrome"){
        QUrlQuery query;query.addQueryItem("id",item.value("id").toString());
        const QUrl url=authenticatedSubsonicUrl(m_base,m_config,"stream",query);
        m_queuePlaybackKey=QUuid::createUuid().toString(QUuid::Id128);
        emit playRequested(m_provider,url,item.value("title").toString(),item.value("subtitle").toString(),{},false,m_queuePlaybackKey,0);return;
    }
    const QString id=item.value("id").toString();
    double start=qMax(0.0,item.value("resume").toDouble());const double duration=item.value("duration").toDouble();if(duration>0&&start>duration-10)start=0;
    QJsonObject body{{"UserId",m_config.value("userId")},{"StartTimeTicks",qint64(start*10000000)},
        {"IsPlayback",true},{"EnableDirectPlay",true},{"EnableDirectStream",true},{"EnableTranscoding",true},
        {"MaxStreamingBitrate",40000000},{"MaxWidth",1920},{"MaxHeight",1080},{"MaxFramerate",30},{"SubtitleStreamIndex",-1},{"DeviceProfile",videoProfile()}};
    request(endpoint("Items/"+encoded(id)+"/PlaybackInfo"),body,true,[this,item,start,id](const QJsonObject &response){
        QJsonObject selected;
        for(const auto &value:response.value("MediaSources").toArray()){
            auto source=value.toObject();if(source.value("SupportsDirectPlay").toBool()){selected=source;break;}
            if(selected.isEmpty()&&!source.value("TranscodingUrl").toString().isEmpty())selected=source;
        }
        if(selected.isEmpty()){m_problem="The server did not provide a compatible video stream.";return;}
        const bool direct=selected.value("SupportsDirectPlay").toBool();
        QUrl url;
        if(direct){QUrlQuery query;query.addQueryItem("Static","true");query.addQueryItem("MediaSourceId",selected.value("Id").toString());url=endpoint("Videos/"+encoded(id)+"/stream",query);}
        else{
            QString path=selected.value("TranscodingUrl").toString();QUrl candidate(path);
            if(candidate.isRelative()){
                if(path.startsWith(m_base.path())&&m_base.path()!="/"){url=m_base;url.setPath("/");url=url.resolved(candidate);}
                else{if(path.startsWith('/'))path.remove(0,1);url=m_base.resolved(QUrl(path));}
            }else url=candidate;
            QUrlQuery query(url);for(const auto &name:{"api_key","apiKey","access_token","token"})query.removeAllQueryItems(name);
            if(start>0){query.removeAllQueryItems("StartTimeTicks");query.addQueryItem("StartTimeTicks",QString::number(qint64(start*10000000)));}url.setQuery(query);
        }
        if(!sameOrigin(url,m_base)){m_problem="The server returned a media address on a different host.";return;}
        const QString key=QUuid::createUuid().toString(QUuid::Id128);
        QJsonObject session{{"ItemId",id},{"MediaSourceId",selected.value("Id")},{"PlaySessionId",response.value("PlaySessionId")},
            {"PlayMethod",direct?"DirectPlay":"Transcode"},{"CanSeek",true},{"offset",direct?0:start}};
        m_sessions.insert(key,session);m_currentSession=key;
        emit playRequested(m_provider,url,item.value("title").toString(),item.value("subtitle").toString(),
            {"Authorization: "+QString::fromUtf8(authorization(true))},true,key,direct?start:0);
    });
}
void MediaLibrary::report(const QString &key,const QString &event,double position,bool paused){
    if(m_provider=="navidrome")return;
    if(m_provider!="jellyfin"||!m_sessions.contains(key))return;
    auto body=m_sessions.value(key);const double offset=body.take("offset").toDouble();
    body.insert("PositionTicks",qint64(qMax(0.0,position+offset)*10000000));body.insert("IsPaused",paused);
    QString path=event=="start"?"Sessions/Playing":(event=="stopped"?"Sessions/Playing/Stopped":"Sessions/Playing/Progress");
    request(endpoint(path),body,true,[](const QJsonObject &){},true);
    if(event=="stopped"&&body.value("PlayMethod")=="Transcode"&&!body.value("PlaySessionId").toString().isEmpty()){
        QUrlQuery query;query.addQueryItem("deviceId",m_deviceId);query.addQueryItem("playSessionId",body.value("PlaySessionId").toString());
        QNetworkRequest cleanup(endpoint("Videos/ActiveEncodings",query));cleanup.setTransferTimeout(10000);cleanup.setRawHeader("Authorization",authorization());cleanup.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
        auto *reply=m_network.deleteResource(cleanup);connect(reply,&QNetworkReply::finished,reply,&QObject::deleteLater);
    }
    if(event=="stopped"){
        const auto id=body.value("ItemId").toString();double resume=qMax(0.,position+offset);
        if(m_lastItem.value("id")==id){const double duration=m_lastItem.value("duration").toDouble();if(duration>0&&resume>duration-10)resume=0;m_lastItem["resume"]=resume;}
        if(m_detail.value("id")==id)m_detail["resume"]=resume;
        for(int i=0;i<m_items.size();i++){auto item=m_items[i].toMap();if(item.value("id")==id){item["resume"]=resume;m_items[i]=item;}}
        m_sessions.remove(key);storeItems(m_items);
    }
}

QJsonObject MediaLibrary::imageSettings()const{return m_configured&&m_provider=="jellyfin"?QJsonObject{{"url",m_base.toString()},{"accessToken",m_config.value("accessToken")}}:QJsonObject{};}
void MediaLibrary::loadFilters(){
    if(m_filtersRequested||!m_configured)return;
    if(m_provider=="navidrome"){
        m_filtersRequested=true;
        request(authenticatedSubsonicUrl(m_base,m_config,"getGenres"),{},false,[this](const QJsonObject &data){
            QVariantList genres;QSet<QString> seen;
            for(const auto &v:data.value("genres").toObject().value("genre").toArray()){
                const auto o=v.toObject();const QString name=o.value("value").toString().trimmed().left(256);
                if(name.isEmpty()||seen.contains(name)||o.value("albumCount").toInt(1)==0)continue;
                seen.insert(name);genres.append(QVariantMap{{"id",name},{"name",name}});
            }
            std::sort(genres.begin(),genres.end(),[](const QVariant &a,const QVariant &b){return QString::localeAwareCompare(a.toMap().value("name").toString(),b.toMap().value("name").toString())<0;});
            m_genres=genres;emit changed();
        },true);return;
    }
    if(m_config.value("userId").toString().isEmpty())return;m_filtersRequested=true;
    QUrlQuery query;query.addQueryItem("UserId",m_config.value("userId").toString());query.addQueryItem("SortBy","SortName");
    request(endpoint("Genres",query),{},false,[this](const QJsonObject &data){m_genres.clear();for(auto v:data.value("Items").toArray()){auto o=v.toObject();m_genres.append(QVariantMap{{"id",o.value("Id").toString()},{"name",o.value("Name").toString()}});}emit changed();},true);
    request(endpoint("Users/"+encoded(m_config.value("userId").toString())+"/Views"),{},false,[this](const QJsonObject &data){m_libraries.clear();for(auto v:data.value("Items").toArray()){auto o=v.toObject();m_libraries.append(QVariantMap{{"id",o.value("Id").toString()},{"name",o.value("Name").toString()}});}emit changed();},true);
}
void MediaLibrary::setGenre(const QString &value){if(m_busy||value==m_genre)return;m_genre=value.left(128);m_offset=0;m_parent.clear();m_history.clear();m_detail.clear();refresh();}
void MediaLibrary::setSection(const QString &value){if(m_busy||value==m_section||!QStringList{"all","movies","series","resume","recent","collections"}.contains(value))return;m_section=value;m_offset=0;m_parent.clear();m_search.clear();m_history.clear();m_detail.clear();refresh();}
void MediaLibrary::setLibrary(const QString &id){if(m_busy)return;m_library=id.left(256);m_parent.clear();m_history.clear();m_offset=0;refresh();}
void MediaLibrary::inspect(int index){if(m_busy||index<0||index>=m_items.size())return;auto item=m_items[index].toMap();if(QStringList{"series","season","folder","collectionfolder","boxset"}.contains(item.value("type").toString()))open(index);else{m_detail=item;emit changed();}}
void MediaLibrary::closeDetails(){m_detail.clear();emit changed();}
void MediaLibrary::playSelection(bool restart){if(m_detail.isEmpty()||m_busy)return;auto item=m_detail;if(restart)item["resume"]=0;play(item);}

QJsonObject MediaLibrary::snapshot()const{
    QJsonArray items;for(auto v:m_items){auto item=v.toMap();items.append(QJsonObject{{"id",item.value("id").toString()},{"title",item.value("title").toString()},{"type",item.value("type").toString()}});}
    return {{"playMethod",m_sessions.value(m_currentSession).value("PlayMethod")},{"configured",m_configured},{"busy",m_busy},{"cached",m_cached},{"total",m_total},{"genre",m_genre},{"section",m_section},{"decade",m_decade},{"items",items},{"detail",QJsonObject::fromVariantMap(m_detail)},{"genres",QJsonArray::fromVariantList(m_genres)},{"error",m_problem}};
}

void MediaLibrary::setDecade(int decade){if(m_busy||decade==m_decade||(decade!=0&&(decade<1900||decade>2100||decade%10)))return;m_decade=decade;m_offset=0;m_parent.clear();m_history.clear();m_detail.clear();refresh();}
void MediaLibrary::loadMore(){if(m_busy||m_loadingMore||!canNext())return;if(m_provider=="jellyfin")m_offset=m_items.size();m_loadingMore=true;browse();}
void MediaLibrary::setMusicBrowse(const QString &section,const QString &value){
    if(m_provider!="navidrome"||!m_configured||m_busy||!QStringList{"all","alphabeticalByName","alphabeticalByArtist","recent","frequent","highest","starred","byGenre","byYear"}.contains(section))return;
    const QString genre=section=="byGenre"?value.left(256):QString();const int decade=section=="byYear"?value.toInt():0;
    if(section=="byGenre"&&genre.isEmpty())return;
    if(section=="byYear"&&(decade<1900||decade>2100||decade%10))return;
    m_section=section;m_genre=genre;m_decade=decade;m_parent.clear();m_search.clear();m_history.clear();m_title="Music";refresh();
}

namespace {
QString mediaConfigPath(){return qEnvironmentVariable("THINKSMART_MEDIA_CONFIG",QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/media.json");}
bool readMediaSettings(QJsonObject &settings){
    QFile file(mediaConfigPath());
    if(!file.exists()){settings={};return true;}
    if(file.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)||!file.open(QIODevice::ReadOnly)||file.size()>65536)return false;
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject())return false;
    settings=doc.object();return true;
}
bool safeMediaToken(const QString &token){return !token.isEmpty()&&token.size()<=8192&&!token.contains('\r')&&!token.contains('\n')&&!token.contains('"')&&!token.contains(',');}
}
QVariantMap MediaLibrary::connectionSettings()const{
    QJsonObject all;const bool readable=readMediaSettings(all);const auto c=all.value(m_provider).toObject();
    const bool token=!c.value(m_provider=="navidrome"?"apiKey":"accessToken").toString().isEmpty();
    return {{"url",c.value("url").toString()},{"username",c.value("username").toString()},
        {"authentication",token?"token":"account"},{"hasSecret",token||!c.value("password").toString().isEmpty()},
        {"allowHttp",c.value("allowHttp").toBool()},{"readable",readable}};
}
void MediaLibrary::cancelConnectionCheck(){
    if(m_connectionReply){m_connectionReply->disconnect(this);m_connectionReply->abort();m_connectionReply->deleteLater();m_connectionReply=nullptr;}
    m_connectionBusy=false;m_connectionSucceeded=false;m_connectionMessage.clear();m_connectionDraft={};m_connectionOriginal={};emit connectionChanged();
}
void MediaLibrary::finishConnection(bool ok,const QString &message){
    m_connectionBusy=false;m_connectionSucceeded=ok;m_connectionMessage=message;m_connectionDraft={};m_connectionOriginal={};emit connectionChanged();
}
void MediaLibrary::checkConnection(const QVariantMap &settings,bool save){
    if(m_connectionBusy)return;
    cancelConnectionCheck();
    auto fail=[this](const QString &message){finishConnection(false,message);};
    if(!DeviceConfig::enabled(m_provider)){fail("Enable this application before configuring its connection.");return;}
    QJsonObject all;if(!readMediaSettings(all)){fail("Cannot read the private media settings. Repair the file or its permissions first.");return;}
    m_connectionOriginal=all.value(m_provider).toObject();
    const QString address=settings.value("url").toString().trimmed();QUrl url(address);
    if(address.size()>2048||!url.isValid()||url.host().isEmpty()||!url.userInfo().isEmpty()||url.hasQuery()||url.hasFragment()||(url.scheme()!="http"&&url.scheme()!="https")){fail("Enter a full HTTP or HTTPS server address, including its port if needed.");return;}
    if(url.scheme()=="http"&&!settings.value("allowHttp").toBool()){fail("Enable Allow HTTP for this private-network connection, or use HTTPS.");return;}
    QString path=url.path();if(!path.endsWith('/'))path+='/';url.setPath(path);m_connectionBase=url;
    QUrl old(m_connectionOriginal.value("url").toString());QString oldPath=old.path();if(!oldPath.endsWith('/'))oldPath+='/';old.setPath(oldPath);
    const bool sameServer=old==url;
    const bool token=settings.value("authentication").toString()=="token";
    const QString username=settings.value("username").toString().trimmed();QString secret=settings.value("secret").toString();
    const QString secretKey=token?(m_provider=="navidrome"?"apiKey":"accessToken"):"password";
    if(secret.isEmpty()&&sameServer&&(token||username==m_connectionOriginal.value("username").toString()))secret=m_connectionOriginal.value(secretKey).toString();
    if(secret.isEmpty()||secret.size()>8192||(!token&&(username.isEmpty()||username.size()>256))){fail("Enter the credentials for this server. Saved secrets can only be reused for the same server and account.");return;}
    if(token&&!safeMediaToken(secret)){fail("Enter a valid API key or access token.");return;}
    m_connectionDraft={{"url",url.toString()},{"allowHttp",settings.value("allowHttp").toBool()}};
    m_connectionBusy=true;m_saveConnection=save;m_connectionMessage="Checking connection…";emit connectionChanged();
    if(m_provider=="navidrome"){
        if(token)m_connectionDraft.insert("apiKey",secret);
        else {m_connectionDraft.insert("username",username);m_connectionDraft.insert("password",secret);}
        connectionRequest("getAlbumList2",{},false,[this](const QJsonObject &data){
            const auto response=data.value("subsonic-response").toObject();
            if(response.value("status")!="ok"){finishConnection(false,"Music server rejected the credentials or library request.");return;}
            if(!response.value("albumList2").isObject()){finishConnection(false,"The server did not return a music library.");return;}
            verifiedConnection();
        });
    }else{
        auto verify=[this](){connectionRequest("Users/Me",{},false,[this](const QJsonObject &user){
            const QString id=user.value("Id").toString();if(id.isEmpty()||id.size()>256){finishConnection(false,"The server did not return a Jellyfin user account.");return;}
            m_connectionDraft.insert("userId",id);verifiedConnection();
        });};
        if(token){m_connectionDraft.insert("accessToken",secret);verify();}
        else connectionRequest("Users/AuthenticateByName",{{"Username",username},{"Pw",secret}},true,[this,username,verify](const QJsonObject &data){
            const QString accessToken=data.value("AccessToken").toString();
            if(!safeMediaToken(accessToken)){finishConnection(false,"Jellyfin did not issue a valid access token.");return;}
            m_connectionDraft.insert("accessToken",accessToken);m_connectionDraft.insert("username",username);verify();
        });
    }
}
void MediaLibrary::connectionRequest(const QString &path,const QJsonObject &body,bool post,std::function<void(const QJsonObject &)> done){
    QUrl url;
    if(m_provider=="navidrome")url=authenticatedSubsonicUrl(m_connectionBase,m_connectionDraft,path,QUrlQuery("type=newest&size=1"));
    else url=m_connectionBase.resolved(QUrl(path));
    QNetworkRequest request(url);request.setTransferTimeout(15000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Accept","application/json");request.setHeader(QNetworkRequest::ContentTypeHeader,"application/json");
    if(m_provider=="jellyfin"){
        QByteArray auth="MediaBrowser Client=\"ThinkSmart\", Device=\"ThinkSmart View\", DeviceId=\""+(m_deviceId.isEmpty()?QUuid::createUuid().toByteArray(QUuid::Id128):m_deviceId.toUtf8())+"\", Version=\"0.2\"";
        const QString token=m_connectionDraft.value("accessToken").toString();if(!token.isEmpty())auth+=", Token=\""+token.toUtf8()+"\"";
        request.setRawHeader("Authorization",auth);
    }
    auto reply=post?m_network.post(request,QJsonDocument(body).toJson(QJsonDocument::Compact)):m_network.get(request);m_connectionReply=reply;
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 received,qint64 total){if(received>1024*1024||total>1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,done](){
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto error=reply->error();const QByteArray bytes=reply->readAll();m_connectionReply=nullptr;reply->deleteLater();
        if(error!=QNetworkReply::NoError||http<200||http>=300||bytes.size()>1024*1024){finishConnection(false,http==401||http==403?"Access denied. Check the account or token.":http>=300&&http<400?"The server redirected the request. Enter its final address instead.":"Cannot connect. Check the server address, network access and HTTPS certificate.");return;}
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(bytes,&parse);
        if(parse.error!=QJsonParseError::NoError||!doc.isObject()){finishConnection(false,"The server returned an invalid response.");return;}
        done(doc.object());
    });
}
void MediaLibrary::verifiedConnection(){
    if(!m_saveConnection){finishConnection(true,"Connected successfully. Settings have not been saved.");return;}
    QJsonObject all;if(!readMediaSettings(all)||all.value(m_provider).toObject()!=m_connectionOriginal){finishConnection(false,"Media settings changed while checking. Reopen this editor and try again.");return;}
    all.insert(m_provider,m_connectionDraft);
    const QString path=mediaConfigPath();const QString directory=QFileInfo(path).absolutePath();
    if(!QDir().mkpath(directory)||!QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)){finishConnection(false,"Cannot create the private settings directory.");return;}
    QSaveFile file(path);const auto bytes=QJsonDocument(all).toJson();
    if(bytes.size()>65536||!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)||file.write(bytes)!=bytes.size()||!file.commit()){finishConnection(false,"Cannot save the private media settings.");return;}
    finishConnection(true,"Connection saved. Restarting the dashboard…");emit connectionSaved();
}

QString MediaLibrary::queuePath()const{
    const QJsonObject identity{{"url",m_base.toString()},{"username",m_config.value("username")},{"apiKey",m_config.value("apiKey")}};
    const auto hash=QCryptographicHash::hash(QJsonDocument(identity).toJson(QJsonDocument::Compact),QCryptographicHash::Sha256).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/music/queue-"+QString::fromLatin1(hash)+".json";
}
void MediaLibrary::loadQueue(){
    QFile file(queuePath());if(!file.exists())return;
    auto invalid=[this](){m_queueWritable=false;m_queueProblem="Cannot read the saved queue. Check its file and private permissions.";};
    if(file.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)||!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024){invalid();return;}
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);const auto root=doc.object();
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||root.value("version").toInt()!=1||!root.value("items").isArray()||root.value("items").toArray().size()>2000){invalid();return;}
    QVariantList items;
    for(const auto &v:root.value("items").toArray()){
        const auto o=v.toObject();const auto id=o.value("id").toString();
        if(id.isEmpty()||id.size()>256||!o.value("title").isString()||o.value("title").toString().size()>256||o.value("subtitle").toString().size()>256||o.value("type")!="song"){invalid();return;}
        items.append(QVariantMap{{"id",id},{"title",o.value("title").toString()},{"subtitle",o.value("subtitle").toString()},{"type","song"},{"duration",o.value("duration").toDouble()}});
    }
    const int index=root.value("index").toInt(-1);if(index<-1||index>=items.size()){invalid();return;}
    m_queue=items;m_queueIndex=index;m_queueFinished=root.value("finished").toBool();m_queueModel.replace(items);
}
bool MediaLibrary::saveQueue(const QVariantList &items,int index,bool finished){
    if(m_provider!="navidrome"||!configured()||!m_queueWritable)return false;
    const QString path=queuePath(),directory=QFileInfo(path).absolutePath();
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"index",index},{"finished",finished},{"items",QJsonArray::fromVariantList(items)}}).toJson(QJsonDocument::Compact);
    QFile previous(path);
    if(previous.open(QIODevice::ReadOnly)&&previous.size()<=2*1024*1024&&previous.readAll()==bytes){m_queueProblem.clear();return true;}
    QSaveFile file(path);
    if(items.size()>2000||bytes.size()>2*1024*1024||!QDir().mkpath(directory)||!QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner)||!file.open(QIODevice::WriteOnly)||!file.setPermissions(QFile::ReadOwner|QFile::WriteOwner)||file.write(bytes)!=bytes.size()||!file.commit()){
        m_queueProblem="Cannot save the music queue.";return false;
    }
    m_queueProblem.clear();return true;
}
bool MediaLibrary::enqueueTracks(const QVariantList &items){
    if(m_provider!="navidrome"||!configured())return false;
    QVariantList tracks;
    for(const auto &v:items){const auto item=v.toMap();if(item.value("type")!="song")continue;
        tracks.append(QVariantMap{{"id",item.value("id").toString().left(256)},{"title",item.value("title").toString().left(256)},{"subtitle",item.value("subtitle").toString().left(256)},{"type","song"},{"duration",item.value("duration").toDouble()}});
    }
    if(tracks.isEmpty())return false;
    if(m_queue.size()+tracks.size()>2000){m_queueProblem="The queue is full (2,000 tracks). Clear it before adding more.";emit queueChanged();return false;}
    const int index=m_queueFinished?m_queue.size():m_queueIndex;
    if(!saveQueue(m_queue+tracks,index,false)){emit queueChanged();return false;}
    m_queue+=tracks;m_queueIndex=index;m_queueFinished=false;m_queueModel.append(tracks);emit queueChanged();emit queueAppended();return true;
}
bool MediaLibrary::enqueueAlbum(){return inAlbum()&&!m_busy&&enqueueTracks(m_items);}
void MediaLibrary::playQueue(int index){
    if(m_provider!="navidrome"||!configured()||m_queue.isEmpty())return;
    if(index<0)index=m_queueFinished||m_queueIndex<0?0:m_queueIndex;
    if(index>=m_queue.size())return;
    m_queueIndex=index;m_queueFinished=false;saveQueue(m_queue,index,false);emit queueChanged();play(m_queue[index].toMap());
}
void MediaLibrary::replayQueue(){playQueue(0);}
void MediaLibrary::clearQueue(){
    if(m_provider!="navidrome"||!configured())return;
    if(!saveQueue({},-1,false)){emit queueChanged();return;}
    cancelMusicRequests();m_queue.clear();m_queueModel.replace({});m_queueIndex=-1;m_queueFinished=false;m_lastItem.clear();m_queuePlaybackKey.clear();emit queueChanged();emit queueCleared();
}

void MediaLibrary::completed(const QString &key){
    if(m_provider!="navidrome"||key.isEmpty()||key!=m_queuePlaybackKey||m_queue.isEmpty())return;
    m_queuePlaybackKey.clear();
    if(m_queueIndex+1<m_queue.size()){playQueue(m_queueIndex+1);return;}
    m_queueFinished=true;saveQueue(m_queue,m_queueIndex,true);emit queueChanged();
}

namespace {
QJsonArray musicSlice(const QJsonArray &items,int offset,int limit){QJsonArray result;for(int i=qMax(0,offset);i<items.size()&&i<offset+limit;i++)result.append(items[i]);return result;}
}
bool MediaLibrary::knowsMusic(const QString &id,const QString &type)const{
    return m_provider=="navidrome"&&configured()&&DeviceConfig::enabled("navidrome")&&m_musicKnown.contains(type+":"+id);
}
QJsonObject MediaLibrary::rememberMusic(const QJsonObject &record,const QString &type){
    const QString id=record.value("id").toString();if(id.isEmpty()||id.size()>256)return {};
    QJsonObject result{{"id",id},{"type",type},{"title",record.value(type=="song"?"title":"name").toString(record.value("title").toString()).left(256)},
        {"artist",record.value("artist").toString().left(256)},{"album",record.value("album").toString().left(256)},
        {"year",record.value("year").toInt()},{"duration",qMax(0.,record.value("duration").toDouble())}};
    const QString key=type+":"+id;m_musicKnown.insert(key,result);m_musicKnownOrder.removeAll(key);m_musicKnownOrder.append(key);
    while(m_musicKnownOrder.size()>256)m_musicKnown.remove(m_musicKnownOrder.takeFirst());
    return result;
}
void MediaLibrary::musicGet(const QString &method,QUrlQuery query,std::function<void(QJsonObject,QString)> done){
    if(m_musicLookupBusy){done({},"Another music lookup is in progress. Retry after it completes.");return;}
    m_musicLookupBusy=true;QNetworkRequest request(authenticatedSubsonicUrl(m_base,m_config,method,query));
    request.setTransferTimeout(15000);request.setRawHeader("Accept","application/json");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    auto reply=m_network.get(request);
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 received,qint64 total){if(received>4*1024*1024||total>4*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,done](){
        m_musicLookupBusy=false;const int status=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto bytes=reply->readAll();const auto error=reply->error();reply->deleteLater();
        if(error!=QNetworkReply::NoError||status<200||status>=300||bytes.size()>4*1024*1024){done({},status==401||status==403?"Navidrome access denied. Check Music connection settings.":"Cannot reach Navidrome. Check the server and network.");return;}
        QJsonParseError parse;const auto response=QJsonDocument::fromJson(bytes,&parse).object().value("subsonic-response").toObject();
        if(parse.error!=QJsonParseError::NoError||response.value("status")!="ok"){done({},"Navidrome rejected the request or returned invalid data.");return;}
        done(response,{});
    });
}
QJsonObject MediaLibrary::musicControl(const QJsonObject &request){
    auto failure=[](const QString &error){return QJsonObject{{"ok",false},{"error",error}};};
    if(m_provider!="navidrome"||!DeviceConfig::enabled("navidrome"))return failure("The music library module is disabled.");
    if(!configured())return failure("Configure Navidrome in Settings → Pages & modules → Music → Edit first.");
    const QString action=request.value("action").toString();
    if(!musicActions().contains(action))return failure("Unknown music action.");
    const int offset=request.value("offset").toInt(0),limit=request.value("limit").toInt(5);
    for(const auto &key:{"offset","limit"})if(request.contains(key)&&(!request.value(key).isDouble()||request.value(key).toDouble()!=request.value(key).toInt()))return failure("Music numeric arguments must be integers.");
    if(offset<0||offset>2000||limit<1||limit>10)return failure("Use offset 0–2000 and limit 1–10.");
    if(action=="music.clear"){clearQueue();if(!m_queueProblem.isEmpty())return failure(m_queueProblem);}
    else if(action=="music.play"&&request.value("id").toString().isEmpty()){
        if(m_queue.isEmpty())return failure("The music queue is empty. Search and select a track or album first.");playQueue();
    }else if(action=="music.replay"){
        if(m_queue.isEmpty())return failure("The music queue is empty.");replayQueue();
    }else if(action=="music.next"||action=="music.previous"){
        if(action=="music.next"?!queueCanNext():!queueCanPrevious())return failure("There is no track in that direction.");advance(action=="music.next"?1:-1);
    }else if(action!="music.queue")return failure("This music action requires the asynchronous tool interface.");
    QJsonArray items;
    for(int i=offset;i<qMin(m_queue.size(),offset+limit);i++){
        const auto item=m_queue[i].toMap();auto row=rememberMusic({{"id",item.value("id").toString()},{"title",item.value("title").toString()},{"artist",item.value("subtitle").toString()},{"duration",item.value("duration").toDouble()}},"song");
        row.insert("index",i);items.append(row);
    }
    return {{"ok",true},{"configured",true},{"queueCount",m_queue.size()},{"queueIndex",m_queueIndex},{"finished",m_queueFinished},{"items",items},{"more",offset+items.size()<m_queue.size()},{"accepted",action!="music.queue"}};
}
void MediaLibrary::musicRequest(const QJsonObject &request,std::function<void(QJsonObject)> done,std::function<bool()> allowed){
    auto fail=[done](const QString &error){done({{"ok",false},{"error",error}});};
    if(m_provider!="navidrome"||!DeviceConfig::enabled("navidrome")){fail("The music library module is disabled.");return;}
    if(!configured()){fail("Configure Navidrome in Music connection settings first.");return;}
    if(allowed&&!allowed()){fail("Music request cancelled.");return;}
    const QString action=request.value("action").toString();
    if(!musicActions().contains(action)){fail("Unknown music action.");return;}
    const QStringList fields{"action","requestId","search","id","type","mode","genre","decade","offset","limit"};
    for(auto i=request.begin();i!=request.end();++i)if(!fields.contains(i.key())){fail("Unknown music argument.");return;}
    for(const auto &field:{"search","id","type","mode","genre"})if(request.contains(field)&&!request.value(field).isString()){fail("Music text arguments must be strings.");return;}
    for(const auto &field:{"offset","limit","decade"})if(request.contains(field)&&(!request.value(field).isDouble()||!std::isfinite(request.value(field).toDouble())||request.value(field).toDouble()!=request.value(field).toInt())){fail("Music numeric arguments must be integers.");return;}
    const int offset=request.value("offset").toInt(0),limit=request.value("limit").toInt(5);
    if(offset<0||offset>2000||limit<1||limit>10){fail("Use offset 0–2000 and limit 1–10.");return;}
    const QString id=request.value("id").toString(),type=request.value("type").toString();
    if(id.size()>256||request.value("search").toString().size()>200||request.value("genre").toString().size()>256){fail("Music argument is too long.");return;}
    if(action=="music.queue"||action=="music.next"||action=="music.previous"||action=="music.replay"||action=="music.clear"||(action=="music.play"&&id.isEmpty())){done(musicControl(request));return;}
    QString method;QUrlQuery query;
    if(action=="music.search"){
        const QString text=request.value("search").toString().trimmed();if(text.isEmpty()){fail("Provide an artist, band, album or track search.");return;}
        method="search3";query.addQueryItem("query",text);
        for(const auto &kind:{"artist","album","song"}){query.addQueryItem(QString(kind)+"Count",QString::number(limit));query.addQueryItem(QString(kind)+"Offset",QString::number(offset));}
    }else if(action=="music.genres")method="getGenres";
    else if(action=="music.browse"){
        method="getAlbumList2";const auto mode=request.value("mode").toString("newest");
        if(!QStringList{"newest","alphabeticalByName","alphabeticalByArtist","recent","frequent","highest","starred","byGenre","byYear"}.contains(mode)){fail("Choose an advertised album browse mode.");return;}
        query.addQueryItem("type",mode);query.addQueryItem("size",QString::number(limit));query.addQueryItem("offset",QString::number(offset));
        if(mode=="byGenre"){const auto genre=request.value("genre").toString().trimmed();if(genre.isEmpty()){fail("Choose a genre from music.genres.");return;}query.addQueryItem("genre",genre);}
        if(mode=="byYear"){const int decade=request.value("decade").toInt();if(decade<1900||decade>2100||decade%10){fail("Choose a decade such as 1990.");return;}query.addQueryItem("fromYear",QString::number(decade+9));query.addQueryItem("toYear",QString::number(decade));}
    }else{
        const QString expected=action=="music.artist"?"artist":action=="music.album"?"album":type;
        if((expected!="artist"&&expected!="album"&&expected!="song")||!knowsMusic(id,expected)){fail("Unknown music ID. Use music.search, music.artist, music.album or music.queue first.");return;}
        if(action=="music.play"||action=="music.enqueue"){
            if(type!="song"&&type!="album"){fail("Select a song or album, not an artist. List the artist's albums first.");return;}
            if(type=="song"){
                const auto item=m_musicKnown.value("song:"+id);QVariantMap track{{"id",id},{"title",item.value("title").toString()},{"subtitle",item.value("artist").toString()},{"type","song"},{"duration",item.value("duration").toDouble()}};
                if(action=="music.enqueue"){
                    if(!enqueueTracks({track})){fail(m_queueProblem);return;}
                }else{
                    if(!saveQueue({track},0,false)){fail(m_queueProblem);return;}m_queue={track};m_queueModel.replace(m_queue);m_queueIndex=0;m_queueFinished=false;playQueue(0);
                }
                done({{"ok",true},{"accepted",true},{"added",1},{"queueCount",m_queue.size()}});return;
            }
        }
        method=expected=="artist"?"getArtist":"getAlbum";query.addQueryItem("id",id);
    }
    const auto generation=m_musicRequestGeneration;
    musicGet(method,query,[this,request,action,offset,limit,done,fail,allowed,generation](QJsonObject response,QString error){
        if(generation!=m_musicRequestGeneration||(allowed&&!allowed())){fail("Music request cancelled.");return;}
        if(!error.isEmpty()){fail(error);return;}
        QJsonArray items;int total=-1;bool more=false;
        if(action=="music.search"){
            const auto found=response.value("searchResult3").toObject();
            for(const auto &kind:{"artist","album","song"}){const auto list=found.value(kind).toArray();for(const auto &row:musicSlice(list,0,limit)){auto item=rememberMusic(row.toObject(),kind);if(!item.isEmpty())items.append(item);}more=more||list.size()>=limit;}
        }else if(action=="music.genres"){
            QJsonArray genres;const auto text=request.value("search").toString().trimmed();
            for(const auto &row:response.value("genres").toObject().value("genre").toArray()){const auto name=row.toObject().value("value").toString().left(256);if(!name.isEmpty()&&(text.isEmpty()||name.contains(text,Qt::CaseInsensitive)))genres.append(name);}
            total=genres.size();for(const auto &name:musicSlice(genres,offset,limit))items.append(name);more=offset+limit<total;
        }else{
            QJsonArray rows;QString kind="album";
            if(action=="music.browse")rows=response.value("albumList2").toObject().value("album").toArray();
            else if(action=="music.artist"){rows=response.value("artist").toObject().value("album").toArray();total=rows.size();}
            else{rows=response.value("album").toObject().value("song").toArray();total=rows.size();kind="song";}
            if(action=="music.play"||action=="music.enqueue"){
                if(rows.isEmpty()||rows.size()>2000){fail("The album has no playable tracks or exceeds the queue limit.");return;}
                QVariantList tracks;for(const auto &row:rows){const auto track=normalizeItem(row.toObject(),true,true);if(!track.isEmpty())tracks.append(QVariantMap{{"id",track.value("id")},{"title",track.value("title")},{"subtitle",track.value("subtitle")},{"type","song"},{"duration",track.value("duration")}});}
                if(tracks.isEmpty()){fail("No playable tracks in this album.");return;}
                if(action=="music.enqueue"){if(!enqueueTracks(tracks)){fail(m_queueProblem);return;}}
                else{
                    if(!saveQueue(tracks,0,false)){fail(m_queueProblem);return;}m_queue=tracks;m_queueModel.replace(m_queue);m_queueIndex=0;m_queueFinished=false;playQueue(0);
                }
                done({{"ok",true},{"accepted",true},{"added",tracks.size()},{"queueCount",m_queue.size()}});return;
            }
            const int start=action=="music.browse"?0:offset;
            for(const auto &row:musicSlice(rows,start,limit)){auto item=rememberMusic(row.toObject(),kind);if(!item.isEmpty())items.append(item);}
            more=action=="music.browse"?rows.size()>=limit:offset+limit<total;
        }
        done({{"ok",true},{"items",items},{"count",total>=0?total:items.size()},{"more",more},{"offset",offset},{"nextOffset",offset+limit}});
    });
}
