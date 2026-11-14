#include "deviceconfig.h"
#include "bluetooth.h"
#include <QJsonObject>
#include <sys/prctl.h>
#include <csignal>

QDBusArgument &operator<<(QDBusArgument &a,const BluezInterfaces &v){a.beginMap(QMetaType::fromType<QString>(),QMetaType::fromType<QVariantMap>());for(auto i=v.cbegin();i!=v.cend();++i){a.beginMapEntry();a<<i.key()<<i.value();a.endMapEntry();}a.endMap();return a;}
const QDBusArgument &operator>>(const QDBusArgument &a,BluezInterfaces &v){a.beginMap();while(!a.atEnd()){QString k;QVariantMap x;a.beginMapEntry();a>>k>>x;a.endMapEntry();v.insert(k,x);}a.endMap();return a;}
QDBusArgument &operator<<(QDBusArgument &a,const BluezObjects &v){a.beginMap(QMetaType::fromType<QDBusObjectPath>(),QMetaType::fromType<BluezInterfaces>());for(auto i=v.cbegin();i!=v.cend();++i){a.beginMapEntry();a<<i.key()<<i.value();a.endMapEntry();}a.endMap();return a;}
const QDBusArgument &operator>>(const QDBusArgument &a,BluezObjects &v){a.beginMap();while(!a.atEnd()){QDBusObjectPath k;BluezInterfaces x;a.beginMapEntry();a>>k>>x;a.endMapEntry();v.insert(k,x);}a.endMap();return a;}

PairingAgent::PairingAgent(BluetoothAudio *owner):QObject(owner),m_owner(owner){}
bool PairingAgent::legitimate(){
    if(!calledFromDBus())return false;
    const auto owner=QDBusConnection::systemBus().interface()->serviceOwner("org.bluez");
    if(!owner.isValid()||message().service()!=owner.value()){sendErrorReply("org.bluez.Error.Rejected","Unrecognized pairing service");return false;}
    return true;
}
void PairingAgent::answer(bool accept){
    if(m_pending.type()==QDBusMessage::InvalidMessage){m_owner->cancelConfirmation();return;}
    if(accept)m_owner->m_approved.insert(m_device);
    QDBusConnection::systemBus().send(accept?m_pending.createReply():m_pending.createErrorReply("org.bluez.Error.Rejected","Pairing declined"));
    m_pending=QDBusMessage();m_device.clear();m_owner->cancelConfirmation();
}
void PairingAgent::ask(const QDBusObjectPath &device,const QString &pin){
    if(!legitimate())return;
    if(!m_owner->pairing()||m_pending.type()!=QDBusMessage::InvalidMessage){sendErrorReply("org.bluez.Error.Rejected","Open the pairing window first");return;}
    setDelayedReply(true);m_pending=message();m_device=device.path();m_owner->askConfirmation(device.path(),pin);
}
void PairingAgent::Release(){if(!legitimate())return;answer(false);m_owner->m_registered=false;}
void PairingAgent::Cancel(){if(!legitimate())return;answer(false);}
QString PairingAgent::RequestPinCode(const QDBusObjectPath &){if(legitimate())sendErrorReply("org.bluez.Error.Rejected","Legacy PIN pairing is unsupported");return {};}
uint PairingAgent::RequestPasskey(const QDBusObjectPath &){if(legitimate())sendErrorReply("org.bluez.Error.Rejected","Passkey entry is unsupported");return 0;}
void PairingAgent::DisplayPinCode(const QDBusObjectPath &,const QString &){if(legitimate())sendErrorReply("org.bluez.Error.Rejected","Legacy PIN pairing is unsupported");}
void PairingAgent::DisplayPasskey(const QDBusObjectPath &device,uint pin,ushort){if(legitimate())m_owner->askConfirmation(device.path(),QString("%1").arg(pin,6,10,QChar('0')));}
void PairingAgent::RequestConfirmation(const QDBusObjectPath &device,uint pin){ask(device,QString("%1").arg(pin,6,10,QChar('0')));}
void PairingAgent::RequestAuthorization(const QDBusObjectPath &device){ask(device,{});}
void PairingAgent::AuthorizeService(const QDBusObjectPath &device,const QString &uuid){
    if(!legitimate())return;
    const QString code=uuid.toLower().mid(4,4);
    if(!m_owner->allowedDevice(device.path())||!QStringList{"110a","110b","110c","110d","110e","110f"}.contains(code))sendErrorReply("org.bluez.Error.Rejected","Audio service not authorized");
}
BluetoothAudio::BluetoothAudio(QObject *parent):QObject(parent),m_bus(QDBusConnection::systemBus()),m_agent(this){
    qDBusRegisterMetaType<BluezInterfaces>();qDBusRegisterMetaType<BluezObjects>();
    m_receiver.setChildProcessModifier([](){prctl(PR_SET_PDEATHSIG,SIGTERM);});
    connect(&m_receiver,&QProcess::readyReadStandardError,this,[this](){m_receiver.readAllStandardError();});
    connect(&m_receiver,&QProcess::errorOccurred,this,[this](){if(!m_active)return;m_active=false;m_problem="Cannot start Bluetooth audio receiver.";emit changed();});
    connect(&m_receiver,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){if(m_active&&code!=0)m_problem="Bluetooth audio receiver stopped.";m_active=false;emit changed();});
    m_pairTimer.setSingleShot(true);m_pairTimer.setInterval(180000);connect(&m_pairTimer,&QTimer::timeout,this,&BluetoothAudio::endPairing);
    m_poll.setInterval(2000);connect(&m_poll,&QTimer::timeout,this,&BluetoothAudio::refresh);
    if(DeviceConfig::enabled("bluetooth")&&!qEnvironmentVariableIsSet("THINKSMART_DISABLE_BLUETOOTH")){m_bus.registerObject("/org/thinksmart/agent",&m_agent,QDBusConnection::ExportAllSlots);m_poll.start();QTimer::singleShot(0,this,&BluetoothAudio::refresh);}
}
BluetoothAudio::~BluetoothAudio(){
    m_agent.answer(false);m_receiver.disconnect(this);if(m_receiver.state()!=QProcess::NotRunning){m_receiver.terminate();if(!m_receiver.waitForFinished(500)){m_receiver.kill();m_receiver.waitForFinished(500);}}
    if(m_registered){auto msg=QDBusMessage::createMethodCall("org.bluez","/org/bluez","org.bluez.AgentManager1","UnregisterAgent");msg<<QVariant::fromValue(QDBusObjectPath("/org/thinksmart/agent"));m_bus.asyncCall(msg);}
    if(pairing())endPairing();m_bus.unregisterObject("/org/thinksmart/agent");
}
void BluetoothAudio::call(const QString &path,const QString &interface,const QString &method,const QVariantList &args,std::function<void(bool)> done){
    auto msg=QDBusMessage::createMethodCall("org.bluez",path,interface,method);msg.setArguments(args);
    auto watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(msg,5000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher,done](){QDBusPendingReply<> reply=*watcher;bool ok=!reply.isError();if(!ok){m_problem="Bluetooth request failed. Check the phone connection.";emit changed();}if(done)done(ok);watcher->deleteLater();});
}
void BluetoothAudio::setAdapter(const QString &key,const QVariant &value){if(!m_adapter.isEmpty())call(m_adapter,"org.freedesktop.DBus.Properties","Set",{"org.bluez.Adapter1",key,QVariant::fromValue(QDBusVariant(value))});}
void BluetoothAudio::registerAgent(){
    if(m_registered)return;
    call("/org/bluez","org.bluez.AgentManager1","RegisterAgent",{QVariant::fromValue(QDBusObjectPath("/org/thinksmart/agent")),"DisplayYesNo"},[this](bool ok){if(ok){m_registered=true;call("/org/bluez","org.bluez.AgentManager1","RequestDefaultAgent",{QVariant::fromValue(QDBusObjectPath("/org/thinksmart/agent"))},[this](bool ok){if(ok&&!pairing()){setAdapter("Pairable",false);setAdapter("Discoverable",false);}});}});
}
void BluetoothAudio::refresh(){
    if(!DeviceConfig::enabled("bluetooth")||m_refreshing||!m_bus.isConnected())return;m_refreshing=true;
    auto message=QDBusMessage::createMethodCall("org.bluez","/","org.freedesktop.DBus.ObjectManager","GetManagedObjects");
    auto watcher=new QDBusPendingCallWatcher(m_bus.asyncCall(message,3000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher](){
        m_refreshing=false;QDBusPendingReply<BluezObjects> reply=*watcher;watcher->deleteLater();
        if(reply.isError()){m_adapter.clear();m_registered=false;emit changed();return;}
        m_objects=reply.value();QString adapter,device;QVariantMap deviceProps;m_devices.clear();m_streaming=false;
        for(auto i=m_objects.cbegin();i!=m_objects.cend();++i){
            if(i.value().contains("org.bluez.Adapter1"))adapter=i.key().path();
            auto props=i.value().value("org.bluez.Device1");if(props.isEmpty())continue;
            if(props.value("Paired").toBool())m_devices.append(QVariantMap{{"path",i.key().path()},{"name",props.value("Alias")},{"connected",props.value("Connected").toBool()}});
            if(props.value("Connected").toBool()&&device.isEmpty()){device=i.key().path();deviceProps=props;}
            if(props.value("Paired").toBool()&&m_approved.contains(i.key().path())){
                call(i.key().path(),"org.freedesktop.DBus.Properties","Set",{"org.bluez.Device1","Trusted",QVariant::fromValue(QDBusVariant(true))});m_approved.remove(i.key().path());
            }
        }
        for(auto i=m_objects.cbegin();i!=m_objects.cend();++i){auto t=i.value().value("org.bluez.MediaTransport1");if(t.value("State").toString()=="active"){device=qvariant_cast<QDBusObjectPath>(t.value("Device")).path();deviceProps=m_objects.value(QDBusObjectPath(device)).value("org.bluez.Device1");m_streaming=true;break;}}
        m_adapter=adapter;m_device=device;m_phoneName=deviceProps.value("Alias").toString();m_player.clear();m_trackTitle.clear();m_trackArtist.clear();m_playState.clear();
        for(auto i=m_objects.cbegin();i!=m_objects.cend();++i)if(!device.isEmpty()&&i.key().path().startsWith(device+"/")&&i.value().contains("org.bluez.MediaPlayer1")){
            m_player=i.key().path();auto props=i.value().value("org.bluez.MediaPlayer1");m_playState=props.value("Status").toString();auto track=qdbus_cast<QVariantMap>(props.value("Track"));m_trackTitle=track.value("Title").toString();m_trackArtist=track.value("Artist").toString();break;
        }
        if(!m_adapter.isEmpty()&&!m_registered)registerAgent();emit changed();
    });
}
bool BluetoothAudio::allowedDevice(const QString &path)const{return m_approved.contains(path)||m_objects.value(QDBusObjectPath(path)).value("org.bluez.Device1").value("Paired").toBool();}
void BluetoothAudio::askConfirmation(const QString &device,const QString &pin){m_confirmationName=m_objects.value(QDBusObjectPath(device)).value("org.bluez.Device1").value("Alias","Phone").toString();m_confirmationCode=pin;emit changed();}
void BluetoothAudio::cancelConfirmation(){m_confirmationName.clear();m_confirmationCode.clear();emit changed();}
void BluetoothAudio::confirmPairing(bool accept){m_agent.answer(accept);}
void BluetoothAudio::startPairing(){if(!DeviceConfig::enabled("bluetooth"))return;if(!available()){m_problem="Bluetooth adapter is unavailable.";emit changed();return;}m_problem.clear();setAdapter("Powered",true);setAdapter("Alias","ThinkSmart Speaker");setAdapter("DiscoverableTimeout",uint(180));setAdapter("PairableTimeout",uint(180));setAdapter("Pairable",true);setAdapter("Discoverable",true);m_pairTimer.start();emit changed();}
void BluetoothAudio::endPairing(){m_pairTimer.stop();m_agent.answer(false);setAdapter("Discoverable",false);setAdapter("Pairable",false);cancelConfirmation();emit changed();}
void BluetoothAudio::connectPhone(const QString &path){if(!allowedDevice(path))return;for(const auto &d:m_devices)if(d.toMap().value("connected").toBool()&&d.toMap().value("path").toString()!=path)call(d.toMap().value("path").toString(),"org.bluez.Device1","Disconnect");call(path,"org.bluez.Device1","Connect",{},[this](bool){refresh();});}
void BluetoothAudio::disconnectPhone(){if(!m_device.isEmpty())call(m_device,"org.bluez.Device1","Disconnect",{},[this](bool){refresh();});}
void BluetoothAudio::forgetPhone(const QString &path){if(allowedDevice(path))call(m_adapter,"org.bluez.Adapter1","RemoveDevice",{QVariant::fromValue(QDBusObjectPath(path))},[this](bool){refresh();});}
void BluetoothAudio::startReceiver(){if(!DeviceConfig::enabled("bluetooth"))return;if(m_receiver.state()!=QProcess::NotRunning)return;m_problem.clear();m_active=true;m_receiver.start("bluealsa-aplay",{"--profile-a2dp","--pcm=default","--single-audio","--volume=software"});emit changed();}
void BluetoothAudio::stopReceiver(){m_active=false;m_receiver.terminate();emit changed();}
void BluetoothAudio::killReceiver(){m_receiver.kill();}
void BluetoothAudio::transport(const QString &method){if(!m_player.isEmpty())call(m_player,"org.bluez.MediaPlayer1",method);else if(!m_device.isEmpty())call(m_device,"org.bluez.MediaControl1",method);else{m_problem="Connect a phone, then start playback on it.";emit changed();}}
QString BluetoothAudio::state()const{if(!m_active)return "stopped";if(!m_problem.isEmpty()&&m_receiver.state()==QProcess::NotRunning)return "error";if(m_streaming||m_playState=="playing")return "playing";return connected()?"paused":"stopped";}
QString BluetoothAudio::status()const{if(!available())return "Bluetooth unavailable";if(!m_problem.isEmpty())return m_problem;if(pairing())return "Visible as ThinkSmart Speaker for three minutes";if(!connected())return "Pair a phone to use this device as a speaker";if(!active())return m_phoneName+" connected · phone audio is off";return state()=="playing"?"Playing from "+m_phoneName:"Ready · start playback on "+m_phoneName;}
QJsonObject BluetoothAudio::snapshot()const{return {{"available",available()},{"connected",connected()},{"pairing",pairing()},{"active",active()},{"state",state()},{"status",status()}};}
