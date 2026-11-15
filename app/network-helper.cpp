#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>
#include <algorithm>
#include <grp.h>
#include <pwd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

using Settings=QMap<QString,QVariantMap>;
Q_DECLARE_METATYPE(Settings)
namespace {
const QString service="org.freedesktop.NetworkManager",root="/org/freedesktop/NetworkManager";
const QString deviceInterface=service+".Device",wirelessInterface=deviceInterface+".Wireless",connectionInterface=service+".Settings.Connection";
QJsonObject error(const QString &message){return {{"ok",false},{"error",message}};}
struct Failure {QString message;};
QVariant plain(QVariant v){while(v.metaType()==QMetaType::fromType<QDBusVariant>())v=qvariant_cast<QDBusVariant>(v).variant();return v;}
class Network {
    QDBusConnection bus=QDBusConnection::systemBus();
    QVariantList call(const QString &path,const QString &iface,const QString &method,const QVariantList &args={}){
        auto request=QDBusMessage::createMethodCall(service,path,iface,method);request.setArguments(args);
        auto reply=bus.call(request,QDBus::Block,2500);
        if(reply.type()==QDBusMessage::ErrorMessage)throw Failure{"NetworkManager could not complete the request. Check the connection or password and try again."};
        return reply.arguments();
    }
    QVariantMap properties(const QString &path,const QString &iface){return qdbus_cast<QVariantMap>(call(path,"org.freedesktop.DBus.Properties","GetAll",{iface}).value(0));}
    Settings settings(const QString &path){return qdbus_cast<Settings>(call(path,connectionInterface,"GetSettings").value(0));}
    QString device(){
        const auto devices=qdbus_cast<QList<QDBusObjectPath>>(call(root,service,"GetDevices").value(0));
        for(const auto &d:devices)if(plain(properties(d.path(),deviceInterface).value("DeviceType")).toUInt()==2)return d.path();
        throw Failure{"No Wi-Fi adapter is available."};
    }
    QList<QDBusObjectPath> profiles(){return qdbus_cast<QList<QDBusObjectPath>>(call(root+"/Settings",service+".Settings","ListConnections").value(0));}
    QString profile(const QString &uuid){
        if(!QRegularExpression("^[a-fA-F0-9-]{36}$").match(uuid).hasMatch())throw Failure{"Invalid saved network."};
        for(const auto &p:profiles()){
            const auto s=settings(p.path());const auto c=s.value("connection");
            if(c.value("uuid").toString()==uuid&&c.value("type")=="802-11-wireless")return p.path();
        }
        throw Failure{"Saved Wi-Fi network not found."};
    }
    void preserveSecrets(const QString &path,Settings &s){
        if(!s.contains("802-11-wireless-security"))return;
        const auto secrets=qdbus_cast<Settings>(call(path,connectionInterface,"GetSecrets",{QString("802-11-wireless-security")}).value(0));
        for(auto group=secrets.begin();group!=secrets.end();++group){auto values=s.value(group.key());for(auto i=group->begin();i!=group->end();++i)values.insert(i.key(),i.value());s.insert(group.key(),values);}
    }
    QString security(const QVariantMap &p){
        const auto flags=plain(p.value("WpaFlags")).toUInt()|plain(p.value("RsnFlags")).toUInt();
        if(flags&0x100)return "wpa-psk";
        if(flags&0x400)return "sae";
        return plain(p.value("Flags")).toUInt()&1?"unsupported":"open";
    }
    void checkPassword(const QString &password,const QString &kind){
        const auto bytes=password.toUtf8();
        if(kind=="wpa-psk"&&!((bytes.size()>=8&&bytes.size()<=63)||(bytes.size()==64&&QRegularExpression("^[a-fA-F0-9]+$").match(password).hasMatch())))throw Failure{"WPA/WPA2 passwords need 8–63 bytes, or a 64-digit hexadecimal key."};
        if(kind=="sae"&&(bytes.isEmpty()||bytes.size()>63))throw Failure{"Enter a password of up to 63 bytes."};
    }
    void activate(const QString &path,const QString &dev){call(root,service,"ActivateConnection",{QVariant::fromValue(QDBusObjectPath(path)),QVariant::fromValue(QDBusObjectPath(dev)),QVariant::fromValue(QDBusObjectPath("/"))});}
public:
    QJsonObject snapshot(){
        const auto dev=device();const auto d=properties(dev,deviceInterface),w=properties(dev,wirelessInterface);
        auto manager=properties(root,service);
        QString currentUuid;
        const auto active=qdbus_cast<QDBusObjectPath>(plain(d.value("ActiveConnection"))).path();
        if(!active.isEmpty()&&active!="/")currentUuid=plain(properties(active,service+".Connection.Active").value("Uuid")).toString();
        QJsonArray saved;QMap<QByteArray,QString> savedSsid;
        for(const auto &path:profiles()){
            const auto s=settings(path.path());const auto c=s.value("connection");if(c.value("type")!="802-11-wireless")continue;
            const auto ssid=s.value("802-11-wireless").value("ssid").toByteArray();const auto uuid=c.value("uuid").toString();
            savedSsid.insert(ssid,uuid);
            saved.append(QJsonObject{{"id",uuid},{"name",c.value("id").toString()},{"ssid",QString::fromUtf8(ssid)},{"autoconnect",c.value("autoconnect",true).toBool()},
                {"priority",c.value("autoconnect-priority",0).toInt()},{"hidden",s.value("802-11-wireless").value("hidden").toBool()},
                {"security",s.contains("802-11-wireless-security")?s.value("802-11-wireless-security").value("key-mgmt").toString():QString("open")},{"connected",uuid==currentUuid}});
        }
        const auto aps=qdbus_cast<QList<QDBusObjectPath>>(call(dev,wirelessInterface,"GetAllAccessPoints").value(0));
        const auto activeAp=qdbus_cast<QDBusObjectPath>(plain(w.value("ActiveAccessPoint"))).path();
        QMap<QByteArray,QJsonObject> unique;
        for(const auto &path:aps.mid(0,96)){
            const auto p=properties(path.path(),service+".AccessPoint");const auto ssid=plain(p.value("Ssid")).toByteArray();if(ssid.isEmpty())continue;
            const auto kind=security(p);const int strength=plain(p.value("Strength")).toUInt();const bool connected=path.path()==activeAp;
            const auto key=ssid+'\0'+kind.toUtf8();
            if(unique.contains(key)&&!connected&&(unique[key].value("connected").toBool()||unique[key].value("strength").toInt()>=strength))continue;
            unique.insert(key,QJsonObject{{"id",path.path()},{"ssid",QString::fromUtf8(ssid)},{"strength",strength},{"security",kind},{"connected",connected},{"savedId",savedSsid.value(ssid)}});
        }
        auto list=unique.values();std::sort(list.begin(),list.end(),[](auto a,auto b){if(a.value("connected")!=b.value("connected"))return a.value("connected").toBool();return a.value("strength").toInt()>b.value("strength").toInt();});
        QJsonArray networks;for(const auto &n:list)networks.append(n);
        QStringList addresses;const auto ip=qdbus_cast<QDBusObjectPath>(plain(d.value("Ip4Config"))).path();
        if(!ip.isEmpty()&&ip!="/")for(const auto &a:qdbus_cast<QList<QVariantMap>>(properties(ip,service+".IP4Config").value("AddressData")))addresses<<a.value("address").toString();
        const auto state=plain(d.value("State")).toUInt();
        const QString status=state==100?"Connected":state==120?"Connection failed · check the password":state>=40&&state<100?"Connecting…":state==20?"Wi-Fi unavailable":"Not connected";
        return {{"ok",true},{"networks",networks},{"saved",saved},{"currentId",currentUuid},{"addresses",addresses.join(", ")},{"status",status},
            {"connecting",state>=40&&state<100},{"enabled",plain(manager.value("WirelessEnabled")).toBool()},{"lastScan",double(plain(w.value("LastScan")).toLongLong())}};
    }
    QJsonObject request(const QJsonObject &r){
        const auto action=r.value("action").toString();
        if(action=="status")return snapshot();
        const auto dev=device();
        if(action=="scan")call(dev,wirelessInterface,"RequestScan",{QVariantMap{}});
        else if(action=="disconnect")call(dev,deviceInterface,"Disconnect");
        else if(action=="forget")call(profile(r.value("id").toString()),connectionInterface,"Delete");
        else if(action=="preferences"){
            const auto priority=r.value("priority");
            if(!r.value("autoconnect").isBool()||!priority.isDouble()||priority.toDouble()!=priority.toInt()||priority.toInt()<-999||priority.toInt()>999)throw Failure{"Choose automatic connection and a priority from -999 to 999."};
            const auto path=profile(r.value("id").toString());auto s=settings(path);preserveSecrets(path,s);
            if(s.contains("802-1x"))throw Failure{"Edit enterprise-network preferences using NetworkManager's administration tools."};
            s["connection"].insert("autoconnect",r.value("autoconnect").toBool());s["connection"].insert("autoconnect-priority",priority.toInt());
            call(path,connectionInterface,"Update",{QVariant::fromValue(s)});
        }else if(action=="connectSaved"){
            const auto path=profile(r.value("id").toString());const auto password=r.value("password").toString();
            if(!password.isEmpty()){
                auto s=settings(path);auto security=s.value("802-11-wireless-security");const auto kind=security.value("key-mgmt").toString();
                if(kind!="wpa-psk"&&kind!="sae")throw Failure{"Password editing is supported for personal WPA networks only."};
                checkPassword(password,kind);preserveSecrets(path,s);s["802-11-wireless-security"].insert("psk",password);s["802-11-wireless-security"].insert("psk-flags",uint(0));
                call(path,connectionInterface,"Update",{QVariant::fromValue(s)});
            }
            activate(path,dev);
        }else if(action=="connect"){
            const bool hidden=r.value("hidden").toBool();QByteArray ssid;QString kind;
            if(hidden){ssid=r.value("ssid").toString().toUtf8();kind=r.value("security").toString();if(kind!="open"&&kind!="wpa-psk")throw Failure{"Choose open or WPA/WPA2 personal security."};}
            else{
                const auto id=r.value("id").toString();const auto aps=qdbus_cast<QList<QDBusObjectPath>>(call(dev,wirelessInterface,"GetAllAccessPoints").value(0));
                if(std::none_of(aps.begin(),aps.end(),[&](const auto &p){return p.path()==id;}))throw Failure{"Network is no longer available. Scan again."};
                const auto ap=properties(id,service+".AccessPoint");ssid=plain(ap.value("Ssid")).toByteArray();kind=security(ap);
            }
            if(ssid.isEmpty()||ssid.size()>32)throw Failure{"Network names must contain 1–32 UTF-8 bytes."};
            if(kind=="unsupported")throw Failure{"This network needs enterprise or legacy security setup outside this dialog."};
            // Reuse an existing profile instead of creating duplicates, and keep
            // its priority, hidden-network setting and other local customisation.
            for(const auto &p:profiles()){
                auto s=settings(p.path());if(s.value("connection").value("type")!="802-11-wireless"||s.value("802-11-wireless").value("ssid").toByteArray()!=ssid)continue;
                const auto oldKind=s.contains("802-11-wireless-security")?s.value("802-11-wireless-security").value("key-mgmt").toString():QString("open");
                if(kind!=oldKind)continue;
                if(kind!="open"){
                    const auto password=r.value("password").toString();checkPassword(password,kind);preserveSecrets(p.path(),s);
                    s["802-11-wireless-security"].insert("psk",password);s["802-11-wireless-security"].insert("psk-flags",uint(0));
                    call(p.path(),connectionInterface,"Update",{QVariant::fromValue(s)});
                }
                activate(p.path(),dev);return {{"ok",true},{"message","Connection requested"}};
            }
            Settings s{{"connection",{{"id","ThinkSmart: "+QString::fromUtf8(ssid)},{"uuid",QUuid::createUuid().toString(QUuid::WithoutBraces)},{"type","802-11-wireless"},{"autoconnect",true}}},
                {"802-11-wireless",{{"ssid",ssid},{"mode","infrastructure"},{"hidden",hidden}}},{"ipv4",{{"method","auto"}}},{"ipv6",{{"method","auto"}}}};
            if(kind!="open"){
                const auto password=r.value("password").toString();checkPassword(password,kind);
                s.insert("802-11-wireless-security",{{"key-mgmt",kind},{"psk",password},{"psk-flags",uint(0)}});
            }
            call(root,service,"AddAndActivateConnection",{QVariant::fromValue(s),QVariant::fromValue(QDBusObjectPath(dev)),QVariant::fromValue(QDBusObjectPath("/"))});
        }else throw Failure{"Unknown Wi-Fi operation."};
        return {{"ok",true},{"message",action=="scan"?"Scan requested":action=="disconnect"?"Disconnected":action.startsWith("connect")?"Connection requested":"Saved"}};
    }
};
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);qDBusRegisterMetaType<Settings>();qDBusRegisterMetaType<QList<QVariantMap>>();
    QString socket="/run/thinksmart-network/control.sock";uid_t allowed=0;gid_t group=0;
    if(argc==3&&QByteArray(argv[1])=="--test-directory"){
        const QFileInfo dir(QString::fromLocal8Bit(argv[2]));
        if(!dir.isDir()||dir.isSymLink()||dir.ownerId()!=getuid()||(dir.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther)))return 2;
        socket=dir.absoluteFilePath()+"/network.sock";allowed=getuid();group=getgid();
    }else{
        if(argc!=1||geteuid()!=0)return 2;
        const auto user=getpwnam("thinksmart");if(!user)return 2;allowed=user->pw_uid;group=user->pw_gid;
    }
    QLocalServer server;QLocalServer::removeServer(socket);if(!server.listen(socket))return 1;
    if(chown(socket.toLocal8Bit().constData(),geteuid(),group)!=0||chmod(socket.toLocal8Bit().constData(),0660)!=0)return 1;
    Network network;
    QObject::connect(&server,&QLocalServer::newConnection,&app,[&](){while(server.hasPendingConnections()){
        auto *client=server.nextPendingConnection();ucred peer{};socklen_t length=sizeof(peer);
        if(getsockopt(client->socketDescriptor(),SOL_SOCKET,SO_PEERCRED,&peer,&length)!=0||(peer.uid!=0&&peer.uid!=allowed)){client->abort();client->deleteLater();continue;}
        QObject::connect(client,&QLocalSocket::disconnected,client,&QObject::deleteLater);
        QTimer::singleShot(3000,client,[client](){if(!client->property("handled").toBool())client->disconnectFromServer();});
        QObject::connect(client,&QLocalSocket::readyRead,&app,[&,client](){
            if(client->property("handled").toBool())return;
            auto bytes=client->property("input").toByteArray()+client->readAll();
            if(bytes.size()>16384){client->abort();return;}client->setProperty("input",bytes);if(!bytes.contains('\n'))return;
            client->setProperty("handled",true);client->setProperty("input",QVariant{});
            QJsonObject reply;
            try{QJsonParseError parse;const auto doc=QJsonDocument::fromJson(bytes.left(bytes.indexOf('\n')),&parse);
                if(parse.error!=QJsonParseError::NoError||!doc.isObject())throw Failure{"Expected one Wi-Fi request object."};
                reply=network.request(doc.object());
            }catch(const Failure &f){reply=error(f.message);}catch(...){reply=error("Wi-Fi request failed.");}
            bytes.fill('\0');client->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');client->disconnectFromServer();
        });
    }});
    return app.exec();
}
