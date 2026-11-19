#include "tvarchive.h"
#include "deviceconfig.h"
#include "localproxy.h"
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <QJsonDocument>
#include <QDateTime>
#include <QRegularExpression>
#include <QCollator>
#include <QRandomGenerator>
#include <QFutureWatcher>
#include <QPromise>
#include <QThreadPool>
#include <memory>
#include <QSet>
#ifdef THINKSMART_ENABLE_TVARCHIVE_PROXY
#include <QNetworkProxy>
#include <QSslConfiguration>
#include <QSslEllipticCurve>
#endif
#include <algorithm>
#include <cmath>

namespace {
QString identity(const QString &prefix,const QString &text){return prefix+QString::fromLatin1(QCryptographicHash::hash((TvArchiveClient::collection()+'/'+text.normalized(QString::NormalizationForm_C)).toUtf8(),QCryptographicHash::Sha256).toHex().left(32));}
bool safeName(const QString &s){return !s.isEmpty()&&s.size()<2000&&!s.startsWith('/')&&!s.contains('\\')&&!s.contains(QChar::Null)&&!s.split('/').contains("..")&&!s.split('/').contains(".");}
QString stem(QString s){s=s.left(s.lastIndexOf('.'));s.remove(QRegularExpression("_(?:512kb|h264|ia|mpeg4)$",QRegularExpression::CaseInsensitiveOption));return s;}
QString readable(QString s){s.replace(QRegularExpression("[._]+")," ");return s.simplified();}
double number(const QJsonValue &v){return v.toVariant().toDouble();}
QJsonObject read(const QString &path,int max=16*1024*1024){QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>max)return {};return QJsonDocument::fromJson(f.readAll()).object();}
bool write(const QString &path,const QJsonObject &d){QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))return false;f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto b=QJsonDocument(d).toJson(QJsonDocument::Compact);return f.write(b)==b.size()&&f.commit();}
bool validId(const QString &id){return QRegularExpression("^tva:[0-9a-f]{32}$").match(id).hasMatch();}
}
QString TvArchiveClient::storagePath(){return qEnvironmentVariable("THINKSMART_TVARCHIVE_FILE",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/tvarchive/state.json");}
QString TvArchiveClient::cachePath(){return qEnvironmentVariable("THINKSMART_TVARCHIVE_CACHE",QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/tvarchive/catalogue.json");}
QString TvArchiveClient::proxyUrl(){
#ifdef THINKSMART_ENABLE_TVARCHIVE_PROXY
    return localHttpProxy(qEnvironmentVariable("THINKSMART_TVARCHIVE_PROXY"));
#else
    return {};
#endif
}
QJsonArray TvArchiveClient::parseCatalogue(const QJsonObject &metadata){
    const auto files=metadata.contains("files")?metadata.value("files").toArray():metadata.value("result").toArray();if(files.size()>20000)return {};QMap<QString,QJsonObject> byName,chosen;
    for(const auto &v:files){const auto f=v.toObject();const auto name=f.value("name").toString();if(safeName(name))byName.insert(name,f);}
    for(auto it=byName.cbegin();it!=byName.cend();++it){
        const auto f=it.value();const auto name=it.key();const auto suffix=QFileInfo(name).suffix().toLower();if(!QStringList{"mp4","m4v","avi","mkv"}.contains(suffix)||f.value("private").toBool())continue;
        const auto size=number(f.value("size"));if(!std::isfinite(size)||size<=0)continue;
        QString original=name;QSet<QString> seen;
        for(int n=0;n<5;++n){auto p=byName.value(original).value("original").toString();if(p.isEmpty()||!safeName(p)||seen.contains(p)||!QStringList{"mp4","m4v","avi","mkv","wmv","mpg","mpeg","mov"}.contains(QFileInfo(p).suffix().toLower()))break;seen.insert(p);if(!byName.contains(p)){const auto relative=QFileInfo(original).path()+'/'+p;if(byName.contains(relative))p=relative;else break;}original=p;}
        const auto key=stem(original);const auto parts=original.split('/');QString seriesName=parts.size()>1?parts.first():"Classic TV";
        seriesName.remove(QRegularExpression("[ ._-]+(?:Complete[ ._-]+Collection|S[0-9]{1,2}-S[0-9]{1,2}|webdl|xvid|h264|h\\.264).*",QRegularExpression::CaseInsensitiveOption));seriesName=readable(seriesName);
        const auto seriesId=identity("tvs:",parts.size()>1?parts.first():"Classic TV");QString title=readable(QFileInfo(stem(original)).fileName());
        int season=0,episode=0;
        auto se=QRegularExpression("(?:S|Season[ ._-]*|Series[ ._-]*)([0-9]{1,2})[ ._-]*E(?:p(?:isode)?)?[ ._-]*([0-9]{1,3})",QRegularExpression::CaseInsensitiveOption).match(original);
        if(se.hasMatch()){season=se.captured(1).toInt();episode=se.captured(2).toInt();}
        else {auto sx=QRegularExpression("\\b([0-9]{1,2})x([0-9]{1,3})\\b",QRegularExpression::CaseInsensitiveOption).match(original);if(sx.hasMatch()){season=sx.captured(1).toInt();episode=sx.captured(2).toInt();}else{auto ss=QRegularExpression("(?:Season|Series)[ ._-]*([0-9]{1,2})",QRegularExpression::CaseInsensitiveOption).match(original);if(ss.hasMatch())season=ss.captured(1).toInt();auto ee=QRegularExpression("(?:Episode|\\bEp)[ ._-]*([0-9]{1,3})",QRegularExpression::CaseInsensitiveOption).match(original);if(ee.hasMatch())episode=ee.captured(1).toInt();}}
        const auto baseName=readable(QFileInfo(stem(original)).fileName());
        if(!se.hasMatch()&&episode==0){
            if(!season){const auto ss=QRegularExpression("(?:Season|Series|(?:^|[ /._-])S)[ ._-]*([0-9]{1,2})(?:[ /._-]|$)",QRegularExpression::CaseInsensitiveOption).match(original);if(ss.hasMatch())season=ss.captured(1).toInt();}
            const auto single=QRegularExpression("(?:^|[ ._-])E([0-9]{1,3})(?:[ ._-]|$)",QRegularExpression::CaseInsensitiveOption).match(baseName);
            if(single.hasMatch())episode=single.captured(1).toInt();
            else{const auto compact=QRegularExpression("^([0-9]{1,2})([0-9]{2})[ -]").match(baseName);if(compact.hasMatch()&&season>0&&compact.captured(1).toInt()==season)episode=compact.captured(2).toInt();}
        }
        if(!se.hasMatch()&&episode==0&&season>0){
            const auto leading=QRegularExpression("^([0-9]{1,3})[ -]").match(baseName);
            if(leading.hasMatch())episode=leading.captured(1).toInt();
            else{auto tail=baseName;tail.remove(QRegularExpression('^'+QRegularExpression::escape(seriesName)+"[ -]*",QRegularExpression::CaseInsensitiveOption));if(QRegularExpression("^[0-9]{1,3}$").match(tail).hasMatch())episode=tail.toInt();}
        }
        const bool extra=QRegularExpression("/(?:Extras|Specials|Movies)/",QRegularExpression::CaseInsensitiveOption).match(original).hasMatch()||(episode==0&&!se.hasMatch());
        title.remove(QRegularExpression('^'+QRegularExpression::escape(seriesName)+"[ -]*",QRegularExpression::CaseInsensitiveOption));title.remove(QRegularExpression("^.*?\\b(?:S[0-9]+[ ]*E[0-9]+|[0-9]+x[0-9]+|E[0-9]{1,3})[ -]*",QRegularExpression::CaseInsensitiveOption));title.remove(QRegularExpression("\\[[^\\]]*\\]"));title.remove(QRegularExpression("^(?:[0-9]{3,4}|[Ee][0-9]{1,3})[ -]+"));title=title.simplified();if(title.isEmpty())title=episode?"Episode "+QString::number(episode):readable(QFileInfo(key).fileName());
        QUrl url("https://archive.org");url.setPath("/download/"+collection()+'/'+name);
        const int height=int(number(f.value("height"))),width=int(number(f.value("width")));const int quality=(suffix=="mp4"||suffix=="m4v"?0:100)+(height>720?10:0);
        QJsonObject row{{"id",identity("tva:",key)},{"seriesId",seriesId},{"seriesTitle",seriesName},{"title",title.left(300)},{"name",name},{"canonical",key},{"url",url.toString(QUrl::FullyEncoded)},{"season",season},{"number",episode},{"extra",extra},{"size",size},{"format",suffix.toUpper()},{"width",width},{"height",height},{"duration",number(f.value("length"))},{"quality",quality}};
        const auto old=chosen.value(key);if(old.isEmpty()||quality<old.value("quality").toInt()||(quality==old.value("quality").toInt()&&size<old.value("size").toDouble()))chosen.insert(key,row);
    }
    auto rows=chosen.values();QCollator collator;collator.setNumericMode(true);collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(rows.begin(),rows.end(),[&](const auto &a,const auto &b){const int series=collator.compare(a.value("seriesTitle").toString(),b.value("seriesTitle").toString());if(series)return series<0;if(a.value("extra").toBool()!=b.value("extra").toBool())return !a.value("extra").toBool();for(const auto &k:{"season","number"})if(a.value(k)!=b.value(k))return a.value(k).toInt()<b.value(k).toInt();return collator.compare(a.value("canonical").toString(),b.value("canonical").toString())<0;});
    QJsonArray out;for(auto r:rows){r.remove("quality");out.append(r);}return out;
}
TvArchiveClient::TvArchiveClient(QObject *parent):QObject(parent){
    if(!DeviceConfig::enabled("tvarchive"))return;
#ifdef THINKSMART_ENABLE_TVARCHIVE_PROXY
    const auto proxy=proxyUrl();if(!qEnvironmentVariable("THINKSMART_TVARCHIVE_PROXY").trimmed().isEmpty()&&proxy.isEmpty()){m_healthy=false;m_problem="Invalid TV Archive proxy configuration.";return;}
    if(!proxy.isEmpty()){QUrl u(proxy);m_network.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy,u.host(),quint16(u.port())));}
#endif
    load();updateModels();
}
TvArchiveClient::~TvArchiveClient(){if(m_reply){m_reply->disconnect(this);m_reply->abort();}}
void TvArchiveClient::load(){
    m_state={{"version",1},{"autoplay",false},{"progress",QJsonObject{}},{"continue",QJsonObject{}}};
    if(QFile::exists(storagePath())){
        const auto s=read(storagePath(),4*1024*1024);bool ok=s.value("version")==1&&s.value("autoplay").isBool()&&s.value("progress").isObject()&&s.value("continue").isObject()&&s.value("progress").toObject().size()<=10000;
        const auto progress=s.value("progress").toObject();for(auto it=progress.begin();it!=progress.end();++it){const auto p=it.value().toObject();const double pos=p.value("position").toDouble(-1),duration=p.value("duration").toDouble(-1);if(!validId(it.key())||!std::isfinite(pos)||pos<0||pos>7*86400||!std::isfinite(duration)||duration<0||duration>7*86400||!p.value("completed").isBool())ok=false;}
        const auto cursors=s.value("continue").toObject();if(cursors.size()>10000)ok=false;
        for(auto it=cursors.begin();it!=cursors.end();++it)if(!QRegularExpression("^tvs:[0-9a-f]{32}$").match(it.key()).hasMatch()||!validId(it.value().toString()))ok=false;
        if(s.contains("last")&&!validId(s.value("last").toString()))ok=false;
        if(!ok){m_healthy=false;m_problem="TV Archive progress file is invalid; left untouched. Repair it before playback.";}else m_state=s;
    }
    const auto cache=read(cachePath());if(cache.value("version")==1&&cache.value("collection")==collection()){
        const auto rows=cache.value("episodes").toArray();bool ok=rows.size()<=10000;for(const auto &v:rows){auto r=v.toObject();const QUrl u(r.value("url").toString());if(!validId(r.value("id").toString())||u.scheme()!="https"||u.host()!="archive.org"||!u.path().startsWith("/download/"+collection()+'/')||!u.userInfo().isEmpty()||u.hasQuery()||u.hasFragment()||u.port(-1)!=-1||!safeName(u.path().mid(("/download/"+collection()+'/').size()))||!QRegularExpression("^tvs:[0-9a-f]{32}$").match(r.value("seriesId").toString()).hasMatch())ok=false;}
        if(ok){m_catalogue=rows;indexCatalogue();m_updated=qint64(cache.value("updated").toDouble());}
    }
    m_selected=m_state.value("selected").toString();if(m_selected.isEmpty())m_selected=episode(m_state.value("last").toString()).value("seriesId").toString();
}
bool TvArchiveClient::commit(const QJsonObject &value){if(!m_healthy)return false;if(!write(storagePath(),value)){m_problem="Cannot save TV Archive progress.";emit changed();return false;}m_state=value;updateModels();return true;}
void TvArchiveClient::indexCatalogue(){
    m_byId.clear();m_firstBySeries.clear();
    for(const auto &value:m_catalogue){const auto row=value.toObject();const auto id=row.value("id").toString(),series=row.value("seriesId").toString();m_byId.insert(id,row);if(!m_firstBySeries.contains(series))m_firstBySeries.insert(series,id);}
}
QJsonObject TvArchiveClient::episode(const QString &id)const{return m_byId.value(id);}
bool TvArchiveClient::knows(const QString &id)const{return !episode(id).isEmpty();}
QJsonObject TvArchiveClient::summary(const QJsonObject &e)const{if(e.isEmpty())return {};auto row=e;row.remove("url");row.remove("name");row.remove("canonical");const auto p=m_state.value("progress").toObject().value(e.value("id").toString()).toObject();for(auto it=p.begin();it!=p.end();++it)row.insert(it.key(),it.value());return row;}
QString TvArchiveClient::continueId(const QString &seriesId)const{const auto stored=m_state.value("continue").toObject().value(seriesId).toString();if(episode(stored).value("seriesId")==seriesId)return stored;return m_firstBySeries.value(seriesId);}
QVariantMap TvArchiveClient::selected()const{for(const auto &v:m_catalogue){const auto e=v.toObject();if(e.value("seriesId")==m_selected)return QJsonObject{{"id",m_selected},{"title",e.value("seriesTitle")},{"next",summary(episode(continueId(m_selected)))}}.toVariantMap();}return {};}
QVariantMap TvArchiveClient::current()const{const auto last=episode(m_state.value("last").toString());if(last.isEmpty())return {};return summary(episode(continueId(last.value("seriesId").toString()))).toVariantMap();}
QString TvArchiveClient::status()const{if(busy())return "Updating catalogue…";if(m_catalogue.isEmpty())return "Update to load the Archive catalogue";return "Internet Archive · "+QString::number(m_catalogue.size())+" episodes"+(m_updated?" · "+QDateTime::fromSecsSinceEpoch(m_updated).date().toString("d MMM yyyy"):QString());}
QStringList TvArchiveClient::seasons()const{QSet<int> nums;for(const auto &v:m_catalogue){const auto e=v.toObject();if(e.value("seriesId")==m_selected)nums.insert(e.value("season").toInt());}QList<int> sorted=nums.values();std::sort(sorted.begin(),sorted.end());QStringList out{"All episodes"};for(auto n:sorted)out<<QString::number(n);return out;}
void TvArchiveClient::updateModels(){
    QJsonArray seriesRows,episodeRows;QMap<QString,QJsonObject> shows;QStringList order;
    for(const auto &v:m_catalogue){const auto e=v.toObject();const auto id=e.value("seriesId").toString();if(!shows.contains(id)){order<<id;shows[id]={{"id",id},{"title",e.value("seriesTitle")},{"count",0}};}auto show=shows[id];show.insert("count",show.value("count").toInt()+1);shows[id]=show;
        if(id==m_selected&&(m_season.isEmpty()||e.value("season").toInt()==m_season.toInt())&&(m_query.isEmpty()||e.value("title").toString().contains(m_query,Qt::CaseInsensitive)||e.value("seriesTitle").toString().contains(m_query,Qt::CaseInsensitive)))episodeRows.append(summary(e));
    }
    for(const auto &id:order){auto s=shows[id];if(!m_query.isEmpty()&&!s.value("title").toString().contains(m_query,Qt::CaseInsensitive)){bool match=false;for(const auto &v:m_catalogue)if(v.toObject().value("seriesId")==id&&v.toObject().value("title").toString().contains(m_query,Qt::CaseInsensitive)){match=true;break;}if(!match)continue;}s.insert("next",summary(episode(continueId(id))));seriesRows.append(s);}
    m_series.setRows(seriesRows);m_episodes.setRows(episodeRows);emit changed();
}
void TvArchiveClient::setQuery(const QString &value){m_query=value.trimmed().left(100);updateModels();}
void TvArchiveClient::setSeason(const QString &value){m_season=value=="All episodes"?QString():value;updateModels();}
void TvArchiveClient::selectSeries(const QString &id){if(!DeviceConfig::enabled("tvarchive"))return;m_selected=id;m_season.clear();updateModels();}
void TvArchiveClient::setAutoplay(bool value){if(!DeviceConfig::enabled("tvarchive"))return;auto s=m_state;s.insert("autoplay",value);commit(s);}
void TvArchiveClient::setActive(bool value){if(!DeviceConfig::enabled("tvarchive")||m_active==value)return;m_active=value;emit changed();if(value&&!qEnvironmentVariableIsSet("THINKSMART_DISABLE_TVARCHIVE")&&(m_catalogue.isEmpty()||QDateTime::currentSecsSinceEpoch()-m_updated>90*86400))refresh();}
void TvArchiveClient::refresh(){
    if(!DeviceConfig::enabled("tvarchive")||!m_healthy||busy())return;const auto now=QDateTime::currentSecsSinceEpoch();if(now-m_attempt<5)return;m_attempt=now;if(m_healthy)m_problem.clear();
    QUrl url("https://archive.org/metadata/"+collection()+"/files?start=0&count=20000");
#ifdef THINKSMART_TVARCHIVE_TESTING
    if(qEnvironmentVariableIsSet("THINKSMART_TVARCHIVE_TEST_URL"))url=QUrl(qEnvironmentVariable("THINKSMART_TVARCHIVE_TEST_URL"));
#endif
    QNetworkRequest request(url);request.setRawHeader("User-Agent","ThinkSmartHome/0.4 (TV Archive client)");request.setTransferTimeout(45000);request.setMaximumRedirectsAllowed(3);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
#ifdef THINKSMART_ENABLE_TVARCHIVE_PROXY
    // Keep the TLS ClientHello small on constrained proxy/tunnel paths. Modern
    // hybrid key shares can exceed a tunnel packet and stall the handshake.
    // TLS 1.3 and certificate verification remain enabled.
    if(!proxyUrl().isEmpty()){
        auto tls=request.sslConfiguration();const auto curve=QSslEllipticCurve::fromShortName("X25519");
        if(curve.isValid()){tls.setEllipticCurves({curve});request.setSslConfiguration(tls);}
    }
#endif
    auto *r=m_network.get(request);m_reply=r;r->setReadBufferSize(16*1024*1024+1);connect(r,&QNetworkReply::downloadProgress,r,[r](qint64 n,qint64 total){if(n>16*1024*1024||total>16*1024*1024)r->abort();});
    connect(r,&QNetworkReply::finished,this,[this,r](){
        const auto bytes=r->isReadable()?r->readAll():QByteArray();const bool ok=r->error()==QNetworkReply::NoError;const auto error=r->errorString();m_reply=nullptr;r->deleteLater();
        if(!ok){m_problem="Cannot update TV Archive. Saved catalogue is kept. "+error;emit changed();return;}
        // Catalogue normalization can be noticeable on the device's small CPU.
        // Keep input, playback and the saved catalogue usable while it runs.
        m_parsing=true;auto *watcher=new QFutureWatcher<QJsonArray>(this);auto promise=std::make_shared<QPromise<QJsonArray>>();promise->start();
        connect(watcher,&QFutureWatcher<QJsonArray>::finished,this,[this,watcher](){
            const auto rows=watcher->result();watcher->deleteLater();m_parsing=false;
            if(rows.isEmpty())m_problem="Cannot update TV Archive. Saved catalogue is kept. No playable files returned.";
            else{m_catalogue=rows;indexCatalogue();m_updated=QDateTime::currentSecsSinceEpoch();write(cachePath(),{{"version",1},{"collection",collection()},{"updated",m_updated},{"episodes",rows}});if(m_healthy)m_problem.clear();}
            updateModels();
        });
        watcher->setFuture(promise->future());QThreadPool::globalInstance()->start([promise,bytes](){promise->addResult(parseCatalogue(QJsonDocument::fromJson(bytes).object()));promise->finish();});emit changed();
    });emit changed();
}
bool TvArchiveClient::start(const QString &id,bool restart,bool shuffle,const QString &scope){
    if(!DeviceConfig::enabled("tvarchive")||!m_healthy)return false;const auto e=episode(id);if(e.isEmpty()){m_problem="Choose an episode from the loaded catalogue.";emit changed();return false;}
    emit interruptPlayback();auto state=m_state;auto progress=state.value("progress").toObject();auto p=progress.value(id).toObject();if(progress.size()>=10000&&!progress.contains(id)){m_problem="Saved TV progress is full.";emit changed();return false;}
    if(restart||p.value("completed").toBool())p.insert("position",0);if(!p.contains("position"))p.insert("position",0);if(!p.contains("duration"))p.insert("duration",qMax(0.,e.value("duration").toDouble()));p.insert("completed",false);p.insert("lastPlayed",QDateTime::currentSecsSinceEpoch());progress.insert(id,p);state.insert("progress",progress);state.insert("last",id);state.insert("selected",e.value("seriesId"));auto cursor=state.value("continue").toObject();cursor.insert(e.value("seriesId").toString(),id);state.insert("continue",cursor);
    if(!commit(state))return false;if(m_selected!=e.value("seriesId").toString()){m_selected=e.value("seriesId").toString();m_query.clear();m_season.clear();updateModels();}m_playing=id;m_started=false;m_shuffle=shuffle;m_randomScope=scope;m_problem.clear();emit changed();emit playRequested(QUrl(e.value("url").toString()),e.value("seriesTitle").toString()+" · "+e.value("title").toString(),"tva/"+id,p.value("position").toDouble());return true;
}
bool TvArchiveClient::play(const QString &id,bool restart){return start(id,restart,false);}
bool TvArchiveClient::resume(const QString &seriesId){QString id;if(seriesId.isEmpty()){const auto e=episode(m_state.value("last").toString());id=e.isEmpty()?QString():continueId(e.value("seriesId").toString());}else id=continueId(seriesId);return play(id);}
bool TvArchiveClient::random(const QString &seriesId){if(!DeviceConfig::enabled("tvarchive"))return false;QStringList ids;for(const auto &v:m_catalogue){auto e=v.toObject();if(seriesId.isEmpty()||e.value("seriesId")==seriesId)ids<<e.value("id").toString();}if(ids.isEmpty())return false;if(ids.size()>1)ids.removeAll(m_playing);return start(ids[QRandomGenerator::global()->bounded(ids.size())],true,true,seriesId);}
void TvArchiveClient::report(const QString &key,const QString &event,double position,double duration,bool){
    if(!DeviceConfig::enabled("tvarchive")||!m_healthy||key!="tva/"+m_playing||m_playing.isEmpty()||!std::isfinite(position)||position<0||position>7*86400)return;auto s=m_state;auto all=s.value("progress").toObject();auto p=all.value(m_playing).toObject();
    if(event=="start")m_started=true;else{if(!m_started)return;p.insert("position",position);}if(std::isfinite(duration)&&duration>0&&duration<7*86400)p.insert("duration",duration);all.insert(m_playing,p);if(all==s.value("progress").toObject())return;s.insert("progress",all);commit(s);
}
QString TvArchiveClient::nextId(const QString &id)const{const auto e=episode(id);bool found=false;for(const auto &v:m_catalogue){const auto r=v.toObject();if(found&&r.value("seriesId")==e.value("seriesId")&&r.value("extra").toBool()==e.value("extra").toBool())return r.value("id").toString();if(r.value("id")==id)found=true;}return {};}
void TvArchiveClient::completed(const QString &key){
    if(!DeviceConfig::enabled("tvarchive")||!m_healthy||!m_started||key!="tva/"+m_playing)return;const auto id=m_playing;const auto e=episode(id);auto s=m_state;auto all=s.value("progress").toObject();auto p=all.value(id).toObject();p.insert("completed",true);if(p.value("duration").toDouble()>0)p.insert("position",p.value("duration"));all.insert(id,p);s.insert("progress",all);const auto next=nextId(id);if(!next.isEmpty()){auto cursor=s.value("continue").toObject();cursor.insert(e.value("seriesId").toString(),next);s.insert("continue",cursor);}if(!commit(s))return;m_started=false;if(autoplay()){if(m_shuffle)random(m_randomScope);else if(!next.isEmpty())play(next);}
}
QJsonObject TvArchiveClient::request(const QJsonObject &r){
    if(!DeviceConfig::enabled("tvarchive"))return {{"ok",false},{"error","TV Archive is disabled."}};const auto action=r.value("action").toString();bool ok=true;QJsonObject result;
    if(action=="tvarchive.refresh")refresh();else if(action=="tvarchive.play")ok=play(r.value("id").toString());else if(action=="tvarchive.resume")ok=resume(r.value("id").toString());else if(action=="tvarchive.random")ok=random(r.value("id").toString());else if(action=="tvarchive.autoplay"){if(!r.value("enabled").isBool())ok=false;else{setAutoplay(r.value("enabled").toBool());ok=autoplay()==r.value("enabled").toBool();}}
    else if(action=="tvarchive.list"){const auto sid=r.value("id").toString(),query=r.value("search").toString();QJsonArray rows;QSet<QString> seen;for(const auto &v:m_catalogue){auto e=v.toObject();if(!query.isEmpty()&&!e.value("title").toString().contains(query,Qt::CaseInsensitive)&&!e.value("seriesTitle").toString().contains(query,Qt::CaseInsensitive))continue;if(!sid.isEmpty()){if(e.value("seriesId")!=sid)continue;rows.append(summary(e));}else{const auto id=e.value("seriesId").toString();if(seen.contains(id))continue;seen.insert(id);rows.append(QJsonObject{{"id",id},{"title",e.value("seriesTitle")},{"next",summary(episode(continueId(id)))}});}if(rows.size()>=20)break;}result.insert("items",rows);}
    else if(action!="tvarchive.status")return {{"ok",false},{"error","Unknown TV Archive action."}};
    result.insert("ok",ok);result.insert("current",QJsonObject::fromVariantMap(current()));result.insert("autoplay",autoplay());result.insert("loaded",!m_catalogue.isEmpty());result.insert("active",active());result.insert("updated",m_updated);result.insert("busy",busy());result.insert("error",ok?m_problem:!m_problem.isEmpty()?m_problem:"Choose a known series/episode from TV Archive.");if(action=="tvarchive.play"||action=="tvarchive.resume"||action=="tvarchive.random")result.insert("accepted",ok);return result;
}
