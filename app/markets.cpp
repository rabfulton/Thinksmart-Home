#include "markets.h"
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
#include <QRegularExpression>
#include <QUuid>
#include <cmath>
#include <algorithm>

namespace {
qint64 stamp(const QJsonValue &v){return qint64(v.toDouble());}
bool positive(const QJsonValue &v){return v.isDouble()&&std::isfinite(v.toDouble())&&v.toDouble()>0&&v.toDouble()<1e15;}
bool inPeriod(const QJsonObject &period,qint64 now){return now>=stamp(period.value("start"))&&now<stamp(period.value("end"));}
bool writeJson(const QString &path,const QJsonObject &data){
    QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile f(path);
    if(!f.open(QIODevice::WriteOnly))return false;
    f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);const auto b=QJsonDocument(data).toJson(QJsonDocument::Compact);
    return f.write(b)==b.size()&&f.commit();
}
QJsonObject resultBody(const QJsonObject &body){const auto r=body.value("chart").toObject().value("result").toArray();return r.isEmpty()?QJsonObject{}:r.first().toObject();}
}
QString MarketsClient::settingsPath(){return qEnvironmentVariable("THINKSMART_MARKETS_FILE",QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/markets.json");}
QString MarketsClient::cachePath(){return qEnvironmentVariable("THINKSMART_MARKETS_CACHE",QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/markets.json");}
QString MarketsClient::canonical(QString s){
    s=s.trimmed().toUpper();
    if(QStringList{"SPX500USD","SPX","SP500","S&P500","S&P 500","^SPX"}.contains(s))s="^GSPC";
    if(s=="DXY")s="DX-Y.NYB";if(s=="BTC")s="BTC-USD";if(s=="ETH")s="ETH-USD";
    return QRegularExpression("^[A-Z0-9^][A-Z0-9.^=_-]{0,23}$").match(s).hasMatch()?s:QString();
}
QJsonArray MarketsClient::chartPoints(const QJsonObject &body){
    const auto r=resultBody(body);const auto times=r.value("timestamp").toArray();const auto qs=r.value("indicators").toObject().value("quote").toArray();
    if(qs.isEmpty())return {};const auto values=qs.first().toObject().value("close").toArray();QJsonArray out;
    const int step=std::max(1,int(std::ceil(times.size()/400.0)));qint64 last=0;
    for(int i=0;i<times.size()&&i<values.size();++i){
        const auto t=stamp(times[i]);if(t<=last||!positive(values[i]))continue;
        if(i%step&&i!=times.size()-1)continue;
        out.append(QJsonObject{{"time",t},{"value",values[i]}});last=t;
    }
    return out;
}
QJsonObject MarketsClient::normalize(const QJsonObject &body,const QString &symbol,qint64 now){
    const auto r=resultBody(body),m=r.value("meta").toObject();const auto t=stamp(m.value("regularMarketTime"));
    if(m.value("symbol").toString().compare(symbol,Qt::CaseInsensitive)!=0||!positive(m.value("regularMarketPrice"))||t<=0||t>now+120)return {};
    const bool crypto=m.value("instrumentType")=="CRYPTOCURRENCY";const auto periods=m.value("currentTradingPeriod").toObject();
    QString state="Unknown";const auto regular=periods.value("regular").toObject();
    if(crypto)state="24/7";
    else if(!regular.isEmpty()){
        state=inPeriod(regular,now)?"Open":"Closed";
        if(m.value("instrumentType")=="EQUITY"){
            if(inPeriod(periods.value("pre").toObject(),now))state="Pre-market";
            if(inPeriod(periods.value("post").toObject(),now))state="After-hours";
        }
    }
    QJsonValue base=m.value("previousClose");if(!positive(base))base=m.value("chartPreviousClose");
    if(crypto){
        // Rolling 24-hour comparison, not the provider's midnight close.
        base=QJsonValue();qint64 best=601;
        for(const auto &v:chartPoints(body)){const auto p=v.toObject();const auto distance=std::abs(stamp(p.value("time"))-(t-86400));if(distance<best){best=distance;base=p.value("value");}}
    }
    QJsonValue change,percent;
    if(positive(base)){change=m.value("regularMarketPrice").toDouble()-base.toDouble();percent=100*change.toDouble()/base.toDouble();}
    const auto name=m.value("longName").toString(m.value("shortName").toString(symbol)).left(100);
    return {{"symbol",symbol},{"label",symbol=="^GSPC"?"S&P 500":symbol=="DX-Y.NYB"?"DXY":symbol=="BTC-USD"?"BTC":symbol=="ETH-USD"?"ETH":symbol},
        {"name",name},{"price",m.value("regularMarketPrice")},{"currency",m.value("currency").toString().left(8)},
        {"change",change},{"percent",percent},{"changePeriod",crypto?"24h":"day"},{"quoteTime",t},{"fetchedAt",now},
        {"marketStatus",state},{"periods",periods},{"instrumentType",m.value("instrumentType")},{"provider","Yahoo Finance"},
        {"timing","Free feed; may be delayed"},{"regularSessionQuote",!crypto}};
}
MarketsClient::MarketsClient(QObject *parent):QObject(parent){
    if(!DeviceConfig::enabled("markets"))return;load();
    m_timer.setInterval(60000);connect(&m_timer,&QTimer::timeout,this,[this](){
        if(m_active||std::any_of(m_alerts.begin(),m_alerts.end(),[](const auto &v){return v.toObject().value("armed").toBool();})||QDateTime::currentSecsSinceEpoch()-m_lastRefresh>=900)refresh();
        emit changed(); // Ages and session labels progress without an animation timer.
    });
    if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_MARKETS")){m_timer.start();QTimer::singleShot(0,this,&MarketsClient::refresh);}
}
MarketsClient::~MarketsClient(){for(auto *r:m_network.findChildren<QNetworkReply *>()){r->disconnect(this);r->abort();}}
void MarketsClient::load(){
    QFile f(settingsPath());
    if(f.exists()){
        bool valid=f.open(QIODevice::ReadOnly)&&f.size()<=128*1024;QJsonParseError error;const auto d=QJsonDocument::fromJson(valid?f.readAll():QByteArray(),&error).object();
        const auto symbols=d.value("symbols").toArray();QStringList list;
        valid=valid&&error.error==QJsonParseError::NoError&&d.value("version")==1&&symbols.size()>=1&&symbols.size()<=12&&d.value("alerts").isArray()&&d.value("alerts").toArray().size()<=32;
        for(const auto &v:symbols){const auto s=canonical(v.toString());if(s.isEmpty()||list.contains(s))valid=false;list<<s;}
        QSet<QString> ids;
        for(const auto &v:d.value("alerts").toArray()){
            const auto a=v.toObject();const auto id=a.value("id").toString();
            if(id.isEmpty()||ids.contains(id)||!list.contains(a.value("symbol").toString())||!QStringList{"above","below"}.contains(a.value("direction").toString())||!positive(a.value("price"))||!a.value("armed").isBool()||!a.value("chime").isBool())valid=false;
            ids.insert(id);
        }
        if(!list.contains(d.value("focus").toString())||!QStringList{"list","focus"}.contains(d.value("mode").toString()))valid=false;
        if(valid){m_symbols=list;m_focus=d.value("focus").toString();m_mode=d.value("mode").toString();m_alerts=d.value("alerts").toArray();}
        else {m_storeHealthy=false;m_problem="Markets settings are invalid; file left untouched. Alerts are disabled.";}
    }
    QFile cache(cachePath());if(cache.open(QIODevice::ReadOnly)&&cache.size()<=4*1024*1024){const auto d=QJsonDocument::fromJson(cache.readAll()).object();if(d.value("version")==1){m_quotes=d.value("quotes").toObject();m_charts=d.value("charts").toObject();}}
}
bool MarketsClient::save(){
    if(!m_storeHealthy){emit changed();return false;}
    if(!writeJson(settingsPath(),{{"version",1},{"symbols",QJsonArray::fromStringList(m_symbols)},{"focus",m_focus},{"mode",m_mode},{"alerts",m_alerts}})){m_problem="Cannot save Markets settings.";emit changed();return false;}
    return true;
}
void MarketsClient::saveCache(){writeJson(cachePath(),{{"version",1},{"quotes",m_quotes},{"charts",m_charts}});}
QJsonObject MarketsClient::displayQuote(const QString &symbol)const{
    auto q=m_quotes.value(symbol).toObject();const auto now=QDateTime::currentSecsSinceEpoch();const auto time=stamp(q.value("quoteTime"));
    if(q.isEmpty())q={{"symbol",symbol},{"label",symbol=="^GSPC"?"S&P 500":symbol=="DX-Y.NYB"?"DXY":symbol},{"name",symbol},{"marketStatus","Unknown"}};
    auto state=q.value("marketStatus").toString();const auto periods=q.value("periods").toObject();const auto regular=periods.value("regular").toObject();
    if(q.value("instrumentType")=="CRYPTOCURRENCY")state="24/7";
    else if(!regular.isEmpty()){
        // Once the supplied calendar expires, do not guess holidays or the next session.
        state=now>stamp(regular.value("end"))+86400?"Unknown":inPeriod(regular,now)?"Open":"Closed";
        if(q.value("instrumentType")=="EQUITY"){
            if(inPeriod(periods.value("pre").toObject(),now))state="Pre-market";
            if(inPeriod(periods.value("post").toObject(),now))state="After-hours";
        }
    }
    const bool cached=now-stamp(q.value("fetchedAt"))>300;
    const bool stale=time==0||cached||((state=="Open"||state=="24/7")&&now-time>180);
    q.insert("marketStatus",state);q.insert("available",positive(q.value("price")));q.insert("stale",stale);q.insert("ageSeconds",std::max(qint64(0),now-time));q.insert("cached",cached);
    q.insert("alertEligible",!stale&&(state=="Open"||state=="24/7")&&now-time<=180);
    q.insert("quoteLabel",time?QDateTime::fromSecsSinceEpoch(time).toLocalTime().toString("d MMM HH:mm"):"No quote");
    return q;
}
QVariantList MarketsClient::quotes()const{QVariantList out;for(const auto &s:m_symbols)out<<displayQuote(s).toVariantMap();return out;}
QVariantMap MarketsClient::selected()const{return displayQuote(m_focus).toVariantMap();}
QVariantList MarketsClient::points()const{return m_charts.value(m_focus+":"+m_range).toObject().value("points").toArray().toVariantList();}
void MarketsClient::setActive(bool active){if(!DeviceConfig::enabled("markets"))return;if(m_active==active)return;m_active=active;emit activeChanged();if(active&&!qEnvironmentVariableIsSet("THINKSMART_DISABLE_MARKETS"))refresh();}
bool MarketsClient::setSymbols(const QString &text){
    if(!DeviceConfig::enabled("markets")||!m_storeHealthy)return false;QStringList list;
    for(const auto &part:text.split(QRegularExpression("[,;\\s]+"),Qt::SkipEmptyParts)){const auto s=canonical(part);if(s.isEmpty()){m_problem="Use ticker symbols, such as NVDA, ^GSPC, DXY or BTC-USD.";emit changed();return false;}if(!list.contains(s))list<<s;}
    if(list.isEmpty()||list.size()>12){m_problem="Choose between 1 and 12 symbols.";emit changed();return false;}
    for(const auto &a:m_alerts)if(a.toObject().value("armed").toBool()&&!list.contains(a.toObject().value("symbol").toString())){m_problem="Remove or disable that symbol's price alerts first.";emit changed();return false;}
    const auto old=m_symbols;const auto focus=m_focus;const auto oldAlerts=m_alerts;m_symbols=list;if(!list.contains(m_focus))m_focus=list.first();
    for(int i=m_alerts.size()-1;i>=0;--i)if(!list.contains(m_alerts[i].toObject().value("symbol").toString()))m_alerts.removeAt(i);
    if(!save()){m_symbols=old;m_focus=focus;m_alerts=oldAlerts;return false;}
    // Bound the cache to the current watchlist, including history fetched earlier.
    for(const auto &k:m_quotes.keys())if(!list.contains(k))m_quotes.remove(k);
    for(const auto &k:m_charts.keys())if(!list.contains(k.section(':',0,0)))m_charts.remove(k);
    m_baselines={};m_problem.clear();emit changed();if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_MARKETS"))for(const auto &s:list)enqueue(s,"1D");return true;
}
bool MarketsClient::select(const QString &symbol){
    const auto s=canonical(symbol);if(!DeviceConfig::enabled("markets")||!m_storeHealthy||!m_symbols.contains(s))return false;
    const auto old=m_focus;m_focus=s;if(!save()){m_focus=old;return false;}emit changed();if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_MARKETS"))enqueue(s,m_range);return true;
}
void MarketsClient::setRange(const QString &range){if(!DeviceConfig::enabled("markets")||!QStringList{"1D","1W","1M","1Y"}.contains(range)||m_range==range)return;m_range=range;emit changed();if(!qEnvironmentVariableIsSet("THINKSMART_DISABLE_MARKETS"))enqueue(m_focus,m_range);}
void MarketsClient::setMode(const QString &mode){if(!DeviceConfig::enabled("markets")||!m_storeHealthy||!QStringList{"list","focus"}.contains(mode))return;const auto old=m_mode;m_mode=mode;if(!save())m_mode=old;emit changed();}
bool MarketsClient::addAlert(const QString &symbol,const QString &direction,double price,bool chime){
    const auto s=canonical(symbol);if(!DeviceConfig::enabled("markets")||!m_storeHealthy)return false;
    if(!m_symbols.contains(s)||!QStringList{"above","below"}.contains(direction)||!std::isfinite(price)||price<=0||price>=1e15||m_alerts.size()>=32){m_problem="Choose a watched symbol, above/below and a positive price (maximum 32 alerts).";emit changed();return false;}
    const auto old=m_alerts;m_alerts.append(QJsonObject{{"id",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"symbol",s},{"direction",direction},{"price",price},{"armed",true},{"chime",chime}});
    if(!save()){m_alerts=old;return false;}m_baselines.remove(s);m_problem.clear();emit changed();return true;
}
bool MarketsClient::removeAlert(const QString &id){
    if(!DeviceConfig::enabled("markets")||!m_storeHealthy)return false;auto old=m_alerts;for(int i=0;i<m_alerts.size();++i)if(m_alerts[i].toObject().value("id")==id){m_alerts.removeAt(i);if(!save()){m_alerts=old;return false;}emit changed();return true;}return false;
}
bool MarketsClient::rearmAlert(const QString &id){
    if(!DeviceConfig::enabled("markets")||!m_storeHealthy)return false;auto old=m_alerts;for(int i=0;i<m_alerts.size();++i){auto a=m_alerts[i].toObject();if(a.value("id")==id){a.insert("armed",true);a.remove("triggeredAt");m_alerts[i]=a;if(!save()){m_alerts=old;return false;}m_baselines.remove(a.value("symbol").toString());emit changed();return true;}}return false;
}
void MarketsClient::accept(const QString &symbol,const QJsonObject &quote,qint64 now){
    const auto t=stamp(quote.value("quoteTime"));const auto old=m_quotes.value(symbol).toObject();if(t<stamp(old.value("quoteTime")))return;m_quotes.insert(symbol,quote);
    const auto state=quote.value("marketStatus").toString();const bool eligible=(state=="Open"||state=="24/7")&&now-t<=180&&t<=now+120;
    if(!eligible){m_baselines.remove(symbol);return;}
    const auto before=m_baselines.value(symbol).toObject();if(t<=stamp(before.value("time")))return;
    m_baselines.insert(symbol,QJsonObject{{"time",t},{"price",quote.value("price")}});if(before.isEmpty()||now-stamp(before.value("time"))>180)return;
    const double previous=before.value("price").toDouble(),current=quote.value("price").toDouble();
    for(int i=0;i<m_alerts.size();++i){auto a=m_alerts[i].toObject();if(a.value("symbol")!=symbol||!a.value("armed").toBool())continue;
        const double target=a.value("price").toDouble();const bool crossed=a.value("direction")=="above"?(previous<target&&current>=target):(previous>target&&current<=target);
        if(!crossed)continue;const auto oldAlerts=m_alerts;a.insert("armed",false);a.insert("triggeredAt",now);m_alerts[i]=a;
        if(!save()){m_alerts=oldAlerts;continue;} // Persist disarmed state before notifying.
        const auto text=quote.value("label").toString()+" crossed "+a.value("direction").toString()+" "+QString::number(target,'f',2)+" · "+QString::number(current,'f',2)+" "+quote.value("currency").toString();
        m_notification=text;emit alertTriggered(text,a.value("chime").toBool());
    }
}
void MarketsClient::refresh(){
    const auto now=QDateTime::currentSecsSinceEpoch();if(!DeviceConfig::enabled("markets")||busy()||now<m_retryAfter||now-m_lastRefresh<30)return;
    m_lastRefresh=now;if(m_storeHealthy)m_problem.clear();for(const auto &s:m_symbols)enqueue(s,"1D");if(m_range!="1D")enqueue(m_focus,m_range);
}
void MarketsClient::enqueue(const QString &symbol,const QString &range){
    if(!DeviceConfig::enabled("markets")||!m_symbols.contains(symbol)||QDateTime::currentSecsSinceEpoch()<m_retryAfter)return;
    const auto key=symbol+":"+range;if(m_pending.contains(key)||QDateTime::currentSecsSinceEpoch()-stamp(m_charts.value(key).toObject().value("fetchedAt"))<30)return;m_pending.insert(key);m_queue.enqueue({symbol,range});pump();
}
void MarketsClient::pump(){
    while(m_running<2&&!m_queue.isEmpty()){
        const auto job=m_queue.dequeue();const auto key=job.symbol+":"+job.range;
        if(!m_symbols.contains(job.symbol)){m_pending.remove(key);continue;}++m_running;
        QUrl url("https://query1.finance.yahoo.com/v8/finance/chart/"+QString::fromLatin1(QUrl::toPercentEncoding(job.symbol)));
        QUrlQuery query;query.addQueryItem("range",job.range=="1D"?(job.symbol.endsWith("-USD")?"2d":"1d"):job.range=="1W"?"5d":job.range=="1M"?"1mo":"1y");
        query.addQueryItem("interval",job.range=="1D"?"5m":job.range=="1W"?"15m":job.range=="1M"?"60m":"1d");query.addQueryItem("includePrePost","false");url.setQuery(query);
#ifdef THINKSMART_MARKETS_TESTING
        if(qEnvironmentVariableIsSet("THINKSMART_MARKETS_TEST_URL")){QUrl test(qEnvironmentVariable("THINKSMART_MARKETS_TEST_URL"));test.setPath("/"+job.symbol);test.setQuery(query);url=test;}
#endif
        QNetworkRequest request(url);request.setRawHeader("User-Agent","Mozilla/5.0 ThinkSmart-Home/0.3");request.setTransferTimeout(12000);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);
        auto *reply=m_network.get(request);connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 n,qint64 total){if(n>1024*1024||total>1024*1024)reply->abort();});
        connect(reply,&QNetworkReply::finished,this,[this,reply,job,key](){
            --m_running;m_pending.remove(key);const auto bytes=reply->readAll();const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();const bool ok=reply->error()==QNetworkReply::NoError&&code==200&&bytes.size()<=1024*1024;reply->deleteLater();
            const auto now=QDateTime::currentSecsSinceEpoch();const auto body=QJsonDocument::fromJson(bytes).object();const auto q=normalize(body,job.symbol,now);
            if(!m_symbols.contains(job.symbol)){pump();emit changed();return;}
            if(!ok||q.isEmpty()){
                if(m_storeHealthy)m_problem="Some quotes could not update; cached prices are kept.";
                if(code==429){m_retryAfter=now+900;m_queue.clear();m_pending.clear();m_problem="Free feed rate limit; retrying in 15 minutes.";}
            }else {
                if(job.range=="1D")accept(job.symbol,q,now);
                auto points=chartPoints(body);if(job.range=="1D"&&q.value("instrumentType")=="CRYPTOCURRENCY"){QJsonArray day;for(const auto &v:points)if(stamp(v.toObject().value("time"))>=stamp(q.value("quoteTime"))-86400)day.append(v);points=day;}
                m_charts.insert(key,QJsonObject{{"points",points},{"fetchedAt",now}});saveCache();
            }
            pump();emit changed();
        });
    }
    emit changed();
}
QJsonObject MarketsClient::snapshot()const{return {{"provider","Yahoo Finance (unofficial free endpoint)"},{"timing","Polling; quotes may be delayed"},{"busy",busy()},{"error",m_problem},{"symbols",QJsonArray::fromStringList(m_symbols)},{"quotes",QJsonArray::fromVariantList(quotes())},{"focus",m_focus},{"range",m_range},{"alerts",m_alerts},{"mode",m_mode},{"alertPolicy","One-shot threshold crossings; fresh quotes <=180 seconds only, regular session or 24/7; no alerts from cached/replayed data."}};}
QJsonObject MarketsClient::request(const QJsonObject &r){
    if(!DeviceConfig::enabled("markets"))return {{"ok",false},{"error","Markets is disabled."}};
    const auto a=r.value("action").toString();bool ok=true;
    if(a=="markets.refresh")refresh();
    else if(a=="markets.watchlist"){
        if(!r.value("symbols").isArray())ok=false;else {QStringList list;for(const auto &v:r.value("symbols").toArray())list<<v.toString();ok=setSymbols(list.join(','));}
    }else if(a=="markets.focus"){ok=select(r.value("symbol").toString());if(ok){setMode("focus");if(r.contains("range"))setRange(r.value("range").toString());}}
    else if(a=="markets.alert.add")ok=r.value("price").isDouble()&&addAlert(r.value("symbol").toString(),r.value("direction").toString(),r.value("price").toDouble(),r.value("chime").toBool());
    else if(a=="markets.alert.remove")ok=removeAlert(r.value("id").toString());
    else if(a=="markets.alert.rearm")ok=rearmAlert(r.value("id").toString());
    else if(a!="markets.get")return {{"ok",false},{"error","Unknown Markets action."}};
    auto out=snapshot();out.insert("ok",ok);if(!ok)out.insert("error",m_problem.isEmpty()?"Invalid Markets arguments.":m_problem);
    if(a=="markets.get"&&r.contains("symbol")){const auto s=canonical(r.value("symbol").toString());if(!m_symbols.contains(s))return {{"ok",false},{"error","Symbol is not in the watchlist."}};out.insert("quote",displayQuote(s));}
    return out;
}
