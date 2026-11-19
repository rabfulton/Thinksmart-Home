#include "audiobooks.h"
#include "deviceconfig.h"
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QSaveFile>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QTextDocumentFragment>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QPointer>
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
#include <QNetworkProxy>
#endif
#include <QImageReader>
#include <QBuffer>
#include <QSet>
#include <algorithm>
#include <cmath>

namespace {
QString plain(const QString &s,int limit=12000){return QTextDocumentFragment::fromHtml(s).toPlainText().trimmed().left(limit);}
QString catalogUrl(QString value){
    QUrl u(value);if(u.host().toLower()!="librivox.org"||!u.userInfo().isEmpty()||u.port(-1)!=-1)return {};
    const auto parts=u.path().split('/',Qt::SkipEmptyParts);if(parts.size()!=1||QStringList{"feed","api","category","pages","rss"}.contains(parts.first())||parts.first().contains('.'))return {};
    u.setScheme("https");u.setQuery(QString());u.setFragment(QString());u.setPath('/'+parts.first()+'/');return u.toString();
}
QString catalogId(const QString &url){return "catalog:"+QUrl(url).path().split('/',Qt::SkipEmptyParts).value(0);}
QUrl audioUrl(const QString &s){QUrl u(s);const auto host=u.host().toLower();if((host!="archive.org"&&!host.endsWith(".archive.org"))||!u.userInfo().isEmpty()||u.port(-1)!=-1)return {};if(u.scheme()!="https"&&u.scheme()!="http")return {};u.setScheme("https");return u;}
bool numericId(const QString &id){return QRegularExpression("^lv:[1-9][0-9]{0,8}$").match(id).hasMatch();}
qint64 stamp(const QJsonValue &v){return qint64(v.toDouble());}
QJsonObject readJson(const QString &path,int limit=2*1024*1024){QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>limit)return {};return QJsonDocument::fromJson(f.readAll()).object();}
bool writeJson(const QString &path,const QJsonObject &data){QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))return false;f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto bytes=QJsonDocument(data).toJson(QJsonDocument::Compact);return f.write(bytes)==bytes.size()&&f.commit();}
QString metadataPath(const QString &id){return AudiobooksClient::cacheDirectory()+"/books/"+id.mid(3)+".json";}
QString chapterKey(const QString &book,const QString &chapter){return "ab/"+book+"/"+chapter;}
}
QString AudiobooksClient::libraryPath(){return qEnvironmentVariable("THINKSMART_AUDIOBOOKS_FILE",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/audiobooks/library.json");}
QString AudiobooksClient::cacheDirectory(){return qEnvironmentVariable("THINKSMART_AUDIOBOOKS_CACHE",QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/audiobooks");}
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
QString AudiobooksClient::proxyUrl(){
    const QUrl u(qEnvironmentVariable("THINKSMART_AUDIOBOOKS_PROXY").trimmed());
    if(!u.isValid()||u.scheme()!="http"||!QStringList{"127.0.0.1","::1","localhost"}.contains(u.host())||u.port()<1||u.port()>65535||!u.userInfo().isEmpty()||u.hasQuery()||u.hasFragment()||(!u.path().isEmpty()&&u.path()!="/"))return {};
    return u.toString(QUrl::RemovePath);
}
#endif
QStringList AudiobooksClient::playerOptions(const QString &source){
    if(source!="audiobooks")return {};
    QStringList options{"--tls-verify=yes","--tls-ca-file=/etc/ssl/certs/ca-certificates.crt"};
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
    const auto proxy=proxyUrl();if(!proxy.isEmpty())options<<"--http-proxy="+proxy;
#endif
    return options;
}
QJsonObject AudiobooksClient::initialBook(){return {{"id","lv:753"},{"title","Moby Dick, or the Whale"},{"author","Herman Melville"},{"readers","Stewart Wills"},{"catalogUrl","https://librivox.org/moby-dick-by-herman-melville/"},{"language","English"},{"chapterCount",44},{"duration",88670},{"chapterIndex",0},{"position",0},{"completed",false}};}
QJsonObject AudiobooksClient::normalize(const QJsonObject &in){
    const auto id=in.value("id").toVariant().toString();if(!numericId("lv:"+id))return {};const auto title=plain(in.value("title").toString(),500),url=catalogUrl(in.value("url_librivox").toString());if(title.isEmpty()||url.isEmpty())return {};
    QStringList authors,genres,readers;for(const auto &v:in.value("authors").toArray()){auto a=v.toObject();authors<<plain(a.value("first_name").toString()+" "+a.value("last_name").toString(),150);}
    for(const auto &v:in.value("genres").toArray())genres<<plain(v.toObject().value("name").toString(),100);
    QJsonArray tracks;QSet<QString> ids;
    for(const auto &v:in.value("sections").toArray()){
        const auto s=v.toObject();const auto tid=s.value("id").toVariant().toString();const auto stream=audioUrl(s.value("listen_url").toString());const int number=s.value("section_number").toVariant().toInt();
        if(!QRegularExpression("^[1-9][0-9]{0,10}$").match(tid).hasMatch()||ids.contains(tid)||stream.isEmpty()||number<1)continue;
        const double duration=s.value("playtime").toVariant().toDouble();if(!std::isfinite(duration)||duration<0||duration>7*86400)continue;
        QStringList voices;for(const auto &r:s.value("readers").toArray()){const auto name=plain(r.toObject().value("display_name").toString(),100);if(!name.isEmpty()){voices<<name;if(!readers.contains(name)&&readers.size()<10)readers<<name;}}
        tracks.append(QJsonObject{{"id",tid},{"number",number},{"title",plain(s.value("title").toString(),500)},{"url",stream.toString()},{"duration",duration},{"readers",voices.join(", ")}});ids.insert(tid);if(tracks.size()>=1500)break;
    }
    QList<QJsonObject> ordered;for(const auto &v:tracks)ordered<<v.toObject();std::sort(ordered.begin(),ordered.end(),[](const auto &a,const auto &b){return a.value("number").toInt()<b.value("number").toInt();});tracks={};for(const auto &v:ordered)tracks.append(v);
    QJsonObject out{{"id","lv:"+id},{"title",title},{"author",authors.join(", ")},{"description",plain(in.value("description").toString())},{"catalogUrl",url},{"language",plain(in.value("language").toString(),80)},
        {"genres",QJsonArray::fromStringList(genres)},{"readers",readers.join(", ")},{"chapterCount",in.value("num_sections").toVariant().toInt()},{"duration",in.value("totaltimesecs").toVariant().toDouble()},{"tracks",tracks}};
    const auto cover=audioUrl(in.value("coverart_thumbnail").toString());if(!cover.isEmpty())out.insert("cover",cover.toString());return out;
}
QJsonArray AudiobooksClient::parseTop(const QByteArray &bytes,QString *generated){
    const auto doc=QJsonDocument::fromJson(bytes).object();if(generated)*generated=doc.value("generatedAt").toString().left(40);QJsonArray result;QSet<QString> ids;
    for(const auto &v:doc.value("books").toArray()){
        const auto r=v.toObject();const int rank=r.value("r").toInt();const auto url=catalogUrl(r.value("l").toString());const auto title=plain(r.value("t").toString(),500);if(rank<1||rank>500||url.isEmpty()||title.isEmpty()||ids.contains(url))continue;
        result.append(QJsonObject{{"id",catalogId(url)},{"title",title},{"author",plain(r.value("a").toString(),300)},{"catalogUrl",url},{"genres",QJsonArray::fromStringList(r.value("y").toString().left(500).split('|',Qt::SkipEmptyParts))},{"duration",r.value("h").toDouble()*3600},{"rank",rank}});ids.insert(url);
    }
    QList<QJsonObject> ordered;for(const auto &v:result)ordered<<v.toObject();std::sort(ordered.begin(),ordered.end(),[](const auto &a,const auto &b){return a.value("rank").toInt()<b.value("rank").toInt();});result={};for(const auto &v:ordered){if(result.size()>=100)break;result.append(v);}return result;
}
QJsonObject AudiobooksClient::parsePicks(const QByteArray &bytes){
    QXmlStreamReader xml(bytes);QJsonArray rows,periods;QSet<QString> periodsSeen;
    while(!xml.atEnd()&&periods.size()<20){xml.readNext();if(!xml.isStartElement()||xml.name()!=u"item")continue;QString title,date,content,description,link;
        while(xml.readNextStartElement()){
            const auto n=xml.name();if(n==u"title")title=xml.readElementText();else if(n==u"pubDate")date=xml.readElementText();else if(n==u"encoded")content=xml.readElementText();else if(n==u"description")description=xml.readElementText();else if(n==u"link")link=xml.readElementText();else xml.skipCurrentElement();
        }
        const auto t=QDateTime::fromString(date,Qt::RFC2822Date);if(!t.isValid())continue;const auto period=QString::number(t.toSecsSinceEpoch());if(periodsSeen.contains(period))continue;
        QRegularExpression anchors(R"rx(<a\b[^>]*href\s*=\s*["']([^"']+)["'][^>]*>([\s\S]*?)</a>)rx",QRegularExpression::CaseInsensitiveOption);auto matches=anchors.globalMatch(content.isEmpty()?description:content);QSet<QString> seen;QJsonArray books;
        while(matches.hasNext()&&books.size()<30){const auto m=matches.next();const auto url=catalogUrl(m.captured(1));const auto name=plain(m.captured(2),500);if(url.isEmpty()||name.isEmpty()||seen.contains(url))continue;seen.insert(url);books.append(QJsonObject{{"id",catalogId(url)},{"title",name},{"catalogUrl",url},{"period",period}});}
        if(books.isEmpty())continue;periodsSeen.insert(period);periods.append(QJsonObject{{"id",period},{"title",plain(title,200)},{"label",t.date().toString("MMM yyyy")+" · "+plain(title,120)},{"date",t.date().toString(Qt::ISODate)}});for(const auto &b:books)if(rows.size()<400)rows.append(b);
    }
    if(xml.hasError())return {};return {{"books",rows},{"periods",periods}};
}
AudiobooksClient::AudiobooksClient(QObject *parent):QObject(parent){
    if(!DeviceConfig::enabled("audiobooks"))return;
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
    const auto proxy=proxyUrl();
    if(!qEnvironmentVariable("THINKSMART_AUDIOBOOKS_PROXY").trimmed().isEmpty()&&proxy.isEmpty()){m_storeHealthy=false;m_problem="Invalid Audiobooks proxy. Use an HTTP proxy on localhost with a port, or remove THINKSMART_AUDIOBOOKS_PROXY.";return;}
    if(!proxy.isEmpty()){const QUrl u(proxy);m_archiveNetwork.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy,u.host(),quint16(u.port())));}
#endif
    load();updateModel();
}
AudiobooksClient::~AudiobooksClient(){
    auto abort=[this](QNetworkAccessManager &manager){for(auto *r:manager.findChildren<QNetworkReply *>()){r->disconnect(this);r->abort();}};
    abort(m_network);
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
    abort(m_archiveNetwork);
#endif
}
void AudiobooksClient::load(){
    QFile f(libraryPath());if(!f.exists())m_books.insert("lv:753",initialBook());else{
        QJsonParseError e;const bool readable=f.open(QIODevice::ReadOnly)&&f.size()<=1024*1024;const auto d=QJsonDocument::fromJson(readable?f.readAll():QByteArray(),&e).object();const auto rows=d.value("books").toArray();bool valid=readable&&e.error==QJsonParseError::NoError&&d.value("version")==1&&d.value("books").isArray()&&rows.size()<=100;
        for(const auto &v:rows){const auto b=v.toObject();const auto id=b.value("id").toString();const auto pos=b.value("position").toDouble(-1);if(!numericId(id)||m_books.contains(id)||b.value("title").toString().isEmpty()||catalogUrl(b.value("catalogUrl").toString()).isEmpty()||!std::isfinite(pos)||pos<0||pos>7*86400||b.value("chapterIndex").toInt(-1)<0||b.value("chapterIndex").toInt()>1499)valid=false;m_books.insert(id,b);}
        m_current=d.value("currentId").toString();if(!m_current.isEmpty()&&!m_books.contains(m_current))valid=false;
        if(!valid){m_books.clear();m_current.clear();m_storeHealthy=false;m_problem="Saved audiobook positions are invalid. File left untouched; repair it before playback.";}
    }
    const auto c=readJson(cacheDirectory()+"/discovery.json");m_top=c.value("top").toArray();m_picks=c.value("picks").toArray();m_periods=c.value("periods").toArray();m_topAt=stamp(c.value("topAt"));m_picksAt=stamp(c.value("picksAt"));m_topGenerated=c.value("topGenerated").toString();
    if(m_top.isEmpty()){QFile seed(":/data/audiobooks/top100.json");if(seed.open(QIODevice::ReadOnly))m_top=parseTop(seed.readAll(),&m_topGenerated);}
    m_bundledTop=m_topAt<=0&&!m_top.isEmpty();
    if(!m_periods.isEmpty())m_period=m_periods.first().toObject().value("id").toString();for(const auto &v:m_top)m_known.insert(v.toObject().value("id").toString(),v.toObject());for(const auto &v:m_picks)m_known.insert(v.toObject().value("id").toString(),v.toObject());
}
QJsonObject AudiobooksClient::summary(QJsonObject b)const{b.remove("tracks");const auto saved=m_books.value(b.value("id").toString());b.insert("saved",!saved.isEmpty());for(const auto &k:{"chapterId","chapterIndex","position","completed","lastPlayed"})if(saved.contains(k))b.insert(k,saved.value(k));return b;}
QJsonObject AudiobooksClient::known(const QString &id)const{if(m_books.contains(id))return m_books.value(id);if(m_known.contains(id))return m_known.value(id);if(m_metadata.contains(id))return m_metadata.value(id);if(id=="lv:753")return initialBook();return {};}
QVariantMap AudiobooksClient::current()const{return summary(m_books.value(m_current,m_storeHealthy&&DeviceConfig::enabled("audiobooks")?initialBook():QJsonObject())).toVariantMap();}
QVariantMap AudiobooksClient::detail()const{return summary(m_detail).toVariantMap();}
QVariantList AudiobooksClient::chapters()const{return m_detail.value("tracks").toArray().toVariantList();}
QStringList AudiobooksClient::genres()const{QStringList r{"All genres"};for(const auto &v:m_top)for(const auto &g:v.toObject().value("genres").toArray())if(!r.contains(g.toString()))r<<g.toString();std::sort(r.begin()+1,r.end());return r;}
QString AudiobooksClient::status()const{if(m_preparing)return "Preparing audiobook…";if(busy())return "Loading…";if(m_tab=="top")return "Top 100 · bstjohn.net · Gutenberg popularity"+(m_topGenerated.isEmpty()?QString():" · "+m_topGenerated.left(10));if(m_tab=="picks")return "LibriVox monthly picks · dates as published";if(m_tab=="search")return "LibriVox catalogue · 20 results at a time";return "Your books · listening positions saved on this device";}
void AudiobooksClient::updateModel(){
    QJsonArray rows;if(m_tab=="mine"){QList<QJsonObject> books=m_books.values();std::stable_sort(books.begin(),books.end(),[this](const auto &a,const auto &b){if(a.value("id")==m_current)return b.value("id")!=m_current;if(b.value("id")==m_current)return false;return stamp(a.value("lastPlayed"))>stamp(b.value("lastPlayed"));});for(const auto &b:books)rows.append(summary(b));}
    else for(const auto &v:m_tab=="top"?m_top:m_tab=="picks"?m_picks:m_search){auto b=v.toObject();if(m_tab=="picks"&&b.value("period")!=m_period)continue;if(m_tab=="top"&&!m_genre.isEmpty()&&!b.value("genres").toArray().contains(m_genre))continue;rows.append(summary(b));}
    m_items.setRows(rows);emit changed();
}
void AudiobooksClient::setActive(bool active){if(!DeviceConfig::enabled("audiobooks")||active==m_active)return;m_active=active;emit activeChanged();if(active&&!qEnvironmentVariableIsSet("THINKSMART_DISABLE_AUDIOBOOKS")){if(m_tab=="top")loadTop();else if(m_tab=="picks")loadPicks();}}
void AudiobooksClient::setTab(const QString &tab){if(!DeviceConfig::enabled("audiobooks")||!QStringList{"mine","top","search","picks"}.contains(tab))return;closeDetails();m_tab=tab;updateModel();if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_AUDIOBOOKS")){if(tab=="top")loadTop();if(tab=="picks")loadPicks();}}
void AudiobooksClient::setGenre(const QString &genre){m_genre=genre=="All genres"?QString():genre;updateModel();}
void AudiobooksClient::setPeriod(const QString &period){for(const auto &v:m_periods)if(v.toObject().value("id")==period){m_period=period;updateModel();return;}}
void AudiobooksClient::cacheDiscovery(){writeJson(cacheDirectory()+"/discovery.json",{{"top",m_top},{"picks",m_picks},{"periods",m_periods},{"topAt",m_topAt},{"picksAt",m_picksAt},{"topGenerated",m_topGenerated}});}
void AudiobooksClient::get(const QUrl &url,std::function<void(QByteArray)> done){if(!DeviceConfig::enabled("audiobooks")||!m_storeHealthy){done({});return;}m_jobs.enqueue({url,std::move(done)});pump();}
void AudiobooksClient::pump(){
    while(m_running<2&&!m_jobs.isEmpty()){
        auto job=m_jobs.dequeue();if(QDateTime::currentSecsSinceEpoch()<m_retryAfter){job.done({});continue;}++m_running;auto url=job.url;
#ifdef THINKSMART_AUDIOBOOKS_TESTING
        if(qEnvironmentVariableIsSet("THINKSMART_AUDIOBOOKS_TEST_BASE")){const auto original=url;url=QUrl(qEnvironmentVariable("THINKSMART_AUDIOBOOKS_TEST_BASE"));url.setPath(original.host().contains("bstjohn")?"/top":original.path().contains("monthly-picks")?"/picks":original.path().startsWith("/api/")?"/api":"/catalog"+original.path());url.setQuery(original.query());}
#endif
        QNetworkRequest request(url);request.setRawHeader("User-Agent","ThinkSmartHome/0.3 (native LibriVox browser)");request.setTransferTimeout(20000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);request.setMaximumRedirectsAllowed(4);
        auto *manager=&m_network;
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
        if(!audioUrl(job.url.toString()).isEmpty())manager=&m_archiveNetwork;
#endif
        auto *r=manager->get(request);r->setReadBufferSize(2*1024*1024+1);connect(r,&QNetworkReply::downloadProgress,r,[r](qint64 n,qint64 total){if(n>2*1024*1024||total>2*1024*1024)r->abort();});connect(r,&QNetworkReply::readyRead,r,[r](){if(r->bytesAvailable()>2*1024*1024)r->abort();});
        connect(r,&QNetworkReply::finished,this,[this,r,job](){--m_running;const int code=r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const auto bytes=r->isReadable()?r->readAll():QByteArray();const bool ok=r->error()==QNetworkReply::NoError&&code==200&&bytes.size()<=2*1024*1024;m_requestError=ok?QString():code>0?QString("HTTP %1").arg(code):r->errorString();r->deleteLater();if(code==429){m_retryAfter=QDateTime::currentSecsSinceEpoch()+300;m_problem="LibriVox rate limit; retrying in five minutes.";}job.done(ok?bytes:QByteArray());pump();emit changed();});
    }emit changed();
}
void AudiobooksClient::loadTop(bool force){
    const auto now=QDateTime::currentSecsSinceEpoch();if(!force&&(m_bundledTop||(!m_top.isEmpty()&&now-m_topAt<7*86400)))return;if(now-m_topAttempt<(force?3:60)){if(force){m_problem="Please wait a few seconds before updating again.";emit changed();}return;}m_topAttempt=now;
    get(QUrl("https://www.bstjohn.net/data/audiobooks-full.json"),[this](const auto &bytes){QString date;const auto rows=parseTop(bytes,&date);if(rows.isEmpty())m_problem="Cannot update Top 100; saved recommendations are kept. "+m_requestError;else{m_top=rows;m_bundledTop=false;m_topGenerated=date;m_topAt=QDateTime::currentSecsSinceEpoch();for(const auto &v:rows)m_known.insert(v.toObject().value("id").toString(),v.toObject());cacheDiscovery();}updateModel();});
}
void AudiobooksClient::loadPicks(bool force){
    const auto now=QDateTime::currentSecsSinceEpoch();if(!force&&!m_picks.isEmpty()&&now-m_picksAt<86400)return;if(now-m_picksAttempt<(force?3:60)){if(force){m_problem="Please wait a few seconds before updating again.";emit changed();}return;}m_picksAttempt=now;
    get(QUrl("https://librivox.org/category/monthly-picks/feed/"),[this](const auto &bytes){const auto parsed=parsePicks(bytes);if(parsed.value("books").toArray().isEmpty())m_problem="Cannot update monthly picks; saved selections are kept. "+m_requestError;else{m_picks=parsed.value("books").toArray();m_periods=parsed.value("periods").toArray();m_picksAt=QDateTime::currentSecsSinceEpoch();bool found=false;for(const auto &v:m_periods)found|=v.toObject().value("id")==m_period;if(!found)m_period=m_periods.first().toObject().value("id").toString();for(const auto &v:m_picks)m_known.insert(v.toObject().value("id").toString(),v.toObject());cacheDiscovery();}updateModel();});
}
void AudiobooksClient::refresh(){if(!DeviceConfig::enabled("audiobooks"))return;if(m_storeHealthy)m_problem.clear();if(m_tab=="top")loadTop(true);else if(m_tab=="picks")loadPicks(true);else if(m_tab=="search"&&!m_query.isEmpty())search(m_query,m_byAuthor);}
void AudiobooksClient::search(const QString &text,bool author){if(!DeviceConfig::enabled("audiobooks"))return;m_query=text.trimmed().left(100);m_byAuthor=author;m_offset=0;++m_searchSerial;m_search={};m_tab="search";m_hasMore=false;closeDetails();updateModel();if(m_query.isEmpty())return;runSearch(false);}
void AudiobooksClient::loadMore(){if(m_tab=="search"&&m_hasMore&&!busy()&&m_offset<1000){m_offset+=20;runSearch(true);}}
void AudiobooksClient::runSearch(bool append,Done done){
    const auto serial=m_searchSerial;QUrl url("https://librivox.org/api/feed/audiobooks/");QUrlQuery q;q.addQueryItem("format","json");q.addQueryItem(m_byAuthor?"author":"title",m_query);q.addQueryItem("limit","20");q.addQueryItem("offset",QString::number(m_offset));url.setQuery(q);
    get(url,[this,serial,append,done](const auto &bytes){const auto doc=QJsonDocument::fromJson(bytes).object();QJsonArray rows;for(const auto &v:doc.value("books").toArray()){const auto book=normalize(v.toObject());if(!book.isEmpty()){auto b=book;b.remove("tracks");rows.append(b);m_known.insert(b.value("id").toString(),b);}}
        const bool ok=!bytes.isEmpty()&&doc.contains("books");if(serial==m_searchSerial){if(!append)m_search={};QSet<QString> seen;for(const auto &v:m_search)seen.insert(v.toObject().value("id").toString());for(const auto &v:rows)if(!seen.contains(v.toObject().value("id").toString()))m_search.append(v);m_hasMore=doc.value("books").toArray().size()==20&&m_offset<1000;if(!ok)m_problem="Cannot search LibriVox right now.";updateModel();}
        if(done){QJsonArray small;for(int i=0;i<rows.size()&&i<5;++i)small.append(summary(rows[i].toObject()));done({{"ok",ok},{"books",small},{"error",ok?QString():"LibriVox search unavailable"}});}
    });
}
void AudiobooksClient::remember(const QJsonObject &book){const auto id=book.value("id").toString();m_metadata.insert(id,book);m_known.insert(id,summary(book));writeJson(metadataPath(id),book);while(m_metadata.size()>30){auto it=m_metadata.begin();if(it.key()==m_current||it.key()==m_detail.value("id").toString())++it;if(it==m_metadata.end())break;m_metadata.erase(it);}QDir dir(cacheDirectory()+"/books");const auto files=dir.entryInfoList({"*.json"},QDir::Files,QDir::Time);for(int i=30;i<files.size();++i)QFile::remove(files[i].absoluteFilePath());}
void AudiobooksClient::fetchBook(const QString &id,Done done){
    QUrl url("https://librivox.org/api/feed/audiobooks/");QUrlQuery q;q.addQueryItem("id",id.mid(3));q.addQueryItem("format","json");q.addQueryItem("extended","1");q.addQueryItem("coverart","1");url.setQuery(q);
    get(url,[this,id,done](const auto &bytes){const auto rows=QJsonDocument::fromJson(bytes).object().value("books").toArray();const auto b=rows.isEmpty()?QJsonObject():normalize(rows.first().toObject());if(b.value("id")!=id||b.value("tracks").toArray().isEmpty()){m_problem="Cannot load this recording's chapters. Try again.";done({});return;}remember(b);done(b);});
}
void AudiobooksClient::resolve(const QString &id,Done done){
    auto b=known(id);if(b.isEmpty()){m_problem="Choose a book from My books, Top 100, picks or search.";done({});return;}
    QString resolved=id;if(!numericId(resolved))for(const auto &saved:m_books)if(saved.value("catalogUrl")==b.value("catalogUrl")){resolved=saved.value("id").toString();break;}
    if(numericId(resolved)){
        auto full=m_metadata.value(resolved);if(full.isEmpty())full=readJson(metadataPath(resolved));if(full.value("id")==resolved&&!full.value("tracks").toArray().isEmpty()){m_metadata.insert(resolved,full);done(full);return;}
    }
    if(m_resolving.contains(id)){m_resolving[id].append(std::move(done));return;}m_resolving[id].append(std::move(done));
    auto finish=[this,id](QJsonObject book){const auto callbacks=m_resolving.take(id);for(const auto &callback:callbacks)callback(book);emit changed();};
    if(numericId(resolved)){fetchBook(resolved,finish);return;}
    const auto page=catalogUrl(b.value("catalogUrl").toString());if(page.isEmpty()){finish({});return;}
    get(QUrl(page),[this,finish](const auto &bytes){const auto match=QRegularExpression(R"rx((?:https?://(?:www\.)?librivox\.org)?/rss/([0-9]+))rx").match(QString::fromUtf8(bytes));if(!match.hasMatch()){m_problem="Cannot resolve this catalogue edition. Try a title search.";finish({});return;}fetchBook("lv:"+match.captured(1),finish);});
}
bool AudiobooksClient::inspect(const QString &id){if(!DeviceConfig::enabled("audiobooks")||known(id).isEmpty())return false;const auto serial=++m_inspectSerial;m_detail=known(id);emit changed();resolve(id,[this,serial](QJsonObject b){if(serial!=m_inspectSerial||b.isEmpty())return;m_detail=b;loadCover();emit changed();});return true;}
void AudiobooksClient::loadCover(){
    const auto id=m_detail.value("id").toString();const auto url=audioUrl(m_detail.value("cover").toString());if(!numericId(id)||url.isEmpty())return;
    const auto dir=cacheDirectory()+"/covers",path=dir+'/'+id.mid(3)+".img";
    auto show=[this,id,path](){if(m_detail.value("id")==id){m_detail.insert("coverLocal",QUrl::fromLocalFile(path).toString());emit changed();}};
    if(QFileInfo(path).isFile()){show();return;}
    get(url,[this,path,dir,show](const QByteArray &bytes){
        if(bytes.isEmpty())return;QBuffer buffer;buffer.setData(bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);const auto size=reader.size();if(!reader.canRead()||size.width()<1||size.height()<1||size.width()>2048||size.height()>2048)return;
        QDir().mkpath(dir);QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))return;f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);if(f.write(bytes)!=bytes.size()||!f.commit())return;
        QDir cache(dir);const auto files=cache.entryInfoList({"*.img"},QDir::Files,QDir::Time);for(int i=30;i<files.size();++i)QFile::remove(files[i].absoluteFilePath());show();
    });
}
void AudiobooksClient::closeDetails(){++m_inspectSerial;m_detail={};emit changed();}
bool AudiobooksClient::commit(const QMap<QString,QJsonObject> &books,const QString &current){
    if(!m_storeHealthy)return false;QJsonArray rows;for(auto b:books){b.remove("tracks");b.remove("saved");if(b.contains("description"))b.insert("description",b.value("description").toString().left(1000));rows.append(b);}if(!writeJson(libraryPath(),{{"version",1},{"currentId",current},{"books",rows}})){m_problem="Cannot save your listening position; playback was not started.";emit changed();return false;}m_books=books;m_current=current;updateModel();return true;
}
bool AudiobooksClient::saveBook(const QString &id){if(!DeviceConfig::enabled("audiobooks")||!m_storeHealthy||known(id).isEmpty())return false;resolve(id,[this](QJsonObject b){if(b.isEmpty())return;const auto id=b.value("id").toString();if(m_books.contains(id))return;if(m_books.size()>=100){m_problem="My books is full (100 books).";emit changed();return;}b.remove("tracks");b.insert("chapterIndex",0);b.insert("position",0);b.insert("completed",false);auto books=m_books;books.insert(id,b);commit(books,m_current.isEmpty()?id:m_current);});return true;}
bool AudiobooksClient::removeBook(const QString &id){if(!DeviceConfig::enabled("audiobooks")||!m_storeHealthy||!m_books.contains(id))return false;if(id==m_playing.value("id").toString())emit interruptPlayback();auto books=m_books;books.remove(id);return commit(books,m_current==id?(books.isEmpty()?QString():books.firstKey()):m_current);}
void AudiobooksClient::cancelPending(){++m_playSerial;m_preparing=false;emit changed();}
bool AudiobooksClient::play(const QString &id,bool restart){
    if(!DeviceConfig::enabled("audiobooks")||!m_storeHealthy||known(id).isEmpty())return false;const auto serial=++m_playSerial;m_preparing=true;if(m_storeHealthy)m_problem.clear();emit changed();resolve(id,[this,serial,restart](QJsonObject b){if(serial!=m_playSerial)return;m_preparing=false;if(!b.isEmpty())beginBook(b,restart);else emit changed();});return true;
}
bool AudiobooksClient::resume(){return play(m_current.isEmpty()?"lv:753":m_current);}
bool AudiobooksClient::beginBook(const QJsonObject &full,bool restart,int chapter){
    if(!m_storeHealthy)return false;const auto id=full.value("id").toString();if(!m_books.contains(id)&&m_books.size()>=100){m_problem="My books is full (100 books).";emit changed();return false;}const auto tracks=full.value("tracks").toArray();auto b=m_books.value(id);double position=b.value("position").toDouble();int index=b.value("chapterIndex").toInt();
    if(restart||b.value("completed").toBool()){index=0;position=0;}else if(chapter>=0){index=chapter;position=0;}else if(!b.value("chapterId").toString().isEmpty()){bool found=false;for(int i=0;i<tracks.size();++i)if(tracks[i].toObject().value("id")==b.value("chapterId")){index=i;found=true;break;}if(!found){m_problem="This recording's chapter list changed; choose a chapter to continue.";emit changed();return false;}}
    if(index<0||index>=tracks.size())return false;const auto track=tracks[index].toObject();const auto url=audioUrl(track.value("url").toString());if(url.isEmpty())return false;
    emit interruptPlayback();b=summary(full);b.remove("saved");b.insert("chapterIndex",index);b.insert("chapterId",track.value("id"));b.insert("chapterTitle",track.value("title"));b.insert("position",qBound(0.,position,track.value("duration").toDouble()>0?track.value("duration").toDouble():position));b.insert("completed",false);b.insert("lastPlayed",QDateTime::currentSecsSinceEpoch());auto books=m_books;if(!books.contains(id)&&books.size()>=100){m_problem="My books is full (100 books).";emit changed();return false;}books.insert(id,b);if(!commit(books,id))return false;
    m_playing=full;m_key=chapterKey(id,track.value("id").toString());m_started=false;emit playRequested(url,full.value("title").toString(),track.value("title").toString(),m_key,b.value("position").toDouble());return true;
}
bool AudiobooksClient::playChapter(int index){if(!DeviceConfig::enabled("audiobooks")||!m_storeHealthy||m_detail.value("tracks").toArray().isEmpty())return false;return beginBook(m_detail,false,index);}
bool AudiobooksClient::advance(int delta){if(!DeviceConfig::enabled("audiobooks")||m_playing.isEmpty())return false;const auto b=m_books.value(m_playing.value("id").toString());const int index=b.value("chapterIndex").toInt()+(delta<0?-1:1);if(index>=0&&index<m_playing.value("tracks").toArray().size())return beginBook(m_playing,false,index);return false;}
void AudiobooksClient::report(const QString &key,const QString &event,double position,bool){
    if(!DeviceConfig::enabled("audiobooks")||!m_storeHealthy||key!=m_key||!std::isfinite(position)||position<0||position>7*86400)return;
    const auto parts=key.split('/');if(parts.size()!=3||parts[0]!="ab"||!m_books.contains(parts[1]))return;auto b=m_books.value(parts[1]);if(b.value("chapterId").toString()!=parts[2])return;
    if(event=="start"){m_started=true;b.insert("lastPlayed",QDateTime::currentSecsSinceEpoch());}
    else {if(!m_started)return;b.insert("position",position);}
    if(b==m_books.value(parts[1]))return;auto books=m_books;books[parts[1]]=b;commit(books,m_current);
}
void AudiobooksClient::completed(const QString &key){
    if(key!=m_key||!m_started||m_playing.isEmpty()||!DeviceConfig::enabled("audiobooks"))return;const auto id=m_playing.value("id").toString();auto b=m_books.value(id);const int index=b.value("chapterIndex").toInt();const auto tracks=m_playing.value("tracks").toArray();
    if(index+1<tracks.size()){beginBook(m_playing,false,index+1);return;}b.insert("completed",true);b.insert("position",tracks[index].toObject().value("duration"));auto books=m_books;books.insert(id,b);commit(books,m_current);m_key.clear();m_started=false;
}
QJsonObject AudiobooksClient::snapshot()const{QJsonArray shelf;for(const auto &b:m_books)shelf.append(summary(b));return {{"current",QJsonObject::fromVariantMap(current())},{"books",shelf},{"preparing",m_preparing},{"busy",busy()},{"error",m_problem},{"provider","LibriVox"}};}
QJsonObject AudiobooksClient::request(const QJsonObject &r){
    if(!DeviceConfig::enabled("audiobooks"))return {{"ok",false},{"error","Audiobooks is disabled."}};const auto action=r.value("action").toString();bool ok=true;
    if(action=="audiobooks.play")ok=play(r.value("id").toString());else if(action=="audiobooks.resume")ok=resume();else if(action=="audiobooks.next")ok=advance(1);else if(action=="audiobooks.previous")ok=advance(-1);
    else if(action=="audiobooks.save")ok=saveBook(r.value("id").toString());else if(action=="audiobooks.remove")ok=removeBook(r.value("id").toString());
    else if(action!="audiobooks.status"&&action!="audiobooks.list")return {{"ok",false},{"error","Unknown audiobook action."}};
    auto out=snapshot();out.insert("ok",ok);if(action=="audiobooks.play"||action=="audiobooks.resume")out.insert("accepted",ok);if(!ok)out.insert("error",m_problem.isEmpty()?"Choose a known book.":m_problem);return out;
}
void AudiobooksClient::searchRequest(const QJsonObject &r,Done done){
    if(!DeviceConfig::enabled("audiobooks")){done({{"ok",false},{"error","Audiobooks is disabled."}});return;}const auto text=r.value("search").toString().trimmed();if(text.isEmpty()||text.size()>100){done({{"ok",false},{"error","Supply a title or author search, at most 100 characters."}});return;}m_query=text;m_byAuthor=r.value("category")=="author";m_offset=0;++m_searchSerial;runSearch(false,std::move(done));
}
