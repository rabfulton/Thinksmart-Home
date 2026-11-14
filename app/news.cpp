#include "deviceconfig.h"
#include "news.h"
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QTextDocumentFragment>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUrlQuery>
#include <QDateTime>
#include <QTimeZone>
#include <QStandardPaths>
#include <QSaveFile>
#include <QFile>
#include <QDir>
#include <QCryptographicHash>
#include <QSet>

namespace {
constexpr qint64 daySeconds=24*60*60;
void scheduleDaily(QTimer &timer,qint64 updated){
    const auto remaining=qBound<qint64>(1,daySeconds-(QDateTime::currentSecsSinceEpoch()-updated),daySeconds);
    timer.start(int(remaining*1000));
}
QString plain(const QString &html,int limit=20000){return QTextDocumentFragment::fromHtml(html).toPlainText().trimmed().left(limit);}
QString when(qint64 time){return time>0?QDateTime::fromSecsSinceEpoch(time,QTimeZone("Europe/London")).toString("d MMM · HH:mm"):QString();}
QString cachePath(const QString &name){return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/news/"+name+".json";}
QJsonObject readCache(const QString &name){QFile f(cachePath(name));if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return {};return QJsonDocument::fromJson(f.readAll()).object();}
void writeCache(const QString &name,const QJsonObject &data){
    const auto path=cachePath(name);QDir().mkpath(QFileInfo(path).absolutePath());
    const auto bytes=QJsonDocument(data).toJson(QJsonDocument::Compact);if(bytes.size()>1024*1024)return;
    QSaveFile f(path);if(f.open(QIODevice::WriteOnly)){f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);f.write(bytes);f.commit();}
}
void shutdown(QNetworkAccessManager &network){for(auto reply:network.findChildren<QNetworkReply *>()){reply->disconnect();reply->abort();}}
void fetch(QNetworkAccessManager &network,const QUrl &url,std::function<void(const QByteArray &)> done){
    QNetworkRequest request(url);request.setTransferTimeout(15000);request.setMaximumRedirectsAllowed(4);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader("User-Agent","ThinkSmartHome/0.3 (native news reader)");
    auto reply=network.get(request);constexpr int limit=2*1024*1024;reply->setReadBufferSize(limit+1);
    QObject::connect(reply,&QNetworkReply::readyRead,reply,[reply](){if(reply->bytesAvailable()>limit)reply->abort();});
    QObject::connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 received,qint64 total){if(received>limit||total>limit)reply->abort();});
    QObject::connect(reply,&QNetworkReply::finished,&network,[reply,done](){
        const bool ok=reply->error()==QNetworkReply::NoError&&reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==200;
        const auto data=reply->readAll();reply->deleteLater();done(ok&&data.size()<=limit?data:QByteArray());
    });
}
QString itemId(const QVariant &v){return v.toMap().value("id").toString();}
QVariantMap normalizeStory(const QJsonObject &o){
    const auto id=o.value("id").toInteger();if(id<=0||o.value("deleted").toBool()||o.value("dead").toBool()||o.value("title").toString().isEmpty())return {};
    QUrl url(o.value("url").toString());if(url.scheme()!="https"&&url.scheme()!="http")url=QUrl();
    return {{"id",QString::number(id)},{"title",plain(o.value("title").toString(),1000)},{"text",plain(o.value("text").toString())},
        {"url",url.toString()},{"domain",url.host()},{"by",o.value("by").toString().left(100)},{"points",o.value("score").toInt()},
        {"count",o.value("descendants").toInt()},{"when",when(o.value("time").toInteger())},{"kids",o.value("kids").toArray().toVariantList()}};
}
QString updatedLabel(qint64 updated,bool busy,bool error){if(busy)return "Updating…";if(!updated)return "Not updated yet";return QString(error?"Saved · ":"Updated · ")+when(updated);}
}
QVariant NewsEntries::data(const QModelIndex &index,int role)const{if(!index.isValid()||index.row()<0||index.row()>=m_rows.size()||role!=Qt::UserRole)return {};return m_rows[index.row()];}
void NewsEntries::update(const QVariantList &rows){
    // Insert/remove only changed IDs, so opening replies preserves the reading position.
    for(int i=0;i<rows.size();++i){
        if(i<m_rows.size()&&itemId(m_rows[i])!=itemId(rows[i])){
            int found=-1;for(int j=i+1;j<m_rows.size();++j)if(itemId(m_rows[j])==itemId(rows[i])){found=j;break;}
            if(found>=0){beginRemoveRows({},i,found-1);m_rows.erase(m_rows.begin()+i,m_rows.begin()+found);endRemoveRows();}
            else{beginInsertRows({},i,i);m_rows.insert(i,rows[i]);endInsertRows();}
        }else if(i>=m_rows.size()){beginInsertRows({},i,i);m_rows.append(rows[i]);endInsertRows();}
        if(m_rows[i]!=rows[i]){m_rows[i]=rows[i];emit dataChanged(index(i),index(i),{Qt::UserRole});}
    }
    if(m_rows.size()>rows.size()){beginRemoveRows({},rows.size(),m_rows.size()-1);m_rows.erase(m_rows.begin()+rows.size(),m_rows.end());endRemoveRows();}
}

HeadlinesClient::HeadlinesClient(const QUrl &base,QObject *parent):QObject(parent),m_base(base){
    m_section=m_settings.value("news/section","WORLD_FINANCE").toString();bool valid=false;for(auto s:sections())valid|=s.toMap().value("id")==m_section;if(!valid)m_section="WORLD_FINANCE";
    load();m_poll.setSingleShot(true);connect(&m_poll,&QTimer::timeout,this,&HeadlinesClient::refresh);
    if(DeviceConfig::enabled("news")&&!qEnvironmentVariableIsSet("THINKSMART_DISABLE_NEWS"))scheduleDaily(m_poll,m_updated);
}
HeadlinesClient::~HeadlinesClient(){shutdown(m_network);}
QVariantList HeadlinesClient::sections()const{return {QVariantMap{{"id","WORLD_FINANCE"},{"label","World & finance"}},QVariantMap{{"id","WORLD"},{"label","International"}},QVariantMap{{"id","FINANCE"},{"label","Global finance"}},QVariantMap{{"id","BUSINESS"},{"label","Business"}},QVariantMap{{"id","TECHNOLOGY"},{"label","Technology"}},QVariantMap{{"id","SCIENCE"},{"label","Science"}}};}
void HeadlinesClient::load(){const auto c=readCache("headlines-"+m_section);m_items.update(c.value("items").toArray().toVariantList());m_updated=c.value("updated").toInteger();}
QString HeadlinesClient::status()const{return updatedLabel(m_updated,m_busy,!m_problem.isEmpty());}
void HeadlinesClient::setActive(bool active){if(active==m_active)return;m_active=active;if(active&&QDateTime::currentSecsSinceEpoch()-m_updated>=daySeconds)refresh();emit changed();}
void HeadlinesClient::setSection(const QString &section){
    if(section==m_section)return;bool valid=false;for(auto s:sections())valid|=s.toMap().value("id")==section;if(!valid)return;
    ++m_generation;m_section=section;m_settings.setValue("news/section",section);m_busy=false;m_problem.clear();m_attempt=0;load();emit changed();refresh();
}
QUrl HeadlinesClient::feedUrl(const QString &section)const{
    QUrl u=m_base.resolved(QUrl(section=="FINANCE"?QString("rss/search"):"rss/headlines/section/topic/"+section));
    QUrlQuery q;q.addQueryItem("hl","en-GB");q.addQueryItem("gl","GB");q.addQueryItem("ceid","GB:en");
    if(section=="FINANCE")q.addQueryItem("q",R"query(("global markets" OR "world economy" OR "central banks" OR "bond markets" OR "stock markets") when:2d)query");
    u.setQuery(q);return u;
}
QVariantMap HeadlinesClient::story(int index)const{const auto rows=m_items.rows();return index>=0&&index<rows.size()?rows[index].toMap():QVariantMap();}
QVariantList HeadlinesClient::parseRss(const QByteArray &bytes){
    QXmlStreamReader xml(bytes);QVariantList result;QSet<QString> seen;
    while(!xml.atEnd()){
        xml.readNext();if(!xml.isStartElement()||xml.name()!=u"item")continue;
        QString title,url,guid,source,date,description;
        while(xml.readNextStartElement()){
            const auto name=xml.name().toString();
            if(name=="title")title=xml.readElementText();else if(name=="link")url=xml.readElementText();else if(name=="guid")guid=xml.readElementText();
            else if(name=="source")source=xml.readElementText();else if(name=="pubDate")date=xml.readElementText();else if(name=="description")description=xml.readElementText();else xml.skipCurrentElement();
        }
        title=plain(title,1000);source=plain(source,150);if(!source.isEmpty()&&title.endsWith(" - "+source))title.chop(source.size()+3);
        const QUrl link(url.trimmed());if(title.isEmpty()||link.host().isEmpty()||(link.scheme()!="http"&&link.scheme()!="https"))continue;
        const QString id=QString::fromLatin1(QCryptographicHash::hash((guid.isEmpty()?url:guid).toUtf8(),QCryptographicHash::Sha256).toHex());if(seen.contains(id))continue;seen.insert(id);
        const auto time=QDateTime::fromString(date.endsWith(" GMT")?date.chopped(4)+" +0000":date,Qt::RFC2822Date);result.append(QVariantMap{{"id",id},{"title",title},{"source",source},{"url",link.toString()},{"summary",plain(description,12000)},{"when",time.isValid()?when(time.toSecsSinceEpoch()):QString()}});
        if(result.size()>=50)break;
    }
    return xml.hasError()?QVariantList():result;
}
void HeadlinesClient::refresh(){
    const qint64 now=QDateTime::currentSecsSinceEpoch();if(!DeviceConfig::enabled("news")||qEnvironmentVariableIsSet("THINKSMART_DISABLE_NEWS")||m_busy||now-m_attempt<60)return;
    m_attempt=now;m_busy=true;m_problem.clear();m_parts=m_section=="WORLD_FINANCE"?QStringList{"WORLD","FINANCE"}:QStringList{m_section};m_results.clear();
    for(const auto &part:m_parts){QVariantList saved;for(auto row:m_items.rows())if(row.toMap().value("section")==part)saved.append(row);m_results.append(saved);}
    const auto generation=++m_generation;emit changed();fetchPart(0,generation);
}
void HeadlinesClient::fetchPart(int part,quint64 generation){
    if(generation!=m_generation)return;
    fetch(m_network,feedUrl(m_parts[part]),[this,part,generation](const QByteArray &bytes){
        if(generation!=m_generation)return;auto rows=parseRss(bytes);
        if(rows.isEmpty())m_problem="Some feeds are unavailable. Saved headlines are kept.";
        else{for(auto &row:rows){auto item=row.toMap();item.insert("section",m_parts[part]);row=item;}m_results[part]=rows;}
        if(part+1<m_parts.size()){QTimer::singleShot(1000,this,[this,part,generation](){fetchPart(part+1,generation);});return;}
        QVariantList merged;QSet<QString> seen;
        for(int i=0;i<50&&merged.size()<50;i++)for(const auto &list:m_results)if(merged.size()<50&&i<list.size()&&!seen.contains(itemId(list[i]))){seen.insert(itemId(list[i]));merged.append(list[i]);}
        if(!merged.isEmpty()){m_items.update(merged);if(m_problem.isEmpty())m_updated=QDateTime::currentSecsSinceEpoch();writeCache("headlines-"+m_section,{{"updated",m_updated},{"items",QJsonArray::fromVariantList(merged)}});}
        m_busy=false;m_poll.start(int(daySeconds*1000));emit changed();
    });
}

HackerNewsClient::HackerNewsClient(const QUrl &base,QObject *parent):QObject(parent),m_base(base){
    const auto c=readCache("hn-stories");m_items.update(c.value("items").toArray().toVariantList());m_updated=c.value("updated").toInteger();
    m_poll.setSingleShot(true);connect(&m_poll,&QTimer::timeout,this,&HackerNewsClient::refresh);
    if(DeviceConfig::enabled("news")&&!qEnvironmentVariableIsSet("THINKSMART_DISABLE_NEWS"))scheduleDaily(m_poll,m_updated);
}
HackerNewsClient::~HackerNewsClient(){shutdown(m_network);}
QString HackerNewsClient::status()const{return updatedLabel(m_updated,m_busy,!m_problem.isEmpty());}
void HackerNewsClient::setActive(bool active){if(active==m_active)return;m_active=active;if(active&&QDateTime::currentSecsSinceEpoch()-m_updated>=daySeconds)refresh();emit changed();}
void HackerNewsClient::get(const QString &path,std::function<void(const QByteArray &)> done,quint64 thread){m_jobs.enqueue({m_base.resolved(QUrl(path)),done,thread});pump();}
void HackerNewsClient::pump(){
    while(m_inflight<4&&!m_jobs.isEmpty()){
        auto job=m_jobs.dequeue();if(job.thread&&job.thread!=m_thread)continue;++m_inflight;
        fetch(m_network,job.url,[this,job](const QByteArray &data){--m_inflight;if(!job.thread||job.thread==m_thread)job.done(data);pump();});
    }
}
void HackerNewsClient::refresh(){
    const auto now=QDateTime::currentSecsSinceEpoch();if(!DeviceConfig::enabled("news")||qEnvironmentVariableIsSet("THINKSMART_DISABLE_NEWS")||m_busy||now-m_attempt<60)return;
    m_attempt=now;m_busy=true;m_problem.clear();emit changed();
    get("topstories.json",[this](const QByteArray &bytes){
        const auto doc=QJsonDocument::fromJson(bytes);if(!doc.isArray()||doc.array().isEmpty()){m_busy=false;m_problem="Cannot update Hacker News. Saved stories are kept.";m_poll.start(int(daySeconds*1000));emit changed();return;}
        m_order.clear();m_stories.clear();QSet<QString> seen;
        for(auto v:doc.array()){auto id=QString::number(v.toInteger());if(v.toInteger()>0&&!seen.contains(id)){seen.insert(id);m_order.append(id);}if(m_order.size()>=25)break;}
        m_feedPending=m_order.size();if(!m_feedPending){m_busy=false;m_poll.start(int(daySeconds*1000));emit changed();return;}
        for(auto id:m_order)get("item/"+id+".json",[this,id](const QByteArray &data){
            const auto o=QJsonDocument::fromJson(data).object();const auto row=normalizeStory(o);if(row.value("id")==id)m_stories.insert(id,row);
            if(--m_feedPending)return;
            QVariantList rows;for(const auto &key:m_order)if(m_stories.contains(key))rows.append(m_stories[key]);
            if(!rows.isEmpty()){m_items.update(rows);m_updated=QDateTime::currentSecsSinceEpoch();writeCache("hn-stories",{{"updated",m_updated},{"items",QJsonArray::fromVariantList(rows)}});}else m_problem="Cannot update Hacker News. Saved stories are kept.";
            m_busy=false;m_poll.start(int(daySeconds*1000));emit changed();
        });
    });
}
void HackerNewsClient::openStory(int index){
    const auto rows=m_items.rows();if(index<0||index>=rows.size())return;closeStory();m_selected=rows[index].toMap();
    const auto cached=readCache("hn-thread-"+m_selected.value("id").toString());
    if(cached.value("id").toString()==m_selected.value("id").toString()){
        m_threadUpdated=cached.value("updated").toInteger();
        const auto nodes=cached.value("nodes").toObject();for(auto i=nodes.begin();i!=nodes.end();++i)if(m_nodes.size()<500)m_nodes.insert(i.key(),i.value().toObject().toVariantMap());
        const auto expanded=cached.value("expanded").toObject();for(auto i=expanded.begin();i!=expanded.end();++i)m_expanded.insert(i.key(),qBound(0,i.value().toInt(),500));
        m_rootCount=qBound(10,cached.value("roots").toInt(),500);
    }else m_rootCount=10;
    rebuildComments();emit discussionChanged();
    if(!m_nodes.isEmpty()&&QDateTime::currentSecsSinceEpoch()-m_threadUpdated>=daySeconds&&!qEnvironmentVariableIsSet("THINKSMART_DISABLE_NEWS"))reloadComments();
    else loadNodes(m_selected.value("kids").toList(),m_rootCount);
}
void HackerNewsClient::closeStory(){++m_thread;m_pending=0;m_threadUpdated=0;m_selected.clear();m_nodes.clear();m_loading.clear();m_expanded.clear();m_commentsProblem.clear();m_comments.update({});emit discussionChanged();}
void HackerNewsClient::reloadComments(){
    if(m_selected.isEmpty()||commentsBusy()||qEnvironmentVariableIsSet("THINKSMART_DISABLE_NEWS"))return;
    m_pending=1;m_commentsProblem.clear();emit discussionChanged();const auto id=m_selected.value("id").toString();
    get("item/"+id+".json",[this,id](const QByteArray &bytes){
        m_pending=0;auto story=normalizeStory(QJsonDocument::fromJson(bytes).object());
        if(story.value("id")!=id){m_commentsProblem="Cannot update this discussion. Saved comments are kept.";emit discussionChanged();return;}
        m_selected=story;m_nodes.clear();m_expanded.clear();m_rootCount=10;rebuildComments();loadNodes(m_selected.value("kids").toList(),m_rootCount);emit discussionChanged();
    },m_thread);
}
bool HackerNewsClient::moreComments()const{return m_rootCount<m_selected.value("kids").toList().size()&&m_nodes.size()<500;}
void HackerNewsClient::loadMoreComments(){if(commentsBusy()||!moreComments())return;m_rootCount+=10;rebuildComments();loadNodes(m_selected.value("kids").toList(),m_rootCount);}
void HackerNewsClient::toggleReplies(const QString &id){
    if(commentsBusy()||!m_nodes.contains(id))return;
    if(m_expanded.contains(id)){m_expanded.remove(id);rebuildComments();saveThread();return;}
    m_expanded.insert(id,10);rebuildComments();loadNodes(m_nodes[id].value("kids").toList(),10);
}
void HackerNewsClient::moreReplies(const QString &id){if(commentsBusy()||!m_expanded.contains(id))return;m_expanded[id]+=10;rebuildComments();loadNodes(m_nodes[id].value("kids").toList(),m_expanded[id]);}
void HackerNewsClient::loadNodes(const QVariantList &ids,int count){
    const auto generation=m_thread;QStringList pending;
    for(int i=0;i<ids.size()&&i<count&&m_nodes.size()+m_loading.size()<500;i++){
        const auto id=QString::number(ids[i].toLongLong());if(ids[i].toLongLong()<=0||m_nodes.contains(id)||m_loading.contains(id))continue;
        m_loading.insert(id);pending.append(id);
    }
    m_pending+=pending.size();emit discussionChanged();
    if(pending.isEmpty()){saveThread();return;}
    for(auto id:pending)get("item/"+id+".json",[this,id,generation](const QByteArray &bytes){
        if(generation!=m_thread)return;const auto o=QJsonDocument::fromJson(bytes).object();QVariantMap row;
        if(o.value("id").toInteger()!=id.toLongLong()||o.value("type")!="comment"){
            row={{"id",id},{"text","Comment unavailable."}};m_commentsProblem="Some comments could not be loaded. Retry to fetch them again.";
        }else{
            const bool removed=o.value("deleted").toBool()||o.value("dead").toBool();
            row={{"id",id},{"text",removed?QString("[removed]"):plain(o.value("text").toString())},{"by",o.value("by").toString().left(100)},
                {"when",when(o.value("time").toInteger())},{"kids",o.value("kids").toArray().toVariantList()}};
        }
        m_nodes.insert(id,row);m_loading.remove(id);--m_pending;if(!bytes.isEmpty())m_threadUpdated=QDateTime::currentSecsSinceEpoch();rebuildComments();if(!m_pending)saveThread();emit discussionChanged();
    },generation);
}
void HackerNewsClient::rebuildComments(){
    QVariantList rows;QSet<QString> seen;
    std::function<void(const QVariantList &,int,int)> append=[&](const QVariantList &ids,int count,int depth){
        if(depth>30)return;for(int i=0;i<ids.size()&&i<count&&rows.size()<500;i++){
            const QString id=QString::number(ids[i].toLongLong());if(ids[i].toLongLong()<=0||seen.contains(id))continue;seen.insert(id);
            auto row=m_nodes.value(id);if(row.isEmpty())row={{"id",id},{"text","Loading comment…"},{"loading",true}};
            const auto kids=row.value("kids").toList();row.insert("depth",depth);row.insert("replyCount",kids.size());row.insert("expanded",m_expanded.contains(id));row.insert("moreReplies",m_expanded.value(id)<kids.size()&&m_nodes.size()<500);
            rows.append(row);if(m_expanded.contains(id))append(kids,m_expanded[id],depth+1);
        }
    };
    append(m_selected.value("kids").toList(),m_rootCount,0);m_comments.update(rows);
}
void HackerNewsClient::saveThread(){
    if(m_selected.isEmpty()||commentsBusy())return;QJsonObject nodes,expanded;
    for(auto i=m_nodes.begin();i!=m_nodes.end();++i)nodes.insert(i.key(),QJsonObject::fromVariantMap(i.value()));
    for(auto i=m_expanded.begin();i!=m_expanded.end();++i)expanded.insert(i.key(),i.value());
    writeCache("hn-thread-"+m_selected.value("id").toString(),{{"id",m_selected.value("id").toString()},{"nodes",nodes},{"expanded",expanded},{"roots",m_rootCount},{"updated",m_threadUpdated}});
    QDir dir(QFileInfo(cachePath("hn-stories")).absolutePath());const auto files=dir.entryInfoList({"hn-thread-*.json"},QDir::Files,QDir::Time);
    for(int i=12;i<files.size();i++)QFile::remove(files[i].absoluteFilePath());
}
