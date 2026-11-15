#include <QtTest>
#include <QDBusVirtualObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QTemporaryDir>
#include <QLocalSocket>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QScopeGuard>

using Settings=QMap<QString,QVariantMap>;
Q_DECLARE_METATYPE(Settings)
class FakeNetwork:public QDBusVirtualObject {
public:
    QString base="/org/freedesktop/NetworkManager",uuid="11111111-1111-1111-1111-111111111111";
    Settings wifi,ethernet,created,updated;
    int scans=0,activations=0,deletions=0,updates=0;
    FakeNetwork(){reset();}
    void reset(){
        wifi={{"connection",{{"id","Fixture WPA"},{"uuid",uuid},{"type","802-11-wireless"},{"autoconnect",true},{"autoconnect-priority",100}}},
            {"802-11-wireless",{{"ssid",QByteArray("Fixture WPA")},{"hidden",false}}},{"802-11-wireless-security",{{"key-mgmt","wpa-psk"},{"psk-flags",uint(0)}}}};
        ethernet={{"connection",{{"id","Ethernet fixture"},{"uuid","22222222-2222-2222-2222-222222222222"},{"type","802-3-ethernet"}}}};
        created.clear();updated.clear();scans=activations=deletions=updates=0;
    }
    QString introspect(const QString &)const override{return {};}
    bool handleMessage(const QDBusMessage &m,const QDBusConnection &bus)override{
        const auto path=m.path(),method=m.member();QVariantList out;
        auto reply=[&](QVariantList values={}){bus.send(m.createReply(values));return true;};
        if(method=="GetDevices")return reply({QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(base+"/Devices/1")})});
        if(method=="ListConnections")return reply({QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(base+"/Settings/1"),QDBusObjectPath(base+"/Settings/2")})});
        if(method=="GetSettings")return reply({QVariant::fromValue(path.endsWith("/1")?wifi:ethernet)});
        if(method=="GetSecrets")return reply({QVariant::fromValue(Settings{{"802-11-wireless-security",{{"psk","fixture-original-password"}}}})});
        if(method=="GetAllAccessPoints")return reply({QVariant::fromValue(QList<QDBusObjectPath>{QDBusObjectPath(base+"/AccessPoint/1"),QDBusObjectPath(base+"/AccessPoint/2"),QDBusObjectPath(base+"/AccessPoint/3")})});
        if(method=="GetAll"){
            QVariantMap p;const auto iface=m.arguments().value(0).toString();
            if(path==base)p={{"WirelessEnabled",true}};
            else if(path.endsWith("/Devices/1")&&iface.endsWith("Wireless"))p={{"ActiveAccessPoint",QVariant::fromValue(QDBusObjectPath(base+"/AccessPoint/1"))},{"LastScan",qlonglong(123)}};
            else if(path.endsWith("/Devices/1"))p={{"DeviceType",uint(2)},{"State",uint(100)},{"ActiveConnection",QVariant::fromValue(QDBusObjectPath(base+"/ActiveConnection/1"))},{"Ip4Config",QVariant::fromValue(QDBusObjectPath(base+"/IP4Config/1"))}};
            else if(path.contains("/ActiveConnection/"))p={{"Uuid",uuid}};
            else if(path.contains("/IP4Config/"))p={{"AddressData",QVariant::fromValue(QList<QVariantMap>{QVariantMap{{"address","192.0.2.10"}}})}};
            else if(path.contains("/AccessPoint/")){
                const bool secured=path.endsWith("/1");
                p={{"Ssid",QByteArray(secured?"Fixture WPA":path.endsWith("/2")?"Fixture Open":"Enterprise")},{"Strength",uchar(secured?75:50)},
                    {"Flags",uint(secured||path.endsWith("/3")?1:0)},{"WpaFlags",uint(0)},{"RsnFlags",uint(secured?0x100:path.endsWith("/3")?0x200:0)}};
            }
            return reply({p});
        }
        if(method=="RequestScan"){scans++;return reply();}
        if(method=="ActivateConnection"){activations++;return reply({QVariant::fromValue(QDBusObjectPath(base+"/ActiveConnection/1"))});}
        if(method=="AddAndActivateConnection"){
            created=qdbus_cast<Settings>(m.arguments().first());activations++;
            return reply({QVariant::fromValue(QDBusObjectPath(base+"/Settings/3")),QVariant::fromValue(QDBusObjectPath(base+"/ActiveConnection/3"))});
        }
        if(method=="Update"){updated=qdbus_cast<Settings>(m.arguments().first());wifi=updated;updates++;return reply();}
        if(method=="Delete"){deletions++;return reply();}
        if(method=="Disconnect")return reply();
        bus.send(m.createErrorReply("org.freedesktop.DBus.Error.UnknownMethod","Unsupported mock request"));return true;
    }
};
class NetworkTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QProcess helper;
    FakeNetwork network;
    QJsonObject request(QJsonObject r){
        QLocalSocket client;client.connectToServer(directory.filePath("network.sock"));
        if(!client.waitForConnected(1000))return {};
        client.write(QJsonDocument(r).toJson(QJsonDocument::Compact)+'\n');client.flush();
        QElapsedTimer timer;timer.start();QByteArray bytes;
        while(!bytes.contains('\n')&&timer.elapsed()<10000){QCoreApplication::processEvents();bytes+=client.readAll();QTest::qWait(5);}
        return QJsonDocument::fromJson(bytes.trimmed()).object();
    }
private slots:
    void initTestCase(){
        qDBusRegisterMetaType<Settings>();qDBusRegisterMetaType<QList<QVariantMap>>();
        QVERIFY(QDBusConnection::sessionBus().registerService("org.freedesktop.NetworkManager"));
        QVERIFY(QDBusConnection::sessionBus().registerVirtualObject(network.base,&network,QDBusConnection::SubPath));
        qputenv("DBUS_SYSTEM_BUS_ADDRESS",qgetenv("DBUS_SESSION_BUS_ADDRESS"));
        helper.start(QCoreApplication::applicationDirPath()+"/thinksmart-network",{"--test-directory",directory.path()});QVERIFY(helper.waitForStarted());
        QTRY_VERIFY(QFile::exists(directory.filePath("network.sock")));
    }
    void init(){network.reset();}
    void listsWifiAndSavedProfilesWithoutSecrets(){
        auto result=request({{"action","status"}});QVERIFY(result.value("ok").toBool());
        QCOMPARE(result.value("networks").toArray().size(),3);QCOMPARE(result.value("saved").toArray().size(),1);
        QCOMPARE(result.value("addresses").toString(),QString("192.0.2.10"));
        QVERIFY(!QJsonDocument(result).toJson().contains("fixture-original-password"));
        QCOMPARE(result.value("saved").toArray().first().toObject().value("priority").toInt(),100);
        QVERIFY(request({{"action","scan"}}).value("ok").toBool());QCOMPARE(network.scans,1);
    }
    void connectsSavedWithoutChangingCredentials(){
        QVERIFY(request({{"action","connectSaved"},{"id",network.uuid}}).value("ok").toBool());
        QCOMPARE(network.activations,1);QCOMPARE(network.updates,0);
        QVERIFY(request({{"action","connectSaved"},{"id",network.uuid},{"password","new-fixture-password"}}).value("ok").toBool());
        QCOMPARE(network.updated.value("802-11-wireless-security").value("psk").toString(),QString("new-fixture-password"));
    }
    void openAndHiddenProfilesHaveCorrectSecurity(){
        QVERIFY(request({{"action","connect"},{"id",network.base+"/AccessPoint/2"}}).value("ok").toBool());
        QVERIFY(!network.created.contains("802-11-wireless-security"));
        QVERIFY(request({{"action","connect"},{"hidden",true},{"ssid","Hidden fixture"},{"security","open"}}).value("ok").toBool());
        QVERIFY(network.created.value("802-11-wireless").value("hidden").toBool());
        QVERIFY(!network.created.contains("802-11-wireless-security"));
        QVERIFY(request({{"action","connect"},{"hidden",true},{"ssid","Hidden secure"},{"security","wpa-psk"},{"password","fixture-password"}}).value("ok").toBool());
        QCOMPARE(network.created.value("802-11-wireless-security").value("psk").toString(),QString("fixture-password"));
    }
    void preferencesKeepExistingPasswordsAndRestrictProfiles(){
        QVERIFY(request({{"action","preferences"},{"id",network.uuid},{"autoconnect",false},{"priority",10}}).value("ok").toBool());
        QCOMPARE(network.updated.value("connection").value("autoconnect").toBool(),false);
        QCOMPARE(network.updated.value("802-11-wireless-security").value("psk").toString(),QString("fixture-original-password"));
        QVERIFY(!request({{"action","forget"},{"id","22222222-2222-2222-2222-222222222222"}}).value("ok").toBool());
        QCOMPARE(network.deletions,0);
        QVERIFY(request({{"action","forget"},{"id",network.uuid}}).value("ok").toBool());QCOMPARE(network.deletions,1);
    }
    void rejectsUnknownOperationsBadPasswordsAndPaths(){
        QVERIFY(!request({{"action","shell"},{"command","anything"}}).value("ok").toBool());
        QVERIFY(!request({{"action","connect"},{"id","/arbitrary/path"}}).value("ok").toBool());
        QVERIFY(!request({{"action","connect"},{"hidden",true},{"ssid","Test"},{"security","wpa-psk"},{"password","short"}}).value("ok").toBool());
        QVERIFY(!request({{"action","connect"},{"id",network.base+"/AccessPoint/3"}}).value("ok").toBool());
        QCOMPARE(network.activations,0);
    }
    void cleanupTestCase(){helper.terminate();QVERIFY(helper.waitForFinished(2000));QVERIFY(helper.readAllStandardError().isEmpty());}
};
QTEST_GUILESS_MAIN(NetworkTests)
#include "network-tests.moc"
