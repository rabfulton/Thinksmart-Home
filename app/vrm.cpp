#include "deviceconfig.h"
#include "vrm.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <cmath>
#include <functional>

namespace {
struct Metric { const char *key; const char *unit; int decimals; };
const Metric definitions[]={{"soc","%",1},{"solar","W",0},{"batteryPower","W",0},
    {"voltage","V",1},{"current","A",1},{"dcLoad","W",0},{"acLoad","W",0},
    {"yieldToday","kWh",2},{"yieldYesterday","kWh",2}};
QString age(qint64 timestamp) {
    if(timestamp<=0)return "Time unknown";
    qint64 seconds=qMax(0LL,QDateTime::currentSecsSinceEpoch()-timestamp);
    if(seconds<60)return "Just now";
    if(seconds<3600)return QString::number(seconds/60)+" min ago";
    if(seconds<86400)return QString::number(seconds/3600)+" h ago";
    return QString::number(seconds/86400)+" d ago";
}
QString date(qint64 timestamp) {
    return timestamp>0?QDateTime::fromSecsSinceEpoch(timestamp).toLocalTime().toString("d MMM yyyy, HH:mm"):"Time unknown";
}
bool old(qint64 timestamp) {
    return timestamp<=0||QDateTime::currentSecsSinceEpoch()-timestamp>1800;
}
QJsonObject reading(const QJsonObject &record) {
    const auto raw=record.value("rawValue");bool ok=false;
    double number=raw.isDouble()?raw.toDouble():raw.toString().toDouble(&ok);
    if((!raw.isDouble()&&!ok)||!std::isfinite(number))return {};
    qint64 timestamp=record.value("timestamp").toVariant().toLongLong();
    if(timestamp>1000000000000LL)timestamp/=1000;
    if(timestamp<=0||timestamp>QDateTime::currentSecsSinceEpoch()+300)timestamp=0;
    return {{"value",number},{"timestamp",timestamp}};
}
}
QJsonObject VrmClient::normalize(const QJsonArray &records) {
    auto first=[&](const QString &code) {
        QJsonObject result;
        for(const auto &item:records) {
            auto record=item.toObject();if(record.value("code")!=code)continue;
            const auto value=reading(record);
            if(!value.isEmpty()&&(result.isEmpty()||value.value("timestamp").toDouble()>result.value("timestamp").toDouble()))result=value;
        }
        return result;
    };
    auto sum=[&](const QStringList &codes) {
        double total=0;qint64 timestamp=-1;bool found=false;
        for(const auto &item:records) {
            auto record=item.toObject();if(!codes.contains(record.value("code").toString()))continue;
            const auto value=reading(record);if(value.isEmpty())return QJsonObject{};
            found=true;total+=value.value("value").toDouble();
            const qint64 time=value.value("timestamp").toVariant().toLongLong();
            timestamp=timestamp<0?time:qMin(timestamp,time);
        }
        return found?QJsonObject{{"value",total},{"timestamp",timestamp}}:QJsonObject{};
    };
    QJsonObject result;
    const QPair<const char *,const char *> codes[]={{"soc","bs"},{"batteryPower","bp"},{"voltage","bv"},{"current","bc"},{"dcLoad","dc"}};
    for(const auto &code:codes){auto r=first(code.second);if(!r.isEmpty())result.insert(code.first,r);}
    auto solar=first("Pdc");if(solar.isEmpty())solar=sum({"PVP"});
    if(!solar.isEmpty())result.insert("solar",solar);
    const QPair<const char *,QStringList> aggregates[]={{"acLoad",{"OP1","OP2","OP3"}},{"yieldToday",{"YT"}},{"yieldYesterday",{"YY"}}};
    for(const auto &metric:aggregates){auto r=sum(metric.second);if(!r.isEmpty())result.insert(metric.first,r);}
    if(result.value("soc").toObject().value("value").toDouble(-1)<0||result.value("soc").toObject().value("value").toDouble(101)>100)result.remove("soc");
    return result;
}
VrmClient::VrmClient(QObject *parent):QObject(parent) {
    if(!DeviceConfig::enabled("vrm"))return;
    QFile config(qEnvironmentVariable("THINKSMART_VRM_CONFIG",QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)+"/vrm.json"));
    if(!config.exists()){m_problem="Add a read-only VRM connection to show your energy data.";return;}
    const auto exposed=QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|QFile::ReadOther|QFile::WriteOther|QFile::ExeOther;
    if(config.permissions()&exposed){m_problem="VRM credentials need private file permissions (600).";return;}
    if(!config.open(QIODevice::ReadOnly)||config.size()>65536){m_problem="Cannot read VRM connection settings.";return;}
    QJsonParseError error;auto settings=QJsonDocument::fromJson(config.readAll(),&error).object();
    if(error.error!=QJsonParseError::NoError){m_problem="Invalid VRM connection settings.";return;}
    m_token=settings.value("token").toString().trimmed();
    m_configuredSite=settings.value("installationId").toVariant().toInt();
    if(m_token.isEmpty()){m_problem="Add a read-only VRM connection to show your energy data.";return;}
    if(m_token.contains('\r')||m_token.contains('\n')){m_token.clear();m_problem="Invalid VRM token.";return;}
    if(m_configuredSite>0){m_site=m_configuredSite;loadCache();}
    m_poll.setInterval(60000);
    connect(&m_poll,&QTimer::timeout,this,[this](){emit changed();refresh();});m_poll.start();
    QTimer::singleShot(0,this,&VrmClient::refresh);
}
VrmClient::~VrmClient(){
    for(auto reply:m_network.findChildren<QNetworkReply *>()){reply->disconnect(this);reply->abort();}
}
QVariantMap VrmClient::metrics() const {
    QVariantMap result;
    for(const auto &definition:definitions) {
        const auto raw=m_readings.value(definition.key).toObject();
        const bool available=raw.value("value").isDouble();
        const qint64 timestamp=raw.value("timestamp").toVariant().toLongLong();
        const double value=raw.value("value").toDouble();
        QString text="—";
        if(available) {
            if(QString(definition.unit)=="W"&&std::abs(value)>=1000)text=QString::number(value/1000,'f',2)+" kW";
            else text=QString::number(value,'f',definition.decimals)+(QString(definition.unit)=="%"?"%":" "+QString(definition.unit));
        }
        result.insert(definition.key,QVariantMap{{"available",available},{"text",text},{"unit",QString(definition.unit)},{"value",available?QVariant(value):QVariant()},
            {"timestamp",timestamp},{"age",available?age(timestamp):"Not reported"},{"when",date(timestamp)},{"stale",old(timestamp)}});
    }
    return result;
}
qint64 VrmClient::reportTime() const {
    const auto battery=m_readings.value("soc").toObject();
    // The card's main value is SoC. Optional/retired devices retain independent ages.
    if(!battery.isEmpty())return battery.value("timestamp").toVariant().toLongLong();
    qint64 latest=0;
    for(const auto &key:{"solar","acLoad","batteryPower"})latest=qMax(latest,m_readings.value(key).toObject().value("timestamp").toVariant().toLongLong());
    return latest;
}
bool VrmClient::stale() const {
    return hasData()&&old(reportTime());
}
QString VrmClient::status() const {
    if(!configured())return "VRM not connected";
    if(!hasData())return m_busy?"Connecting to VRM…":(m_sites.size()>1&&m_site==0?"Choose an installation":"No readings available");
    if(!m_cloudOk)return "Saved readings";
    return stale()?"Last known readings":"Latest readings";
}
QString VrmClient::lastReport() const {return hasData()?"Measured "+date(reportTime())+" · "+age(reportTime()):"No measurements yet";}
QString VrmClient::lastCheck() const {return m_fetchedAt>0?"VRM checked "+date(m_fetchedAt):"VRM has not been checked yet";}
void VrmClient::refresh() {
    if(!DeviceConfig::enabled("vrm"))return;
    if(!configured()||m_busy||QDateTime::currentSecsSinceEpoch()<m_retryAfter)return;
    if(m_site>0)get(QString("installations/%1/diagnostics?count=1000").arg(m_site),Diagnostics);
    else if(m_sites.isEmpty())get("users/me",User);
}
void VrmClient::get(const QString &path,Stage stage) {
    m_busy=true;emit changed();
    QNetworkRequest request(QUrl("https://vrmapi.victronenergy.com/v2/"+path));
    request.setRawHeader("X-Authorization",("Token "+m_token).toUtf8());
    request.setRawHeader("Accept","application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    auto reply=m_network.get(request);
    connect(reply,&QNetworkReply::downloadProgress,this,[reply](qint64 received,qint64 total){if(received>2*1024*1024||total>2*1024*1024)reply->abort();});
    connect(reply,&QNetworkReply::finished,this,[this,reply,stage](){
        m_busy=false;
        const int http=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error()!=QNetworkReply::NoError||http!=200) {
            m_cloudOk=false;
            if(http==401||http==403)m_problem="VRM access denied. Check the read-only token and installation access.";
            else if(http==429){
                bool ok=false;int seconds=reply->rawHeader("Retry-After").toInt(&ok);
                m_retryAfter=QDateTime::currentSecsSinceEpoch()+(ok?qBound(60,seconds,3600):300);
                m_problem="VRM rate limit reached. Refresh will retry later.";
            }else m_problem="Cannot reach VRM. Any saved measurements are kept.";
        }else {
            const QByteArray bytes=reply->readAll();QJsonParseError error;
            const auto response=QJsonDocument::fromJson(bytes,&error).object();
            if(bytes.size()>2*1024*1024||error.error!=QJsonParseError::NoError||!response.value("success").toBool()) {
                m_cloudOk=false;m_problem="VRM returned an invalid response. Saved measurements are kept.";
            }else consume(response,stage);
        }
        reply->deleteLater();emit changed();
    });
}
void VrmClient::consume(const QJsonObject &response,Stage stage) {
    m_problem.clear();
    if(stage==User) {
        const auto user=response.value("user").toObject();
        const int id=user.value("id").toVariant().toInt();
        if(id<=0){m_problem="Cannot identify the VRM account.";return;}
        get(QString("users/%1/installations").arg(id),Sites);return;
    }
    if(stage==Sites) {
        m_sites.clear();
        for(const auto &item:response.value("records").toArray()) {
            const auto record=item.toObject();int id=record.value("idSite").toVariant().toInt();
            if(id>0)m_sites.append(QVariantMap{{"id",id},{"name",record.value("name").toString("VRM installation")}});
        }
        if(m_sites.isEmpty()){m_problem="This token has no accessible installations.";return;}
        if(m_sites.size()==1){selectInstallation(m_sites.first().toMap().value("id").toInt());return;}
        const int saved=m_settings.value("vrm/installationId",0).toInt();
        for(const auto &site:m_sites)if(site.toMap().value("id").toInt()==saved){selectInstallation(saved);return;}
        return;
    }
    const auto readings=normalize(response.value("records").toArray());
    if(readings.isEmpty()){m_problem="VRM has no usable measurements for this installation.";m_cloudOk=false;return;}
    m_readings=readings;m_cloudOk=true;m_fetchedAt=QDateTime::currentSecsSinceEpoch();saveCache();
}
void VrmClient::selectInstallation(int id) {
    if(id<=0||m_busy)return;
    bool allowed=id==m_configuredSite;
    for(const auto &site:m_sites)if(site.toMap().value("id").toInt()==id){allowed=true;m_siteName=site.toMap().value("name").toString();}
    if(!allowed)return;
    if(m_site!=id){m_readings={};m_cloudOk=false;m_fetchedAt=0;m_site=id;loadCache();}
    m_settings.setValue("vrm/installationId",id);refresh();emit changed();
}
QString VrmClient::cachePath() const {
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+QString("/vrm-%1.json").arg(m_site);
}
void VrmClient::loadCache() {
    QFile file(cachePath());if(!file.open(QIODevice::ReadOnly)||file.size()>2*1024*1024)return;
    const auto cache=QJsonDocument::fromJson(file.readAll()).object();
    if(cache.value("version").toInt()!=1||cache.value("installationId").toInt()!=m_site)return;
    QJsonObject valid;
    const auto cached=cache.value("readings").toObject();
    for(const auto &definition:definitions) {
        const auto r=cached.value(definition.key).toObject();
        const auto value=r.value("value");
        const qint64 timestamp=r.value("timestamp").toVariant().toLongLong();
        if(value.isDouble()&&std::isfinite(value.toDouble())&&timestamp>=0&&timestamp<=QDateTime::currentSecsSinceEpoch()+300)valid.insert(definition.key,r);
    }
    m_readings=valid;m_fetchedAt=cache.value("fetchedAt").toVariant().toLongLong();
    if(m_siteName.isEmpty())m_siteName=cache.value("siteName").toString();
}
void VrmClient::saveCache() {
    const QString directory=QFileInfo(cachePath()).absolutePath();
    if(!QDir().mkpath(directory))return;
    QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(cachePath());if(!file.open(QIODevice::WriteOnly))return;
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    file.write(QJsonDocument(QJsonObject{{"version",1},{"installationId",m_site},{"siteName",m_siteName},
        {"fetchedAt",m_fetchedAt},{"readings",m_readings}}).toJson(QJsonDocument::Compact));
    file.commit();
}
