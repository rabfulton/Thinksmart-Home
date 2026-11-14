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
    m_title=provider=="navidrome"?"Recently added albums":"Films & series";
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
            if(!quiet)m_problem=http==401||http==403?"Server access denied. Check the connection credentials.":"Cannot reach the server. Any cached library is kept.";
        }else if(quiet&&bytes.trimmed().isEmpty()){done({});}
        else{
            QJsonParseError error;QJsonObject data=QJsonDocument::fromJson(bytes,&error).object();
            if(error.error!=QJsonParseError::NoError){if(!quiet)m_problem="Invalid server response.";}
            else{
                if(m_provider=="navidrome")data=data.value("subsonic-response").toObject();
                if(m_provider=="navidrome"&&data.value("status")!="ok"){if(!quiet)m_problem=QString("Music server rejected the request (code %1).").arg(data.value("error").toObject().value("code").toInt());}
                else done(data);
            }
        }
        reply->deleteLater();if(!quiet)emit changed();
    });
}
QVariantMap MediaLibrary::route()const{return {{"parent",m_parent},{"search",m_search},{"title",m_title},{"offset",m_offset},{"genre",m_genre},{"section",m_section},{"library",m_library},{"decade",m_decade}};}
void MediaLibrary::restoreRoute(const QVariantMap &r){m_parent=r.value("parent").toString();m_search=r.value("search").toString();m_title=r.value("title").toString();m_offset=r.value("offset").toInt();m_genre=r.value("genre").toString();m_section=r.value("section","all").toString();m_library=r.value("library").toString();m_decade=r.value("decade").toInt();}
void MediaLibrary::refresh(){if(!m_configured||m_busy)return;if(m_provider=="jellyfin"){m_loadingMore=false;m_offset=0;}if(m_provider=="jellyfin"&&m_config.value("userId").toString().isEmpty()){request(endpoint("Users/Me"),{},false,[this](const QJsonObject &user){QString id=user.value("Id").toString();if(id.isEmpty()){m_problem="A Jellyfin user ID is needed for this token.";return;}m_config.insert("userId",id);browse();});}else browse();}
void MediaLibrary::browse(){
    if(!m_loadingMore)loadCache();
    if(m_provider=="jellyfin")loadFilters();
    if(m_provider=="navidrome"){
        QUrlQuery query;QString method;
        if(!m_parent.isEmpty()){method="getAlbum";query.addQueryItem("id",m_parent);}
        else if(!m_search.isEmpty()){method="search3";query.addQueryItem("query",m_search);query.addQueryItem("songCount","60");query.addQueryItem("songOffset",QString::number(m_offset));query.addQueryItem("albumCount","0");query.addQueryItem("artistCount","0");}
        else{method="getAlbumList2";query.addQueryItem("type","newest");query.addQueryItem("size","60");query.addQueryItem("offset",QString::number(m_offset));}
        request(authenticatedSubsonicUrl(m_base,m_config,method,query),{},false,[this](const QJsonObject &response){
            const bool songs=!m_parent.isEmpty()||!m_search.isEmpty();QJsonArray list;
            if(!m_parent.isEmpty())list=response.value("album").toObject().value("song").toArray();
            else if(!m_search.isEmpty())list=response.value("searchResult3").toObject().value("song").toArray();
            else list=response.value("albumList2").toObject().value("album").toArray();
            QVariantList items;for(const auto &r:list){auto item=normalizeItem(r.toObject(),true,songs);if(!item.isEmpty())items.append(item);}storeItems(items);
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
    }else if(type=="song"){m_queue=m_items;m_queueIndex=index;play(item);}
    else if(type=="movie"||type=="episode")play(item);
}
void MediaLibrary::back(){if(m_busy||m_history.isEmpty())return;restoreRoute(m_history.takeLast().toMap());refresh();}
void MediaLibrary::page(int delta){if(m_busy||(!canNext()&&delta>0)||(!canPrevious()&&delta<0))return;m_offset=qMax(0,m_offset+delta*60);refresh();}
void MediaLibrary::search(const QString &text){if(m_busy)return;m_history.append(route());m_parent.clear();m_search=text.trimmed().left(200);m_title=m_search.isEmpty()?(m_provider=="navidrome"?"Recently added albums":"Films & series"):"Search: "+m_search;m_offset=0;refresh();}
void MediaLibrary::advance(int delta){int index=m_queueIndex+delta;if(index<0||index>=m_queue.size())return;m_queueIndex=index;play(m_queue.at(index).toMap());}
void MediaLibrary::replay(){if(!m_lastItem.isEmpty()&&!m_busy)play(m_lastItem);}
QString MediaLibrary::cachePath()const{
    const QString identity=m_provider+m_base.toString()+m_config.value("username").toString()+m_config.value("userId").toString()+m_config.value("apiKey").toString()+m_parent+m_search+m_genre+m_section+m_library+QString::number(m_decade)+(m_provider=="jellyfin"?QString():QString::number(m_offset));
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/library-"+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex())+".json";
}
void MediaLibrary::loadCache(){
    m_items.clear();m_itemModel.replace(m_items);m_cached=false;m_savedAt=0;
    QFile file(cachePath());if(!file.open(QIODevice::ReadOnly)||file.size()>8*1024*1024)return;
    const auto o=QJsonDocument::fromJson(file.readAll()).object();if(o.value("version").toInt()!=1)return;
    for(const auto &r:o.value("items").toArray()){auto item=r.toObject();if(item.value("id").isString()&&item.value("title").isString())m_items.append(item.toVariantMap());}
    m_itemModel.replace(m_items);m_total=o.value("total").toInt(-1);
    m_savedAt=o.value("savedAt").toVariant().toLongLong();m_cached=!m_items.isEmpty();emit changed();
}
void MediaLibrary::storeItems(const QVariantList &items){
    m_items=items;m_itemModel.replace(m_items);m_cached=false;m_savedAt=QDateTime::currentSecsSinceEpoch();
    const QString directory=QFileInfo(cachePath()).absolutePath();QDir().mkpath(directory);QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"total",m_total},{"savedAt",m_savedAt},{"items",QJsonArray::fromVariantList(items)}}).toJson(QJsonDocument::Compact);
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
    if(!configured()||m_busy)return;m_lastItem=item;
    if(m_provider=="navidrome"){
        QUrlQuery query;query.addQueryItem("id",item.value("id").toString());
        const QUrl url=authenticatedSubsonicUrl(m_base,m_config,"stream",query);
        emit playRequested(m_provider,url,item.value("title").toString(),item.value("subtitle").toString(),{},false,QUuid::createUuid().toString(QUuid::Id128),0);return;
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
    if(m_filtersRequested||m_config.value("userId").toString().isEmpty())return;m_filtersRequested=true;
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
void MediaLibrary::loadMore(){if(m_provider!="jellyfin"||m_busy||m_loadingMore||!canNext())return;m_offset=m_items.size();m_loadingMore=true;browse();}
